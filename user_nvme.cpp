#include "user_nvme.h"
#include "dma_pool.h"

#include <cstdio>
#include <cstring>
#include <iostream>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/mman.h>

#define QUEUE_SIZE     64
#define CMD_ENTRY_SIZE 64
#define CMP_ENTRY_SIZE 16

using std::cout;
using std::endl;

UserNVMe::UserNVMe()
    : m_bar0(nullptr), m_device(-1), m_group(-1), m_container(-1),
      m_asq(nullptr), m_acq(nullptr) {}

UserNVMe::~UserNVMe() {
    if (m_bar0 && m_bar0 != (uint8_t*)(-1))
        munmap(m_bar0, reg.size);
    if (m_device > 0)
        close(m_device);
    if (m_group > 0)
        close(m_group);
    if (m_container)
        close(m_container);
}

int UserNVMe::initMMIO() {
    m_container = open("/dev/vfio/vfio", O_RDWR);
    if (m_container < 0) return perror("vfio"), 1;

    ioctl(m_container, VFIO_GET_API_VERSION);
    ioctl(m_container, VFIO_CHECK_EXTENSION, VFIO_TYPE1_IOMMU);

    m_group = open("/dev/vfio/13", O_RDWR);
    if (m_group < 0) return perror("vfio group"), 2;

    ioctl(m_group, VFIO_GROUP_SET_CONTAINER, &m_container);
    ioctl(m_container, VFIO_SET_IOMMU, VFIO_TYPE1_IOMMU);

    m_device = ioctl(m_group, VFIO_GROUP_GET_DEVICE_FD, "0000:01:00.0");
    if (m_device < 0) return perror("device fd"), 3;

    ioctl(m_device, VFIO_DEVICE_GET_REGION_INFO, &reg);

    m_bar0 = (uint8_t*)mmap(nullptr, reg.size, PROT_READ | PROT_WRITE, MAP_SHARED, m_device, reg.offset);
    if (m_bar0 == MAP_FAILED) return perror("mmap"), 4;

    printf("Mapped BAR0 at %p\n", m_bar0);
    return 0;
}

void UserNVMe::printCap() {
    auto cap = *(volatile uint64_t*)(m_bar0 + NVME_REG_CAP);
    printf("NVMe Cap: 0x%016lx\n", cap);

    uint16_t mqes = cap & 0xFFFF;
    uint8_t dstrd = (cap >> 32) & 0xF;
    uint8_t mpsmin = (cap >> 48) & 0xF;
    uint8_t mpsmax = (cap >> 52) & 0xF;

    printf("  MQES   = 0x%04x\n", mqes);
    printf("  DSTRD  = 0x%x (stride = %u bytes)\n", dstrd, 1 << (2 + dstrd));
    printf("  MPSMIN = 0x%x (%u bytes)\n", mpsmin, 1 << (12 + mpsmin));
    printf("  MPSMAX = 0x%x (%u bytes)\n", mpsmax, 1 << (12 + mpsmax));
}

void UserNVMe::printVersion() {
    uint32_t ver = *(volatile uint32_t*)(m_bar0 + NVME_REG_VS);
    printf("NVMe Version: %d.%d.%d\n", ver >> 16, (ver >> 8) & 0xFF, ver & 0xFF);
}

int UserNVMe::setupAdminQueue() {
    m_asq = aligned_alloc(4096, QUEUE_SIZE * CMD_ENTRY_SIZE);
    m_acq = aligned_alloc(4096, QUEUE_SIZE * CMP_ENTRY_SIZE);
    if (!m_asq || !m_acq) return perror("queue alloc"), -1;

    *(volatile uint32_t*)(m_bar0 + NVME_REG_AQA) =
        ((QUEUE_SIZE - 1) << 16) | (QUEUE_SIZE - 1);

    uint64_t asq_dma = mapDMA(m_container, m_asq, QUEUE_SIZE * CMD_ENTRY_SIZE, 0x100000);
    uint64_t acq_dma = mapDMA(m_container, m_acq, QUEUE_SIZE * CMD_ENTRY_SIZE, 0x200000);

    *(volatile uint64_t*)(m_bar0 + NVME_REG_ASQ) = asq_dma;
    *(volatile uint64_t*)(m_bar0 + NVME_REG_ACQ) = acq_dma;

    *(volatile uint32_t*)(m_bar0 + NVME_REG_CC) =
        (6 << 20) | (4 << 16) | 1;  // IOSQES, IOCQES, EN

    while (!(*(volatile uint32_t*)(m_bar0 + NVME_REG_CSTS) & 0x1))
        usleep(1000);

    return 0;
}

uint64_t UserNVMe::mapDMA(int container_fd, void* vaddr, size_t size, uint64_t iova) {
    struct vfio_iommu_type1_dma_map dma_map = {0};

    dma_map.argsz = sizeof(dma_map);
    dma_map.flags = VFIO_DMA_MAP_FLAG_READ | VFIO_DMA_MAP_FLAG_WRITE;
    dma_map.vaddr = (uintptr_t)vaddr;
    dma_map.size = size;
    dma_map.iova = iova;

    if (ioctl(container_fd, VFIO_IOMMU_MAP_DMA, &dma_map) < 0) {
        perror("VFIO_IOMMU_MAP_DMA");
        return 0;
    }

    return iova;
}

void UserNVMe::identifyController() {
    void* id_buf = aligned_alloc(4096, 4096);
    if (!id_buf) return perror("alloc id_buf"), void();

    memset(id_buf, 0, 4096);
    uint64_t id_dma = mapDMA(m_container, id_buf, 4096, 0x300000);
    if (!id_dma) return;

    auto* sqe = reinterpret_cast<nvme_command*>(m_asq);
    memset(sqe, 0, sizeof(nvme_command));
    sqe->opc = 0x06;
    sqe->cid = 1;
    sqe->nsid = 0;
    sqe->prp1 = id_dma;
    sqe->prp2 = 0;
    sqe->cdw10 = 1;

    *(volatile uint32_t*)(m_bar0 + NVME_REG_SQTDBL) = 1;
    __sync_synchronize();

    auto* cqe = reinterpret_cast<nvme_completion*>(m_acq);
    const int max_wait_ms = 1000;
    int waited = 0;

    while (((cqe->status & 1) != 1) || (cqe->cid != 1)) {
        usleep(1000);
        if (++waited >= max_wait_ms) {
            fprintf(stderr, "Timeout waiting for Identify command completion\n");
            return;
        }
    }

    printf("Completion status: 0x%04x, CID: %u\n", cqe->status, cqe->cid);
    printf("Status Code: 0x%x, Type: 0x%x\n",
           (cqe->status >> 1) & 0x7FF, (cqe->status >> 9) & 0x7);

    uint8_t* id_data = static_cast<uint8_t*>(id_buf);
    printf("Model Number: %.40s\n", id_data + 24);
    printf("Serial Number: %.20s\n", id_data + 4);
    printf("Firmware Rev:  %.8s\n", id_data + 64);
}

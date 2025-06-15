#include <stdio.h>

#include <fcntl.h>
#include <sys/ioctl.h>

#include <sys/mman.h>
#include <unistd.h>

#include <iostream>

using namespace std;

//1c5c:1f69

#include "user_nvme.h"

UserNVMe::UserNVMe() : m_bar0(nullptr), m_device(-1), m_group(-1), m_container(-1) {}

UserNVMe::~UserNVMe() {
    // Clean up
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
    if (m_container < 0) { perror("vfio"); return 1; }

    // Set container to work with VFIO
    ioctl(m_container, VFIO_GET_API_VERSION);
    ioctl(m_container, VFIO_CHECK_EXTENSION, VFIO_TYPE1_IOMMU);

    // Open the IOMMU group (find your group number in /sys/kernel/iommu_groups)
    m_group = open("/dev/vfio/13", O_RDWR); // e.g., /dev/vfio/1
    if (m_group < 0) {
        perror("device");
        return 2;
    }

    ioctl(m_group, VFIO_GROUP_SET_CONTAINER, &m_container);

    // Enable IOMMU
    ioctl(m_container, VFIO_SET_IOMMU, VFIO_TYPE1_IOMMU);

    // Get device fd for your PCI device
    m_device = ioctl(m_group, VFIO_GROUP_GET_DEVICE_FD, "0000:01:00.0");
    if (m_device < 0) {
        perror("device");
        return 3;
    }

    // Map BAR0
    
    ioctl(m_device, VFIO_DEVICE_GET_REGION_INFO, &reg);

    m_bar0 = (uint8_t*)mmap(0, reg.size, PROT_READ | PROT_WRITE, MAP_SHARED, m_device, reg.offset);

    if (m_bar0 == MAP_FAILED) {
        perror("mmap");
        return 4;
    }


    // Now bar0 points to NVMe controller registers
    // e.g., doorbell registers, admin queue registers, etc.

    // You can create your submission & completion queues here in memory,
    // ring doorbells via bar0, and poll completion queues.

    printf("Mapped BAR0 at %p\n", m_bar0);

    return 0;
}

void UserNVMe::printCap() {
    volatile uint64_t * pcap = (volatile uint64_t*)((uint8_t*)m_bar0 + 0x0000);
    uint64_t cap = *pcap;
    printf("NVMe Cap: 0x%016lx\n", cap);

        // Optionally decode fields (per NVMe spec)
    uint16_t mqes = (cap >> 0) & 0xFFFF;    // Max Queue Entries Supported
    uint8_t  cqr  = (cap >> 16) & 0x1;      // Contiguous Queues Required
    uint8_t  ams  = (cap >> 17) & 0x3;      // Arbitration Mechanisms Supported
    uint8_t  to   = (cap >> 24) & 0xFF;     // Timeout (in 500ms units)
    uint8_t  dstrd = (cap >> 32) & 0xF;     // Doorbell Stride
    uint8_t  nssrs = (cap >> 36) & 0x1;     // NVM Subsystem Reset Supported
    uint8_t  css   = (cap >> 37) & 0xFF;    // Command Sets Supported
    uint8_t  mpsmin = (cap >> 48) & 0xF;    // Minimum Memory Page Size
    uint8_t  mpsmax = (cap >> 52) & 0xF;    // Maximum Memory Page Size

    printf("Decoded CAP fields:\n");
    printf("  MQES   = 0x%04x (max queue entries)\n", mqes);
    printf("  CQR    = %u\n", cqr);
    printf("  AMS    = 0x%x\n", ams);
    printf("  TO     = %u (%.1f sec)\n", to, to * 0.5);
    printf("  DSTRD  = 0x%x (stride = %u bytes)\n", dstrd, 1 << (2 + dstrd));
    printf("  NSSRS  = %u\n", nssrs);
    printf("  CSS    = 0x%x\n", css);
    printf("  MPSMIN = 0x%x (min page size = %u bytes)\n", mpsmin, 1 << (12 + mpsmin));
    printf("  MPSMAX = 0x%x (max page size = %u bytes)\n", mpsmax, 1 << (12 + mpsmax));
}
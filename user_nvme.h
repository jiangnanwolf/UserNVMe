#ifndef _USER_NVME_H_
#define _USER_NVME_H_

#include <stdint.h>
#include <linux/vfio.h>
#include <cstddef>

#define NVME_REG_CAP    (0x00)
#define NVME_REG_VS     (0x08)
#define NVME_REG_INTMS  (0x0C)
#define NVME_REG_INTMC  (0x10)
#define NVME_REG_CC     (0x14)
#define NVME_REG_CSTS   (0x1C)
#define NVME_REG_AQA    (0x24)
#define NVME_REG_ASQ    (0x28)
#define NVME_REG_ACQ    (0x30)
#define NVME_REG_CMBLOC (0x38)
#define NVME_REG_CMBSZ  (0x3C)
#define NVME_REG_CRTO   (0x20)
#define NVME_REG_SQTDBL (0x1000 + 0x00)
#define NVME_REG_CQHDBL (0x1000 + 0x04)

struct nvme_command {
    uint8_t opc;      // Opcode
    uint8_t fuse;     // Fused operation
    uint16_t cid;     // Command Identifier
    uint32_t nsid;    // Namespace ID
    uint64_t rsvd2;
    uint64_t mptr;    // Metadata Pointer (optional, usually 0)
    uint64_t prp1;    // PRP Entry 1 (data buffer address)
    uint64_t prp2;    // PRP Entry 2 (optional)
    uint32_t cdw10;   // Command-specific
    uint32_t cdw11;   // Command-specific
    uint32_t cdw12;
    uint32_t cdw13;
    uint32_t cdw14;
    uint32_t cdw15;
};

struct nvme_completion {
    uint32_t result;
    uint32_t rsvd;
    uint16_t sq_head;
    uint16_t sq_id;
    uint16_t cid;
    uint16_t status;
};

class UserNVMe
{
    uint8_t *m_bar0;

    int m_device, m_group, m_container;

    void* m_asq;  // admin submission queue
    void* m_acq;  // admin completion queue
    
    struct vfio_region_info reg = { .argsz = sizeof(reg), .index = VFIO_PCI_BAR0_REGION_INDEX };

    uint64_t mapDMA(int container_fd, void* vaddr, size_t size, uint64_t iova);

public:

    UserNVMe();

    ~UserNVMe();

    int initMMIO();

    void printCap();
    void printVersion();

    int setupAdminQueue();

    void identifyController();
};

#endif
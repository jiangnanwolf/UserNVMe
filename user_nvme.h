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

class UserNVMe
{
    uint8_t *m_bar0;

    int m_device, m_group, m_container;
    
    struct vfio_region_info reg = { .argsz = sizeof(reg), .index = VFIO_PCI_BAR0_REGION_INDEX };

    uint64_t mapDMA(int container_fd, void* vaddr, size_t size, uint64_t iova);

public:

    UserNVMe();

    ~UserNVMe();

    int initMMIO();

    void printCap();
    void printVersion();

    int setupAdminQueue();
};

#endif
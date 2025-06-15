#ifndef _USER_NVME_H_
#define _USER_NVME_H_

#include <stdint.h>
#include <linux/vfio.h>

#define NVME_REG_CAP    (0x0000)
#define NVME_REG_VS     (0x0008)

class UserNVMe
{
    uint8_t *m_bar0;

    int m_device, m_group, m_container;
    
    struct vfio_region_info reg = { .argsz = sizeof(reg), .index = VFIO_PCI_BAR0_REGION_INDEX };

public:

    UserNVMe();

    ~UserNVMe();

    int initMMIO();

    void printCap();
    void printVersion();
};

#endif
#ifndef _USER_NVME_H_
#define _USER_NVME_H_

#include <stdint.h>
#include <linux/vfio.h>

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
};

#endif
// dma_pool.h
#pragma once
#include <vector>
#include <bitset>
#include <stdexcept>
#include <sys/mman.h>
#include <linux/vfio.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <cstring>
#include <cstdint>

constexpr size_t DMA_POOL_SIZE = 64 * 1024 * 1024; // 64MB
constexpr size_t DMA_BLOCK_SIZE = 4096; // 4KB blocks
constexpr size_t DMA_MAX_BLOCKS = DMA_POOL_SIZE / DMA_BLOCK_SIZE;

struct DmaMapping {
    void* vaddr;
    uint64_t iova;
    size_t size;
};

class DmaPool {
public:
    DmaPool(int vfio_container_fd, uint64_t iova_base = 0x100000000)
        : m_container(vfio_container_fd), m_iova_base(iova_base) {

        m_pool = mmap(nullptr, DMA_POOL_SIZE,
                     PROT_READ | PROT_WRITE,
                     MAP_SHARED | MAP_ANONYMOUS | MAP_HUGETLB | MAP_POPULATE,
                     -1, 0);

        if (m_pool == MAP_FAILED)
            throw std::runtime_error("Failed to mmap hugepage memory");

        struct vfio_iommu_type1_dma_map dma_map = {};
        dma_map.argsz = sizeof(dma_map);
        dma_map.vaddr = reinterpret_cast<uint64_t>(m_pool);
        dma_map.size = DMA_POOL_SIZE;
        dma_map.iova = m_iova_base;
        dma_map.flags = VFIO_DMA_MAP_FLAG_READ | VFIO_DMA_MAP_FLAG_WRITE;

        if (ioctl(m_container, VFIO_IOMMU_MAP_DMA, &dma_map) < 0) {
            munmap(m_pool, DMA_POOL_SIZE);
            throw std::runtime_error("VFIO_IOMMU_MAP_DMA failed");
        }

        m_free_blocks.set(); // mark all blocks free
    }

    ~DmaPool() {
        struct vfio_iommu_type1_dma_unmap unmap = {};
        unmap.argsz = sizeof(unmap);
        unmap.iova = m_iova_base;
        unmap.size = DMA_POOL_SIZE;
        ioctl(m_container, VFIO_IOMMU_UNMAP_DMA, &unmap);

        munmap(m_pool, DMA_POOL_SIZE);
    }

    std::vector<DmaMapping> allocate_scatter_gather(size_t num_blocks) {
        if (num_blocks == 0 || num_blocks > DMA_MAX_BLOCKS)
            throw std::invalid_argument("Invalid block count");

        std::vector<DmaMapping> mappings;
        for (size_t i = 0; i < DMA_MAX_BLOCKS && mappings.size() < num_blocks; ++i) {
            if (m_free_blocks.test(i)) {
                m_free_blocks.reset(i);
                void* vaddr = reinterpret_cast<uint8_t*>(m_pool) + i * DMA_BLOCK_SIZE;
                uint64_t iova = m_iova_base + i * DMA_BLOCK_SIZE;
                mappings.push_back({vaddr, iova, DMA_BLOCK_SIZE});
            }
        }

        if (mappings.size() < num_blocks) {
            // Rollback
            for (const auto& m : mappings) release(m);
            throw std::runtime_error("Not enough DMA blocks for scatter-gather allocation");
        }

        return mappings;
    }

    void release(const DmaMapping& mapping) {
        size_t offset = reinterpret_cast<uint8_t*>(mapping.vaddr) - reinterpret_cast<uint8_t*>(m_pool);
        size_t index = offset / DMA_BLOCK_SIZE;
        size_t count = mapping.size / DMA_BLOCK_SIZE;
        for (size_t i = 0; i < count && index + i < DMA_MAX_BLOCKS; ++i)
            m_free_blocks.set(index + i);
    }

private:
    int m_container;
    void* m_pool;
    uint64_t m_iova_base;
    std::bitset<DMA_MAX_BLOCKS> m_free_blocks;
};

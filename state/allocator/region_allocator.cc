#include "region_allocator.h"

RDMARegionAllocator* RDMARegionAllocator::rdma_region_allocator = nullptr;

bool RDMARegionAllocator::create_instance(MetaManager* global_meta_mgr, t_id_t thread_num_per_machine) {
    if(rdma_region_allocator == nullptr) {
        rdma_region_allocator = new (std::nothrow) RDMARegionAllocator(global_meta_mgr, thread_num_per_machine);
    }
    return (rdma_region_allocator == nullptr);
}

void RDMARegionAllocator::destroy_instance() {
    delete rdma_region_allocator;
    rdma_region_allocator = nullptr;
}
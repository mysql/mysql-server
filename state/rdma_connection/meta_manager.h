#pragma once

#include <unordered_map>
#include <assert.h>

#include "util/common.h"
#include "rlib/rdma_ctrl.hpp"

// 这里不能include，会产生一些很奇怪的错误导致编译失败
// #include "state_store/redo_log.h"
// #include "state_store/txn_list.h"

using namespace rdmaio;

/**
 * Remote node's ip&port info
*/
class RemoteNode {
public:
    node_id_t node_id;
    std::string ip;
    int port;
};

/**
 * MetaManager is used for MasterNode to manage the meta info in StateNode.
 * There are at least three replicas for a StateNode, 
 * the MasterNode has to write state info into three replicas
*/
class MetaManager {
public:
    static bool create_instance();

    static void destroy_instance();

    static MetaManager* get_instance() {
        assert(global_meta_mgr != nullptr);
        return global_meta_mgr;
    }

    node_id_t GetMemStoreMeta(std::string& remote_ip, int remote_port);

    void GetMRMeta(const RemoteNode& node);

    /**
     * Get the MemoryAttr of log_buffer in the specific remote StateNode
    */
    ALWAYS_INLINE
    const MemoryAttr& GetRemoteLogBufMR(const node_id_t node_id) const {
        auto mrsearch = remote_log_buf_mrs.find(node_id);
        assert(mrsearch != remote_log_buf_mrs.end());
        return mrsearch->second;
    }

    /**
     * Get the MemoryAttr of lock_buffer in the specific remote StateNode
    */
    ALWAYS_INLINE
    const MemoryAttr& GetRemoteLockBufMR(const node_id_t node_id) const {
        auto mrsearch = remote_lock_buf_mrs.find(node_id);
        assert(mrsearch != remote_lock_buf_mrs.end());
        return mrsearch->second;
    }

    /**
     * Get the MemoryAttr of the txn_list in the specific remote StateNode
    */
    ALWAYS_INLINE
    const MemoryAttr& GetRemoteTxnListMR(const node_id_t node_id) const {
        auto mrsearch = remote_txn_list_mrs.find(node_id);
        assert(mrsearch != remote_txn_list_mrs.end());
        return mrsearch->second;
    }
    
    ALWAYS_INLINE
    node_id_t GetPrimaryNodeID() const {
        assert(remote_nodes.size() > 0);
        // the first remote_node is primary node, others are replicas
        return remote_nodes[0].node_id;
    }

    ALWAYS_INLINE
    offset_t GetTxnListLatchAddr() {
        return txn_list_latch_addr;
    }

    ALWAYS_INLINE
    offset_t GetTxnListBitmapAddr() {
        return txn_list_bitmap_addr;
    }

    ALWAYS_INLINE
    offset_t GetTxnAddrByIndex(int index) {
        return txn_list_base_addr + index * txn_size;
    }

    ALWAYS_INLINE
    size_t GetTxnBitmapSize() {
        return txn_bitmap_size;
    }

    ALWAYS_INLINE
    size_t GetRedoLogRemoteBufSize() { return redo_log_remote_buf_size; }

    ALWAYS_INLINE
    offset_t GetRedoLogRemoteBufLatchAddr() {
        return redo_log_remote_buf_latch_addr;
    }

    ALWAYS_INLINE
    offset_t GetRedoLogCurrAddr() { return redo_log_curr_addr; }

    ALWAYS_INLINE
    void SetRedoLogCurrAddr(offset_t redo_log_curr_addr_) {
        redo_log_curr_addr = redo_log_curr_addr_;
    }

    ALWAYS_INLINE
    void SetRedoLogSize(size_t sz) { this->redo_log_remote_buf_size = sz; }

    ALWAYS_INLINE
    size_t GetRedoLogSize(size_t sz) { return redo_log_remote_buf_size; }

private:
    MetaManager();
    ~MetaManager() {}

    static MetaManager* global_meta_mgr;
    node_id_t local_machine_id;
    // MemoryAttrs for various states in remote StateNodes, there may be multiple StateNodes because we assume that the StateNode can elasticly expand.
    std::unordered_map<node_id_t, MemoryAttr> remote_lock_buf_mrs;          // MemoryAttr for lock_buffer in remote StateNodes
    std::unordered_map<node_id_t, MemoryAttr> remote_txn_list_mrs;          // MemoryAttr for txn_list in remote StateNodes
    std::unordered_map<node_id_t, MemoryAttr> remote_log_buf_mrs;           // MemoryAttr for log_buffer in remote StateNodes

    // meta info for txn_list
    offset_t txn_list_latch_addr = 0;   // base address for txn_list_latch
    offset_t txn_list_bitmap_addr = 0;  // base address for txn_list bitmap
    offset_t txn_list_base_addr = 0;    // base address for txn_list
    size_t txn_size;                // size for each txn_item in txn_list
    size_t txn_bitmap_size;             // size for txn_list bitmap, initiated 

    // meta info for redo log
    offset_t redo_log_remote_buf_latch_addr = 0;  // base address for redo log latch
    // size of redo log buffer, OS_FILE_LOG_BLOCK_SIZE is 512B initially
    size_t redo_log_remote_buf_size = 64 * 1024 * 512;
    offset_t redo_log_base_addr = 0;  // base address for redo log buffer
    size_t log_buf_data_size;         // size of each log in redo log buffer
    // 防止 redo log buffer 和 txn list 地址冲突，覆盖数据
    offset_t redo_log_curr_addr = 0;  // current address of redo log buffer

public:
    RNicHandler* opened_rnic;
    RdmaCtrlPtr global_rdma_ctrl;           // rdma controller used by QPManager and local RDMA Region
    std::vector<RemoteNode> remote_nodes;   // remote state nodes
};
#pragma once

#include "util/common.h"

const offset_t  LOG_BUFFER_SIZE = 1024 * 1024 * 1024;

/**
 * Every thread has a LogOffsetAllocator to manage the LogOffset in remote StateNode.
*/
class LogOffsetAllocator {
public:
    LogOffsetAllocator(t_id_t tid, t_id_t num_thread) {
        auto per_thread_remote_log_buffer_size = LOG_BUFFER_SIZE / num_thread;
        start_log_offset = tid * per_thread_remote_log_buffer_size;
        end_log_offset = (tid + 1) * per_thread_remote_log_buffer_size;
        current_log_offset = 0;
    }

    offset_t GetNextLogOffset(node_id_t node_id, size_t log_entry_size) {
        if(unlikely(start_log_offset + current_log_offset + log_entry_size > (offset_t)end_log_offset)) {
            // current_log_offset = 0;
            // @StateReplicateTODO: 当一块region满了之后应该分配一个新的region
        }
        offset_t offset = start_log_offset + current_log_offset;
        current_log_offset += log_entry_size;
        return offset;
    }

private:
    offset_t start_log_offset;
    int64_t end_log_offset;
    int64_t current_log_offset;
};
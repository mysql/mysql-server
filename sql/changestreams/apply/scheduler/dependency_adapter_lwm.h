// Copyright (c) 2026, Oracle and/or its affiliates.
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License, version 2.0,
// as published by the Free Software Foundation.
//
// This program is distributed to work with certain software (including
// but not limited to OpenSSL) that is licensed under separate terms,
// as designated in a particular file or component or in included license
// documentation.  The authors of MySQL hereby grant you an additional
// permission to link the program and your derivative works with the
// separately licensed software that they have either included with
// the program or referenced in the documentation.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License, version 2.0, for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA.

#ifndef MYSQL_CSA_DEPENDENCY_ADAPTER_LWM_H
#define MYSQL_CSA_DEPENDENCY_ADAPTER_LWM_H

#include <ankerl/unordered_dense.h>
#include <algorithm>
#include <limits>
#include "sql/changestreams/apply/scheduler/dependency_adapter.h"

namespace mysql::csa {

/// @brief Dependency adapter for LWM (Low Water Mark) based scheduling.
/// Translates transaction sequence numbers into scheduler task IDs for
/// LWM-based execution.
///
/// @section Indexing Concepts
///
/// This adapter handles key indexing differences between source and replica:
/// - seq_num: Originates from the source (e.g., primary server) and starts at 1
///   for the first transaction in each group. It resets to 1 at group
///   boundaries (e.g., server restarts, configuration changes, or failures).
///   seq_num=0 is reserved for invalid cases (e.g., SEQ_UNINIT).
/// - task_id: Assigned sequentially by the scheduler on the replica, starting
///   from 0 and monotonically increasing as long as the applier is running.
///   Unlike seq_num, task_id does not reset and continues growing across
///   seq_num resets. Wrap-around handling: If task_id wraps (uint64_t
///   overflow), the adapter detects it (new < max && max near UINT64_MAX),
///   clears mappings, resets barrier to 0, and continues from the new baseline
///   to maintain order.
/// - LWM (Low Water Mark): Tracks the longest prefix of tasks executed in
///   commit order (with replica_preserve_commit_order=1). It is computed on
///   the replica based on task_ids.
///   - LWM = 0: No tasks have executed.
///   - LWM = 1: Exactly one task has executed (task_id = 0).
///   - If tasks with task_id 0 through 3 have executed in order, LWM = 4.
///   LWM advances as tasks commit, representing the next task_id that can
///   proceed without violating order.
///
/// In CSA, transactions include a sequence number (seq_num) and last_committed
/// (commit_parent), which is the seq_num of the transaction that must commit
/// before this one, or 0 if the dependency is unknown (e.g., SEQ_UNINIT or
/// first transaction in a group).
///
/// This adapter maintains a mapping from seq_num to task_id. The solve()
/// function computes the clock delay for a given task, which is the target
/// LWM value at which the task can execute. This delay represents the
/// dependency relative to the initial LWM (equivalent to the absolute LWM
/// value required). The task is delayed until the current LWM reaches or
/// exceeds this value.
///
/// The delay is computed as the maximum of:
/// - A barrier value (id_after_barrier), set when commit_parent == 0 to handle
///   unknown dependencies by ensuring the task waits for all prior tasks
///   (including itself if first, but resulting in delay=0 for immediate
///   execution).
/// - (task_id of commit_parent + 1), if the mapping for commit_parent exists
///   and this value exceeds the barrier.
///
/// When commit_parent == 0 (unknown dependency, e.g., SEQ_UNINIT or group
/// start/first transaction), the seq_to_task map is cleared to bound its size,
/// a new barrier is set to the current max_task_id (the task_id of this task),
/// and the delay equals this barrier. For the first transaction (task_id=0),
/// barrier=0 and delay=0, allowing immediate execution. For subsequent tasks
/// with unknown dependency, it sets a barrier to wait for prior tasks. This
/// ensures ordering across group boundaries without retaining stale mappings.
///
/// @section Example
/// Delays indicate the LWM value required for execution. Assume seq_num starts
/// at 1, with commit_parent=0 for the first transaction (unknown dependency,
/// immediate execution):
/// - Transaction task_id=0, seq_num=1, commit_parent=0 → delay=0 (unknown
/// dependency; execute immediately as first task)
/// - Transaction task_id=1, seq_num=2, commit_parent=1 → delay=1 (wait for LWM
/// >=1, i.e., task 0 committed)
/// - Transaction task_id=2, seq_num=3, commit_parent=1 → delay=1 (depends on
/// task 0)
/// - Transaction task_id=3, seq_num=4, commit_parent=2 → delay=2 (wait for LWM
/// >=2, i.e., task 1 committed)
/// - Transaction task_id=4, seq_num=5, commit_parent=0 → delay=4 (unknown
/// dependency; barrier set to wait for all prior tasks)
/// - Transaction task_id=5, seq_num=6, commit_parent=3 → delay=4 (wait for LWM
/// >=4, i.e., task 3 committed; respects barrier)
///
/// Group boundary (seq_num resets to 1; map cleared, new barrier set):
/// - Transaction task_id=6, seq_num=1, commit_parent=0 → delay=6 (unknown
/// dependency; map cleared, barrier=6; wait for LWM >=6)
/// - Transaction task_id=7, seq_num=2, commit_parent=1 → delay=7 (depends on
/// task_id=6 (seq_num=1); wait for LWM >=7)
class Dependency_adapter_lwm : public Dependency_adapter {
 public:
  using Task_id = Dependency_adapter::Task_id;
  using Task_id_resolved = Dependency_adapter::Task_id_resolved;
  using Clock_delay = Dependency_adapter::Clock_delay;
  Dependency_adapter_lwm() { seq_to_task.reserve(25000); }

  inline std::pair<Clock_delay, Task_id_resolved> solve(
      Task_id task_id, int64_t seq_num, int64_t commit_parent) override {
    const uint64_t current_task_id = task_id.get();
    const bool is_contiguous = (m_prev_seq + 1) == seq_num;

    // Detect wrap-around: if new task_id < max_task_id and max_task_id is near
    // overflow
    if (current_task_id < max_task_id &&
        max_task_id > (std::numeric_limits<uint64_t>::max() / 2)) {
      // Assume wrap-around occurred; reset state to new epoch
      seq_to_task.clear();
      id_after_barrier = 0;
      m_dense_mapping = false;
      max_task_id = current_task_id;
    } else {
      max_task_id = current_task_id;
    }

    // Translate commit_parent to task ID if valid
    uint64_t delay = id_after_barrier;
    if (commit_parent > 0 && is_contiguous) {
      uint64_t parent_id{0};
      bool has_parent{false};
      // Common case fast path: commit_parent equals previous sequence.
      if (commit_parent == m_prev_seq) {
        parent_id = m_prev_task_id;
        has_parent = true;
      } else if (m_dense_mapping && commit_parent >= m_dense_base_seq &&
                 commit_parent <= m_prev_seq) {
        parent_id = m_dense_base_task_id +
                    static_cast<uint64_t>(commit_parent - m_dense_base_seq);
        has_parent = true;
      } else {
        auto it = seq_to_task.find(commit_parent);
        if (it != seq_to_task.end()) {
          parent_id = it->second;
          has_parent = true;
        }
      }
      if (has_parent) {
        uint64_t next_parent =
            (parent_id == std::numeric_limits<uint64_t>::max()) ? 0
                                                                : parent_id + 1;
        if (next_parent > id_after_barrier ||
            (next_parent < parent_id &&
             id_after_barrier > (std::numeric_limits<uint64_t>::max() / 2))) {
          delay = next_parent;
        }
      }
      // If not found, assume cleared, use id_after_barrier
    } else if (commit_parent == 0 || seq_num <= m_prev_seq ||
               (m_prev_seq + 1) < seq_num || seq_num <= 0) {
      seq_to_task.clear();
      id_after_barrier = max_task_id;
      delay = id_after_barrier;
      m_dense_mapping = (seq_num > 0);
      m_dense_base_seq = seq_num;
      m_dense_base_task_id = current_task_id;
    } else {
      // Irregular sequence progression - fall back to explicit mapping.
      m_dense_mapping = false;
    }
    // For other invalid cases, use current id_after_barrier

    // In dense contiguous region, seq->task_id mapping is arithmetic and does
    // not require hash-table updates.
    if (!m_dense_mapping) {
      seq_to_task.insert_or_assign(seq_num, current_task_id);
    }
    m_prev_seq = seq_num;
    m_prev_task_id = current_task_id;

    return std::make_pair<Clock_delay, Task_id_resolved>(delay, {});
  }

  /// Get tracked mapping size for unit tests.
  /// In dense mode, this is the logical contiguous range size (not hash
  /// entries), because seq->task mapping is resolved arithmetically.
  /// @return The number of tracked tasks.
  std::size_t size() const {
    if (!m_dense_mapping) {
      return seq_to_task.size();
    }
    if (m_prev_seq < m_dense_base_seq) {
      return 0;
    }
    return static_cast<std::size_t>(m_prev_seq - m_dense_base_seq + 1);
  }

 private:
  ankerl::unordered_dense::map<int64_t, uint64_t> seq_to_task;
  uint64_t max_task_id{1};
  uint64_t id_after_barrier{0};
  /// Previous sequence number to check continuity
  int64_t m_prev_seq{0};
  /// Previous task id for fast-path parent resolution.
  uint64_t m_prev_task_id{0};
  /// True when current seq->task mapping is dense and can be resolved
  /// arithmetically (without hash inserts/lookups for most operations).
  bool m_dense_mapping{false};
  /// First seq/task pair for the current dense mapping range.
  int64_t m_dense_base_seq{0};
  uint64_t m_dense_base_task_id{0};
};

}  // namespace mysql::csa

#endif  // MYSQL_CSA_DEPENDENCY_ADAPTER_LWM_H

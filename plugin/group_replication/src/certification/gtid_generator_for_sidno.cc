// Copyright (c) 2023, 2024, Oracle and/or its affiliates.
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License, version 2.0,
// as published by the Free Software Foundation.
//
// This program is designed to work with certain software (including
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

#include "plugin/group_replication/include/certification/gtid_generator_for_sidno.h"
#include <mysql/components/services/log_builtins.h>
#include "my_dbug.h"
#include "plugin/group_replication/include/plugin_utils.h"

namespace gr {

// This method will return the next GNO for the current transaction, it
// will work with two behaviours:
//
// 1) member_uuid == NULL || m_gtid_assignment_block_size <= 1
//    View_change_log_events creation does call this method with
//    member_uuid set to NULL to force it to be created with the
//    first available GNO of the group. This will ensure that all
//    members do use the same GNO for it.
//    After a View_change_log_event is created we recompute available
//    GNOs to ensure that all members do have the same available GNOs
//    set.
//    This branch is also used when m_gtid_assignment_block_size is
//    set to 1, meaning that GNO will be assigned sequentially
//    according with certification order.
//
// 2) On the second branch we assign GNOs according to intervals
//    assigned to each member.
//    To avoid having eternal gaps when a member do use all of its
//    assigned GNOs, periodically we recompute the intervals, this
//    will make that GNOs available to other members.
//    The GNO is generated within the interval of available GNOs for
//    a given member.
//    When a member exhaust its assigned GNOs we reserve more for it
//    from the available GNOs set.
std::pair<rpl_gno, mysql::utils::Return_status>
Gtid_generator_for_sidno::get_next_available_gtid(const char *member_uuid,
                                                  const Gtid_set &gtid_set) {
  DBUG_TRACE;

  rpl_gno generated_gno = 0;
  Gno_generation_result code;
  auto invalid_gno_res = std::make_pair(-1, mysql::utils::Return_status::error);

  // Special case for events like the View Change log event and generator
  // with m_gtid_assignment_block_size equal to 1
  if (member_uuid == nullptr || m_block_size <= 1) {
    std::tie(generated_gno, code) =
        get_next_available_gtid_candidate(1, GNO_END, gtid_set);
    if (code != Gno_generation_result::ok) {
      assert(code != Gno_generation_result::gtid_block_overflow);
      return invalid_gno_res;
    }
  } else {
    // After a number of rounds equal to block size the blocks are
    // collected back so that the GTID holes can be filled up by
    // following transactions from other members.
    if (m_counter % (m_block_size + 1) == 0)
      compute_group_available_gtid_intervals(gtid_set);

    auto it = m_assigned_intervals.end();

    // GTID is assigned in blocks to each member and are consumed
    // from that block unless a new block is needed.
    do {
      it = get_assigned_interval(member_uuid, gtid_set);
      if (it == m_assigned_intervals.end()) {
        return invalid_gno_res;
      }
      std::tie(generated_gno, code) = get_next_available_gtid_candidate(
          it->second.start, it->second.end, gtid_set);
    } while (code == Gno_generation_result::gtid_block_overflow);

    if (code != Gno_generation_result::ok) {
      return invalid_gno_res;
    }
    it->second.start = generated_gno;
    ++m_counter;
  }

  assert(generated_gno > 0);
  return std::make_pair(generated_gno, mysql::utils::Return_status::ok);
}

Gtid_generator_for_sidno::Gtid_generator_for_sidno(rpl_sidno sidno,
                                                   std::size_t block_size)
    : m_sidno(sidno), m_block_size(block_size) {}

std::pair<rpl_gno, Gtid_generator_for_sidno::Gno_generation_result>
Gtid_generator_for_sidno::get_next_available_gtid_candidate(
    rpl_gno start, rpl_gno end, const Gtid_set &gtid_set) const {
  DBUG_TRACE;
  assert(start > 0);
  assert(start <= end);

  rpl_gno candidate = start;
  Gtid_set::Const_interval_iterator ivit(&gtid_set, m_sidno);

  while (true) {
    assert(candidate >= start);
    const Gtid_set::Interval *iv = ivit.get();
    rpl_gno next_interval_start = iv != nullptr ? iv->start : GNO_END;

    // Correct interval.
    if (candidate < next_interval_start) {
      if (candidate <= end)
        return std::make_pair(candidate, Gno_generation_result::ok);
      else {
        return std::make_pair(-2, Gno_generation_result::gtid_block_overflow);
      }
    }

    if (iv == nullptr) {
      LogPluginErr(ERROR_LEVEL, ER_GRP_RPL_CANT_GENERATE_GTID);
      return std::make_pair(-1, Gno_generation_result::gno_exhausted);
    }

    candidate = std::max(candidate, iv->end);
    ivit.next();
  }
}

void Gtid_generator_for_sidno::compute_group_available_gtid_intervals(
    const Gtid_set &gtid_set) {
  DBUG_TRACE;

  m_counter = 1;
  m_assigned_intervals.clear();
  m_available_intervals.clear();

  // Compute the GTID intervals that are available by inverting the
  // group_gtid_executed or group_gtid_extracted intervals.
  Gtid_set::Const_interval_iterator ivit(&gtid_set, m_sidno);

  const Gtid_set::Interval *iv = nullptr, *iv_next = nullptr;

  // The fist interval: UUID:100 -> we have the interval 1-99
  if ((iv = ivit.get()) != nullptr) {
    if (iv->start > 1) {
      Gtid_set::Interval interval = {1, iv->start - 1, nullptr};
      m_available_intervals.push_back(interval);
    }
  }

  // For each used interval find the upper bound and from there
  // add the free GTIDs up to the next interval or GNO_END.
  while ((iv = ivit.get()) != nullptr) {
    ivit.next();
    iv_next = ivit.get();

    rpl_gno start = iv->end;
    rpl_gno end = GNO_END;
    if (iv_next != nullptr) end = iv_next->start - 1;

    assert(start <= end);
    Gtid_set::Interval interval = {start, end, nullptr};
    m_available_intervals.push_back(interval);
  }

  // No GTIDs used, so the available interval is the complete set.
  if (m_available_intervals.size() == 0) {
    Gtid_set::Interval interval = {1, GNO_END, nullptr};
    m_available_intervals.push_back(interval);
  }
}

Gtid_generator_for_sidno::Assigned_intervals_it
Gtid_generator_for_sidno::get_assigned_interval(const std::string &member_id,
                                                const Gtid_set &gtid_set) {
  auto it = m_assigned_intervals.find(member_id);
  bool is_interval_exhausted = false;
  if (it != m_assigned_intervals.end()) {
    if (it->second.start >= it->second.end) {
      is_interval_exhausted = true;
    }
  }
  if (it == m_assigned_intervals.end() || is_interval_exhausted) {
    // There is no block assigned to this member so get one.
    it = reserve_gtid_block(member_id, gtid_set);
  }
  return it;
}

Gtid_generator_for_sidno::Assigned_intervals_it
Gtid_generator_for_sidno::reserve_gtid_block(const std::string &member_id,
                                             const Gtid_set &gtid_set) {
  DBUG_TRACE;
  assert(m_block_size > 1);

  Gtid_set::Interval reserved_block;

  // We are out of intervals, we need to force intervals computation.
  if (m_available_intervals.size() == 0)
    compute_group_available_gtid_intervals(gtid_set);

  if (m_available_intervals.size() == 0) {  // now we know that gno exhausted
    return m_assigned_intervals.end();
  }

  auto it = m_available_intervals.begin();

  // We always have one or more intervals, the only thing to check
  // is if the first interval is exhausted, if so we need to purge
  // it to avoid future use.
  if (m_block_size > it->end - it->start) {
    reserved_block = *it;
    m_available_intervals.erase(it);
  } else {
    reserved_block.start = it->start;
    reserved_block.end = it->start + m_block_size - 1;
    it->start = reserved_block.end + 1;
    assert(reserved_block.start <= reserved_block.end);
    assert(reserved_block.start < it->start);
  }
  // insert or update existing map entry with reserved_block (new interval)
  return m_assigned_intervals.insert_or_assign(member_id, reserved_block).first;
}

}  // namespace gr

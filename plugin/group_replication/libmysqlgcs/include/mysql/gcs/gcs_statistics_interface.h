/* Copyright (c) 2014, 2024, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is designed to work with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have either included with
   the program or referenced in the documentation.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#ifndef GCS_STATISTICS_INTERFACE_INCLUDED
#define GCS_STATISTICS_INTERFACE_INCLUDED

#include <stdint.h>
#include <list>
#include <string>

/**
 * @brief Container struct that represents a single node suspicious.
 *
 * It is represented by two fields:
 * - A node address in GCS format (ip:port)
 * - The number of suspicious that this node had
 */
struct Gcs_node_suspicious {
  std::string m_node_address{};
  uint64_t m_node_suspicious_count{0};
};

/**
  @class Gcs_statistics_interface

  This interface represents all statistics that a binding implementation should
  provide.
*/
class Gcs_statistics_interface {
 public:
  /**
   * @brief Sum of all proposals that were initiated and terminated in this
   * node.
   *
   * @return uint64_t the value of all sucessful proposal rounds
   */
  virtual uint64_t get_all_sucessful_proposal_rounds() const = 0;

  /**
   * @brief Sum of all empty proposal rounds that were initiated and terminated
   * in this node.
   *
   * @return uint64_t the value of empty proposal rounds
   */
  virtual uint64_t get_all_empty_proposal_rounds() const = 0;

  /**
   * @brief Sum of all socket-level bytes that were sent to all group nodes
   * originating on this node. Socket-level bytes mean that we will report
   * more data here than in the sent messages, because they are multiplexed
   * and sent to each member.
   *
   * As an example, if we have a group with 3 members and we send a 100 bytes
   * message, this value will account for 300 bytes, since we send 100 bytes
   * to each node.
   *
   * @return uint64_t the value of all bytes sent
   */
  virtual uint64_t get_all_bytes_sent() const = 0;

  /**
   * @brief The sum of elapsed time of all consensus rounds started and finished
   * in this node. Togheter with count_all_consensus_proposals, we can identify
   * if the individual consensus time has a trend of going up, thus signaling
   * a possible problem.
   *
   * @return unsigned long long the aggregated value of all proposal times
   */
  virtual unsigned long long get_cumulative_proposal_time() const = 0;

  /**
   * @brief A list of pairs between a group member address and the number of
   * times the local node has seen it as suspected.
   *
   * It contains all the suspicious from all nodes that belong or belonged to
   * the group. This means that it might contain MORE information that just
   * the current members. It also only contains ONLY the members that were
   * UNREACHABLE in any given moment. Any member that was never faulty, will
   * not be presented in this list.
   *
   * Any client calling this API must filter out or add in any information
   * that finds suitable to present to an end user.
   */
  virtual void get_suspicious_count(
      std::list<Gcs_node_suspicious> &suspicious) const = 0;

  /**
   * @brief The number of full 3-Phase PAXOS that this node initiated. If this
   * number grows, it means that at least of the node is having issues
   * answering to Proposals, either by slowliness or network issues.
   * Use togheter with count_member_failure_suspicions to try and do some
   * diagnose.
   *
   * @return uint64_t the value of all 3-phase PAXOS runs
   */
  virtual uint64_t get_all_full_proposal_count() const = 0;

  /**
   * @brief The number of high-level messages that this node sent to the group.
   * These messages are the ones the we receive via the API to be proposed
   * to the group. XCom has a batching mechanism, that will gather these
   * messages and propose them all togheter. This will acocunt the number
   * of message before being batched.
   *
   * @return uint64_t the value for all messages sent
   */
  virtual uint64_t get_all_messages_sent() const = 0;

  /**
   * @brief The sum of all socket-level bytes that were received to from group
   * nodes having as a destination this node.
   *
   * @return uint64_t the value for all message bytes received
   */
  virtual uint64_t get_all_message_bytes_received() const = 0;

  /**
   * @brief The time in which our last consensus proposal was approved. Reported
   * in a timestamp format. This is an indicator if the group is halted or
   * making slow progress.
   *
   * @return unsigned long long the timestamp value of the last proposal round
   */
  virtual unsigned long long get_last_proposal_round_time() const = 0;

  virtual ~Gcs_statistics_interface() = default;
};

#endif  // GCS_STATISTICS_INTERFACE_INCLUDED

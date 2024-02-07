/* Copyright (c) 2023, 2024, Oracle and/or its affiliates.

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

#ifndef GCS_XCOM_STATISTICS_MANAGER_INCLUDED
#define GCS_XCOM_STATISTICS_MANAGER_INCLUDED

#include "plugin/group_replication/libmysqlgcs/include/mysql/gcs/gcs_statistics_interface.h"

#include <map>
#include <vector>

/**
 * @brief Enumerate that identifies all counter statistics.
 */
enum Gcs_counter_statistics_enum : int {
  kSucessfulProposalRounds = 0,  // get_all_sucessful_proposal_rounds
  kEmptyProposalRounds,          // get_all_empty_proposal_rounds
  kFullProposalCount,            // get_all_full_proposal_count
  kMessagesSent,                 // get_all_messages_sent
  kGcsCounterStatisticsEnumEnd
};

/**
 * @brief Enumerate that identifies all cumulative statistics.
 */
enum Gcs_cumulative_statistics_enum : int {
  kBytesSent = 0,         // get_all_bytes_sent
  kMessageBytesReceived,  // get_all_message_bytes_received
  kGcsCumulativeStatisticsEnumEnd
};

/**
 * @brief Enumerate that identifies all time statistics.
 */
enum Gcs_time_statistics_enum : int {
  kCumulativeProposalTime = 0,  // cumulative_proposal_time
  kLastProposalRoundTime,       // last_proposal_round_time
  kGcsTimeStatisticsEnumEnd
};

/**
 * @brief This class is the storage and provider of all statistics coming
 * from either XCom and GCS.
 */
class Gcs_xcom_statistics_manager_interface {
 public:
  Gcs_xcom_statistics_manager_interface() = default;
  virtual ~Gcs_xcom_statistics_manager_interface() = default;

  // SUM VARS
  /**
   * @brief Get the value of a provided statistic which is of a cumulative
   * nature.
   *
   * @param to_get The statistic to get.
   *
   * @return uint64_t the value of the statistic
   */
  virtual uint64_t get_sum_var_value(
      Gcs_cumulative_statistics_enum to_get) const = 0;

  /**
   * @brief Sets the value of a provided statistic which is of a cumulative
   * nature.
   *
   * @param to_set The statistic to add.
   * @param to_add The value to add to the provided statistic.
   */
  virtual void set_sum_var_value(Gcs_cumulative_statistics_enum to_set,
                                 uint64_t to_add) = 0;

  // COUNT VARS
  /**
   * @brief Get the value of a provided statistic which is of a discrete growth
   * nature.
   *
   * @param to_get The statistic to get.
   *
   * @return uint64_t the value of the statistic
   */
  virtual uint64_t get_count_var_value(
      Gcs_counter_statistics_enum to_get) const = 0;

  /**
   * @brief Sets the value of a provided statistic which is of a discrete growth
   * nature. It always adds "+1" to the current value.
   *
   * @param to_set The statistic to add.
   */
  virtual void set_count_var_value(Gcs_counter_statistics_enum to_set) = 0;

  // TIMESTAMP VALUES
  /**
   * @brief Get the value of a provided statistic which is a timestamp.
   *
   * @param to_get The statistic to set.
   *
   * @return unsigned long long The timestamp value of that statistic.
   */
  virtual unsigned long long get_timestamp_var_value(
      Gcs_time_statistics_enum to_get) const = 0;

  /**
   * @brief Sets the value of a provided timestamp statistic which is of a
   * certain specific value.
   *
   * @param to_set The statistic to set.
   * @param new_value The new value of that statistic.
   */
  virtual void set_timestamp_var_value(Gcs_time_statistics_enum to_set,
                                       unsigned long long new_value) = 0;

  /**
   * @brief Sets the value of a provided timestamp statistic which is of a
   * cumulative nature.
   *
   * @param to_set The statistic to add.
   * @param to_add The value to add to the provided statistic.
   */
  virtual void set_sum_timestamp_var_value(Gcs_time_statistics_enum to_set,
                                           unsigned long long to_add) = 0;

  // ALL OTHER VARS
  /**
   * @brief Get all suspicious seen by this node.
   *
   * @return std::vector<Gcs_node_suspicious> a vector containing all suspicious
   *                                          seen by this local node
   */
  virtual std::vector<Gcs_node_suspicious> get_all_suspicious() const = 0;

  /**
   * @brief Adds a suspicious count for a provided node.
   *
   * @param node_id The node identifier to add a suspicious. It must be in
   *                address:port format as provided by
   *                @see Gcs_member_identifier
   */
  virtual void add_suspicious_for_a_node(std::string node_id) = 0;
};

class Gcs_xcom_statistics_manager_interface_impl
    : public Gcs_xcom_statistics_manager_interface {
 public:
  Gcs_xcom_statistics_manager_interface_impl()
      : m_sum_statistics(kGcsCumulativeStatisticsEnumEnd),
        m_count_statistics(kGcsCounterStatisticsEnumEnd),
        m_time_statistics(kGcsTimeStatisticsEnumEnd) {}

  virtual ~Gcs_xcom_statistics_manager_interface_impl() override = default;

  // SUM VARS
  /**
   * @see Gcs_xcom_statistics_manager_interface::get_sum_var_value
   */
  uint64_t get_sum_var_value(
      Gcs_cumulative_statistics_enum to_get) const override;

  /**
   * @see Gcs_xcom_statistics_manager_interface::set_sum_var_value
   */
  void set_sum_var_value(Gcs_cumulative_statistics_enum to_set,
                         uint64_t to_add) override;

  // COUNT VARS
  /**
   * @see Gcs_xcom_statistics_manager_interface::get_count_var_value
   */
  uint64_t get_count_var_value(
      Gcs_counter_statistics_enum to_get) const override;

  /**
   * @see Gcs_xcom_statistics_manager_interface::set_count_var_value
   */
  void set_count_var_value(Gcs_counter_statistics_enum to_set) override;

  // TIMESTAMP VALUES
  /**
   * @see Gcs_xcom_statistics_manager_interface::get_timestamp_var_value
   */
  unsigned long long get_timestamp_var_value(
      Gcs_time_statistics_enum to_get) const override;

  /**
   * @see Gcs_xcom_statistics_manager_interface::set_timestamp_var_value
   */
  void set_timestamp_var_value(Gcs_time_statistics_enum to_set,
                               unsigned long long new_value) override;

  /**
   * @see Gcs_xcom_statistics_manager_interface::set_sum_timestamp_var_value
   */
  void set_sum_timestamp_var_value(Gcs_time_statistics_enum to_set,
                                   unsigned long long to_add) override;

  // ALL OTHER VARS
  /**
   * @see Gcs_xcom_statistics_manager_interface::get_all_suspicious
   */
  std::vector<Gcs_node_suspicious> get_all_suspicious() const override;

  /**
   * @see Gcs_xcom_statistics_manager_interface::add_suspicious_for_a_node
   */
  void add_suspicious_for_a_node(std::string node_id) override;

 private:
  std::vector<uint64_t> m_sum_statistics;
  std::vector<uint64_t> m_count_statistics;
  std::vector<unsigned long long> m_time_statistics;
  std::map<std::string, uint64_t> m_suspicious_statistics;
};

#endif /* GCS_XCOM_STATISTICS_MANAGER_INCLUDED*/

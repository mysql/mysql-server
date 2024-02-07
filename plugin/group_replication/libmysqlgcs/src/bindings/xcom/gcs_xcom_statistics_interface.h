/* Copyright (c) 2015, 2024, Oracle and/or its affiliates.

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

#ifndef GCS_XCOM_STATISTICS_INTERFACE_INCLUDED
#define GCS_XCOM_STATISTICS_INTERFACE_INCLUDED

#include <time.h>
#include <algorithm>

#include "plugin/group_replication/libmysqlgcs/include/mysql/gcs/gcs_statistics_interface.h"
#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/gcs_xcom_statistics_manager.h"

/**
  @class Gcs_xcom_statistics_interface

  This class implements the Gcs_statistics_interface and updater.
*/
class Gcs_xcom_statistics : public Gcs_statistics_interface {
 public:
  explicit Gcs_xcom_statistics(
      Gcs_xcom_statistics_manager_interface *stats_mgr);
  ~Gcs_xcom_statistics() override;

  // Implementation of Gcs_statistics_interface
  /**
   * @see Gcs_statistics_interface::get_all_sucessful_proposal_rounds
   */
  uint64_t get_all_sucessful_proposal_rounds() const override;

  /**
   * @see Gcs_statistics_interface::get_all_empty_proposal_rounds
   */
  uint64_t get_all_empty_proposal_rounds() const override;

  /**
   * @see Gcs_statistics_interface::get_all_bytes_sent
   */
  uint64_t get_all_bytes_sent() const override;

  /**
   * @see Gcs_statistics_interface::get_cumulative_proposal_time
   */
  unsigned long long get_cumulative_proposal_time() const override;

  /**
   * @see Gcs_statistics_interface::get_suspicious_count
   */
  void get_suspicious_count(
      std::list<Gcs_node_suspicious> &suspicious_out) const override;

  /**
   * @see Gcs_statistics_interface::get_all_full_proposal_count
   */
  uint64_t get_all_full_proposal_count() const override;

  /**
   * @see Gcs_statistics_interface::get_all_messages_sent
   */
  uint64_t get_all_messages_sent() const override;

  /**
   * @see Gcs_statistics_interface::get_all_message_bytes_received
   */
  uint64_t get_all_message_bytes_received() const override;

  /**
   * @see Gcs_statistics_interface::get_last_proposal_round_time
   */
  unsigned long long get_last_proposal_round_time() const override;

 private:
  Gcs_xcom_statistics_manager_interface *m_stats_mgr;
};

#endif /* GCS_XCOM_STATISTICS_INTERFACE_INCLUDED */

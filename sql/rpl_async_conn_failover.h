/* Copyright (c) 2020, 2023, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is also distributed with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have included with MySQL.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#ifndef RPL_ASYNC_CONN_FAILOVER_H
#define RPL_ASYNC_CONN_FAILOVER_H

#include "sql/rpl_async_conn_failover_table_operations.h"
#include "sql/rpl_mi.h"

/*
  The class is used to connect to new source in case the
  current replica IO connection gets interrupted.
*/
class Async_conn_failover_manager {
 public:
  enum class DoAutoConnFailoverError {
    /* Success. */
    no_error = 0,

    /* Failed to set/reset network configuration details. */
    retriable_error,

    /* Failed to find alternative source to connect. */
    no_sources_error
  };

  enum class SourceQuorumStatus {
    /* Success. */
    no_error = 0,

    /* Failed to detect if the source belongs to the group majority. */
    fatal_error,

    /* Transient network error connecting to source. */
    transient_network_error,

    /* No Quorum. Source does not belong to the group majority. */
    no_quorum_error
  };

  Async_conn_failover_manager() = delete;

  Async_conn_failover_manager(const Async_conn_failover_manager &) = delete;
  Async_conn_failover_manager(Async_conn_failover_manager &&) = delete;
  Async_conn_failover_manager &operator=(const Async_conn_failover_manager &) =
      delete;
  Async_conn_failover_manager &operator=(Async_conn_failover_manager &&) =
      delete;

  /**
    Re-establishes connection to next available source.

    @param[in] mi              the mi of the failed connection which
                               needs to be reconnected to the new source.
    @param[in] force_highest_weight When true, sender with highest weight is
    chosen, otherwise the next sender from the current one is chosen.

    @retval Please see DoAutoConnFailoverError.
  */
  static DoAutoConnFailoverError do_auto_conn_failover(
      Master_info *mi, bool force_highest_weight);

  /*
    Get source quorum status in case source has Group Replication enabled.

    @param  mysql MYSQL to request uuid from source.
    @param  mi    Master_info to set master_uuid

    @retval Please see SourceQuorumStatus.
  */
  static SourceQuorumStatus get_source_quorum_status(MYSQL *mysql,
                                                     Master_info *mi);

 private:
  /**
    Sets source network configuration details <host, port, network_namespace>
    for the provided Master_info object. The function is used by async conn
    failure to set configuration details of new source.

    @param[in] mi              the channel of the failed connection which
                               needs to be reconnected to the new source.
    @param[in] host the source hostname to be set for Master_info object
    @param[in] port the source port to be set for Master_info object
    @param[in] network_namespace the source network_namespace to be set for
                                 Master_info object

    @retval true   Error
    @retval false  Success
  */
  static bool set_channel_conn_details(Master_info *mi, const std::string host,
                                       const uint port,
                                       const std::string network_namespace);
};
#endif /* RPL_ASYNC_CONN_FAILOVER_H */

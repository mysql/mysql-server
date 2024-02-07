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

#ifndef PLUGIN_STATUS_VARIABLES_INCLUDE
#define PLUGIN_STATUS_VARIABLES_INCLUDE

#include "mysql/plugin.h" /* MYSQL_THD, SHOW_VAR */

/**
  @class Plugin_status_variables

  Group Replication plugin status variables proxy.
*/
class Plugin_status_variables {
 public:
  Plugin_status_variables() = delete;
  ~Plugin_status_variables() = delete;

  /**
    @see Metrics_handler::get_control_messages_sent_count()

    @param[in]  thd     The caller session.
    @param[out] var     The status variable.
    @param[out] buffer  The buffer on which the value will be stored.

    @return the operation status
      @retval 0      OK
      @retval !=0    Error
  */
  static int get_control_messages_sent_count(MYSQL_THD thd, SHOW_VAR *var,
                                             char *buffer);

  /**
    @see Metrics_handler::get_data_messages_sent_count()

    @param[in]  thd     The caller session.
    @param[out] var     The status variable.
    @param[out] buffer  The buffer on which the value will be stored.

    @return the operation status
      @retval 0      OK
      @retval !=0    Error
  */
  static int get_data_messages_sent_count(MYSQL_THD thd, SHOW_VAR *var,
                                          char *buffer);

  /**
    @see Metrics_handler::get_control_messages_sent_bytes_sum()

    @param[in]  thd     The caller session.
    @param[out] var     The status variable.
    @param[out] buffer  The buffer on which the value will be stored.

    @return the operation status
      @retval 0      OK
      @retval !=0    Error
  */
  static int get_control_messages_sent_bytes_sum(MYSQL_THD thd, SHOW_VAR *var,
                                                 char *buffer);

  /**
    @see Metrics_handler::get_data_messages_sent_bytes_sum()

    @param[in]  thd     The caller session.
    @param[out] var     The status variable.
    @param[out] buffer  The buffer on which the value will be stored.

    @return the operation status
      @retval 0      OK
      @retval !=0    Error
  */
  static int get_data_messages_sent_bytes_sum(MYSQL_THD thd, SHOW_VAR *var,
                                              char *buffer);

  /**
    @see Metrics_handler::get_control_messages_sent_roundtrip_time_sum()

    @param[in]  thd     The caller session.
    @param[out] var     The status variable.
    @param[out] buffer  The buffer on which the value will be stored.

    @return the operation status
      @retval 0      OK
      @retval !=0    Error
  */
  static int get_control_messages_sent_roundtrip_time_sum(MYSQL_THD thd,
                                                          SHOW_VAR *var,
                                                          char *buffer);

  /**
    @see Metrics_handler::get_data_messages_sent_roundtrip_time_sum()

    @param[in]  thd     The caller session.
    @param[out] var     The status variable.
    @param[out] buffer  The buffer on which the value will be stored.

    @return the operation status
      @retval 0      OK
      @retval !=0    Error
  */
  static int get_data_messages_sent_roundtrip_time_sum(MYSQL_THD thd,
                                                       SHOW_VAR *var,
                                                       char *buffer);

  /**
    @see Metrics_handler::get_transactions_consistency_before_begin_count()

    @param[in]  thd     The caller session.
    @param[out] var     The status variable.
    @param[out] buffer  The buffer on which the value will be stored.

    @return the operation status
      @retval 0      OK
      @retval !=0    Error
  */
  static int get_transactions_consistency_before_begin_count(MYSQL_THD thd,
                                                             SHOW_VAR *var,
                                                             char *buffer);

  /**
    @see Metrics_handler::get_transactions_consistency_before_begin_time_sum()

    @param[in]  thd     The caller session.
    @param[out] var     The status variable.
    @param[out] buffer  The buffer on which the value will be stored.

    @return the operation status
      @retval 0      OK
      @retval !=0    Error
  */
  static int get_transactions_consistency_before_begin_time_sum(MYSQL_THD thd,
                                                                SHOW_VAR *var,
                                                                char *buffer);

  /**
    @see Metrics_handler::get_transactions_consistency_after_termination_count()

    @param[in]  thd     The caller session.
    @param[out] var     The status variable.
    @param[out] buffer  The buffer on which the value will be stored.

    @return the operation status
      @retval 0      OK
      @retval !=0    Error
  */
  static int get_transactions_consistency_after_termination_count(MYSQL_THD thd,
                                                                  SHOW_VAR *var,
                                                                  char *buffer);

  /**
    @see
    Metrics_handler::get_transactions_consistency_after_termination_time_sum()

    @param[in]  thd     The caller session.
    @param[out] var     The status variable.
    @param[out] buffer  The buffer on which the value will be stored.

    @return the operation status
      @retval 0      OK
      @retval !=0    Error
  */
  static int get_transactions_consistency_after_termination_time_sum(
      MYSQL_THD thd, SHOW_VAR *var, char *buffer);

  /**
    @see Metrics_handler::get_transactions_consistency_after_sync_count()

    @param[in]  thd     The caller session.
    @param[out] var     The status variable.
    @param[out] buffer  The buffer on which the value will be stored.

    @return the operation status
      @retval 0      OK
      @retval !=0    Error
  */
  static int get_transactions_consistency_after_sync_count(MYSQL_THD thd,
                                                           SHOW_VAR *var,
                                                           char *buffer);

  /**
    @see Metrics_handler::get_transactions_consistency_after_sync_time_sum()

    @param[in]  thd     The caller session.
    @param[out] var     The status variable.
    @param[out] buffer  The buffer on which the value will be stored.

    @return the operation status
      @retval 0      OK
      @retval !=0    Error
  */
  static int get_transactions_consistency_after_sync_time_sum(MYSQL_THD thd,
                                                              SHOW_VAR *var,
                                                              char *buffer);

  /**
    @see Metrics_handler::get_certification_garbage_collector_count()

    @param[in]  thd     The caller session.
    @param[out] var     The status variable.
    @param[out] buffer  The buffer on which the value will be stored.

    @return the operation status
      @retval 0      OK
      @retval !=0    Error
  */
  static int get_certification_garbage_collector_count(MYSQL_THD thd,
                                                       SHOW_VAR *var,
                                                       char *buffer);

  /**
    @see Metrics_handler::get_certification_garbage_collector_time_sum()

    @param[in]  thd     The caller session.
    @param[out] var     The status variable.
    @param[out] buffer  The buffer on which the value will be stored.

    @return the operation status
      @retval 0      OK
      @retval !=0    Error
  */
  static int get_certification_garbage_collector_time_sum(MYSQL_THD thd,
                                                          SHOW_VAR *var,
                                                          char *buffer);

  /**
    @see Gcs_operations::get_all_consensus_proposals_count()

    @param[in]  thd     The caller session.
    @param[out] var     The status variable.
    @param[out] buffer  The buffer on which the value will be stored.

    @return the operation status
      @retval 0      OK
      @retval !=0    Error
  */
  static int get_all_consensus_proposals_count(MYSQL_THD thd, SHOW_VAR *var,
                                               char *buffer);

  /**
    @see Gcs_operations::get_empty_consensus_proposals_count()

    @param[in]  thd     The caller session.
    @param[out] var     The status variable.
    @param[out] buffer  The buffer on which the value will be stored.

    @return the operation status
      @retval 0      OK
      @retval !=0    Error
  */
  static int get_empty_consensus_proposals_count(MYSQL_THD thd, SHOW_VAR *var,
                                                 char *buffer);

  /**
    @see Gcs_operations::get_consensus_bytes_sent_sum()

    @param[in]  thd     The caller session.
    @param[out] var     The status variable.
    @param[out] buffer  The buffer on which the value will be stored.

    @return the operation status
      @retval 0      OK
      @retval !=0    Error
  */
  static int get_consensus_bytes_sent_sum(MYSQL_THD thd, SHOW_VAR *var,
                                          char *buffer);

  /**
    @see Gcs_operations::get_consensus_bytes_received_sum()

    @param[in]  thd     The caller session.
    @param[out] var     The status variable.
    @param[out] buffer  The buffer on which the value will be stored.

    @return the operation status
      @retval 0      OK
      @retval !=0    Error
  */
  static int get_consensus_bytes_received_sum(MYSQL_THD thd, SHOW_VAR *var,
                                              char *buffer);

  /**
    @see Gcs_operations::get_all_consensus_time_sum()

    @param[in]  thd     The caller session.
    @param[out] var     The status variable.
    @param[out] buffer  The buffer on which the value will be stored.

    @return the operation status
      @retval 0      OK
      @retval !=0    Error
  */
  static int get_all_consensus_time_sum(MYSQL_THD thd, SHOW_VAR *var,
                                        char *buffer);

  /**
    @see Gcs_operations::get_extended_consensus_count()

    @param[in]  thd     The caller session.
    @param[out] var     The status variable.
    @param[out] buffer  The buffer on which the value will be stored.

    @return the operation status
      @retval 0      OK
      @retval !=0    Error
  */
  static int get_extended_consensus_count(MYSQL_THD thd, SHOW_VAR *var,
                                          char *buffer);

  /**
    @see Gcs_operations::get_total_messages_sent_count()

    @param[in]  thd     The caller session.
    @param[out] var     The status variable.
    @param[out] buffer  The buffer on which the value will be stored.

    @return the operation status
      @retval 0      OK
      @retval !=0    Error
  */
  static int get_total_messages_sent_count(MYSQL_THD thd, SHOW_VAR *var,
                                           char *buffer);

  /**
    Copy to buffer parameter the last consensus end timestamp in the
    format: 'YYYY-MM-DD hh:mm:ss.microseconds' in UTC timezone.
    If there is no last consensus, a empty string is copied.

    @see Gcs_operations::get_last_consensus_end_timestamp()

    @param[in]  thd     The caller session.
    @param[out] var     The status variable.
    @param[out] buffer  The buffer on which the value will be stored.

    @return the operation status
      @retval 0      OK
      @retval !=0    Error
  */
  static int get_last_consensus_end_timestamp(MYSQL_THD thd, SHOW_VAR *var,
                                              char *buffer);
};

#endif /* PLUGIN_STATUS_VARIABLES_INCLUDE */

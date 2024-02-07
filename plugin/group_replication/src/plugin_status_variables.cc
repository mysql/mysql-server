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

#include "plugin/group_replication/include/plugin_status_variables.h"
#include <mysql/group_replication_priv.h>
#include "plugin/group_replication/include/plugin.h"
#include "plugin/group_replication/include/plugin_handlers/metrics_handler.h"

int Plugin_status_variables::get_control_messages_sent_count(MYSQL_THD,
                                                             SHOW_VAR *var,
                                                             char *buffer) {
  var->type = SHOW_LONGLONG;
  var->value = buffer;
  ulonglong *value = reinterpret_cast<ulonglong *>(buffer);
  *value = metrics_handler->get_control_messages_sent_count();
  return 0;
}

int Plugin_status_variables::get_data_messages_sent_count(MYSQL_THD,
                                                          SHOW_VAR *var,
                                                          char *buffer) {
  var->type = SHOW_LONGLONG;
  var->value = buffer;
  ulonglong *value = reinterpret_cast<ulonglong *>(buffer);
  *value = metrics_handler->get_data_messages_sent_count();
  return 0;
}

int Plugin_status_variables::get_control_messages_sent_bytes_sum(MYSQL_THD,
                                                                 SHOW_VAR *var,
                                                                 char *buffer) {
  var->type = SHOW_LONGLONG;
  var->value = buffer;
  ulonglong *value = reinterpret_cast<ulonglong *>(buffer);
  *value = metrics_handler->get_control_messages_sent_bytes_sum();
  return 0;
}

int Plugin_status_variables::get_data_messages_sent_bytes_sum(MYSQL_THD,
                                                              SHOW_VAR *var,
                                                              char *buffer) {
  var->type = SHOW_LONGLONG;
  var->value = buffer;
  ulonglong *value = reinterpret_cast<ulonglong *>(buffer);
  *value = metrics_handler->get_data_messages_sent_bytes_sum();
  return 0;
}

int Plugin_status_variables::get_control_messages_sent_roundtrip_time_sum(
    MYSQL_THD, SHOW_VAR *var, char *buffer) {
  var->type = SHOW_LONGLONG;
  var->value = buffer;
  ulonglong *value = reinterpret_cast<ulonglong *>(buffer);
  *value = metrics_handler->get_control_messages_sent_roundtrip_time_sum();
  return 0;
}

int Plugin_status_variables::get_data_messages_sent_roundtrip_time_sum(
    MYSQL_THD, SHOW_VAR *var, char *buffer) {
  var->type = SHOW_LONGLONG;
  var->value = buffer;
  ulonglong *value = reinterpret_cast<ulonglong *>(buffer);
  *value = metrics_handler->get_data_messages_sent_roundtrip_time_sum();
  return 0;
}

int Plugin_status_variables::get_transactions_consistency_before_begin_count(
    MYSQL_THD, SHOW_VAR *var, char *buffer) {
  var->type = SHOW_LONGLONG;
  var->value = buffer;
  ulonglong *value = reinterpret_cast<ulonglong *>(buffer);
  *value = metrics_handler->get_transactions_consistency_before_begin_count();
  return 0;
}

int Plugin_status_variables::get_transactions_consistency_before_begin_time_sum(
    MYSQL_THD, SHOW_VAR *var, char *buffer) {
  var->type = SHOW_LONGLONG;
  var->value = buffer;
  ulonglong *value = reinterpret_cast<ulonglong *>(buffer);
  *value =
      metrics_handler->get_transactions_consistency_before_begin_time_sum();
  return 0;
}

int Plugin_status_variables::
    get_transactions_consistency_after_termination_count(MYSQL_THD,
                                                         SHOW_VAR *var,
                                                         char *buffer) {
  var->type = SHOW_LONGLONG;
  var->value = buffer;
  ulonglong *value = reinterpret_cast<ulonglong *>(buffer);
  *value =
      metrics_handler->get_transactions_consistency_after_termination_count();
  return 0;
}

int Plugin_status_variables::
    get_transactions_consistency_after_termination_time_sum(MYSQL_THD,
                                                            SHOW_VAR *var,
                                                            char *buffer) {
  var->type = SHOW_LONGLONG;
  var->value = buffer;
  ulonglong *value = reinterpret_cast<ulonglong *>(buffer);
  *value = metrics_handler
               ->get_transactions_consistency_after_termination_time_sum();
  return 0;
}

int Plugin_status_variables::get_transactions_consistency_after_sync_count(
    MYSQL_THD, SHOW_VAR *var, char *buffer) {
  var->type = SHOW_LONGLONG;
  var->value = buffer;
  ulonglong *value = reinterpret_cast<ulonglong *>(buffer);
  *value = metrics_handler->get_transactions_consistency_after_sync_count();
  return 0;
}

int Plugin_status_variables::get_transactions_consistency_after_sync_time_sum(
    MYSQL_THD, SHOW_VAR *var, char *buffer) {
  var->type = SHOW_LONGLONG;
  var->value = buffer;
  ulonglong *value = reinterpret_cast<ulonglong *>(buffer);
  *value = metrics_handler->get_transactions_consistency_after_sync_time_sum();
  return 0;
}

int Plugin_status_variables::get_certification_garbage_collector_count(
    MYSQL_THD, SHOW_VAR *var, char *buffer) {
  var->type = SHOW_LONGLONG;
  var->value = buffer;
  ulonglong *value = reinterpret_cast<ulonglong *>(buffer);
  *value = metrics_handler->get_certification_garbage_collector_count();
  return 0;
}

int Plugin_status_variables::get_certification_garbage_collector_time_sum(
    MYSQL_THD, SHOW_VAR *var, char *buffer) {
  var->type = SHOW_LONGLONG;
  var->value = buffer;
  ulonglong *value = reinterpret_cast<ulonglong *>(buffer);
  *value = metrics_handler->get_certification_garbage_collector_time_sum();
  return 0;
}

int Plugin_status_variables::get_all_consensus_proposals_count(MYSQL_THD,
                                                               SHOW_VAR *var,
                                                               char *buffer) {
  var->type = SHOW_LONGLONG;
  var->value = buffer;
  ulonglong *value = reinterpret_cast<ulonglong *>(buffer);
  *value = gcs_module->get_all_consensus_proposals_count();
  return 0;
}

int Plugin_status_variables::get_empty_consensus_proposals_count(MYSQL_THD,
                                                                 SHOW_VAR *var,
                                                                 char *buffer) {
  var->type = SHOW_LONGLONG;
  var->value = buffer;
  ulonglong *value = reinterpret_cast<ulonglong *>(buffer);
  *value = gcs_module->get_empty_consensus_proposals_count();
  return 0;
}

int Plugin_status_variables::get_consensus_bytes_sent_sum(MYSQL_THD,
                                                          SHOW_VAR *var,
                                                          char *buffer) {
  var->type = SHOW_LONGLONG;
  var->value = buffer;
  ulonglong *value = reinterpret_cast<ulonglong *>(buffer);
  *value = gcs_module->get_consensus_bytes_sent_sum();
  return 0;
}

int Plugin_status_variables::get_consensus_bytes_received_sum(MYSQL_THD,
                                                              SHOW_VAR *var,
                                                              char *buffer) {
  var->type = SHOW_LONGLONG;
  var->value = buffer;
  ulonglong *value = reinterpret_cast<ulonglong *>(buffer);
  *value = gcs_module->get_consensus_bytes_received_sum();
  return 0;
}

int Plugin_status_variables::get_all_consensus_time_sum(MYSQL_THD,
                                                        SHOW_VAR *var,
                                                        char *buffer) {
  var->type = SHOW_LONGLONG;
  var->value = buffer;
  ulonglong *value = reinterpret_cast<ulonglong *>(buffer);
  *value = gcs_module->get_all_consensus_time_sum();
  return 0;
}

int Plugin_status_variables::get_extended_consensus_count(MYSQL_THD,
                                                          SHOW_VAR *var,
                                                          char *buffer) {
  var->type = SHOW_LONGLONG;
  var->value = buffer;
  ulonglong *value = reinterpret_cast<ulonglong *>(buffer);
  *value = gcs_module->get_extended_consensus_count();
  return 0;
}

int Plugin_status_variables::get_total_messages_sent_count(MYSQL_THD,
                                                           SHOW_VAR *var,
                                                           char *buffer) {
  var->type = SHOW_LONGLONG;
  var->value = buffer;
  ulonglong *value = reinterpret_cast<ulonglong *>(buffer);
  *value = gcs_module->get_total_messages_sent_count();
  return 0;
}

int Plugin_status_variables::get_last_consensus_end_timestamp(MYSQL_THD,
                                                              SHOW_VAR *var,
                                                              char *buffer) {
  assert(SHOW_VAR_FUNC_BUFF_SIZE > MAX_DATE_STRING_REP_LENGTH);
  var->type = SHOW_CHAR;
  var->value = nullptr;

  uint64_t microseconds_since_epoch =
      gcs_module->get_last_consensus_end_timestamp();
  if (microseconds_since_epoch > 0) {
    microseconds_to_datetime_str(microseconds_since_epoch, buffer, 6);
    var->value = buffer;
  }

  return 0;
}

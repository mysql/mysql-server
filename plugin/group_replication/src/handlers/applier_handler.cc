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

#include "plugin/group_replication/include/handlers/applier_handler.h"

#include <stddef.h>

#include <mysql/components/services/log_builtins.h>
#include "my_dbug.h"
#include "plugin/group_replication/include/plugin.h"

Applier_handler::Applier_handler() = default;

int Applier_handler::initialize() {
  DBUG_TRACE;
  return 0;
}

int Applier_handler::terminate() {
  DBUG_TRACE;
  return 0;
}

int Applier_handler::initialize_repositories(bool reset_logs,
                                             ulong plugin_shutdown_timeout) {
  DBUG_TRACE;

  int error = 0;

  if (reset_logs) {
    LogPluginErr(INFORMATION_LEVEL, ER_GRP_RPL_PURGE_APPLIER_LOGS);

    if ((error = channel_interface.purge_logs(false))) {
      /* purecov: begin inspected */
      LogPluginErr(ERROR_LEVEL, ER_GRP_RPL_RESET_APPLIER_MODULE_LOGS_ERROR);
      return error;
      /* purecov: end */
    }
  }

  channel_interface.set_stop_wait_timeout(plugin_shutdown_timeout);

  error = channel_interface.initialize_channel(
      /*host*/ const_cast<char *>("<NULL>"), /*port*/ 0, /*user*/ nullptr,
      /*pass*/ nullptr, /*use_ssl*/ false, /*ssl_ca*/ nullptr,
      /*ssl_capath*/ nullptr, /*ssl_cert*/ nullptr, /*ssl_cipher*/ nullptr,
      /*ssl_key*/ nullptr, /*ssl_crl*/ nullptr, /*ssl_crlpath*/ nullptr,
      /*ssl_verify*/ false,
      /*priority*/ GROUP_REPLICATION_APPLIER_THREAD_PRIORITY,
      /*retry_count*/ 0,
      /*preserve_logs*/ true, /*public_key_path*/ nullptr,
      /*get_public_key*/ false, /*compression_alg*/ nullptr,
      /*compression_level*/ 0, /*tls_version*/ nullptr, /*tls_cipher*/ nullptr,
      /*ignore_ws_mem_limit*/ true, /*allow_drop_write_set*/ true);

  if (error) {
    LogPluginErr(ERROR_LEVEL,
                 ER_GRP_RPL_APPLIER_THD_SETUP_ERROR); /* purecov: inspected */
  }

  return error;
}

int Applier_handler::start_applier_thread() {
  DBUG_TRACE;

  int error = channel_interface.start_threads(false, true, nullptr, false);
  if (error) {
    LogPluginErr(ERROR_LEVEL, ER_GRP_RPL_APPLIER_THD_START_ERROR);
  }

  return error;
}

int Applier_handler::stop_applier_thread() {
  DBUG_TRACE;

  int error = 0;

  if (!channel_interface.is_applier_thread_running()) return 0;

  if ((error = channel_interface.stop_threads(false, true))) {
    LogPluginErr(ERROR_LEVEL,
                 ER_GRP_RPL_APPLIER_THD_STOP_ERROR); /* purecov: inspected */
  }

  return error;
}

int Applier_handler::handle_event(Pipeline_event *pevent, Continuation *cont) {
  DBUG_TRACE;
  Pipeline_event::Pipeline_event_type event_type =
      pevent->get_pipeline_event_type();
  switch (event_type) {
    case Pipeline_event::Pipeline_event_type::PEVENT_DATA_PACKET_TYPE_E:
      return handle_binary_log_event(pevent, cont);
    case Pipeline_event::Pipeline_event_type::PEVENT_BINARY_LOG_EVENT_TYPE_E:
      return handle_binary_log_event(pevent, cont);
    case Pipeline_event::Pipeline_event_type::PEVENT_APPLIER_ONLY_EVENT_E:
      return handle_applier_event(pevent, cont);
    default:
      next(pevent, cont);
      return 0;
  }
}

int Applier_handler::handle_binary_log_event(Pipeline_event *event,
                                             Continuation *cont) {
  Data_packet *p = nullptr;
  int error = event->get_Packet(&p);
  DBUG_EXECUTE_IF("applier_handler_force_error_on_pipeline", error = 1;);
  if (error || (p == nullptr)) {
    LogPluginErr(ERROR_LEVEL, ER_GRP_RPL_FETCH_TRANS_DATA_FAILED);
    error = 1;
    goto end;
  }

  /*
    There is no need to queue Transaction_context_log_event to
    server applier, this event is only need for certification,
    performed on the previous handler.
  */
  if (event->get_event_type() !=
      mysql::binlog::event::TRANSACTION_CONTEXT_EVENT) {
    error = channel_interface.queue_packet((const char *)p->payload, p->len);

    if (mysql::binlog::event::Log_event_type_helper::is_assigned_gtid_event(
            event->get_event_type())) {
      applier_module->get_pipeline_stats_member_collector()
          ->increment_transactions_waiting_apply();
    }
  }

end:

  if (error)
    cont->signal(error);
  else
    next(event, cont);
  return error;
}

int Applier_handler::handle_applier_event(Pipeline_event *event,
                                          Continuation *cont) {
  return next(event, cont);
}

int Applier_handler::handle_action(Pipeline_action *action) {
  DBUG_TRACE;
  int error = 0;

  Plugin_handler_action action_type =
      (Plugin_handler_action)action->get_action_type();

  switch (action_type) {
    case HANDLER_START_ACTION:
      error = start_applier_thread();
      break;
    case HANDLER_STOP_ACTION:
      error = stop_applier_thread();
      break;
    case HANDLER_APPLIER_CONF_ACTION: {
      Handler_applier_configuration_action *conf_action =
          (Handler_applier_configuration_action *)action;

      if (conf_action->is_initialization_conf()) {
        channel_interface.set_channel_name(conf_action->get_applier_name());
        error = initialize_repositories(
            conf_action->is_reset_logs_planned(),
            conf_action->get_applier_shutdown_timeout());
      } else {
        ulong timeout = conf_action->get_applier_shutdown_timeout();
        channel_interface.set_stop_wait_timeout(timeout);
      }
      break;
    }
    default:
      break;
  }

  if (error) return error;

  return next(action);
}

bool Applier_handler::is_unique() { return true; }

int Applier_handler::get_role() { return APPLIER; }

bool Applier_handler::is_applier_thread_waiting() {
  DBUG_TRACE;

  bool result = channel_interface.is_applier_thread_waiting();

  return result;
}

int Applier_handler::wait_for_gtid_execution(double timeout) {
  DBUG_TRACE;

  int error = channel_interface.wait_for_gtid_execution(timeout);

  return error;
}

int Applier_handler::wait_for_gtid_execution(std::string &retrieved_set,
                                             double timeout,
                                             bool update_THD_status) {
  DBUG_TRACE;

  int error = channel_interface.wait_for_gtid_execution(retrieved_set, timeout,
                                                        update_THD_status);

  return error;
}

int Applier_handler::is_partial_transaction_on_relay_log() {
  return channel_interface.is_partial_transaction_on_relay_log();
}

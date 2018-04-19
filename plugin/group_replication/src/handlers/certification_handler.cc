/* Copyright (c) 2014, 2018, Oracle and/or its affiliates. All rights reserved.

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

#include "plugin/group_replication/include/handlers/certification_handler.h"

#include <mysql/components/services/log_builtins.h>
#include "my_dbug.h"
#include "my_inttypes.h"
#include "plugin/group_replication/include/handlers/pipeline_handlers.h"
#include "plugin/group_replication/include/plugin.h"

using std::string;
const int GTID_WAIT_TIMEOUT = 30;  // 30 seconds

Certification_handler::Certification_handler()
    : cert_module(NULL),
      applier_module_thd(NULL),
      group_sidno(0),
      transaction_context_packet(NULL),
      transaction_context_pevent(NULL) {}

Certification_handler::~Certification_handler() {
  delete transaction_context_pevent;
  delete transaction_context_packet;
}

int Certification_handler::initialize() {
  DBUG_ENTER("Certification_handler::initialize");
  DBUG_ASSERT(cert_module == NULL);
  cert_module = new Certifier();
  DBUG_RETURN(0);
}

int Certification_handler::terminate() {
  DBUG_ENTER("Certification_handler::terminate");
  int error = 0;

  if (cert_module == NULL) DBUG_RETURN(error); /* purecov: inspected */

  delete cert_module;
  cert_module = NULL;
  DBUG_RETURN(error);
}

int Certification_handler::handle_action(Pipeline_action *action) {
  DBUG_ENTER("Certification_handler::handle_action");

  int error = 0;

  Plugin_handler_action action_type =
      (Plugin_handler_action)action->get_action_type();

  if (action_type == HANDLER_CERT_CONF_ACTION) {
    Handler_certifier_configuration_action *conf_action =
        (Handler_certifier_configuration_action *)action;

    error =
        cert_module->initialize(conf_action->get_gtid_assignment_block_size());

    group_sidno = conf_action->get_group_sidno();
  } else if (action_type == HANDLER_CERT_INFO_ACTION) {
    Handler_certifier_information_action *cert_inf_action =
        (Handler_certifier_information_action *)action;

    error = cert_module->set_certification_info(
        cert_inf_action->get_certification_info());
  } else if (action_type == HANDLER_VIEW_CHANGE_ACTION) {
    View_change_pipeline_action *vc_action =
        (View_change_pipeline_action *)action;

    if (!vc_action->is_leaving()) {
      cert_module->handle_view_change();
    }
  } else if (action_type == HANDLER_THD_ACTION) {
    Handler_THD_setup_action *thd_conf_action =
        (Handler_THD_setup_action *)action;
    applier_module_thd = thd_conf_action->get_THD_object();
  } else if (action_type == HANDLER_STOP_ACTION) {
    error = cert_module->terminate();
  }

  if (error) DBUG_RETURN(error);

  DBUG_RETURN(next(action));
}

int Certification_handler::handle_event(Pipeline_event *pevent,
                                        Continuation *cont) {
  DBUG_ENTER("Certification_handler::handle_event");

  Log_event_type ev_type = pevent->get_event_type();
  switch (ev_type) {
    case binary_log::TRANSACTION_CONTEXT_EVENT:
      DBUG_RETURN(handle_transaction_context(pevent, cont));
    case binary_log::GTID_LOG_EVENT:
      DBUG_RETURN(handle_transaction_id(pevent, cont));
    case binary_log::VIEW_CHANGE_EVENT:
      DBUG_RETURN(extract_certification_info(pevent, cont));
    default:
      next(pevent, cont);
      DBUG_RETURN(0);
  }
}

int Certification_handler::set_transaction_context(Pipeline_event *pevent) {
  DBUG_ENTER("Certification_handler::set_transaction_context");
  int error = 0;

  DBUG_ASSERT(transaction_context_packet == NULL);
  DBUG_ASSERT(transaction_context_pevent == NULL);

  Data_packet *packet = NULL;
  error = pevent->get_Packet(&packet);
  if (error || (packet == NULL)) {
    /* purecov: begin inspected */
    LogPluginErr(ERROR_LEVEL, ER_GRP_RPL_FETCH_TRANS_CONTEXT_FAILED);
    DBUG_RETURN(1);
    /* purecov: end */
  }
  transaction_context_packet = new Data_packet(packet->payload, packet->len);

  DBUG_RETURN(error);
}

int Certification_handler::get_transaction_context(
    Pipeline_event *pevent, Transaction_context_log_event **tcle) {
  DBUG_ENTER("Certification_handler::get_transaction_context");
  int error = 0;

  DBUG_ASSERT(transaction_context_packet != NULL);
  DBUG_ASSERT(transaction_context_pevent == NULL);

  Format_description_log_event *fdle = NULL;
  if (pevent->get_FormatDescription(&fdle) && (fdle == NULL)) {
    /* purecov: begin inspected */
    LogPluginErr(ERROR_LEVEL, ER_GRP_RPL_FETCH_FORMAT_DESC_LOG_EVENT_FAILED);
    DBUG_RETURN(1);
    /* purecov: end */
  }

  transaction_context_pevent =
      new Pipeline_event(transaction_context_packet, fdle, pevent->get_cache());
  Log_event *transaction_context_event = NULL;
  error = transaction_context_pevent->get_LogEvent(&transaction_context_event);
  transaction_context_packet = NULL;
  DBUG_EXECUTE_IF("certification_handler_force_error_on_pipeline", error = 1;);
  if (error || (transaction_context_event == NULL)) {
    LogPluginErr(ERROR_LEVEL, ER_GRP_RPL_FETCH_TRANS_CONTEXT_LOG_EVENT_FAILED);
    DBUG_RETURN(1);
  }

  *tcle =
      static_cast<Transaction_context_log_event *>(transaction_context_event);
  if ((*tcle)->read_snapshot_version()) {
    /* purecov: begin inspected */
    LogPluginErr(ERROR_LEVEL, ER_GRP_RPL_FETCH_SNAPSHOT_VERSION_FAILED);
    DBUG_RETURN(1);
    /* purecov: end */
  }

  DBUG_RETURN(error);
}

void Certification_handler::reset_transaction_context() {
  DBUG_ENTER("Certification_handler::reset_transaction_context");

  /*
    Release memory allocated to transaction_context_packet,
    since it is wrapped by transaction_context_pevent.
  */
  delete transaction_context_pevent;
  transaction_context_pevent = NULL;

  DBUG_VOID_RETURN;
}

int Certification_handler::handle_transaction_context(Pipeline_event *pevent,
                                                      Continuation *cont) {
  DBUG_ENTER("Certification_handler::handle_transaction_context");
  int error = 0;

  error = set_transaction_context(pevent);
  if (error)
    cont->signal(1, true); /* purecov: inspected */
  else
    next(pevent, cont);

  DBUG_RETURN(error);
}

int Certification_handler::handle_transaction_id(Pipeline_event *pevent,
                                                 Continuation *cont) {
  DBUG_ENTER("Certification_handler::handle_transaction_id");
  int error = 0;
  rpl_gno seq_number = 0;
  bool local_transaction = true;
  Transaction_context_log_event *tcle = NULL;
  Log_event *event = NULL;
  Gtid_log_event *gle = NULL;

  /*
    Get transaction context.
  */
  error = get_transaction_context(pevent, &tcle);
  if (error) {
    cont->signal(1, true);
    goto end;
  }

  /*
    Get transaction global identifier event.
  */
  error = pevent->get_LogEvent(&event);
  if (error || (event == NULL)) {
    /* purecov: begin inspected */
    LogPluginErr(ERROR_LEVEL, ER_GRP_RPL_FETCH_GTID_LOG_EVENT_FAILED);
    cont->signal(1, true);
    error = 1;
    goto end;
    /* purecov: end */
  }
  gle = static_cast<Gtid_log_event *>(event);

  local_transaction =
      !strncmp(tcle->get_server_uuid(), local_member_info->get_uuid().c_str(),
               UUID_LENGTH);

  /*
    Certify transaction.
  */
  seq_number =
      cert_module->certify(tcle->get_snapshot_version(), tcle->get_write_set(),
                           !tcle->is_gtid_specified(), tcle->get_server_uuid(),
                           gle, local_transaction);

  if (local_transaction) {
    /*
      Local transaction.
      After certification we need to wake up the waiting thread on the
      plugin to proceed with the transaction processing.
      Sequence number <= 0 means abort, so we need to pass a negative
      value to transaction context.
    */
    Transaction_termination_ctx transaction_termination_ctx;
    memset(&transaction_termination_ctx, 0,
           sizeof(transaction_termination_ctx));
    transaction_termination_ctx.m_thread_id = tcle->get_thread_id();
    if (seq_number > 0) {
      transaction_termination_ctx.m_rollback_transaction = false;
      if (tcle->is_gtid_specified()) {
        transaction_termination_ctx.m_generated_gtid = false;
      } else {
        transaction_termination_ctx.m_generated_gtid = true;
        transaction_termination_ctx.m_sidno = group_sidno;
        transaction_termination_ctx.m_gno = seq_number;
      }
    } else {
      transaction_termination_ctx.m_rollback_transaction = true;
      transaction_termination_ctx.m_generated_gtid = false;
      transaction_termination_ctx.m_sidno = -1;
      transaction_termination_ctx.m_gno = -1;
    }

    if (set_transaction_ctx(transaction_termination_ctx)) {
      /* purecov: begin inspected */
      LogPluginErr(ERROR_LEVEL, ER_GRP_RPL_UPDATE_SERV_CERTIFICATE_FAILED,
                   tcle->get_thread_id());
      cont->signal(1, true);
      error = 1;
      goto end;
      /* purecov: end */
    }

    if (seq_number > 0) {
      if (tcle->is_gtid_specified()) {
        error =
            cert_module->add_specified_gtid_to_group_gtid_executed(gle, true);
        DBUG_EXECUTE_IF("unable_to_add_specified_gtid_for_local_transaction",
                        error = 1;);

        if (error) {
          LogPluginErr(ERROR_LEVEL,
                       ER_GRP_RPL_ADD_GTID_INFO_WITH_LOCAL_GTID_FAILED);
          certification_latch->releaseTicket(tcle->get_thread_id());
          cont->signal(1, true);
          goto end;
        }
      } else {
        if (cert_module->add_group_gtid_to_group_gtid_executed(seq_number,
                                                               true)) {
          /* purecov: begin inspected */
          LogPluginErr(ERROR_LEVEL,
                       ER_GRP_RPL_ADD_GTID_INFO_WITHOUT_LOCAL_GTID_FAILED);
          certification_latch->releaseTicket(tcle->get_thread_id());
          cont->signal(1, true);
          error = 1;
          goto end;
          /* purecov: end */
        }
      }
    }

    if (certification_latch->releaseTicket(tcle->get_thread_id())) {
      /* purecov: begin inspected */
      LogPluginErr(ERROR_LEVEL, ER_GRP_RPL_NOTIFY_CERTIFICATION_OUTCOME_FAILED);
      cont->signal(1, true);
      error = 1;
      goto end;
      /* purecov: end */
    }

    // The pipeline ended for this transaction
    cont->signal(0, true);
  } else {
    /*
      Remote transaction.
    */
    if (seq_number > 0) {
      if (!tcle->is_gtid_specified()) {
        // Create new GTID event.
        Gtid gtid = {group_sidno, seq_number};
        Gtid_specification gtid_specification = {ASSIGNED_GTID, gtid};
        Gtid_log_event *gle_generated = new Gtid_log_event(
            gle->server_id, gle->is_using_trans_cache(), gle->last_committed,
            gle->sequence_number, gle->may_have_sbr_stmts,
            gle->original_commit_timestamp, gle->immediate_commit_timestamp,
            gtid_specification);
        // Copy the transaction length to the new event.
        gle_generated->set_trx_length(gle->transaction_length);

        pevent->reset_pipeline_event();
        pevent->set_LogEvent(gle_generated);

        // Add the gtid information in the executed gtid set for the remote
        // transaction which have gtid specified.
        if (cert_module->add_group_gtid_to_group_gtid_executed(seq_number,
                                                               false)) {
          /* purecov: begin inspected */
          LogPluginErr(ERROR_LEVEL,
                       ER_GRP_RPL_ADD_GTID_INFO_WITHOUT_REMOTE_GTID_FAILED);
          cont->signal(1, true);
          error = 1;
          goto end;
          /* purecov: end */
        }
      }

      else {
        error =
            cert_module->add_specified_gtid_to_group_gtid_executed(gle, false);
        DBUG_EXECUTE_IF("unable_to_add_specified_gtid_for_remote_transaction",
                        error = 1;);

        if (error) {
          /* purecov: begin inspected */
          LogPluginErr(ERROR_LEVEL,
                       ER_GRP_RPL_ADD_GTID_INFO_WITH_REMOTE_GTID_FAILED);
          cont->signal(1, true);
          goto end;
          /* purecov: end */
        }
      }

      // Pass transaction to next action.
      next(pevent, cont);
    } else if (seq_number < 0) {
      error = 1;
      cont->signal(1, true);
      goto end;
    } else {
      // The transaction was negatively certified so discard it.
      cont->signal(0, true);
    }
  }

end:
  reset_transaction_context();
  DBUG_RETURN(error);
}

int Certification_handler::extract_certification_info(Pipeline_event *pevent,
                                                      Continuation *cont) {
  DBUG_ENTER("Certification_handler::extract_certification_info");
  int error = 0;
  Log_event *event = NULL;

  if (pevent->get_event_context() != SINGLE_VIEW_EVENT) {
    /*
      If the current view event is embraced on a transaction:
      GTID, BEGIN, VIEW, COMMIT; it means that we are handling
      a view that was delivered by a asynchronous channel from
      outside of the group.
      On that case we just have to queue it on the group applier
      channel, without any special handling.
    */
    next(pevent, cont);
    DBUG_RETURN(error);
  }

  /*
    If the current view event is a standalone event (not inside a
    transaction), it means that it was injected from GCS on a
    membership change.
    On that case we need to queue it on the group applier wrapped
    on a transaction with a group generated GTID.
  */
  error = pevent->get_LogEvent(&event);
  if (error || (event == NULL)) {
    /* purecov: begin inspected */
    LogPluginErr(ERROR_LEVEL, ER_GRP_RPL_FETCH_VIEW_CHANGE_LOG_EVENT_FAILED);
    cont->signal(1, true);
    DBUG_RETURN(1);
    /* purecov: end */
  }
  View_change_log_event *vchange_event =
      static_cast<View_change_log_event *>(event);

  std::map<std::string, std::string> cert_info;
  cert_module->get_certification_info(&cert_info);
  vchange_event->set_certification_info(&cert_info);

  // Assure the last known local transaction was already executed
  error = wait_for_local_transaction_execution();

  /**
    Create a transactional block for the View change log event
    GTID
    BEGIN
    VCLE
    COMMIT
  */
  if (!error) error = inject_transactional_events(pevent, cont);

  DBUG_RETURN(error);
}

int Certification_handler::wait_for_local_transaction_execution() {
  DBUG_ENTER("Certification_handler::wait_for_local_transaction_execution");
  int error = 0;

  std::string local_gtid_certified_string;
  if (!cert_module->get_local_certified_gtid(local_gtid_certified_string)) {
    DBUG_RETURN(0);  // empty
  }

  Sql_service_command_interface *sql_command_interface =
      new Sql_service_command_interface();

  if (sql_command_interface->establish_session_connection(PSESSION_USE_THREAD,
                                                          GROUPREPL_USER)) {
    /* purecov: begin inspected */
    LogPluginErr(ERROR_LEVEL, ER_GRP_RPL_CONTACT_WITH_SRV_FAILED);
    delete sql_command_interface;
    DBUG_RETURN(1);
    /* purecov: end */
  }

  if ((error = sql_command_interface->wait_for_server_gtid_executed(
           local_gtid_certified_string, GTID_WAIT_TIMEOUT))) {
    /* purecov: begin inspected */
    if (error == 1)  // timeout
    {
      LogPluginErr(ERROR_LEVEL, ER_GRP_RPL_SRV_WAIT_TIME_OUT);
    } else {
      LogPluginErr(ERROR_LEVEL, ER_GRP_RPL_SRV_WAIT_TIME_OUT);
    }
    /* purecov: end */
  }
  delete sql_command_interface;
  DBUG_RETURN(error);
}

int Certification_handler::inject_transactional_events(Pipeline_event *pevent,
                                                       Continuation *cont) {
  DBUG_ENTER("Certification_handler::inject_transactional_events");
  Log_event *event = NULL;
  Format_description_log_event *fd_event = NULL;

  if (pevent->get_LogEvent(&event) || (event == NULL)) {
    /* purecov: begin inspected */
    LogPluginErr(ERROR_LEVEL, ER_GRP_RPL_FETCH_LOG_EVENT_FAILED);
    cont->signal(1, true);
    DBUG_RETURN(1);
    /* purecov: end */
  }

  if (pevent->get_FormatDescription(&fd_event) && (fd_event == NULL)) {
    /* purecov: begin inspected */
    LogPluginErr(ERROR_LEVEL, ER_GRP_RPL_FETCH_FORMAT_DESC_LOG_EVENT_FAILED);
    cont->signal(1, true);
    DBUG_RETURN(1);
    /* purecov: end */
  }

  // GTID event

  Gtid gtid = {group_sidno, cert_module->generate_view_change_group_gno()};
  if (gtid.gno <= 0) {
    cont->signal(1, true);
    DBUG_RETURN(1);
  }
  Gtid_specification gtid_specification = {ASSIGNED_GTID, gtid};
  /**
   The original_commit_timestamp of this Gtid_log_event will be zero
   because the transaction corresponds to a View_change_event, which is
   generated and committed locally by all members. Consequently, there is no
   'original master'. So, instead of each member generating a GTID with
   its own unique original_commit_timestamp (and violating the property that
   the original_commit_timestamp is the same for a given GTID), this timestamp
   will not be defined.
  */
  Gtid_log_event *gtid_log_event = new Gtid_log_event(
      event->server_id, true, 0, 0, true, 0, 0, gtid_specification);

  Pipeline_event *gtid_pipeline_event =
      new Pipeline_event(gtid_log_event, fd_event, pevent->get_cache());
  next(gtid_pipeline_event, cont);

  int error = cont->wait();
  delete gtid_pipeline_event;
  if (error) {
    DBUG_RETURN(0); /* purecov: inspected */
  }

  // BEGIN event

  Log_event *begin_log_event = new Query_log_event(
      applier_module_thd, STRING_WITH_LEN("BEGIN"), true, false, true, 0, true);

  Pipeline_event *begin_pipeline_event =
      new Pipeline_event(begin_log_event, fd_event, pevent->get_cache());
  next(begin_pipeline_event, cont);

  error = cont->wait();
  delete begin_pipeline_event;
  if (error) {
    DBUG_RETURN(0); /* purecov: inspected */
  }

  /*
   Queues the given event.
   As we don't have asynchronous we can use the received Continuation.
   If that is no longer true, another Continuation object must be created here.
  */
  next(pevent, cont);
  error = cont->wait();
  if (error) {
    DBUG_RETURN(0); /* purecov: inspected */
  }

  // COMMIT event

  Log_event *end_log_event =
      new Query_log_event(applier_module_thd, STRING_WITH_LEN("COMMIT"), true,
                          false, true, 0, true);

  Pipeline_event *end_pipeline_event =
      new Pipeline_event(end_log_event, fd_event, pevent->get_cache());
  next(end_pipeline_event, cont);
  delete end_pipeline_event;

  DBUG_RETURN(0);
}

bool Certification_handler::is_unique() { return true; }

int Certification_handler::get_role() { return CERTIFIER; }

Certifier_interface *Certification_handler::get_certifier() {
  return cert_module;
}

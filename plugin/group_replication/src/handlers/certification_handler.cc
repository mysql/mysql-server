/* Copyright (c) 2014, 2023, Oracle and/or its affiliates.

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
#include "plugin/group_replication/include/consistency_manager.h"
#include "plugin/group_replication/include/handlers/pipeline_handlers.h"
#include "plugin/group_replication/include/plugin.h"

using std::string;

Certification_handler::Certification_handler()
    : cert_module(nullptr),
      applier_module_thd(nullptr),
      group_sidno(0),
      transaction_context_packet(nullptr),
      transaction_context_pevent(nullptr) {}

Certification_handler::~Certification_handler() {
  delete transaction_context_pevent;
  delete transaction_context_packet;
  pending_view_change_events_waiting_for_consistent_transactions.clear();
}

int Certification_handler::initialize() {
  DBUG_TRACE;
  assert(cert_module == nullptr);
  cert_module = new Certifier();
  return 0;
}

int Certification_handler::terminate() {
  DBUG_TRACE;
  int error = 0;

  if (cert_module == nullptr) return error; /* purecov: inspected */

  delete cert_module;
  cert_module = nullptr;
  return error;
}

int Certification_handler::handle_action(Pipeline_action *action) {
  DBUG_TRACE;

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

  if (error) return error;

  return next(action);
}

int Certification_handler::handle_event(Pipeline_event *pevent,
                                        Continuation *cont) {
  DBUG_TRACE;

  Log_event_type ev_type = pevent->get_event_type();
  switch (ev_type) {
    case binary_log::TRANSACTION_CONTEXT_EVENT:
      return handle_transaction_context(pevent, cont);
    case binary_log::GTID_LOG_EVENT:
      return handle_transaction_id(pevent, cont);
    case binary_log::VIEW_CHANGE_EVENT:
      return extract_certification_info(pevent, cont);
    default:
      next(pevent, cont);
      return 0;
  }
}

int Certification_handler::set_transaction_context(Pipeline_event *pevent) {
  DBUG_TRACE;
  int error = 0;

  assert(transaction_context_packet == nullptr);
  assert(transaction_context_pevent == nullptr);

  Data_packet *packet = nullptr;
  error = pevent->get_Packet(&packet);
  if (error || (packet == nullptr)) {
    /* purecov: begin inspected */
    LogPluginErr(ERROR_LEVEL, ER_GRP_RPL_FETCH_TRANS_CONTEXT_FAILED);
    return 1;
    /* purecov: end */
  }
  transaction_context_packet =
      new Data_packet(packet->payload, packet->len, key_certification_data);

  DBUG_EXECUTE_IF(
      "group_replication_certification_handler_set_transaction_context", {
        const char act[] =
            "now signal "
            "signal.group_replication_certification_handler_set_transaction_"
            "context_reached "
            "wait_for "
            "signal.group_replication_certification_handler_set_transaction_"
            "context_continue";
        assert(!debug_sync_set_action(current_thd, STRING_WITH_LEN(act)));
      });

  return error;
}

int Certification_handler::get_transaction_context(
    Pipeline_event *pevent, Transaction_context_log_event **tcle) {
  DBUG_TRACE;
  int error = 0;

  assert(transaction_context_packet != nullptr);
  assert(transaction_context_pevent == nullptr);

  Format_description_log_event *fdle = nullptr;
  if (pevent->get_FormatDescription(&fdle) && (fdle == nullptr)) {
    /* purecov: begin inspected */
    LogPluginErr(ERROR_LEVEL, ER_GRP_RPL_FETCH_FORMAT_DESC_LOG_EVENT_FAILED);
    return 1;
    /* purecov: end */
  }

  transaction_context_pevent =
      new Pipeline_event(transaction_context_packet, fdle);
  Log_event *transaction_context_event = nullptr;
  error = transaction_context_pevent->get_LogEvent(&transaction_context_event);
  transaction_context_packet = nullptr;
  DBUG_EXECUTE_IF("certification_handler_force_error_on_pipeline", error = 1;);
  if (error || (transaction_context_event == nullptr)) {
    LogPluginErr(ERROR_LEVEL, ER_GRP_RPL_FETCH_TRANS_CONTEXT_LOG_EVENT_FAILED);
    return 1;
  }

  *tcle =
      static_cast<Transaction_context_log_event *>(transaction_context_event);
  if ((*tcle)->read_snapshot_version()) {
    /* purecov: begin inspected */
    LogPluginErr(ERROR_LEVEL, ER_GRP_RPL_FETCH_SNAPSHOT_VERSION_FAILED);
    return 1;
    /* purecov: end */
  }

  return error;
}

void Certification_handler::reset_transaction_context() {
  DBUG_TRACE;

  /*
    Release memory allocated to transaction_context_packet,
    since it is wrapped by transaction_context_pevent.
  */
  delete transaction_context_pevent;
  transaction_context_pevent = nullptr;
  DBUG_EXECUTE_IF(
      "group_replication_certification_handler_reset_transaction_context", {
        const char act[] =
            "now signal "
            "signal.group_replication_certification_handler_reset_transaction_"
            "context_reached "
            "wait_for "
            "signal.group_replication_certification_handler_reset_transaction_"
            "context_continue";
        assert(!debug_sync_set_action(current_thd, STRING_WITH_LEN(act)));
      });
}

int Certification_handler::handle_transaction_context(Pipeline_event *pevent,
                                                      Continuation *cont) {
  DBUG_TRACE;
  int error = 0;

  error = set_transaction_context(pevent);
  if (error)
    cont->signal(1, true); /* purecov: inspected */
  else
    next(pevent, cont);

  return error;
}

int Certification_handler::handle_transaction_id(Pipeline_event *pevent,
                                                 Continuation *cont) {
  DBUG_TRACE;
  int error = 0;
  rpl_gno seq_number = 0;
  bool local_transaction = true;
  Transaction_context_log_event *tcle = nullptr;
  Log_event *event = nullptr;
  Gtid_log_event *gle = nullptr;
  Members_list *online_members = pevent->get_online_members();

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
  if (error || (event == nullptr)) {
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
    Group contains members that do not support transactions with group
    coordination, thence the transaction must rollback.
  */
  DBUG_EXECUTE_IF(
      "group_replication_force_lower_version_on_group_replication_consistency",
      { online_members = nullptr; };);
  if (pevent->get_consistency_level() >= GROUP_REPLICATION_CONSISTENCY_AFTER &&
      nullptr == online_members) {
    goto after_certify;
  }

  /*
    Certify transaction.
  */
  seq_number =
      cert_module->certify(tcle->get_snapshot_version(), tcle->get_write_set(),
                           !tcle->is_gtid_specified(), tcle->get_server_uuid(),
                           gle, local_transaction);

after_certify:
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
      const rpl_sid *sid = nullptr;
      rpl_sidno sidno = group_sidno;
      rpl_gno gno = seq_number;

      if (tcle->is_gtid_specified()) {
        sid = gle->get_sid();
        sidno = gle->get_sidno(true);
        gno = gle->get_gno();
        error = cert_module->add_specified_gtid_to_group_gtid_executed(gle);
        DBUG_EXECUTE_IF("unable_to_add_specified_gtid_for_local_transaction",
                        error = 1;);

        if (error) {
          LogPluginErr(ERROR_LEVEL,
                       ER_GRP_RPL_ADD_GTID_INFO_WITH_LOCAL_GTID_FAILED);
          transactions_latch->releaseTicket(tcle->get_thread_id());
          cont->signal(1, true);
          goto end;
        }
      } else {
        if (cert_module->add_group_gtid_to_group_gtid_executed(gno)) {
          /* purecov: begin inspected */
          LogPluginErr(ERROR_LEVEL,
                       ER_GRP_RPL_ADD_GTID_INFO_WITHOUT_LOCAL_GTID_FAILED);
          transactions_latch->releaseTicket(tcle->get_thread_id());
          cont->signal(1, true);
          error = 1;
          goto end;
          /* purecov: end */
        }
      }

      if (pevent->get_consistency_level() >=
          GROUP_REPLICATION_CONSISTENCY_AFTER) {
        Transaction_consistency_info *transaction_consistency_info =
            new Transaction_consistency_info(
                tcle->get_thread_id(), local_transaction, sid, sidno, gno,
                pevent->get_consistency_level(), pevent->get_online_members());
        pevent->release_online_members_memory_ownership();
        if (transaction_consistency_manager->after_certification(
                transaction_consistency_info)) {
          /* purecov: begin inspected */
          delete transaction_consistency_info;
          cont->signal(1, true);
          error = 1;
          goto end;
          /* purecov: end */
        }
      }
    }

    /*
      We only release the local transaction here when its consistency
      does not require group coordination.
    */
    if ((seq_number <= 0 || pevent->get_consistency_level() <
                                GROUP_REPLICATION_CONSISTENCY_AFTER) &&
        transactions_latch->releaseTicket(tcle->get_thread_id())) {
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
      const rpl_sid *sid = nullptr;
      rpl_sidno sidno = group_sidno;
      rpl_gno gno = seq_number;

      if (!tcle->is_gtid_specified()) {
        // Create new GTID event.
        Gtid gtid = {sidno, gno};
        Gtid_specification gtid_specification = {ASSIGNED_GTID, gtid};
        Gtid_log_event *gle_generated = new Gtid_log_event(
            gle->server_id, gle->is_using_trans_cache(), gle->last_committed,
            gle->sequence_number, gle->may_have_sbr_stmts,
            gle->original_commit_timestamp, gle->immediate_commit_timestamp,
            gtid_specification, gle->original_server_version,
            gle->immediate_server_version);
        // Copy the transaction length to the new event.
        gle_generated->set_trx_length(gle->transaction_length);
        // Assign the commit order to this transaction.
        const auto ticket_opaque =
            binlog::Bgc_ticket_manager::instance().assign_session_to_ticket();
        gle_generated->set_commit_group_ticket_and_update_transaction_length(
            ticket_opaque.get());

        pevent->reset_pipeline_event();
        pevent->set_LogEvent(gle_generated);

        // Add the gtid information in the executed gtid set for the remote
        // transaction which have gtid specified.
        if (cert_module->add_group_gtid_to_group_gtid_executed(gno)) {
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
        sid = gle->get_sid();
        sidno = gle->get_sidno(true);
        gno = gle->get_gno();

        // Assign the commit order to this transaction.
        const auto ticket_opaque =
            binlog::Bgc_ticket_manager::instance().assign_session_to_ticket();
        gle->set_commit_group_ticket_and_update_transaction_length(
            ticket_opaque.get());

        error = cert_module->add_specified_gtid_to_group_gtid_executed(gle);
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

      if (pevent->get_consistency_level() >=
          GROUP_REPLICATION_CONSISTENCY_AFTER) {
        Transaction_consistency_info *transaction_consistency_info =
            new Transaction_consistency_info(
                tcle->get_thread_id(), local_transaction, sid, sidno, gno,
                pevent->get_consistency_level(), pevent->get_online_members());
        pevent->release_online_members_memory_ownership();
        if (transaction_consistency_manager->after_certification(
                transaction_consistency_info)) {
          /* purecov: begin inspected */
          delete transaction_consistency_info;
          cont->signal(1, true);
          error = 1;
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
  return error;
}

int Certification_handler::extract_certification_info(Pipeline_event *pevent,
                                                      Continuation *cont) {
  DBUG_TRACE;

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
    return 0;
  }
  if (pevent->is_delayed_view_change_waiting_for_consistent_transactions()) {
    pending_view_change_events_waiting_for_consistent_transactions.push_back(
        std::make_unique<View_change_stored_info>(
            pevent, cert_module->generate_view_change_group_gtid(),
            generate_view_change_bgc_ticket()));
    cont->set_transation_discarded(true);
    cont->signal(0, cont->is_transaction_discarded());
    return 0;
  }

  /*
    If the current view event is a standalone event (not inside a
    transaction), it means that it was injected from GCS on a
    membership change.
    On that case we need to queue it on the group applier wrapped
    on a transaction with a group generated GTID.
  */
  int error = log_view_change_event_in_order(pevent, cont);
  if (error) {
    cont->signal(1, false);
  }

  return error;
}

int Certification_handler::inject_transactional_events(
    Pipeline_event *pevent, Gtid gtid, binlog::BgcTicket::ValueType bgc_ticket,
    Continuation *cont) {
  DBUG_TRACE;
  Log_event *event = nullptr;
  Format_description_log_event *fd_event = nullptr;

  if (pevent->get_LogEvent(&event) || (event == nullptr)) {
    /* purecov: begin inspected */
    LogPluginErr(ERROR_LEVEL, ER_GRP_RPL_FETCH_LOG_EVENT_FAILED);
    cont->signal(1, true);
    return 1;
    /* purecov: end */
  }

  if (pevent->get_FormatDescription(&fd_event) && (fd_event == nullptr)) {
    /* purecov: begin inspected */
    LogPluginErr(ERROR_LEVEL, ER_GRP_RPL_FETCH_FORMAT_DESC_LOG_EVENT_FAILED);
    cont->signal(1, true);
    return 1;
    /* purecov: end */
  }

  // GTID event

  /*
    The bgc_ticket must be generated at the same time the GTID is
    generated.
  */
  if (gtid.gno == -1) {
    assert(0 == bgc_ticket);
    gtid = cert_module->generate_view_change_group_gtid();
    bgc_ticket = generate_view_change_bgc_ticket();
  }
  if (gtid.gno <= 0 || 0 == bgc_ticket) {
    cont->signal(1, true);
    return 1;
  }
  Gtid_specification gtid_specification = {ASSIGNED_GTID, gtid};
  /**
   The original_commit_timestamp for this GTID will be different for each
   member that generated this View_change_event.
  */
  uint32_t server_version = do_server_version_int(::server_version);
  auto time_stamp_now = my_micro_time();
  Gtid_log_event *gtid_log_event = new Gtid_log_event(
      event->server_id, true, 0, 0, true, time_stamp_now, time_stamp_now,
      gtid_specification, server_version, server_version);
  gtid_log_event->commit_group_ticket = bgc_ticket;

  Pipeline_event *gtid_pipeline_event =
      new Pipeline_event(gtid_log_event, fd_event);
  next(gtid_pipeline_event, cont);

  int error = cont->wait();
  delete gtid_pipeline_event;
  if (error) {
    return 0; /* purecov: inspected */
  }

  // BEGIN event

  Log_event *begin_log_event = new Query_log_event(
      applier_module_thd, STRING_WITH_LEN("BEGIN"), true, false, true, 0, true);

  Pipeline_event *begin_pipeline_event =
      new Pipeline_event(begin_log_event, fd_event);
  next(begin_pipeline_event, cont);

  error = cont->wait();
  delete begin_pipeline_event;
  if (error) {
    return 0; /* purecov: inspected */
  }

  /*
   Queues the given event.
   As we don't have asynchronous we can use the received Continuation.
   If that is no longer true, another Continuation object must be created here.
  */
  next(pevent, cont);
  error = cont->wait();
  if (error) {
    return 0; /* purecov: inspected */
  }

  // COMMIT event

  Log_event *end_log_event =
      new Query_log_event(applier_module_thd, STRING_WITH_LEN("COMMIT"), true,
                          false, true, 0, true);

  Pipeline_event *end_pipeline_event =
      new Pipeline_event(end_log_event, fd_event);
  next(end_pipeline_event, cont);
  delete end_pipeline_event;

  return 0;
}

int Certification_handler::log_view_change_event_in_order(
    Pipeline_event *view_pevent, Continuation *cont) {
  DBUG_TRACE;

  Gtid gtid = {-1, -1};
  binlog::BgcTicket::ValueType bgc_ticket = 0;

  /*
    If this view was delayed to wait for consistent transactions to finish, we
    need to recover its previously computed GTID information.
  */
  if (view_pevent->is_delayed_view_change_resumed()) {
    auto &stored_view_info =
        pending_view_change_events_waiting_for_consistent_transactions.front();
    gtid = stored_view_info->view_change_gtid;
    bgc_ticket = stored_view_info->bgc_ticket;
    pending_view_change_events_waiting_for_consistent_transactions.pop_front();
  }

  Log_event *event = nullptr;
  const int event_error = view_pevent->get_LogEvent(&event);
  if (event_error || (event == nullptr)) {
    /* purecov: begin inspected */
    LogPluginErr(ERROR_LEVEL, ER_GRP_RPL_FETCH_VIEW_CHANGE_LOG_EVENT_FAILED);
    return 1;
    /* purecov: end */
  }
  View_change_log_event *vchange_event =
      static_cast<View_change_log_event *>(event);
  std::string view_change_event_id(vchange_event->get_view_id());

  // We are just logging old event(s), this packet was created to delay that
  // process
  if (unlikely(view_change_event_id == "-1")) return 0;

  /*
    Certification info needs to be added into the `vchange_event` when this view
    if first handled (no GITD) or when it is being resumed after waiting from
    consistent transactions.
  */
  if ((-1 == gtid.gno) || view_pevent->is_delayed_view_change_resumed()) {
    std::map<std::string, std::string> cert_info;
    cert_module->get_certification_info(&cert_info);
    size_t event_size = 0;
    vchange_event->set_certification_info(&cert_info, &event_size);

    /*
       If certification information is too big this event can't be transmitted
       as it would cause failures on all group members.
       To avoid this, we  now instead encode an error that will make the joiner
       leave the group.
    */
    if (event_size > get_replica_max_allowed_packet()) {
      cert_info.clear();
      cert_info[Certifier::CERTIFICATION_INFO_ERROR_NAME] =
          "Certification information is too large for transmission.";
      vchange_event->set_certification_info(&cert_info, &event_size);
    }
  }

  /**
   Create a transactional block for the View change log event
   GTID
   BEGIN
   VCLE
   COMMIT
  */
  return inject_transactional_events(view_pevent, gtid, bgc_ticket, cont);
}

binlog::BgcTicket::ValueType
Certification_handler::generate_view_change_bgc_ticket() {
  auto &ticket_manager = binlog::Bgc_ticket_manager::instance();
  /*
    Increment ticket so that the all transactions ordered before
    view will have a ticket smaller than the one assigned to the
    view.
  */
  ticket_manager.push_new_ticket();
  ticket_manager.pop_front_ticket();
  /*
    Generate a commit ticket and increment again the ticket so that
    all transactions ordered after the view will have a ticket
    greater that the one assigned to the view.
  */
  auto [ticket, _] =
      ticket_manager.push_new_ticket(binlog::BgcTmOptions::inc_session_count);

  return ticket.get();
}

bool Certification_handler::is_unique() { return true; }

int Certification_handler::get_role() { return CERTIFIER; }

Certifier_interface *Certification_handler::get_certifier() {
  return cert_module;
}

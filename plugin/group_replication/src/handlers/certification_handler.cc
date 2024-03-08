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

#include "plugin/group_replication/include/handlers/certification_handler.h"

#include <mysql/components/services/log_builtins.h>
#include "my_dbug.h"
#include "my_inttypes.h"
#include "plugin/group_replication/include/certification/certified_gtid.h"
#include "plugin/group_replication/include/consistency_manager.h"
#include "plugin/group_replication/include/handlers/pipeline_handlers.h"
#include "plugin/group_replication/include/plugin.h"
#include "plugin/group_replication/include/plugin_handlers/recovery_metadata.h"
#include "scope_guard.h"
#include "string_with_len.h"

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

int Certification_handler::handle_binary_log_event(Pipeline_event *pevent,
                                                   Continuation *cont) {
  DBUG_TRACE;
  mysql::binlog::event::Log_event_type ev_type = pevent->get_event_type();
  switch (ev_type) {
    case mysql::binlog::event::TRANSACTION_CONTEXT_EVENT:
      return handle_transaction_context(pevent, cont);
    case mysql::binlog::event::GTID_LOG_EVENT:
    case mysql::binlog::event::GTID_TAGGED_LOG_EVENT:
      return handle_transaction_id(pevent, cont);
    case mysql::binlog::event::VIEW_CHANGE_EVENT:
      return extract_certification_info(pevent, cont);
    default:
      next(pevent, cont);
      return 0;
  }
  return 0;
}

int Certification_handler::handle_applier_event(Pipeline_event *pevent,
                                                Continuation *cont) {
  Packet *packet = pevent->get_applier_event_packet();
  switch (packet->get_packet_type()) {
    case VIEW_CHANGE_PACKET_TYPE:
      return handle_applier_view_change_packet(pevent, cont);
    default:
      next(pevent, cont);
      return 0;
  }
  return 0;
}

int Certification_handler::handle_applier_view_change_packet(
    Pipeline_event *pevent, Continuation *cont) {
  int error = 0;
  error = handle_view_change_packet_without_vcle(pevent, cont);
  if (!error) next(pevent, cont);
  return 0;
}

int Certification_handler::handle_recovery_metadata(Pipeline_event *pevent,
                                                    Continuation * /* cont */) {
  View_change_packet *view_change_packet =
      static_cast<View_change_packet *>(pevent->get_applier_event_packet());
  int error = 0;
  // Check if I am valid sender, if so, store the metadata.
  if (std::find(view_change_packet->m_valid_sender_list.begin(),
                view_change_packet->m_valid_sender_list.end(),
                local_member_info->get_gcs_member_id()) !=
      view_change_packet->m_valid_sender_list.end()) {
    auto recovery_view_metadata =
        recovery_metadata_module->add_recovery_view_metadata(
            view_change_packet->view_id);
    error = (!recovery_view_metadata.second);

    if (!error) {
      Recovery_metadata_message *metadata_record =
          recovery_view_metadata.first->second;

      // save compressed certification info and executed gtid set
      // in Recovery_metadata_message variables.
      bool status{true};
      try {
        status = cert_module->get_certification_info_recovery_metadata(
            metadata_record);
      } catch (const std::bad_alloc &) {
        LogPluginErr(ERROR_LEVEL, ER_GROUP_REPLICATION_METADATA_MEMORY_ALLOC,
                     "getting certification info and executed gtid set");
        status = true;
      }

      /*
        If certification retrieve fails and the member is ONLINE, send the
        ERROR message.
        If member left the group, VIEW_CHANGE will handle the send of recovery
        metadata.
      */
      if (status) {
        if (local_member_info->get_recovery_status() !=
            Group_member_info::MEMBER_ONLINE) {
          return 0;
        }
        error = true;
      }

      if (!error) {
        // set online members
        metadata_record->set_valid_metadata_senders(
            view_change_packet->m_valid_sender_list);

        // set joining members
        metadata_record->set_joining_members(
            view_change_packet->m_members_joining_in_view);

        /*
          Case 1: Member failed to send the metadata
          If member fails to send the metadata for reasons like big message,
          compression failure, ERROR message will be sent forcing the joiner
          to leave the group.
          Case 2: GCS fails to send the metadata
          If GCS failed to send the metadata leave group will be initiated by
          the GCS.
        */
        if (recovery_metadata_module->send_recovery_metadata(metadata_record)) {
          LogPluginErr(ERROR_LEVEL, ER_GRP_RPL_ERROR_ON_MESSAGE_SENDING,
                       "recovery metadata packet send failed.");
          return 1;
        }
      }
    }
  }
  if (error) {
    error = recovery_metadata_module->send_error_message(
        view_change_packet->view_id);
  }
  return error;
}

int Certification_handler::handle_view_change_packet_without_vcle(
    Pipeline_event *pevent, Continuation *cont) {
  int error = 0;
  /*
   Send the metadata immediately. Otherwise GTID_EXECUTED will change if view
   change is delayed. During the view change delay other transactions get
   executed resulting in update of GTID_EXECUTED. Example: Unprocessed
   consistent transactions of which confirmation is received later can change
   the GTID_EXECUTED. Recovery metadata is a state and delay in processing the
   VIEW_ID will change the state.
   Error is not important here because recovery metadata send is not a
   transaction.
   Since recovery metadata send is not a transaction, failure to send recovery
   metadata is simply to be ignored.
   Recovery metadata send tries to send byte of failure in case of any issue
   like big certification size, compression failure etc.
   If byte of message is not passing from GCS then expect network issues and
   should be handled by the GCS.
  */
  error = handle_recovery_metadata(pevent, cont);
  if (error) {
    // Set ERROR is true but transaction is not disarded
    cont->signal(1, false);
  }
  /*
   Even if view-change is delayed, any transaction post this event should see
   the incremented value of BGC Ticket.
   So increment the BGC ticket immediately.
  */
  increment_bgc_ticket();
  cert_module->gtid_intervals_computation();
  return error;
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

Transaction_termination_ctx generate_transaction_termination_ctx(
    bool is_positively_certified, const Gtid &gtid,
    const Transaction_context_log_event &tcle) {
  // is_positively_certified equal to false means abort, so we need
  // to pass a negative value to transaction context.
  Transaction_termination_ctx result;
  memset(&result, 0, sizeof(result));
  result.m_thread_id = tcle.get_thread_id();
  result.m_rollback_transaction = true;
  result.m_generated_gtid = false;
  result.m_sidno = -1;
  result.m_gno = -1;
  if (is_positively_certified) {
    result.m_rollback_transaction = false;
    result.m_sidno = 0;
    result.m_gno = 0;
    result.m_generated_gtid = tcle.is_gtid_specified() == false;
    if (tcle.is_gtid_specified() == false) {
      result.m_sidno = gtid.sidno;
      result.m_gno = gtid.gno;
    }
  }
  return result;
}

// Create new GTID event.
Gtid_log_event *generate_specified_gtid_event(const Gtid_log_event *gle,
                                              const Gtid &gtid) {
  Gtid_specification gtid_specification = {ASSIGNED_GTID, gtid,
                                           gle->generate_tag_specification()};
  Gtid_log_event *gle_generated = new Gtid_log_event(
      gle->server_id, gle->is_using_trans_cache(), gle->last_committed,
      gle->sequence_number, gle->may_have_sbr_stmts,
      gle->original_commit_timestamp, gle->immediate_commit_timestamp,
      gtid_specification, gle->original_server_version,
      gle->immediate_server_version);
  // Copy the transaction length to the new event.
  gle_generated->set_trx_length(gle->transaction_length);
  return gle_generated;
}

int get_error_code_grp_rpl_add_gtid_info_failed(
    const gr::Certified_gtid &gtid) {
  const auto &is_specified = gtid.is_specified_gtid();
  if (gtid.is_local()) {
    if (is_specified) {
      return ER_GRP_RPL_ADD_GTID_INFO_WITH_LOCAL_GTID_FAILED;
    }
    return ER_GRP_RPL_ADD_GTID_INFO_WITHOUT_LOCAL_GTID_FAILED;
  }
  if (is_specified) {
    return ER_GRP_RPL_ADD_GTID_INFO_WITH_REMOTE_GTID_FAILED;
  }
  return ER_GRP_RPL_ADD_GTID_INFO_WITHOUT_REMOTE_GTID_FAILED;
}

int Certification_handler::handle_transaction_id(Pipeline_event *pevent,
                                                 Continuation *cont) {
  DBUG_TRACE;
  int error = 0;
  bool local_transaction = true;
  Transaction_context_log_event *tcle = nullptr;
  Log_event *event = nullptr;
  Gtid_log_event *gle = nullptr;
  Members_list *online_members = pevent->get_online_members();
  Gtid server_gtid, group_gtid;
  using Certification_result = gr::Certification_result;
  Certification_result certification_result = Certification_result::negative;

  Scope_guard transaction_context_reset_guard(
      [this]() { reset_transaction_context(); });
  /*
    Get transaction context.
  */
  error = get_transaction_context(pevent, &tcle);
  if (error) {
    cont->signal(1, true);
    return error;
  }

  /*
    Get transaction global identifier event.
  */
  error = pevent->get_LogEvent(&event);
  if (error || (event == nullptr)) {
    LogPluginErr(ERROR_LEVEL, ER_GRP_RPL_FETCH_GTID_LOG_EVENT_FAILED);
    cont->signal(1, true);
    return 1;
  }
  gle = static_cast<Gtid_log_event *>(event);

  gr::Gtid_tsid tsid;
  bool is_tsid_specified = false;
  if (tcle->is_gtid_specified()) {
    tsid = gr::Gtid_tsid(gle->get_tsid());
    is_tsid_specified = true;
  } else {
    std::ignore = tsid.from_cstring(get_group_name_var());
    if (gle->is_tagged()) {  // AUTOMATIC:tag
      tsid.set_tag(gle->get_tsid().get_tag());
      is_tsid_specified = true;
    }
  }

  local_transaction =
      !strncmp(tcle->get_server_uuid(), local_member_info->get_uuid().c_str(),
               UUID_LENGTH);

  gr::Certified_gtid certified_gtid(tcle->is_gtid_specified(),
                                    local_transaction);

  /*
    Group contains members that do not support transactions with group
    coordination, thence the transaction must rollback.
  */
  DBUG_EXECUTE_IF(
      "group_replication_force_lower_version_on_group_replication_consistency",
      { online_members = nullptr; };);

  bool do_certification =
      pevent->get_consistency_level() < GROUP_REPLICATION_CONSISTENCY_AFTER ||
      nullptr != online_members;

  if (do_certification) {
    /*
      Certify transaction.
    */
    certified_gtid =
        cert_module->certify(tcle->get_snapshot_version(),
                             tcle->get_write_set(), tcle->is_gtid_specified(),
                             tcle->get_server_uuid(), gle, local_transaction);

    server_gtid = certified_gtid.get_server_gtid();
    group_gtid = certified_gtid.get_group_gtid();
    certification_result = certified_gtid.get_cert_result();
  }

  if (local_transaction) {
    /*
      Local transaction.
      After certification we need to wake up the waiting thread on the
      plugin to proceed with the transaction processing.
    */
    auto transaction_termination_ctx = generate_transaction_termination_ctx(
        certification_result == gr::Certification_result::positive, server_gtid,
        *tcle);

    if (set_transaction_ctx(transaction_termination_ctx)) {
      /* purecov: begin inspected */
      LogPluginErr(ERROR_LEVEL, ER_GRP_RPL_UPDATE_SERV_CERTIFICATE_FAILED,
                   tcle->get_thread_id());
      cont->signal(1, true);
      error = 1;
      return error;
      /* purecov: end */
    }

    if (certification_result == gr::Certification_result::positive) {
      error = cert_module->add_gtid_to_group_gtid_executed(group_gtid);
      DBUG_EXECUTE_IF("unable_to_add_specified_gtid_for_local_transaction",
                      error = 1;);
      if (error) {
        LogPluginErr(ERROR_LEVEL, get_error_code_grp_rpl_add_gtid_info_failed(
                                      certified_gtid));
        transactions_latch->releaseTicket(tcle->get_thread_id());
        cont->signal(1, true);
        return error;
      }

      if (pevent->get_consistency_level() >=
          GROUP_REPLICATION_CONSISTENCY_AFTER) {
        auto transaction_consistency_info =
            std::make_unique<Transaction_consistency_info>(
                tcle->get_thread_id(), local_transaction, tsid,
                is_tsid_specified, server_gtid.sidno, server_gtid.gno,
                pevent->get_consistency_level(), pevent->get_online_members());
        pevent->release_online_members_memory_ownership();
        auto error = transaction_consistency_manager->after_certification(
            std::move(transaction_consistency_info));
        if (error) {
          /* purecov: begin inspected */
          cont->signal(1, true);
          error = 1;
          return error;
          /* purecov: end */
        }
      }
    }

    /*
      We only release the local transaction here when its consistency
      does not require group coordination.
    */
    if ((certification_result != gr::Certification_result::positive ||
         pevent->get_consistency_level() <
             GROUP_REPLICATION_CONSISTENCY_AFTER) &&
        transactions_latch->releaseTicket(tcle->get_thread_id())) {
      /* purecov: begin inspected */
      LogPluginErr(ERROR_LEVEL, ER_GRP_RPL_NOTIFY_CERTIFICATION_OUTCOME_FAILED);
      cont->signal(1, true);
      error = 1;
      return error;
      /* purecov: end */
    }

    // The pipeline ended for this transaction
    cont->signal(0, true);
  } else {
    /*
      Remote transaction.
    */
    if (certification_result == gr::Certification_result::positive) {
      if (!tcle->is_gtid_specified()) {
        auto gle_generated = generate_specified_gtid_event(gle, server_gtid);
        // Assign the commit order to this transaction.
        const auto ticket_opaque =
            binlog::Bgc_ticket_manager::instance().assign_session_to_ticket();
        gle_generated->set_commit_group_ticket_and_update_transaction_length(
            ticket_opaque.get());
        pevent->reset_pipeline_event();
        pevent->set_LogEvent(gle_generated);
      }

      else {
        // Assign the commit order to this transaction.
        const auto ticket_opaque =
            binlog::Bgc_ticket_manager::instance().assign_session_to_ticket();
        gle->set_commit_group_ticket_and_update_transaction_length(
            ticket_opaque.get());
      }

      error = cert_module->add_gtid_to_group_gtid_executed(group_gtid);
      DBUG_EXECUTE_IF("unable_to_add_specified_gtid_for_remote_transaction",
                      error = 1;);
      // Add the gtid information in the executed gtid set for the remote
      // transaction
      if (error) {
        LogPluginErr(ERROR_LEVEL, get_error_code_grp_rpl_add_gtid_info_failed(
                                      certified_gtid));
        cont->signal(1, true);
        error = 1;
        return error;
      }

      if (pevent->get_consistency_level() >=
          GROUP_REPLICATION_CONSISTENCY_AFTER) {
        auto transaction_consistency_info =
            std::make_unique<Transaction_consistency_info>(
                tcle->get_thread_id(), local_transaction, tsid,
                is_tsid_specified, server_gtid.sidno, server_gtid.gno,
                pevent->get_consistency_level(), pevent->get_online_members());
        pevent->release_online_members_memory_ownership();
        auto error = transaction_consistency_manager->after_certification(
            std::move(transaction_consistency_info));
        if (error) {
          /* purecov: begin inspected */
          cont->signal(1, true);
          return error;
          /* purecov: end */
        }
      }

      // Pass transaction to next action.
      next(pevent, cont);
    } else if (certification_result == gr::Certification_result::error) {
      error = 1;
      cont->signal(1, true);
      return error;
    } else {
      // The transaction was negatively certified so discard it.
      cont->signal(0, true);
    }
  }
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
    Gtid generated_gtid;
    std::tie(generated_gtid, std::ignore) =
        cert_module->generate_view_change_group_gtid();
    pending_view_change_events_waiting_for_consistent_transactions.push_back(
        std::make_unique<View_change_stored_info>(
            pevent, generated_gtid, generate_view_change_bgc_ticket()));
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

  mysql::utils::Return_status gno_generation_code =
      gtid.gno <= 0 ? mysql::utils::Return_status::error
                    : mysql::utils::Return_status::ok;
  /*
    The bgc_ticket must be generated at the same time the GTID is
    generated.
  */
  if (gtid.gno == -1) {
    assert(0 == bgc_ticket);
    std::tie(gtid, gno_generation_code) =
        cert_module->generate_view_change_group_gtid();
    bgc_ticket = generate_view_change_bgc_ticket();
  }
  if (gno_generation_code != mysql::utils::Return_status::ok ||
      0 == bgc_ticket) {
    cont->signal(1, true);
    return 1;
  }
  mysql::gtid::Tag_plain empty_tag;
  empty_tag.clear();
  Gtid_specification gtid_specification = {ASSIGNED_GTID, gtid, empty_tag};
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

binlog::BgcTicket::ValueType Certification_handler::increment_bgc_ticket() {
  auto &ticket_manager = binlog::Bgc_ticket_manager::instance();
  /*
    Increment ticket so that the all transactions ordered before
    view will have a ticket smaller than the one assigned to the
    view.
  */
  ticket_manager.push_new_ticket();
  ticket_manager.pop_front_ticket();

  return 0;
}

bool Certification_handler::is_unique() { return true; }

int Certification_handler::get_role() { return CERTIFIER; }

Certifier_interface *Certification_handler::get_certifier() {
  return cert_module;
}

/* Copyright (c) 2014, 2015, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   51 Franklin Street, Suite 500, Boston, MA 02110-1335 USA */

#include "handlers/certification_handler.h"
#include "handlers/pipeline_handlers.h"
#include "plugin.h"
#include "plugin_log.h"


Certification_handler::Certification_handler()
  :cert_module(NULL), gtid_specified(false), seq_number(0)
{}

int
Certification_handler::initialize()
{
  DBUG_ENTER("Certification_handler::initialize");
  DBUG_ASSERT(cert_module == NULL);
  cert_module= new Certifier();
  DBUG_RETURN(0);
}

int
Certification_handler::terminate()
{
  DBUG_ENTER("Certification_handler::terminate");
  int error= 0;

  if(cert_module == NULL)
    DBUG_RETURN(error);

  error= cert_module->terminate();
  delete cert_module;
  cert_module= NULL;
  DBUG_RETURN(error);
}

int
Certification_handler::handle_action(Pipeline_action* action)
{
  DBUG_ENTER("Certification_handler::handle_action");

  int error= 0;

  Plugin_handler_action action_type=
    (Plugin_handler_action)action->get_action_type();

  if (action_type == HANDLER_CERT_CONF_ACTION)
  {
    Handler_certifier_configuration_action* conf_action=
      (Handler_certifier_configuration_action*) action;

    error= cert_module->initialize(conf_action->get_last_delivered_gno());

    group_sidno= conf_action->get_group_sidno();
  }
  else if (action_type == HANDLER_GCS_INTERFACE_ACTION)
  {
    Handler_gcs_interfaces_action *gcs_intf_action=
        (Handler_gcs_interfaces_action*) action;

    cert_module->set_local_member_info(gcs_intf_action->get_local_member_info());
    cert_module->set_gcs_interfaces(gcs_intf_action->get_comm_interface(),
                                    gcs_intf_action->get_control_interface());
  }
  else if (action_type == HANDLER_CERT_INFO_ACTION)
  {
    Handler_certifier_information_action *cert_inf_action=
      (Handler_certifier_information_action*) action;

    cert_module->set_certification_info(cert_inf_action->get_certification_info(),
                                        cert_inf_action->get_sequence_number());
  }
  else if (action_type == HANDLER_VIEW_CHANGE_ACTION)
  {
    View_change_pipeline_action *vc_action=
            (View_change_pipeline_action*) action;

    if (!vc_action->is_leaving())
    {
      cert_module->handle_view_change();
    }
  }

  if(error)
    DBUG_RETURN(error);

  DBUG_RETURN(next(action));
}

int
Certification_handler::handle_event(Pipeline_event *pevent, Continuation *cont)
{
  DBUG_ENTER("Certification_handler::handle_event");

  Log_event_type ev_type= pevent->get_event_type();
  switch (ev_type)
  {
    case binary_log::TRANSACTION_CONTEXT_EVENT:
      DBUG_RETURN(certify(pevent, cont));
    case binary_log::GTID_LOG_EVENT:
      DBUG_RETURN(inject_gtid(pevent, cont));
    case binary_log::VIEW_CHANGE_EVENT:
      DBUG_RETURN(extract_certification_info(pevent, cont));
    default:
      next(pevent, cont);
      DBUG_RETURN(0);
  }
}

int
Certification_handler::certify(Pipeline_event *pevent, Continuation *cont)
{
  DBUG_ENTER("Certification_handler::certify");
  reset_gtid_settings();
  Log_event *event= NULL;
  pevent->get_LogEvent(&event);

  Transaction_context_log_event *tcle= (Transaction_context_log_event*) event;
  rpl_gno seq_number= cert_module->certify(tcle->get_snapshot_version(),
                                           tcle->get_write_set(),
                                           !tcle->is_gtid_specified());

  if (!strncmp(tcle->get_server_uuid(), server_uuid, UUID_LENGTH))
  {
    /*
      Local transaction.
      After certification we need to wake up the waiting thread on the
      plugin to proceed with the transaction processing.
      Sequence number <= 0 means abort, so we need to pass a negative
      value to transaction context.
    */
    Transaction_termination_ctx transaction_termination_ctx;
    memset(&transaction_termination_ctx, 0, sizeof(transaction_termination_ctx));
    transaction_termination_ctx.m_thread_id= tcle->get_thread_id();
    if (seq_number > 0)
    {
      transaction_termination_ctx.m_rollback_transaction= FALSE;
      if (tcle->is_gtid_specified())
        transaction_termination_ctx.m_generated_gtid= FALSE;
      else
      {
        transaction_termination_ctx.m_generated_gtid= TRUE;
        transaction_termination_ctx.m_sidno= group_sidno;
        transaction_termination_ctx.m_gno= seq_number;
      }
    }
    else
    {
      transaction_termination_ctx.m_rollback_transaction= TRUE;
      transaction_termination_ctx.m_generated_gtid= FALSE;
      transaction_termination_ctx.m_sidno= -1;
      transaction_termination_ctx.m_gno= -1;
    }

    if (set_transaction_ctx(transaction_termination_ctx))
    {
      log_message(MY_ERROR_LEVEL,
                  "Unable to update certification result on server side, thread_id: %lu",
                  tcle->get_thread_id());
      cont->signal(1,true);
      DBUG_RETURN(1);
    }

    if (certification_latch->releaseTicket(tcle->get_thread_id()))
    {
      log_message(MY_ERROR_LEVEL, "Failed to notify certification outcome");
      cont->signal(1,true);
      DBUG_RETURN(1);
    }

    //The pipeline ended for this transaction
    cont->signal(0,true);
  }
  else
  {
    /*
      Remote transaction.
     */
    if (seq_number > 0)
    {
      if (tcle->is_gtid_specified())
        set_gtid_specified();
      else
        set_gtid_generated_id(seq_number);

      // Pass transaction to next action.
      next(pevent, cont);
    }
    else
    {
      // The transaction was not certified so discard it.
      cont->signal(0,true);
    }
  }
  DBUG_RETURN(0);
}

int
Certification_handler::inject_gtid(Pipeline_event *pevent, Continuation *cont)
{
  DBUG_ENTER("Certification_handler::inject_gtid");
  Log_event *event= NULL;

  if (!is_gtid_specified())
  {
    pevent->get_LogEvent(&event);
    Gtid_log_event *gle_old= (Gtid_log_event*)event;

    // Create new GTID event.
    Gtid gtid= { group_sidno, get_gtid_generated_id() };
    Gtid_specification gtid_specification= { GTID_GROUP, gtid };
    Gtid_log_event *gle= new Gtid_log_event(gle_old->server_id,
                                            gle_old->is_using_trans_cache(),
                                            gle_old->last_committed,
                                            gle_old->sequence_number,
                                            gtid_specification);

    pevent->reset_pipeline_event();
    pevent->set_LogEvent(gle);
  }

  next(pevent, cont);

  DBUG_RETURN(0);
}

int
Certification_handler::extract_certification_info(Pipeline_event *pevent,
                                                  Continuation *cont)
{
  DBUG_ENTER("Certification_handler::extract_certification_info");
  Log_event *event= NULL;
  pevent->get_LogEvent(&event);
  View_change_log_event *vchange_event= (View_change_log_event*)event;

  rpl_gno sequence_number= 0;
  std::map<std::string, std::string> cert_info;
  cert_module->get_certification_info(&cert_info, &sequence_number);

  vchange_event->set_certification_info(&cert_info);
  vchange_event->set_seq_number(sequence_number);

  next(pevent, cont);
  DBUG_RETURN(0);
}

bool Certification_handler::is_unique()
{
  return true;
}

int Certification_handler::get_role()
{
  return CERTIFIER;
}

Certifier_interface*
Certification_handler::get_certifier()
{
  return cert_module;
}

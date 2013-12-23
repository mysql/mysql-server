/* Copyright (c) 2013, Oracle and/or its affiliates. All rights reserved.

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

#include "certification_handler.h"
#include "../gcs_certifier.h"
#include "../gcs_commit_validation.h"
#include "../gcs_plugin_utils.h"
#include "../observer_trans.h"
#include <gcs_replication.h>

Certifier* cert_map;
Certification_handler::Certification_handler(): seq_number(0)
{}

int
Certification_handler::initialize()
{
  cert_map= new Certifier();
  if(cert_map)
    return 0;
  return 1;
}

int
Certification_handler::terminate()
{
  return 0;
}

int
Certification_handler::handle(PipelineEvent *pevent, Continuation* cont)
{
  DBUG_ENTER("Certification_handler::handle");
  Packet *packet= NULL;
  pevent->get_Packet(&packet);
  Log_event_type ev_type= (Log_event_type) packet->payload[EVENT_TYPE_OFFSET];

  switch (ev_type)
  {
    case TRANSACTION_CONTEXT_EVENT:
      DBUG_RETURN(certify(pevent, cont));
    case GTID_LOG_EVENT:
      DBUG_RETURN(inject_gtid(pevent, cont));
    default:
      next(pevent, cont);
      DBUG_RETURN(0);
  }
}

int
Certification_handler::certify(PipelineEvent *pevent, Continuation *cont)
{
  DBUG_ENTER("Certification_handler::certify");
  Log_event *event= NULL;
  pevent->get_LogEvent(&event);

  Transaction_context_log_event *tcle= (Transaction_context_log_event*) event;
  rpl_gno seq_number= cert_map->certify(tcle);

  // FIXME: This needs to be improved before 0.2
  if (!strncmp(tcle->get_server_uuid(), server_uuid, UUID_LENGTH))
  {
    /*
      Local transaction.
      After a certification we need to wake up the waiting thread on the
      plugin to proceed with the transaction processing. Here we use the
      global array of conditions. We extract the condition variable
      corresponding to the thread_id and store it temporarily in a variable.
      This will be used later in the code to signal the thread after the
      sequence number is updated.
    */
    mysql_cond_t *cond_i=
        get_transaction_wait_cond(tcle->get_thread_id()).first;
    mysql_mutex_t *mutex_i=
        get_transaction_wait_cond(tcle->get_thread_id()).second;

    mysql_mutex_lock(mutex_i);
    if (add_transaction_certification_result(tcle->get_thread_id(),
                                             seq_number,
                                             gcs_cluster_sidno))
    {
      log_message(MY_ERROR_LEVEL,
                  "Unable to update certification result on server side, thread_id: %lu",
                  tcle->get_thread_id());
      cont->signal(1,true);
      DBUG_RETURN(1);
    }

    if (cond_i == NULL || mutex_i == NULL)
    {
      log_message(MY_ERROR_LEVEL,
                  "Got NULL when retrieving the structures for signaling the waiting thread: %lu",
                  tcle->get_thread_id());
      cont->signal(1,true);
      DBUG_RETURN(1);
    }
    mysql_cond_signal(cond_i);
    mysql_mutex_unlock(mutex_i);

    //The pipeline ended for this transaction
    cont->signal(0,true);
  }
  else
  {
    if (!seq_number)
      //The transaction was not certified so discard it.
      cont->signal(0,true);
    else
    {
      set_seq_number(seq_number);
      next(pevent, cont);
    }
  }
  DBUG_RETURN(0);
}

int
Certification_handler::inject_gtid(PipelineEvent *pevent, Continuation *cont)
{
  DBUG_ENTER("Certification_handler::inject_gtid");
  Log_event *event= NULL;
  pevent->get_LogEvent(&event);
  Gtid_log_event *gle_old= (Gtid_log_event*)event;

  // Create new GTID event.
  Gtid gtid= { gcs_cluster_sidno, get_and_reset_seq_number() };
  Gtid_specification spec= { GTID_GROUP, gtid };
  Gtid_log_event *gle= new Gtid_log_event(gle_old->server_id,
                                          gle_old->is_using_trans_cache(),
                                          spec);

  // Pass it to next handler.
  Format_description_log_event *fde= NULL;
  pevent->get_FormatDescription(&fde);
  delete pevent;
  pevent= new PipelineEvent(gle, fde);
  next(pevent, cont);

  DBUG_RETURN(0);
}

bool Certification_handler::is_unique()
{
  return true;
}

Handler_role Certification_handler::get_role()
{
  return CERTIFIER;
}

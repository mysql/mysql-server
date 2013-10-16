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

Certification_handler::Certification_handler()
{}

int
Certification_handler::initialize()
{
  return 0;
}

int
Certification_handler::terminate()
{
  return 0;
}

int
Certification_handler::handle(PipelineEvent *pevent,Continuation* cont)
{
  DBUG_ENTER("Certification_handler::handle");
  //only the first event is needed
  if (pevent->get_event_context() != TRANSACTION_BEGIN)
  {
    next(pevent, cont);
    DBUG_RETURN(0);
  }

  Log_event* event= NULL;
  pevent->get_LogEvent(&event);

  //certification logic
  ulong seq_number= 1;

  //this method should be based on the log event
  if (is_local())
  {
    /*
      After a certification we need to wake up the waiting thread on the
      plugin to proceed with the transaction processing. Here we use the
      global array of conditions. We extract the condition variable
      corresponding to the thread_id and store it temporarily in a variable.
      This will be used later in the code to signal the thread after the
      sequence number is updated.
    */

    //mysql_cond_t cond_i=
    //      find_condition_in_map(map, trxCtd->thread_id);
    //insert_Trx_CTID(&seq_num_map, trxCtd->thread_id, seq_number);
    //mysql_cond_signal(cond_i);

    //The pipeline ended for this transaction
    cont->signal(0,true);
  }
  else
  {
    if (!seq_number)
      //The transaction was not certified so discard it.
      cont->signal(0,true);
    else
      // create CTID event and add it to the stream
      // next(new_CTID_event, cont);
      next(pevent, cont);
    }
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

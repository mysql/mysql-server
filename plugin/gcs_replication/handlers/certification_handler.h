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

#ifndef CERTIFICATION_HANDLER_INCLUDE
#define CERTIFICATION_HANDLER_INCLUDE

#include "../gcs_plugin_utils.h"
#include <applier_interfaces.h>

class Certification_handler : public EventHandler
{
public:
  Certification_handler();
  int handle(PipelineEvent *ev,Continuation* cont);
  int initialize();
  int terminate();
  bool is_unique();
  Handler_role get_role();

  /**
    Sets the value of the transaction sequence number.

    @param[in]  seq_number  transaction sequence number.
  */
  void set_seq_number(rpl_gno seq_number)
  {
    DBUG_ASSERT(seq_number > 0);
    this->seq_number= seq_number;
  }

  /**
    Gets the value of the transaction sequence number.

    @return  transaction sequence number.
  */
  rpl_gno get_and_reset_seq_number()
  {
    DBUG_ASSERT(seq_number > 0);
    rpl_gno res= seq_number;
    seq_number= 0;
    return res;
  }

private:
  rpl_gno seq_number;
  /*
    This method is used to call the certification method of the plugin
    and based on the output it updates the condition variable map and
    signals the thread waiting in the before_commit method.
  */
  int certify(PipelineEvent *ev, Continuation *cont);
  /*
    This method creates a gtid_event for the remote transactions and
    adds it in the pipeline.
  */
  int inject_gtid(PipelineEvent *ev, Continuation *cont);
};

#endif /* CERTIFICATION_HANDLER_INCLUDE */

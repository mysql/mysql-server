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

#ifndef CERTIFICATION_HANDLER_INCLUDE
#define CERTIFICATION_HANDLER_INCLUDE

#include "certifier.h"
#include <mysql/group_replication_priv.h>

class Certification_handler : public Event_handler
{
public:
  Certification_handler();
  int handle_event(Pipeline_event *ev,Continuation *cont);
  int handle_action(Pipeline_action *action);
  int initialize();
  int terminate();
  bool is_unique();
  int get_role();

  Certifier_interface *get_certifier();

  void set_certification_info(std::map<std::string, std::string>* cert_info,
                             rpl_gno seq_number);

private:
  Certifier* cert_module;

  /**
    Flag to indicate that transaction has GTID_NEXT specified or not.
  */
  bool gtid_specified;

  rpl_gno seq_number;

  rpl_sidno cluster_sidno;

  /**
   Inform handler that transaction has GTID_NEXT specified.
   */
  void set_gtid_specified()
  {
    DBUG_ASSERT(seq_number == 0);

    gtid_specified= true;
  }

  /**
    Return True if transaction has GTID_NEXT specified, False otherwise.
   */
  bool is_gtid_specified()
  {
    return gtid_specified;
  }

  /**
    Sets the value of the transaction sequence number.

    @param[in]  seq_number  transaction sequence number.
  */
  void set_gtid_generated_id(rpl_gno seq_number)
  {
    DBUG_ASSERT(seq_number > 0);
    DBUG_ASSERT(gtid_specified == false);

    this->seq_number= seq_number;
  }

  /**
    Gets the value of the transaction sequence number.

    @return  transaction sequence number.
  */
  rpl_gno get_gtid_generated_id()
  {
    DBUG_ASSERT(seq_number > 0);
    DBUG_ASSERT(gtid_specified == false);

    rpl_gno res= seq_number;
    seq_number= 0;
    return res;
  }

  /**
    Reset GTID settings.
   */
  void reset_gtid_settings()
  {
    gtid_specified= false;
    seq_number= 0;
  }

  /*
    This method is used to call the certification method of the plugin
    and based on the output it updates the condition variable map and
    signals the thread waiting in the before_commit method.
  */
  int certify(Pipeline_event *ev, Continuation *cont);
  /*
    This method creates a gtid_event for the remote transactions and
    adds it in the pipeline.
  */
  int inject_gtid(Pipeline_event *ev, Continuation *cont);

  /*
    This method extracts the certification db and the sequence number from
    the certifier injecting them in a View change event to be sent to a possible
    joiner.
  */
  int extract_certification_info(Pipeline_event *pevent, Continuation *cont);
};

#endif /* CERTIFICATION_HANDLER_INCLUDE */

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

#ifndef CERTIFICATION_HANDLER_INCLUDE
#define CERTIFICATION_HANDLER_INCLUDE

#include <mysql/group_replication_priv.h>
#include <string>

#include "plugin/group_replication/include/certifier.h"

class Certification_handler : public Event_handler {
 public:
  Certification_handler();
  virtual ~Certification_handler();
  int handle_event(Pipeline_event *ev, Continuation *cont);
  int handle_action(Pipeline_action *action);
  int initialize();
  int terminate();
  bool is_unique();
  int get_role();

  Certifier_interface *get_certifier();

  int set_certification_info(std::map<std::string, std::string> *cert_info);

 private:
  Certifier *cert_module;

  THD *applier_module_thd;

  rpl_sidno group_sidno;

  Data_packet *transaction_context_packet;
  Pipeline_event *transaction_context_pevent;

  /**
    Set transaction context for next event handler.

    @param[in]   pevent  Pipeline event that wraps
                         Transaction_context_log_event.

    @return  Operation status
      @retval 0      OK
      @retval !=0    Error
   */
  int set_transaction_context(Pipeline_event *pevent);

  /**
    Get transaction context set on previous event handler.

    @param[in]   pevent  Pipeline event that wraps
                         Gtid_log_event.
    @param[out]  tcle    Transaction_context_log_event.

    @return  Operation status
      @retval 0      OK
      @retval !=0    Error
   */
  int get_transaction_context(Pipeline_event *pevent,
                              Transaction_context_log_event **tcle);

  /**
    Reset transaction context.
   */
  void reset_transaction_context();

  /**
    This method handles transaction context events by storing them
    so they can be used on next handler.

    @param[in] pevent   the event to be injected
    @param[in] cont     the object used to wait

    @return the operation status
      @retval 0      OK
      @retval !=0    Error
   */
  int handle_transaction_context(Pipeline_event *pevent, Continuation *cont);

  /**
    This methods handles transaction identifier events, it does two tasks:
      1. Using transaction context previously processed and stored,
         validate that this transaction does not conflict with any other;
      2. If the transaction does not conflict and it is allowed to commit,
         it does inform the server of that decision and does update the
         transaction identifier if needed.

    @param[in] pevent   the event to be injected
    @param[in] cont     the object used to wait

    @return the operation status
      @retval 0      OK
      @retval !=0    Error
  */
  int handle_transaction_id(Pipeline_event *pevent, Continuation *cont);

  /*
    This method extracts the certification db and the sequence number from
    the certifier injecting them in a View change event to be sent to a possible
    joiner.
  */
  int extract_certification_info(Pipeline_event *pevent, Continuation *cont);

  /**
    This methods guarantees that the view change event is logged after local
    transactions are executed.
  */
  int wait_for_local_transaction_execution();

  /**
    Create a transactional block for the received log event
    GTID
    BEGIN
    EVENT
    COMMIT
  */
  int inject_transactional_events(Pipeline_event *pevent, Continuation *cont);
};

#endif /* CERTIFICATION_HANDLER_INCLUDE */

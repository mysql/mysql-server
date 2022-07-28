/* Copyright (c) 2014, 2022, Oracle and/or its affiliates.

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
  ~Certification_handler() override;
  int handle_event(Pipeline_event *ev, Continuation *cont) override;
  int handle_action(Pipeline_action *action) override;
  int initialize() override;
  int terminate() override;
  bool is_unique() override;
  int get_role() override;

  Certifier_interface *get_certifier();

  int set_certification_info(std::map<std::string, std::string> *cert_info);

 private:
  Certifier *cert_module;

  THD *applier_module_thd;

  rpl_sidno group_sidno;

  Data_packet *transaction_context_packet;
  Pipeline_event *transaction_context_pevent;

  /** Are view change on wait for application */
  bool m_view_change_event_on_wait;

  /** View change information information stored for delay */
  struct View_change_stored_info {
    Pipeline_event *view_change_pevent;
    std::string local_gtid_certified;
    Gtid view_change_gtid;
    View_change_stored_info(Pipeline_event *vc_pevent,
                            std::string &local_gtid_string, Gtid gtid)
        : view_change_pevent(vc_pevent),
          local_gtid_certified(local_gtid_string),
          view_change_gtid(gtid) {}
  };

  /** All the VC events pending application due to timeout */
  std::list<View_change_stored_info *> pending_view_change_events;
  /** All the VC events pending application due to consistent transactions */
  std::list<std::unique_ptr<View_change_stored_info>>
      pending_view_change_events_waiting_for_consistent_transactions;

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

    @param local_gtid_certified_string  The set to wait.
                                        If not defined, it extracts the current
    certified set

    @retval 0                        OK
    @retval LOCAL_WAIT_TIMEOUT_ERROR Timeout error on wait
    @retval !=0                      Wait or interface error
  */
  int wait_for_local_transaction_execution(
      std::string &local_gtid_certified_string);

  /**
    Create a transactional block for the received log event
    GTID
    BEGIN
    EVENT
    COMMIT

    @param[in] pevent          the event to be injected
    @param[in, out] gtid       The transaction GTID
                               If {-1, -1}, one will be generated.
    @param[in] cont            the object used to wait


    @return the operation status
      @retval 0      OK
      @retval !=0    Error
  */
  int inject_transactional_events(Pipeline_event *pevent, Gtid *gtid,
                                  Continuation *cont);

  /**
    Try to log a view change event waiting for local certified transactions to
    finish.

    @param[in] view_pevent             the event to be injected
    @param[in, out] local_gtid_string  The local certified transaction set to
    wait If empty, one will be assigned even on timeout
    @param[in, out] gtid               The transaction GTID
                                       If {-1, -1}, one will be generated.
    @param[in] cont                    the object used to wait


    @return the operation status
      @retval 0      OK
      @retval LOCAL_WAIT_TIMEOUT_ERROR Timeout error on wait for local
    transactions
      @retval !=0    Error
  */
  int log_view_change_event_in_order(Pipeline_event *view_pevent,
                                     std::string &local_gtid_string, Gtid *gtid,
                                     Continuation *cont);

  /**
    Store the event for future logging as a timeout occurred.
    This method does 2 things:
      1. If not stored in the past, it stores the Pipeline event to
      be logged in the future
      2. It queues again in the applier a fake View change log event
      to ensure the logging method will be invoked eventually

    @param[in] pevent             The event to be stored
    @param[in] local_gtid_certified_string The local certified transaction set
    to wait
    @param[in] gtid               The transaction GTID
    @param[in] cont               Used to discard or not the transaction


    @return the operation status
      @retval 0      OK
      @retval !=0    Error
  */
  int store_view_event_for_delayed_logging(
      Pipeline_event *pevent, std::string &local_gtid_certified_string,
      Gtid gtid, Continuation *cont);

  /**
    Logs all the delayed View Change log events stored.

    @param[in] cont       the object used to mark error or success

    @return the operation status
      @retval 0      OK
      @retval LOCAL_WAIT_TIMEOUT_ERROR Timeout error on wait for local
    transactions
      @retval !=0    Error
  */
  int log_delayed_view_change_events(Continuation *cont);
};

#endif /* CERTIFICATION_HANDLER_INCLUDE */

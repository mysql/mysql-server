<<<<<<< HEAD
/* Copyright (c) 2014, 2022, Oracle and/or its affiliates.
=======
<<<<<<< HEAD:plugin/group_replication/include/handlers/certification_handler.h
/* Copyright (c) 2014, 2018, Oracle and/or its affiliates. All rights reserved.
=======
/* Copyright (c) 2014, 2023, Oracle and/or its affiliates.
>>>>>>> upstream/cluster-7.6:rapid/plugin/group_replication/include/handlers/certification_handler.h
>>>>>>> pr/231

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

<<<<<<< HEAD:plugin/group_replication/include/handlers/certification_handler.h
#include <mysql/group_replication_priv.h>
#include <string>

#include "plugin/group_replication/include/certifier.h"

class Certification_handler : public Event_handler {
 public:
=======
#include "certifier.h"
#include <mysql/group_replication_priv.h>
#include <string>

class Certification_handler : public Event_handler {
public:
>>>>>>> upstream/cluster-7.6:rapid/plugin/group_replication/include/handlers/certification_handler.h
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

<<<<<<< HEAD:plugin/group_replication/include/handlers/certification_handler.h
 private:
=======
private:
>>>>>>> upstream/cluster-7.6:rapid/plugin/group_replication/include/handlers/certification_handler.h
  Certifier *cert_module;

  THD *applier_module_thd;

  rpl_sidno group_sidno;

  Data_packet *transaction_context_packet;
  Pipeline_event *transaction_context_pevent;

<<<<<<< HEAD
  /** View change information information stored for delay */
  struct View_change_stored_info {
    Pipeline_event *view_change_pevent;
    Gtid view_change_gtid;
    binlog::BgcTicket::ValueType bgc_ticket;
    View_change_stored_info(Pipeline_event *vc_pevent, Gtid gtid,
                            binlog::BgcTicket::ValueType bgc_ticket)
        : view_change_pevent(vc_pevent),
          view_change_gtid(gtid),
          bgc_ticket(bgc_ticket) {}
  };

  /** All the VC events pending application due to consistent transactions */
  std::list<std::unique_ptr<View_change_stored_info>>
      pending_view_change_events_waiting_for_consistent_transactions;
=======
  /** Are view change on wait for application */
  bool m_view_change_event_on_wait;

  /** View change information information stored for delay */
  struct View_change_stored_info {
    Pipeline_event *view_change_pevent;
    std::string local_gtid_certified;
    rpl_gno view_change_event_gno;
    View_change_stored_info(Pipeline_event *vc_pevent, std::string &local_gtid_string, rpl_gno gno)
        : view_change_pevent(vc_pevent),
          local_gtid_certified(local_gtid_string),
          view_change_event_gno(gno)  {}
  };

  /** All the VC events pending application due to timeout */
  std::list<View_change_stored_info*> pending_view_change_events;
>>>>>>> pr/231

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
<<<<<<< HEAD
=======
    This methods guarantees that the view change event is logged after local
    transactions are executed.

    @param local_gtid_certified_string  The set to wait.
                                        If not defined, it extracts the current certified set
    @retval 0                        OK
    @retval LOCAL_WAIT_TIMEOUT_ERROR Timeout error on wait
    @retval !=0                      Wait or interface error
  */
  int wait_for_local_transaction_execution(std::string& local_gtid_certified_string);

  /**
>>>>>>> pr/231
    Create a transactional block for the received log event
    GTID
    BEGIN
    EVENT
    COMMIT

    @param[in] pevent          the event to be injected
<<<<<<< HEAD
    @param[in] gtid            The transaction GTID
                               If {-1, -1}, one will be generated.
    @param[in] bgc_ticket      The commit ticket order for this transaction
                               on the binlog group commit.
                               If 0, one will be generated.
=======
    @param[in, out] event_gno  The transaction GTID gno
                               If -1, one will be generated.
>>>>>>> pr/231
    @param[in] cont            the object used to wait


    @return the operation status
      @retval 0      OK
      @retval !=0    Error
  */
<<<<<<< HEAD
  int inject_transactional_events(Pipeline_event *pevent, Gtid gtid,
                                  binlog::BgcTicket::ValueType bgc_ticket,
                                  Continuation *cont);

  /**
    Try to log a view change event waiting for local certified transactions to
    finish.

    @param[in] view_pevent             the event to be injected
=======
  int inject_transactional_events(Pipeline_event *pevent, rpl_gno *event_gno, Continuation *cont);

  /**
    Try to log a view change event waiting for local certified transactions to finish.

    @param[in] view_pevent             the event to be injected
    @param[in, out] local_gtid_string  The local certified transaction set to wait
                                       If empty, one will be assigned even on timeout
    @param[in, out] event_gno          The transaction GTID gno
                                       If -1, one will be generated.
>>>>>>> pr/231
    @param[in] cont                    the object used to wait


    @return the operation status
      @retval 0      OK
<<<<<<< HEAD
      @retval !=0    Error
  */
  int log_view_change_event_in_order(Pipeline_event *view_pevent,
                                     Continuation *cont);

  /**
    Generate a commit order ticket for the View_change transaction.

    More precisely it will:
     1) increment the current ticket so that the all transactions
        ordered before view will have a ticket smaller than the one
        assigned to the view.
     2) generate the ticket for the view.
     3) increment again the current ticket so that all transactions
        ordered after the view will have a ticket greater that the
        one assigned to the view.

    @return the ticket generated for the view
  */
  binlog::BgcTicket::ValueType generate_view_change_bgc_ticket();
=======
      @retval LOCAL_WAIT_TIMEOUT_ERROR Timeout error on wait for local transactions
      @retval !=0    Error
  */
  int log_view_change_event_in_order(Pipeline_event *view_pevent,
                                     std::string &local_gtid_string,
                                     rpl_gno *event_gno,
                                     Continuation *cont);

  /**
    Store the event for future logging as a timeout occurred.
    This method does 2 things:
      1. If not stored in the past, it stores the Pipeline event to
      be logged in the future
      2. It queues again in the applier a fake View change log event
      to ensure the logging method will be invoked eventually

    @param[in] view_pevent        the event to be stored
    @param[in] local_gtid_string  The local certified transaction set to wait
    @param[in] event_gno          The transaction GTID gno
    @param[in] cont               Used to discard or not the transaction


    @return the operation status
      @retval 0      OK
      @retval !=0    Error
  */
  int store_view_event_for_delayed_logging(Pipeline_event *pevent,
                                           std::string& local_gtid_certified_string,
                                           rpl_gno event_gno,
                                           Continuation *cont);

  /**
    Logs all the delayed View Change log events stored.

    @param[in] cont       the object used to mark error or success

    @return the operation status
      @retval 0      OK
      @retval LOCAL_WAIT_TIMEOUT_ERROR Timeout error on wait for local transactions
      @retval !=0    Error
  */
  int log_delayed_view_change_events(Continuation *cont);

>>>>>>> pr/231
};

#endif /* CERTIFICATION_HANDLER_INCLUDE */

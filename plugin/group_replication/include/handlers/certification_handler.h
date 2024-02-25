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
    Create a transactional block for the received log event
    GTID
    BEGIN
    EVENT
    COMMIT

    @param[in] pevent          the event to be injected
    @param[in] gtid            The transaction GTID
                               If {-1, -1}, one will be generated.
    @param[in] bgc_ticket      The commit ticket order for this transaction
                               on the binlog group commit.
                               If 0, one will be generated.
    @param[in] cont            the object used to wait


    @return the operation status
      @retval 0      OK
      @retval !=0    Error
  */
  int inject_transactional_events(Pipeline_event *pevent, Gtid gtid,
                                  binlog::BgcTicket::ValueType bgc_ticket,
                                  Continuation *cont);

  /**
    Try to log a view change event waiting for local certified transactions to
    finish.

    @param[in] view_pevent             the event to be injected
    @param[in] cont                    the object used to wait


    @return the operation status
      @retval 0      OK
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
};

#endif /* CERTIFICATION_HANDLER_INCLUDE */

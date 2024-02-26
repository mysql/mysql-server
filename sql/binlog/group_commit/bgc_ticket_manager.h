/* Copyright (c) 2022, 2023, Oracle and/or its affiliates.

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

#ifndef BINLOG_BCG_TICKET_MANAGER_H
#define BINLOG_BCG_TICKET_MANAGER_H

#include "sql/binlog/group_commit/atomic_bgc_ticket_guard.h"
#include "sql/containers/integrals_lockfree_queue.h"

namespace binlog {
/**
  Enumeration for passing options to `Bgc_ticket_manager` operations.
 */
enum class BgcTmOptions {
  /** No options */
  empty = 0,
  /**
    While performing some other operation (e.g. push, pop), atomically
    increment the related session counter.
  */
  inc_session_count = 1
};

/**
  @class Bgc_ticket_manager

  Singleton class that manages the grouping of sessions for the Binlog
  Group Commit (BGC), using a ticketing system.

  Entities
  --------
  The envolved abstract entities are:
  - Ticket: represented by a number, it has a processing window within
    which one may decide the amount of sessions one wish to have processed.
  - Session: the unit for what will be processed within a ticket processing
    window. Usually, it represent THD sessions reaching the beginning of
    the binlog group commit but it's just an abstract concept.
  - Front Ticket: the ticket for which the processing window is active.
  - Back Ticket: the ticket that is open to assigning more sessions, it may
    or may not have it's processing window active.
  - On-wait Tickets: tickets that aren't open to assigning more sessions
    and haven't, yet, had their processing window activacted.
  - Session-count Queue: a queue keeping the total sessions assigned to
    each ticket that is closed to assignments (front + on-wait).

  Operations
  ----------
  The overall set of available operations are:
  - Request a session to be assigned to a ticket.
  - Initialize a new back ticket, meaning that after such operation any
    subsequent requests to assign a session to a ticket will assign to the
    newly created ticket.
  - Mark a given session or a set of sessions as processed within the front
    ticket window.
  - Close the processing window for the front ticket and activate the
    processing window for the next ticket in line.

  Members
  -------
  The class variable members are:
  - A pointer to the front ticket.
  - A pointer to the back ticket.
  - A counter for the amount of sessions processed within the front ticket
    window.
  - A counter for the amount of sessions assigned to the back ticket.
  - A queue that holds the amount of sessions assigned to all the tickets
    that are closed for assginment (front + on-wait).

  There is always an active back ticket and an active front ticket. At
  instance initialization, both are set to `1` and, in the possibility of
  the underlying ticket counter variable to wrap around, the first number
  will still be `1`. The maximum value for a ticket is 2^63-1.

  Thread-safe and serialization
  -----------------------------
  All operations are thread-safe. The operations of assigning a session to
  the back ticket (`assign_sesssion_to_ticket`) and of closing the back
  ticket to assignments and creating a new one (`push_new_ticket`) are
  serialized between them. The operations of adding a session to the front
  ticket processed sessions count (`add_processed_sessions_to_front_ticket`) and
  of closing the front ticket processing window (`pop_front_ticket`) are
  serialized between them. Any other type of concurrence between operations is
  serialized only at the atomic variable level.

  Serialization between the above described operations is necessary to keep
  consistent and in-sync the class member values while changing or
  checkings the ticket session counters and changing the ticket pointers.

  Operations are serialized by using the most significant bit of each of
  the ticket pointer atomic variables and mark them as in use (set to `1`)
  or not in use (set to `0`). The first thread to be able to compare the
  ticket pointer atomic variable with a value that has the most significant
  bit unset and exchange it by a value with the most significant bit set
  (using a CAS) gets the ownership over the pointer operations. This
  mechanism allows us to atomically compare two distinct values, the ticket
  value in itself and whether or not it's in use by another thread.

  Usage patterns
  --------------
  The API usage may be divided in three common patterns:
  1. Processing-window-wait: assign a session to a ticket, wait for the
     assigned ticket to get to the front and have an active processing
     window and add the session to the front ticket processed sessions.
  2. Front-ticket-closing: assign a session to a ticket, wait for the
     assigned ticket to get to the front, finish the front ticket
     processing window and notify all waiting threads that a new front
     ticket processing window is activated.
  3. Back-ticket-closing: close the back ticket to assigments and create a
     new back ticket.

  Processing-window-wait
  ----------------------
  In this pattern, the back ticket assigned sessions counter is incremented
  and the back ticket is returned to the API caller. The caller should wait
  for the returned ticket to be the front ticket, in order to be able to
  add the session to the front ticket processed sessions counter. The code
  pattern is something along the lines:

     // Global shared scope
     std::mutex ticket_processing_window_mtx;
     std::condition_variable ticket_processing_window;
     ...
     auto &ticket_manager = binlog::Bgc_ticket_manager::instance();
     auto this_thread_ticket = ticket_manager.assign_session_to_ticket();

     while (this_thread_ticket != ticket_manager.get_front_ticket()) {
       std::unique_lock lock{ticket_processing_window_mtx};
       ticket_processing_window.wait(lock);
     }
     ticket_manager.add_processed_sessions_to_front_ticket(1);
     ...

  Front-ticket-closing
  --------------------
  In this pattern, the back ticket assigned sessions counter is incremented
  and the back ticket is returned to the API caller. The caller should wait
  for the returned ticket to be the front ticket, in order to be able to
  add the session to the front ticket processed sessions counter. Only
  then, it is in a position to close the front ticket processing window,
  start a new front ticket processing window and notify all threads. The
  code pattern is something along the lines:

     // Global shared scope
     std::mutex ticket_processing_window_mtx;
     std::condition_variable ticket_processing_window;
     ...
     auto &ticket_manager = binlog::Bgc_ticket_manager::instance();
     auto this_thread_ticket = ticket_manager.assign_session_to_ticket();

     while (this_thread_ticket != ticket_manager.get_front_ticket()) {
       std::this_thread::yield();
     }
     ticket_manager.add_processed_sessions_to_front_ticket(1);

     while (std::get<1>(ticket_manager.pop_front_ticket()) ==
            this_thread_ticket) {
       {
         std::unique_lock lock{ticket_processing_window_mtx};
         ticket_processing_window.notify_all();
       }
       std::this_thread::yield();
     }
     ...

  Note that there is a loop associated with `pop_front_ticket`. This
  operation only closes the front ticket processing window if the counter
  for the front ticket assigned sessions equals the counter for the front
  ticket processed sessions. Since the function returns an `std::pair` with
  the front ticket values before and after the operation, one may need to
  check if the values are different.

  When `pop_front_ticket` returns different before and after values, it
  means that the front ticket pointer now points to the after value and
  that the assigned sessions count for the before value was poped from the
  session-count queue.

  Back-ticket-closing
  -------------------
  In this pattern, the back ticket assigned sessions count is pushed into
  the session-count queue, set to `0` and the back ticket pointer is set to
  the next value. The code pattern is something along the lines:

     auto [before_ticket, after_ticket] = ticket_manager.push_new_ticket();

  If there is the need to assign a session to the finishing back ticket
  before it's finished and do it atomically with the finish operation, the
  following code pattern is also supported:

     auto [before_ticket, after_ticket] = ticket_manager.push_new_ticket(
         binlog::BgcTmOptions::inc_session_count);

  The above means that a session was assigned to the `before_ticket` (added
  to assigned sessions counter) before such value was pushed to the
  session-count queue.

  @see unittest/gunit/bgc_ticket_manager-t.cc
 */
class Bgc_ticket_manager {
 public:
  using queue_value_type = std::uint64_t;
  using queue_type = container::Integrals_lockfree_queue<queue_value_type>;

  /**
    Maximum allowed number of on-wait tickets and the capacity of the
    underlying session-count queue.
  */
  static constexpr size_t max_concurrent_tickets = 1024;

  /**
    Default destructor.
   */
  virtual ~Bgc_ticket_manager() = default;

  // Remove copy-move semantics
  Bgc_ticket_manager(Bgc_ticket_manager const &) = delete;
  Bgc_ticket_manager(Bgc_ticket_manager &&) = delete;
  Bgc_ticket_manager &operator=(Bgc_ticket_manager const &) = delete;
  Bgc_ticket_manager &operator=(Bgc_ticket_manager &&) = delete;

  /**
    Coalesces all tickets into a single ticket and opens new processing and
    assignment windows.

    @return The ticket manager instance, for chaining purposes.
   */
  Bgc_ticket_manager &coalesce();
  /**
    Assigns a session to the back ticket. It increments the back ticket
    assigned sessions counter.

    @return The ticket the session was assigned to.
   */
  binlog::BgcTicket assign_session_to_ticket();
  /**
    Sets given session count as processed within the front ticket
    processing window. It adds the given sessions parameter to the front
    ticket processed sessions counter.

    @param sessions_to_add The number of sessions to set as processed with
                           the front ticket window.
    @param ticket The session ticket (used for validations).


    @return The number of sessions processed in the front ticket window
            after the operation sucessfully concluded.
   */
  queue_value_type add_processed_sessions_to_front_ticket(
      queue_value_type sessions_to_add, const binlog::BgcTicket &ticket);
  /**
    Retrieves the front ticket, for which the processing window is open.

    @return The front ticket.
   */
  binlog::BgcTicket get_front_ticket() const;
  /**
    Retrieves the back ticket, which is open to session assignments.

    @return The back ticket.
   */
  binlog::BgcTicket get_back_ticket() const;
  /**
    Retrieves the coalesced ticket, if any coalesce.

    @return The coalesced ticket, which may be 0 if no coalesce operation has
            been performed.
   */
  binlog::BgcTicket get_coalesced_ticket() const;
  /**
    Closes the current back ticket to sesssion assignments, pushes the back
    ticket assgined sessions counter to the session-count queue, set it to
    `0` and sets the back ticket pointer to the next value.

    If the back ticket assigned sessions counter is `0` just before pushing
    it to the session-count queue and changing the back ticket pointer,
    none of these two operations will happen and the returned pair will
    have matching values.

    If the `options` parameter has the `inc_session_count` option
    set, the back ticket assigned sessions count is incremented before
    being pusheed to the session-count queue.

    @param options Allowed values are combinations of `empty` and
                   `inc_session_count`

    @return A pair holding the before and after invocation back ticket
            pointer values.
   */
  std::pair<binlog::BgcTicket, binlog::BgcTicket> push_new_ticket(
      BgcTmOptions options = BgcTmOptions::empty);
  /**
    Closes the current front ticket processing window, pops the front
    ticket assigned sessions count from the session-count queue, sets the
    front ticket processed sessions counter to `0` and sets the front
    ticket pointer to the next value in lne.

    If the front ticket processed sessions count doesn't match the front
    ticket assgined sessions count, this function is a no-op and the
    returned pair will have matching values.

    If the `options` parameter has the `inc_session_count` option
    set, the front ticket processed sessions count is incremented before
    being compared with the front ticket assigned sessions count.

    @param options Allowed values are combinations of `empty` and
                   `inc_session_count`

    @return A pair holding the before and after invocation front ticket
            pointer values.
   */
  std::pair<binlog::BgcTicket, binlog::BgcTicket> pop_front_ticket(
      BgcTmOptions options = BgcTmOptions::empty);
  /**
    Returns the textual representation of this object.

    @return a string containing the textual representation of this object.
   */
  std::string to_string() const;
  /**
    Dumps the textual representation of this object into the given output
    stream.

    @param out The stream to dump this object into.
   */
  void to_string(std::ostream &out) const;
  /**
    Dumps the textual representation of an instance of this class into the
    given output stream.

    @param out The output stream to dump the instance to.
    @param to_dump The class instance to dump to the output stream.

    @return The output stream to which the instance was dumped to.
   */
  inline friend std::ostream &operator<<(std::ostream &out,
                                         Bgc_ticket_manager const &to_dump) {
    to_dump.to_string(out);
    return out;
  }

  /**
    Retrieves the single instance of the class.

    @return The single instance of the class.
   */
  static Bgc_ticket_manager &instance();

 private:
  /** The pointer to the ticket that is open to assigning more sessions. */
  AtomicBgcTicket m_back_ticket{binlog::BgcTicket::first_ticket_value};
  /** The pointer to the ticket for which the processing window is active. */
  AtomicBgcTicket m_front_ticket{binlog::BgcTicket::first_ticket_value};
  /**
    The pointer to the coalesced ticket, 0 means that the coalescing has not
    been requested yet.
  */
  AtomicBgcTicket m_coalesced_ticket{0};
  /** The number of sessions assigned to the back ticket. */
  queue_value_type m_back_ticket_sessions_count{0};
  /** The number of sessions processed in the front ticket window. */
  queue_value_type m_front_ticket_processed_sessions_count{0};
  /**
    The queue keeping the total sessions assigned to each ticket that is
    closed to assignments (front + on-wait).
   */
  queue_type m_sessions_per_ticket{max_concurrent_tickets};

  /**
    Default constructor.
   */
  Bgc_ticket_manager() = default;
};
}  // namespace binlog
#endif  // BINLOG_BCG_TICKET_MANAGER_H

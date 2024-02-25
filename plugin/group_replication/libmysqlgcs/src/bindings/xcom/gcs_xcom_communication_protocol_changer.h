/* Copyright (c) 2018, 2023, Oracle and/or its affiliates.

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

#ifndef GCS_XCOM_COMMUNICATION_PROTOCOL_CHANGER_INCLUDED
#define GCS_XCOM_COMMUNICATION_PROTOCOL_CHANGER_INCLUDED

#include <atomic>              // std::atomic
#include <condition_variable>  // std::condition_variable
#include <cstdlib>
#include <future>   // std::future, std::promise
#include <mutex>    // std::mutex
#include <utility>  // std::pair

#include "plugin/group_replication/libmysqlgcs/include/mysql/gcs/gcs_communication_interface.h"  // Gcs_communication_interface
#include "plugin/group_replication/libmysqlgcs/include/mysql/gcs/gcs_member_identifier.h"  // Gcs_member_identifier
#include "plugin/group_replication/libmysqlgcs/include/mysql/gcs/gcs_types.h"  // Gcs_protocol_version
#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/gcs_internal_message.h"  // Cargo_type, Gcs_packet
#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/gcs_message_stages.h"  // Gcs_message_pipeline
#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/gcs_xcom_group_member_information.h"  // Gcs_xcom_node_address, Gcs_xcom_nodes
#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/gcs_xcom_notification.h"  // Gcs_xcom_engine
#include "plugin/group_replication/libmysqlgcs/src/interface/gcs_tagged_lock.h"  // Gcs_tagged_lock

/**
 Implements the communication protocol change logic.

 Design
 =============================================================================
 The algorithm to change the communication protocol is roughly as follows:

     1. Start buffering the node's outgoing messages.
     2. Wait until all the node's outgoing messages have been delivered.
     3. Modify the node's communication protocol version.
     4. Stop buffering the node's outgoing messages and send any messages
        buffered in step (1).

 Implementing the algorithm requires synchronising user threads, which send
 messages, with the GCS thread, which performs communication protocol changes.

 The high level view of the synchronisation protocol between the user and GCS
 threads is the following:

     when send-message(m) from user thread:
       atomically:
         if protocol_changing:
           wait until protocol_changing = false
         nr_msgs_in_transit++
       ...

     when change-protocol(v) from GCS thread:
       atomically:
         protocol_changing := true
       wait until nr_msgs_in_transit = 0
       ...

 We expect that communication protocol changes are rare events, especially
 when compared to sending messages.
 As such, the actual implementation strives to minimise the overhead on the
 code path that sends messages.

 To do this, we use an optimistic synchronisation protocol on the send message
 side, that works as follows:

   Algorithm #0, User thread:
     1.  If no protocol change is ongoing, the user thread will optimistically
         increment the number of messages in transit.
     2.a If a protocol change did not start meanwhile, we are good to go.
     2.b If a protocol change started meanwhile:
       2.b.1. Rollback the increment to the number of messages in transit
       2.b.2. Wait for the protocol change to finish.

 There is an additional action that needs to be performed on step (2.b), but
 we will describe that action when we have the necessary context to understand
 it.

 On the protocol change side, it works as follows:

   Algorithm #1, GCS thread:
     1. Store that a protocol change is ongoing.
     2. When the number of messages in transit is zero:
       2.1. Change the protocol version
       2.2. Wake up any user threads waiting for the protocol change
       2.3. Deem the protocol change finished

 The central part of the Algorithm #1 is step (2).
 The question is: who triggers, and where, step (2)'s condition, i.e. the
 number of in-transit messages is zero?
 Well, the obvious place is that it is the GCS thread itself, when it is
 processing an incoming message.
 If that message comes from us, then we decrease the number of in-transit
 messages, which may set it to zero.

 However, recall that the user threads employ an optimistic synchronisation
 protocol that "acts first, and asks for forgiveness later."
 If the user thread rolls back its increment to the number of in-transit
 messages, it may be the one to set it to zero---see Algorithm #0, step (2.b).
 In this situation, it is the user thread that hits the condition required by
 the GCS thread in Algorithm #1, step (2).
 In order for the GCS thread to finish the protocol change, the user thread
 must somehow signal the GCS thread to trigger its step (2).
 This is the missing action of Algorithm #0, step (2.b).

 So, the final synchronisation protocol of the user thread's side looks like
 this:

   Algorithm #2, User thread:
     1.  If no protocol change is ongoing, the user thread will optimistically
         increment the number of messages in transit.
     2.a If a protocol change did not start meanwhile, we are good to go.
     2.b If a protocol change started meanwhile:
       2.b.1. Rollback the the increment to the number of messages in transit
       2.b.2. If our rollback set the number of messages in transit to zero,
              signal the GCS thread
       2.b.3. Wait for the protocol change to finish.

 Implementation
 =============================================================================
 The implementation attempts to add as little overhead as possible to the
 common case, which is that no protocol change is ongoing.
 This is the fast path of Algorithm #2, step (2.a).
 To achieve this goal, it employs a tagged lock.
 For more details on the tagged lock implementation, see @c Gcs_tagged_lock.

 In a nutshell, the tagged lock is a read-write spin lock which offers the
 following API:

     try_lock() -> bool
     unlock()
     optimistic_read() -> tag
     validate_optimistic_read(tag) -> bool

 For the write-side section, one uses it as a typical spin lock, e.g.:

     do:
       lock_acquired := try_lock()
     while (not lock_acquired)
     write-side section
     unlock()

 For the read-side section, one can use it as follows:

     done := false
     while (not done):
       tag := optimistic_read()
       unsynchronised read-side section
       done := validate_optimistic_read(tag)
       if (not done):
         rollback unsynchronized read-side section

 The idea is to allow an optimistic read-side section that does not perform
 any memory stores.
 This is in contrast with a typical read-write lock, where the read side
 performs some memory stores to account for the reader, e.g. keeping a reader
 counter.
 The trade off is that:

   a. the execution of the read-side of a tagged lock may be concurrent with
      the write-side section if meanwhile the tagged lock is acquired
   b. the read-side of a tagged lock may fail if meanwhile the tagged lock is
      acquired, in which case one may want to rollback the effects of the
      failed read-side section

 The algorithms of the design are implemented as follows:

   Algorithm #1 implementation, GCS thread:
     1. Lock the tagged lock
     2. When the number of messages in transit is zero:
       2.1. Change the protocol version
       2.2. Unlock the tagged lock, signal a condition variable to wake up any
            user threads waiting for the protocol change
       2.3. Deem the protocol change finished

   Algorithm #2 implementation, User thread:
     1.  If the tagged lock is unlocked:
       1.1. Start an optimistic read-side section
       1.2. Atomically increment the number of messages in transit
     2.a If the optimistic read-side section validates, we are good to go.
     2.b If the optimistic read-side section fails validation:
       2.b.1. Atomically rollback the increment to the number of messages
              in transit
       2.b.2. If our rollback set the number of messages in transit to zero,
              signal the GCS thread
       2.b.3. Wait on a condition variable for the protocol change to finish.

 Note that we have concurrent access to the number of messages in transit
 which needs to be synchronised.
 This is done by using an std::atomic to implement the number of messages in
 transit.

 Some final implementation pointers:

   a. Algorithm #1: see the code path that starts on @c set_protocol_version
      and @c finish_protocol_version_change.
   b. Algorithm #2: see the code paths that start on
      @c atomically_increment_nr_packets_in_transit,
      @c adjust_nr_packets_in_transit, and @c decrement_nr_packets_in_transit.
 */
class Gcs_xcom_communication_protocol_changer {
 public:
  explicit Gcs_xcom_communication_protocol_changer(
      Gcs_xcom_engine &gcs_engine, Gcs_message_pipeline &pipeline);

  Gcs_xcom_communication_protocol_changer(
      Gcs_xcom_communication_protocol_changer const &) = delete;
  Gcs_xcom_communication_protocol_changer(
      Gcs_xcom_communication_protocol_changer &&) = delete;

  Gcs_xcom_communication_protocol_changer &operator=(
      Gcs_xcom_communication_protocol_changer const &) = delete;
  Gcs_xcom_communication_protocol_changer &operator=(
      Gcs_xcom_communication_protocol_changer &&) = delete;

  /**
   Retrieves the current protocol version in use.
   @returns the current protocol version in use
   */
  Gcs_protocol_version get_protocol_version() const;

  /**
   Starts a protocol change.
   The protocol change is asynchronous, the caller can wait for the change to
   finish using the returned future.

   Note that for safety this method *must only* be called by the GCS engine
   thread.

   @param new_version The desired protocol version to change to
   @retval {true, future} If successful
   @retval {false, _} If the group does not support the requested protocol
   */
  std::pair<bool, std::future<void>> set_protocol_version(
      Gcs_protocol_version new_version);

  /**
   Retrieves the greatest protocol version currently supported by the group.
   @returns the greatest protocol version currently supported by the group
   */
  Gcs_protocol_version get_maximum_supported_protocol_version() const;

  /**
   Sets the greatest protocol version currently supported by the group.
   @param version protocol
   */
  void set_maximum_supported_protocol_version(Gcs_protocol_version version);

  /**
   Checks whether a protocol change is ongoing.
   @returns true if a protocol change is ongoing, false otherwise
   */
  bool is_protocol_change_ongoing() const;

  /**
   Synchronises user threads, which send messages, with the GCS thread,
   which performs protocol changes.

   This method should be called by user threads when sending a message, before
   the message goes through the pipeline.

   @param cargo The type of message that will be sent
   */
  void atomically_increment_nr_packets_in_transit(Cargo_type const &cargo);

  /**
   After an outgoing message goes through the pipeline, it may produce more
   than one packet. This method adjusts the increment done by
   atomically_increment_nr_packets_in_transit to take into account the
   additional packets produced by the pipeline.

   @param cargo The type of message that will be sent
   @param nr_additional_packets_to_send The number of additional packets that
   will actually be sent
   */
  void adjust_nr_packets_in_transit(
      Cargo_type const &cargo,
      std::size_t const &nr_additional_packets_to_send);

  /**
   Decrement the number of my in-transit packets.

   @param packet The incoming packet
   @param xcom_nodes The XCom membership at the time of delivery
   */
  void decrement_nr_packets_in_transit(Gcs_packet const &packet,
                                       Gcs_xcom_nodes const &xcom_nodes);

  /**
   Due to the synchronisation protocol used between user threads, which send
   messages, and the GCS thread, which performs protocol changes, a user
   thread may be the one to hit the condition that triggers the protocol
   change to finish.

   This function should be called by the user thread when it hits the
   condition, to signal the GCS thread that the protocol change should finish.

   @param caller_tag Identifier of the protocol change
   */
  void finish_protocol_version_change(Gcs_tagged_lock::Tag const caller_tag);

 private:
  /*
   Returns how many packets of mine are in-transit.
   */
  unsigned long get_nr_packets_in_transit() const;

  /*
   Begins a protocol change, and finishes it if the conditions are met, i.e.
   we have no packets in-transit.
   */
  void begin_protocol_version_change(Gcs_protocol_version new_version);

  /*
   Finishes the ongoing protocol change.

   This method must only be called when is_protocol_change_ongoing(), i.e. after
   a call to begin_protocol_version_change(_).
   */
  void commit_protocol_version_change();

  /*
   Releases the tagged lock and notifies threads waiting for the protocol change
   to finish.
   */
  void release_tagged_lock_and_notify_waiters();

  /*
   Auxiliary method to the implementation of
   atomically_increment_nr_packets_in_transit.

   Optimistically assumes a protocol change will not start meanwhile, and
   increments the number of packets in transit.
   */
  std::pair<bool, Gcs_tagged_lock::Tag>
  optimistically_increment_nr_packets_in_transit();

  /*
   Auxiliary method to the implementation of
   atomically_increment_nr_packets_in_transit.

   Rolls back the effects of optimistically_increment_nr_packets_in_transit and
   signals the GCS thread to finish the protocol change if necessary.
   */
  void rollback_increment_nr_packets_in_transit(
      Gcs_tagged_lock::Tag const &tag);

  /*
   Auxiliary method to the implementation of
   atomically_increment_nr_packets_in_transit.

   Waits until the ongoing protocol change finishes.
   */
  void wait_for_protocol_change_to_finish();

  /**
   Tagged lock used for the optimistic synchronisation protocol between user
   threads, which send messages, and the GCS thread, which performs protocol
   changes.
   */
  Gcs_tagged_lock m_tagged_lock;

  /**
   For user threads to wait for an ongoing protocol change to finish.
   */
  std::mutex m_mutex;
  std::condition_variable m_protocol_change_finished;

  /**
   Stores the outcome of the protocol change operation.
   */
  std::promise<void> m_promise;

  /**
   The protocol version we are going to change to when we start a protocol
   change.
   */
  Gcs_protocol_version m_tentative_new_protocol;

  std::atomic<Gcs_protocol_version> m_max_supported_protocol;

  std::atomic<unsigned long> m_nr_packets_in_transit;

  Gcs_xcom_engine &m_gcs_engine;

  Gcs_message_pipeline &m_msg_pipeline;
};

#endif /* GCS_XCOM_COMMUNICATION_PROTOCOL_CHANGER_INCLUDED */

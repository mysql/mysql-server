/* Copyright (c) 2023, 2024, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is designed to work with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have either included with
   the program or referenced in the documentation.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#ifndef METRICS_HANDLER_INCLUDED
#define METRICS_HANDLER_INCLUDED

#include <atomic>
#include "my_systime.h"

/*
  Forward declarations
*/
class Gcs_message;

/**
  @class Metrics

  Handle metrics captured on Group Replication.
*/
class Metrics_handler {
 public:
  enum class enum_message_type {
    CONTROL,  // Control messages.
    DATA      // Messages that contain transaction data.
  };

  Metrics_handler() = default;

  virtual ~Metrics_handler() = default;

  /**
    Return time in microseconds since the Epoch, 1970-01-01 00:00:00 +0000 (UTC)

    @return time in microseconds.
  */
  inline static uint64_t get_current_time() { return my_micro_time(); }

  /**
    Reset the metrics.
  */
  void reset();

  /**
    Number of control messages sent by this member.

    @return number of messages.
  */
  uint64_t get_control_messages_sent_count() const;

  /**
    Number of messages that contain transaction data sent by this member.

    @return number of messages.
  */
  uint64_t get_data_messages_sent_count() const;

  /**
    Sum of bytes of control messages sent by this member.
    The size is the over the wire size.

    @return sum of bytes.
  */
  uint64_t get_control_messages_sent_bytes_sum() const;

  /**
    Sum of bytes of messages that contain transaction data sent by this member.
    The size is the over the wire size.

    @return sum of bytes.
  */
  uint64_t get_data_messages_sent_bytes_sum() const;

  /**
    Sum of the roundtrip time in micro-seconds of control messages sent by this
    member. The time is measured between the send and the delivery of
    the message on the sender member.
    @see Metrics_handler::get_current_time()

    @return sum of the roundtrip time.
  */
  uint64_t get_control_messages_sent_roundtrip_time_sum() const;

  /**
    Sum of the roundtrip time in micro-seconds of messages that contain
    transaction data sent by this member. The time is measured between the send
    and the delivery of the message on the sender member.
    @see Metrics_handler::get_current_time()

    @return sum of the roundtrip time.
  */
  uint64_t get_data_messages_sent_roundtrip_time_sum() const;

  /**
    Number of transactions executed with group_replication_consistency=BEFORE or
    BEFORE_AND_AFTER.

    @return number of transactions.
  */
  uint64_t get_transactions_consistency_before_begin_count() const;

  /**
    Sum of the time that the member waited until its `group_replication_applier`
    channel was consumed before execute the transaction with
    group_replication_consistency=BEFORE or BEFORE_AND_AFTER.
    @see Metrics_handler::get_current_time()

    @return sum of the wait time.
  */
  uint64_t get_transactions_consistency_before_begin_time_sum() const;

  /**
    Number of transactions executed with group_replication_consistency=AFTER or
    BEFORE_AND_AFTER.

    @return number of transactions.
  */
  uint64_t get_transactions_consistency_after_termination_count() const;

  /**
    Sum of the time spent between the delivery of the transaction executed with
    group_replication_consistency= AFTER or BEFORE_AND_AFTER, and the
    acknowledge of the other group members that the transaction is prepared.
    @see Metrics_handler::get_current_time()

    @return sum of the wait time.
  */
  uint64_t get_transactions_consistency_after_termination_time_sum() const;

  /**
    Number of transactions executed with group_replication_consistency=AFTER or
    BEFORE_AND_AFTER.

    @return number of transactions.
  */
  uint64_t get_transactions_consistency_after_sync_count() const;

  /**
    Sum of the time that transactions on secondaries waited to start, while
    waiting for transactions from the primary with
    group_replication_consistency=AFTER or BEFORE_AND_AFTER to be committed.
    @see Metrics_handler::get_current_time()

    @return sum of the wait time.
  */
  uint64_t get_transactions_consistency_after_sync_time_sum() const;

  /**
    Number of times certification garbage collection did run.

    @return number of transactions.
  */
  uint64_t get_certification_garbage_collector_count() const;

  /**
    Sum of the time that garbage collection runs took.
    @see Metrics_handler::get_current_time()

    @return sum of the time.
  */
  uint64_t get_certification_garbage_collector_time_sum() const;

  /**
    Account message sent.

    @param message  the message to be accounted.
  */
  void add_message_sent(const Gcs_message &message);

  /**
    Account transaction that waited until its `group_replication_applier`
    channel was consumed before execute the transaction with
    group_replication_consistency=BEFORE or BEFORE_AND_AFTER.
    @see Metrics_handler::get_current_time()

    @param begin_timestamp  time on which the wait began.
    @param end_timestamp    time on which the wait ended.
  */
  void add_transaction_consistency_before_begin(const uint64_t begin_timestamp,
                                                const uint64_t end_timestamp);

  /**
    Account transaction executed with group_replication_consistency=AFTER or
    BEFORE_AND_AFTER that waited for the acknowledge of the other group members
    that the transaction is prepared.
    @see Metrics_handler::get_current_time()

    @param begin_timestamp  time on which the wait began.
    @param end_timestamp    time on which the wait ended.
  */
  void add_transaction_consistency_after_termination(
      const uint64_t begin_timestamp, const uint64_t end_timestamp);

  /**
    Account transaction that waited for transactions from the primary with
    group_replication_consistency=AFTER or BEFORE_AND_AFTER to be committed.
    @see Metrics_handler::get_current_time()

    @param begin_timestamp  time on which the wait began.
    @param end_timestamp    time on which the wait ended.
  */
  void add_transaction_consistency_after_sync(const uint64_t begin_timestamp,
                                              const uint64_t end_timestamp);

  /**
    Account a certification garbage collection run.
    @see Metrics_handler::get_current_time()

    @param begin_timestamp  time on which the operation began.
    @param end_timestamp    time on which the operation ended.
  */
  void add_garbage_collection_run(const uint64_t begin_timestamp,
                                  const uint64_t end_timestamp);

 private:
  /**
    Account message sent.
    @see Metrics_handler::get_current_time()

    @param type                message type.
    @param bytes               message length.
    @param sent_timestamp      time on which the message was sent.
    @param received_timestamp  time on which the message was received.
  */
  void add_message_sent_internal(const enum_message_type type,
                                 const uint64_t bytes,
                                 const uint64_t sent_timestamp,
                                 const uint64_t received_timestamp);

  std::atomic<uint64_t> m_control_messages_sent_count{0};
  std::atomic<uint64_t> m_data_messages_sent_count{0};

  std::atomic<uint64_t> m_control_messages_sent_bytes_sum{0};
  std::atomic<uint64_t> m_data_messages_sent_bytes_sum{0};

  std::atomic<uint64_t> m_control_messages_sent_roundtrip_time_sum{0};
  std::atomic<uint64_t> m_data_messages_sent_roundtrip_time_sum{0};

  std::atomic<uint64_t> m_transactions_consistency_before_begin_count{0};
  std::atomic<uint64_t> m_transactions_consistency_before_begin_time_sum{0};

  std::atomic<uint64_t> m_transactions_consistency_after_termination_count{0};
  std::atomic<uint64_t> m_transactions_consistency_after_termination_time_sum{
      0};

  std::atomic<uint64_t> m_transactions_consistency_after_sync_count{0};
  std::atomic<uint64_t> m_transactions_consistency_after_sync_time_sum{0};

  std::atomic<uint64_t> m_certification_garbage_collector_count{0};
  std::atomic<uint64_t> m_certification_garbage_collector_time_sum{0};
};

#endif /* METRICS_HANDLER_INCLUDED */

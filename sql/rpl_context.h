/* Copyright (c) 2014, 2024, Oracle and/or its affiliates.

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

#ifndef RPL_SESSION_H
#define RPL_SESSION_H

#include <sys/types.h>
#include <memory>

#include "my_inttypes.h"                                // IWYU pragma: keep
#include "mysql/binlog/event/compression/compressor.h"  // mysql::binlog::event::compression::Compressor
#include "mysql/binlog/event/nodiscard.h"

#include "mysql/binlog/event/compression/factory.h"
#include "sql/binlog/group_commit/bgc_ticket.h"
#include "sql/memory/aligned_atomic.h"
#include "sql/psi_memory_key.h"
#include "sql/resource_blocker.h"  // resource_blocker::User
#include "sql/system_variables.h"

#include <functional>
#include <vector>

class Gtid_set;
class Tsid_map;
class THD;
struct Gtid;

/** Type of replication channel thread/transaction might be associated to*/
enum enum_rpl_channel_type {
  NO_CHANNEL_INFO = 0,       // No information exists about the channel
  RPL_STANDARD_CHANNEL = 1,  // It is a standard replication channel
  GR_APPLIER_CHANNEL = 2,    // It is a GR applier channel
  GR_RECOVERY_CHANNEL = 3    // It is a GR recovery channel
};

/**
   This class is an interface for session consistency instrumentation
   in the server. It holds the context information for a given session.

   It does not require locking since access to this content is mutually
   exclusive by design (only one thread reading or writing to this object
   at a time).
 */
class Session_consistency_gtids_ctx {
 public:
  /**
   This is an interface to be implemented by classes that want to listen
   to changes to this context. This can be used, for instance, by the
   session tracker gtids to become aware of ctx modifications.
   */
  class Ctx_change_listener {
   public:
    Ctx_change_listener() = default;
    virtual ~Ctx_change_listener() = default;
    virtual void notify_session_gtids_ctx_change() = 0;

   private:
    // not implemented
    Ctx_change_listener(const Ctx_change_listener &rsc);
    Ctx_change_listener &operator=(const Ctx_change_listener &rsc);
  };

 private:
  /*
   Local tsid_map to enable a lock free m_gtid_set.
   */
  Tsid_map *m_tsid_map;

  /**
    Set holding the transaction identifiers of the gtids
    to reply back on the response packet.

    Lifecycle: Emptied after the reply is sent back to the application. Remains
    empty until:
    - a RW transaction commits and a GTID is written to the binary log.
    - a RO transaction is issued, the consistency level is set to "Check
      Potential Writes" and the transaction is committed.
  */
  Gtid_set *m_gtid_set;

  /**
   If a listener is registered, e.g., the session track gtids, then this
   points to an instance of such listener.

   Since this context is valid only for one session, there is no need
   to protect this with locks.
  */
  Session_consistency_gtids_ctx::Ctx_change_listener *m_listener;

  /**
   Keeps track of the current session track gtids, so that we capture
   according to what was set before. For instance, if the user does:
   SET @@SESSION.SESSION_TRACK_GTIDS='ALL_GTIDS';
   ...
   SET @@SESSION.SESSION_TRACK_GTIDS='OWN_GTID';

   The last statement should return a set of GTIDs.
  */
  ulong m_curr_session_track_gtids;

 protected:
  /*
     Auxiliary function to determine if GTID collection should take place
     when it is invoked. It takes into consideration the gtid_mode and
     the current session context.

     @param thd the thread context.
     @return true if should collect gtids, false otherwise.
   */
  inline bool shall_collect(const THD *thd);

  /**
   Auxiliary function that allows notification of ctx change listeners.
   */
  inline void notify_ctx_change_listener() {
    m_listener->notify_session_gtids_ctx_change();
  }

 public:
  /**
    Simple constructor.
  */
  Session_consistency_gtids_ctx();

  /**
    The destructor. Deletes the m_gtid_set and the tsid_map.
  */
  virtual ~Session_consistency_gtids_ctx();

  /**
   Registers the listener. The pointer MUST not be NULL.

   @param listener a pointer to the listener to register.
   @param thd THD context associated to this listener.
  */
  void register_ctx_change_listener(
      Session_consistency_gtids_ctx::Ctx_change_listener *listener, THD *thd);

  /**
   Unregisters the listener. The listener MUST have registered previously.

   @param listener a pointer to the listener to register.
  */
  void unregister_ctx_change_listener(
      Session_consistency_gtids_ctx::Ctx_change_listener *listener);

  /**
    This member function MUST return a reference to the set of collected
    GTIDs so far.

    @return the set of collected GTIDs so far.
   */
  inline Gtid_set *state() { return m_gtid_set; }

  /**
     This function MUST be called after the response packet is set to the
     client connected. The implementation may act on the collected state
     for instance to do garbage collection.

     @param thd The thread context.
   * @return true on error, false otherwise.
   */
  virtual bool notify_after_response_packet(const THD *thd);

  /**
     This function SHALL be called once the GTID for the given transaction has
     has been added to GTID_EXECUTED.

     This function SHALL store the data if the
     thd->variables.session_track_gtids is set to a value other than NONE.

     @param thd   The thread context.
     @return true on error, false otherwise.
   */
  virtual bool notify_after_gtid_executed_update(const THD *thd);

  /**
     This function MUST be called after a transaction is committed
     in the server. It should be called regardless whether it is a
     RO or RW transaction. Also, DDLs, DDS are considered transaction
     for what is worth.

     This function SHALL store relevant data for the session consistency.

     @param thd    The thread context.
     @return true on error, false otherwise.
   */
  virtual bool notify_after_transaction_commit(const THD *thd);

  virtual bool notify_after_xa_prepare(const THD *thd) {
    return notify_after_transaction_commit(thd);
  }

  /**
    Update session tracker (m_curr_session_track_gtids) from thd.
  */
  void update_tracking_activeness_from_session_variable(const THD *thd);

 private:
  // not implemented
  Session_consistency_gtids_ctx(const Session_consistency_gtids_ctx &rsc);
  Session_consistency_gtids_ctx &operator=(
      const Session_consistency_gtids_ctx &rsc);
};

/**
  This class tracks the last used GTID per session.
*/
class Last_used_gtid_tracker_ctx {
 public:
  Last_used_gtid_tracker_ctx();
  virtual ~Last_used_gtid_tracker_ctx();

  /**
   Set the last used GTID the session.

   @param[in]  gtid  the used gtid.
   @param[in]  sid   the used sid.
  */
  void set_last_used_gtid(const Gtid &gtid, const mysql::gtid::Tsid &sid);

  /**
   Get the last used GTID the session.

   @param[out]  gtid  the used gtid.
  */
  void get_last_used_gtid(Gtid &gtid);

  /**
   Get the last used TSID of the session.

   @param[out]  tsid the used tsid.
  */
  void get_last_used_tsid(mysql::gtid::Tsid &tsid);

 private:
  std::unique_ptr<Gtid> m_last_used_gtid;
  mysql::gtid::Tsid m_last_used_tsid;
};

class Transaction_compression_ctx {
  using Compressor_t = mysql::binlog::event::compression::Compressor;
  using Grow_calculator_t =
      mysql::binlog::event::compression::buffer::Grow_calculator;
  using Factory_t = mysql::binlog::event::compression::Factory;

 public:
  using Compressor_ptr_t = std::shared_ptr<Compressor_t>;
  using Managed_buffer_sequence_t = Compressor_t::Managed_buffer_sequence_t;
  using Memory_resource_t = mysql::binlog::event::resource::Memory_resource;

  explicit Transaction_compression_ctx(PSI_memory_key key);

  /// Return the compressor.
  ///
  /// This constructs the compressor on the first invocation and
  /// returns the same compressor on subsequent invocations.
  Compressor_ptr_t get_compressor(THD *session);

  /// Return reference to the buffer sequence holding compressed
  /// bytes.
  Managed_buffer_sequence_t &managed_buffer_sequence();

 private:
  Memory_resource_t m_managed_buffer_memory_resource;
  Managed_buffer_sequence_t m_managed_buffer_sequence;
  Compressor_ptr_t m_compressor;
};

/**
  Keeps the THD session context to be used with the
  `Bgc_ticket_manager`. In particular, manages the value of the ticket the
  current THD session has been assigned to.
 */
class Binlog_group_commit_ctx {
 public:
  Binlog_group_commit_ctx() = default;
  virtual ~Binlog_group_commit_ctx() = default;

  /**
    Retrieves the ticket that the THD session has been assigned to. If
    it hasn't been assigned to any yet, returns '0'.

    @return The ticket the THD session has been assigned to, if
            any. Returns `0` if it hasn't.
   */
  binlog::BgcTicket get_session_ticket();
  /**
    Sets the THD session's ticket to the given value.

    @param ticket The ticket to set the THD session to.
   */
  void set_session_ticket(binlog::BgcTicket ticket);
  /**
    Assigns the THD session to the ticket accepting assignments in the
    ticket manager. The method is idem-potent within the execution of a
    statement. This means that it can be invoked several times during the
    execution of a command within the THD session that only once will the
    session be assign to a ticket.
   */
  void assign_ticket();
  /**
    Whether or not the session already waited on the ticket.

    @return true if the session already waited, false otherwise.
   */
  bool has_waited();
  /**
    Marks the underlying session has already waited on the ticket.
   */
  void mark_as_already_waited();
  /**
    Resets the THD session's ticket context.
   */
  void reset();
  /**
    Returns the textual representation of this object;

    @return a string containing the textual representation of this object.
   */
  std::string to_string() const;
  /**
    Dumps the textual representation of this object into the given output
    stream.

    @param out The stream to dump this object into.
   */
  void format(std::ostream &out) const;
  /**
    Dumps the textual representation of an instance of this class into the
    given output stream.

    @param out The output stream to dump the instance to.
    @param to_dump The class instance to dump to the output stream.

    @return The output stream to which the instance was dumped to.
   */
  inline friend std::ostream &operator<<(
      std::ostream &out, Binlog_group_commit_ctx const &to_dump) {
    to_dump.format(out);
    return out;
  }
  /**
    Retrieves the flag for determining if it should be possible to manually
    set the session's ticket.

    @return the reference for the atomic flag.
   */
  static memory::Aligned_atomic<bool> &manual_ticket_setting();

 private:
  /** The ticket the THD session has been assigned to. */
  binlog::BgcTicket m_session_ticket{0};
  /** Whether or not the session already waited on the ticket. */
  bool m_has_waited{false};
};

/*
  This class SHALL encapsulate the replication context associated with the THD
  object.
 */
class Rpl_thd_context {
 public:
  /**
    This structure helps to maintain state of transaction.
    State of transaction is w.r.t delegates
    Please refer Trans_delegate to understand states being referred.
  */
  enum enum_transaction_rpl_delegate_status {
    // Initialized, first state
    TX_RPL_STAGE_INIT = 0,
    // begin is being called
    TX_RPL_STAGE_BEGIN,
    // binlog cache created, transaction will be binlogged
    TX_RPL_STAGE_CACHE_CREATED,
    // before_commit is being called
    TX_RPL_STAGE_BEFORE_COMMIT,
    // before_rollback is being called
    TX_RPL_STAGE_BEFORE_ROLLBACK,
    // transaction has ended
    TX_RPL_STAGE_CONNECTION_CLEANED,
    // end
    TX_RPL_STAGE_END  // Not used
  };

  resource_blocker::User dump_thread_user;

 private:
  Session_consistency_gtids_ctx m_session_gtids_ctx;
  Last_used_gtid_tracker_ctx m_last_used_gtid_tracker_ctx;
  Transaction_compression_ctx m_transaction_compression_ctx;
  /** Manages interaction and keeps context w.r.t `Bgc_ticket_manager` */
  Binlog_group_commit_ctx m_binlog_group_commit_ctx;
  std::vector<std::function<bool()>> m_post_filters_actions;
  /** If this thread is a channel, what is its type*/
  enum_rpl_channel_type rpl_channel_type;

  Rpl_thd_context(const Rpl_thd_context &rsc);
  Rpl_thd_context &operator=(const Rpl_thd_context &rsc);

 public:
  Rpl_thd_context(PSI_memory_key transaction_compression_ctx)
      : m_transaction_compression_ctx(transaction_compression_ctx),
        rpl_channel_type(NO_CHANNEL_INFO) {}

  /**
    Initializers. Clears the writeset session history and re-set delegate state
    to INIT.
  */
  void init();

  inline Session_consistency_gtids_ctx &session_gtids_ctx() {
    return m_session_gtids_ctx;
  }

  inline Last_used_gtid_tracker_ctx &last_used_gtid_tracker_ctx() {
    return m_last_used_gtid_tracker_ctx;
  }

  /**
    Retrieves the class member responsible for managing the interaction
    with `Bgc_ticket_manager`.

    @return The class member responsible for managing the interaction
            with `Bgc_ticket_manager`.
   */
  Binlog_group_commit_ctx &binlog_group_commit_ctx();

  enum_rpl_channel_type get_rpl_channel_type() { return rpl_channel_type; }

  void set_rpl_channel_type(enum_rpl_channel_type rpl_channel_type_arg) {
    rpl_channel_type = rpl_channel_type_arg;
  }

  inline Transaction_compression_ctx &transaction_compression_ctx() {
    return m_transaction_compression_ctx;
  }

  std::vector<std::function<bool()>> &post_filters_actions() {
    return m_post_filters_actions;
  }

  /**
    Sets the transaction states

    @param[in] status state to which THD is progressing
  */
  void set_tx_rpl_delegate_stage_status(
      enum_transaction_rpl_delegate_status status);

  /**
    Returns the transaction state.

    @return status transaction status is returned
  */
  enum_transaction_rpl_delegate_status get_tx_rpl_delegate_stage_status();

 private:
  /* Maintains transaction status of Trans_delegate. */
  enum_transaction_rpl_delegate_status m_tx_rpl_delegate_stage_status{
      TX_RPL_STAGE_INIT};
};

#endif /* RPL_SESSION_H */

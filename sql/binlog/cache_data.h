#ifndef BINLOG_CACHE_DATA_H_INCLUDED
#define BINLOG_CACHE_DATA_H_INCLUDED

#include <map>
#include <string>
#include <string_view>

#include "my_dbug.h"
#include "my_inttypes.h"
#include "my_sys.h"
#include "mysql/binlog/event/binlog_event.h"
#include "mysql/components/services/log_builtins.h"
#include "mysqld_error.h"
#include "sql/binlog_ostream.h"
#include "sql/debug_sync.h"
#include "sql/log_event.h"
#include "sql/mysqld.h"  // binlog_cache_size
#include "sql/sql_class.h"
#include "sql/xa.h"

/**
  @file
  @brief The per-session binary log caches, where a session's binary log
  events are buffered before they are written to a binary log file.

  binlog_cache_data
    Base class. Serializes Log_events into its Binlog_cache_storage, which
    buffers them in memory and spills to a temporary file once
    binlog_cache_size is exceeded.

  binlog_stmt_cache_data
    The statement cache, holding changes to non-transactional tables.

  binlog_trx_cache_data
    The transaction cache, holding changes to transactional tables until
    commit.

  binlog_cache_mngr
    Owns one statement cache and one transaction cache per session, and
    records a logging incident when events could not be cached.
*/

#define MY_OFF_T_UNDEF (~(my_off_t)0UL)

/**
  Caches for non-transactional and transactional data before writing
  it to the binary log.

  @todo All the access functions for the flags suggest that the
  encapsuling is not done correctly, so try to move any logic that
  requires access to the flags into the cache.
*/
class binlog_cache_data {
 public:
  binlog_cache_data(class binlog_cache_mngr &cache_mngr, bool trx_cache_arg,
                    ulong *ptr_binlog_cache_use_arg,
                    ulong *ptr_binlog_cache_disk_use_arg)
      : m_cache_mngr(cache_mngr),
        m_pending(nullptr),
        ptr_binlog_cache_use(ptr_binlog_cache_use_arg),
        ptr_binlog_cache_disk_use(ptr_binlog_cache_disk_use_arg) {
    flags.transactional = trx_cache_arg;
  }

  bool open(my_off_t cache_size, my_off_t max_cache_size);

  Binlog_cache_storage *get_cache() { return &m_cache; }
  int finalize(THD *thd, Log_event *end_event);
  int finalize(THD *thd, Log_event *end_event, XID_STATE *xs);
  int flush(THD *thd, my_off_t *bytes, bool *wrote_xid,
            bool parallelization_barrier);
  int write_event(Log_event *event);
  void set_event_counter(size_t event_counter) {
    m_event_counter = event_counter;
  }
  size_t get_event_counter() const { return m_event_counter; }
  size_t get_compressed_size() const { return m_compressed_size; }
  size_t get_decompressed_size() const { return m_decompressed_size; }
  mysql::binlog::event::compression::type get_compression_type() const {
    return m_compression_type;
  }

  void set_compressed_size(size_t s) { m_compressed_size = s; }
  void set_decompressed_size(size_t s) { m_decompressed_size = s; }
  void set_compression_type(mysql::binlog::event::compression::type t) {
    m_compression_type = t;
  }

  virtual ~binlog_cache_data() {
    assert(is_binlog_empty());
    m_cache.close();
  }

  bool is_binlog_empty() const {
    DBUG_PRINT("debug", ("%s_cache - pending: 0x%llx, bytes: %llu",
                         (flags.transactional ? "trx" : "stmt"),
                         (ulonglong)pending(), (ulonglong)m_cache.length()));
    return pending() == nullptr && m_cache.is_empty();
  }

  bool is_finalized() const { return flags.finalized; }

  Rows_log_event *pending() const { return m_pending; }

  void set_pending(Rows_log_event *const pending) { m_pending = pending; }

  /// @see handle_deferred_cache_write_incident
  void set_incident(
      std::string_view incident_message =
          "Non-transactional changes were not written to the binlog.");

  /// @see handle_deferred_cache_write_incident
  bool has_incident(void) const;

  bool has_xid() const {
    // There should only be an XID event if we are transactional
    assert((flags.transactional && flags.with_xid) || !flags.with_xid);
    return flags.with_xid;
  }

  bool is_trx_cache() const { return flags.transactional; }

  /**
    Returns the checksum algorithm the events of this cache are
    serialized with, recorded at the transaction's first event (see
    write_event).

    @return The algorithm, or BINLOG_CHECKSUM_ALG_UNDEF when no checksum is
            written into this cache.
  */
  mysql::binlog::event::enum_binlog_checksum_alg checksum_alg_in_cache() const {
    return m_checksum_alg_in_cache;
  }

  /**
    Returns true when the events in this cache carry a checksum
    (see write_event).

    @retval true   The events carry their own checksum.
    @retval false  They do not, so a checksum is added when the cache is
                   copied into the binary log.
  */
  bool is_checksum_computed() const {
    return m_checksum_alg_in_cache !=
               mysql::binlog::event::BINLOG_CHECKSUM_ALG_UNDEF &&
           m_checksum_alg_in_cache !=
               mysql::binlog::event::BINLOG_CHECKSUM_ALG_OFF;
  }

  /**
    Captures the large transaction optimization's settings for the whole
    transaction, on the first event written to the cache.

    Called on every write but effective only once, so a change to
    binlog_large_transaction_optimization_enabled or
    binlog_large_transaction_optimization_threshold mid-transaction
    cannot change how that transaction commits.

    The captured values are read back through large_trx_optimization_enabled()
    and large_trx_optimization_threshold().
  */
  void latch_large_trx_optimization();

  /**
    @return Whether this transaction may use the binlog large transaction
            optimization, as captured by latch_large_trx_optimization() at the
            transaction's first event.
  */
  bool large_trx_optimization_enabled() const {
    return m_large_trx_optimization_enabled;
  }

  /**
    @return The spilled size, in bytes, above which this transaction is a
            promotion candidate, as captured by latch_large_trx_optimization()
            at the transaction's first event.
  */
  ulonglong large_trx_optimization_threshold() const {
    return m_large_trx_optimization_threshold;
  }

  /**
    Returns where the transaction's terminating event begins in a promoted
    binary log file, so recovery can seek to it. Recorded by finalize(),
    together with the event's type.

    @return The byte offset from the start of the promoted file, or 0 when the
            transaction has no terminating event. Only a real end event
            (COMMIT / XID / XA_PREPARE) counts; an immediately-logged
            statement finalizes without one.
  */
  my_off_t terminating_event_offset() const {
    return m_terminating_event_offset;
  }

  /**
    Returns the type of the transaction's terminating event in a promoted
    binary log file. Recorded by finalize(), together with its offset.

    @return The event type, meaningful only when terminating_event_offset() is
            nonzero.
  */
  mysql::binlog::event::Log_event_type terminating_event_type() const {
    return m_terminating_event_type;
  }

  my_off_t get_byte_position() const { return m_cache.length(); }

  void cache_state_checkpoint(my_off_t pos_to_checkpoint);

  void cache_state_rollback(my_off_t pos_to_rollback);

  /**
     Reset the cache to unused state when the transaction is finished. It
     drops all data and clears the transaction flags.

     @param preserve_spilled_file  When true, the spilled file is retained
            rather than deleted, because the caller has promoted it into the
            binary log sequence and now owns it.
  */
  virtual void reset(bool preserve_spilled_file = false);

  /**
    Returns information about the cache content with respect to
    the binlog_format of the events.

    This will be used to set a flag on GTID_LOG_EVENT stating that the
    transaction may have SBR statements or not, but the binlog dump
    will show this flag as "rbr_only" when it is not set. That's why
    an empty transaction should return true below, or else an empty
    transaction would be assumed as "rbr_only" even not having RBR
    events.

    When dumping a binary log content using mysqlbinlog client program,
    for any transaction assumed as "rbr_only" it will be printed a
    statement changing the transaction isolation level to READ COMMITTED.
    It doesn't make sense to have an empty transaction "requiring" this
    isolation level change.

    @return true  The cache have SBR events or is empty.
    @return false The cache contains a transaction with no SBR events.
   */
  bool may_have_sbr_stmts() { return flags.with_sbr || !flags.with_rbr; }

  /**
    Check if the binlog cache contains an empty transaction, which has
    two binlog events "BEGIN" and "COMMIT".

    @return true  The binlog cache contains an empty transaction.
    @return false Otherwise.
  */
  bool has_empty_transaction();

  /**
    Check if the binlog cache is empty or contains an empty transaction,
    which has two binlog events "BEGIN" and "COMMIT".

    @return true  The binlog cache is empty or contains an empty transaction.
    @return false Otherwise.
  */
  bool is_empty_or_has_empty_transaction() {
    return is_binlog_empty() || has_empty_transaction();
  }

 protected:
  /*
    This structure should have all cache variables/flags that should be restored
    when a ROLLBACK TO SAVEPOINT statement be executed.
  */
  struct cache_state {
    bool with_sbr;
    bool with_rbr;
    bool with_start;
    bool with_end;
    bool with_content;
    size_t event_counter;
  };
  /*
    For every SAVEPOINT used, we will store a cache_state for the current
    binlog cache position. So, if a ROLLBACK TO SAVEPOINT is used, we can
    restore the cache_state values after truncating the binlog cache.
  */
  std::map<my_off_t, cache_state> cache_state_map;
  /*
    In order to compute the transaction size (because of possible extra checksum
    bytes), we need to keep track of how many events are in the binlog cache.
  */
  size_t m_event_counter = 0;

  /**
    Checksum algorithm the events of this cache are serialized with,
    recorded at the transaction's first event (see write_event). Stays
    BINLOG_CHECKSUM_ALG_UNDEF while no checksum is written into this cache,
    which is the case for the statement cache and whenever the large
    transaction optimization is disabled.
  */
  mysql::binlog::event::enum_binlog_checksum_alg m_checksum_alg_in_cache =
      mysql::binlog::event::BINLOG_CHECKSUM_ALG_UNDEF;

  /**
    Whether the capture has happened, so that only the transaction's first
    event decides and later events leave the values alone.
  */
  bool m_large_trx_optimization_latched = false;
  /**
    Whether this transaction may use the large transaction optimization: the
    value of binlog_large_transaction_optimization_enabled at the transaction's
    first event.

    Group Replication check is folded in here rather than
    reported as a fallback reason, because it must be known before the first
    event is serialized.
  */
  bool m_large_trx_optimization_enabled = false;
  /**
    Value of binlog_large_transaction_optimization_threshold at the
    transaction's first event, in bytes. Compared against the spilled size at
    commit.
  */
  ulonglong m_large_trx_optimization_threshold = 0;

  /**
    Offset of the transaction's terminating event in a promoted binary log
    file, recorded at finalize() together with its type. Stays 0 when the
    transaction has no terminating event.
  */
  my_off_t m_terminating_event_offset = 0;
  /**
    Type of the transaction's terminating event in a promoted binary log file,
    recorded at finalize() together with its offset.
  */
  mysql::binlog::event::Log_event_type m_terminating_event_type =
      mysql::binlog::event::UNKNOWN_EVENT;

  size_t m_compressed_size = 0;
  size_t m_decompressed_size = 0;
  mysql::binlog::event::compression::type m_compression_type =
      mysql::binlog::event::compression::type::NONE;
  /*
    It truncates the cache to a certain position. This includes deleting the
    pending event. It corresponds to rollback statement or rollback to
    a savepoint. It doesn't change transaction state.
   */
  void truncate(my_off_t pos);

  /**
     Flush pending event to the cache buffer.
   */
  int flush_pending_event(THD *thd);

  /**
    Remove the pending event.
   */
  int remove_pending_event();
  struct Flags {
    /*
      Defines if this is either a trx-cache or stmt-cache, respectively, a
      transactional or non-transactional cache.
    */
    bool transactional : 1;

    /*
      This indicates that the cache should be written without BEGIN/END.
    */
    bool immediate : 1;

    /*
      This flag indicates that the buffer was finalized and has to be
      flushed to disk.
     */
    bool finalized : 1;

    /*
      This indicates that either the cache contain an XID event, or it's
      an atomic DDL Query-log-event. In the latter case the flag is set up
      on the statement level, namely when the Query-log-event is cached
      at time the DDL transaction is not committing.
      The flag therefore gets reset when the cache is cleaned due to
      the statement rollback, e.g in case of a DDL post-caching execution
      error.
      Any statement scope flag among other things must consider its
      reset policy when the statement is rolled back.
    */
    bool with_xid : 1;

    /*
      This indicates that the cache contain statements changing content.
    */
    bool with_sbr : 1;

    /*
      This indicates that the cache contain RBR event changing content.
    */
    bool with_rbr : 1;

    /*
      This indicates that the cache contain s transaction start statement.
    */
    bool with_start : 1;

    /*
      This indicates that the cache contain a transaction end event.
    */
    bool with_end : 1;

    /*
      This indicates that the cache contain content other than START/END.
    */
    bool with_content : 1;
  } flags;

  /// Compress the current transaction "in-place", if possible
  ///
  /// This attempts to compress the transaction if it satisfies the
  /// necessary pre-conditions. Otherwise it does nothing.
  ///
  /// @retval true Error: the cache has been corrupted and the
  /// transaction must be aborted.
  ///
  /// @retval false Success: the transaction was either compressed
  /// successfully, or compression was not attempted, or compression
  /// failed and left the uncompressed transaction intact.
  [[nodiscard]] bool compress(THD *thd);

 private:
  /*
    Reference to the cache_mngr which owns this cache.
   */
  class binlog_cache_mngr &m_cache_mngr;

  /*
    Storage for byte data. This binlog_cache_data will serialize
    events into bytes and put them into m_cache.
  */
  Binlog_cache_storage m_cache;

  /*
    Pending binrows event. This event is the event where the rows are currently
    written.
   */
  Rows_log_event *m_pending;

  /**
    This function computes binlog cache and disk usage.
  */
  void compute_statistics();

  /*
    Stores a pointer to the status variable that keeps track of the in-memory
    cache usage. This corresponds to either
      . binlog_cache_use or binlog_stmt_cache_use.
  */
  ulong *ptr_binlog_cache_use;

  /*
    Stores a pointer to the status variable that keeps track of the disk
    cache usage. This corresponds to either
      . binlog_cache_disk_use or binlog_stmt_cache_disk_use.
  */
  ulong *ptr_binlog_cache_disk_use;

  binlog_cache_data &operator=(const binlog_cache_data &info);
  binlog_cache_data(const binlog_cache_data &info);
};

class binlog_stmt_cache_data : public binlog_cache_data {
 public:
  binlog_stmt_cache_data(binlog_cache_mngr &cache_mngr, bool trx_cache_arg,
                         ulong *ptr_binlog_cache_use_arg,
                         ulong *ptr_binlog_cache_disk_use_arg)
      : binlog_cache_data(cache_mngr, trx_cache_arg, ptr_binlog_cache_use_arg,
                          ptr_binlog_cache_disk_use_arg) {}

  using binlog_cache_data::finalize;

  int finalize(THD *thd);
};

class binlog_trx_cache_data : public binlog_cache_data {
 public:
  binlog_trx_cache_data(binlog_cache_mngr &cache_mngr, bool trx_cache_arg,
                        ulong *ptr_binlog_cache_use_arg,
                        ulong *ptr_binlog_cache_disk_use_arg)
      : binlog_cache_data(cache_mngr, trx_cache_arg, ptr_binlog_cache_use_arg,
                          ptr_binlog_cache_disk_use_arg),
        m_cannot_rollback(false),
        before_stmt_pos(MY_OFF_T_UNDEF) {}

  void reset(bool preserve_spilled_file = false) override;

  bool cannot_rollback() const { return m_cannot_rollback; }

  void set_cannot_rollback() { m_cannot_rollback = true; }

  my_off_t get_prev_position() const { return before_stmt_pos; }

  void set_prev_position(my_off_t pos);

  void restore_prev_position();

  void restore_savepoint(my_off_t pos);

  using binlog_cache_data::truncate;

  void truncate(THD *thd, bool all);

 private:
  /*
    It will be set true if any statement which cannot be rolled back safely
    is put in trx_cache.
  */
  bool m_cannot_rollback;

  /*
    Binlog position before the start of the current statement.
  */
  my_off_t before_stmt_pos;

  binlog_trx_cache_data &operator=(const binlog_trx_cache_data &info);
  binlog_trx_cache_data(const binlog_trx_cache_data &info);
};

class binlog_cache_mngr {
  /// Indicates that some events did not get into the cache(s) and most
  /// likely it is incomplete. @see handle_deferred_cache_write_incident
  std::string m_incident;

 public:
#ifndef NDEBUG
  /// The number of times that the incident status has been set due to the
  /// debug symbol binlog_inject_incident.
  int m_injected_incident_count{0};
#endif

  binlog_cache_mngr(ulong *ptr_binlog_stmt_cache_use_arg,
                    ulong *ptr_binlog_stmt_cache_disk_use_arg,
                    ulong *ptr_binlog_cache_use_arg,
                    ulong *ptr_binlog_cache_disk_use_arg)
      : stmt_cache(*this, false, ptr_binlog_stmt_cache_use_arg,
                   ptr_binlog_stmt_cache_disk_use_arg),
        trx_cache(*this, true, ptr_binlog_cache_use_arg,
                  ptr_binlog_cache_disk_use_arg) {}

  bool init();

  binlog_cache_data *get_binlog_cache_data(bool is_transactional) {
    if (is_transactional)
      return &trx_cache;
    else
      return &stmt_cache;
  }

  Binlog_cache_storage *get_stmt_cache() { return stmt_cache.get_cache(); }
  Binlog_cache_storage *get_trx_cache() { return trx_cache.get_cache(); }
  /**
    Convenience method to check if both caches are empty.
   */
  bool is_binlog_empty() const {
    return stmt_cache.is_binlog_empty() && trx_cache.is_binlog_empty();
  }

  int handle_deferred_cache_write_incident(THD *thd);

  /// Check if either of the caches have an incident
  /// @see handle_deferred_cache_write_incident
  bool has_incident() const { return !m_incident.empty(); }

  void set_incident(std::string_view incident_message) {
    assert(!incident_message.empty());
    m_incident = incident_message;
  }

  /*
    clear stmt_cache and trx_cache if they are not empty
  */
  void reset();

#ifndef NDEBUG
  bool dbug_any_finalized() const {
    return stmt_cache.is_finalized() || trx_cache.is_finalized();
  }
#endif

  /**
    Convenience method to flush both caches to the binary log.

    @param thd           The session owning the caches being flushed.
    @param bytes_written Pointer to variable that will be set to the
                         number of bytes written for the flush.
    @param wrote_xid     Pointer to variable that will be set to @c
                         true if any XID event was written to the
                         binary log. Otherwise, the variable will not
                         be touched.
    @return Error code on error, zero if no error.
   */
  int flush(THD *thd, my_off_t *bytes_written, bool *wrote_xid);

  /**
    Check if at least one of transactions and statement binlog caches
    contains an empty transaction, other one is empty or contains an
    empty transaction.

    @return true  At least one of transactions and statement binlog
                  caches an empty transaction, other one is empty
                  or contains an empty transaction.
    @return false Otherwise.
  */
  bool has_empty_transaction();

  binlog_stmt_cache_data stmt_cache;
  binlog_trx_cache_data trx_cache;

 private:
  binlog_cache_mngr &operator=(const binlog_cache_mngr &info);
  binlog_cache_mngr(const binlog_cache_mngr &info);
};

#endif  // BINLOG_CACHE_DATA_H_INCLUDED

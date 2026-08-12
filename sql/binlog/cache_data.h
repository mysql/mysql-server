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
  */
  mysql::binlog::event::enum_binlog_checksum_alg checksum_trx_start() const {
    return m_checksum_trx_start;
  }

  /**
    Returns true when the events in this cache carry a checksum
    (see write_event).
  */
  bool is_checksum_computed() const {
    return m_checksum_trx_start !=
               mysql::binlog::event::BINLOG_CHECKSUM_ALG_UNDEF &&
           m_checksum_trx_start !=
               mysql::binlog::event::BINLOG_CHECKSUM_ALG_OFF;
  }

  void latch_large_trx_optimization() {
    if (!m_large_trx_optimization_latched) {
      m_large_trx_optimization_enabled =
          opt_binlog_large_transaction_optimization_enabled;
      m_large_trx_optimization_threshold =
          opt_binlog_large_transaction_optimization_threshold;
      m_large_trx_optimization_latched = true;
      /*
        Give the spill file a promotable (named) form only when the
        optimization is enabled for this transaction, so a disabled knob leaves
        no visible bolt_ files. This uses the same knob value captured here for
        the promotion decision, so naming and eligibility always agree. The
        reserved header region is applied regardless (see
        IO_CACHE_binlog_cache_storage::open); only the naming is gated.
      */
      m_cache.set_named_file(m_large_trx_optimization_enabled);
    }
  }

  bool large_trx_optimization_enabled() const {
    return m_large_trx_optimization_enabled;
  }

  ulonglong large_trx_optimization_threshold() const {
    return m_large_trx_optimization_threshold;
  }

  /**
    Returns the offset and type of the transaction's terminating event in a
    promoted binary log file. Both values are recorded by finalize().
  */
  my_off_t terminating_event_offset() const {
    return m_terminating_event_offset;
  }
  mysql::binlog::event::Log_event_type terminating_event_type() const {
    return m_terminating_event_type;
  }

  my_off_t get_byte_position() const { return m_cache.length(); }

  void cache_state_checkpoint(my_off_t pos_to_checkpoint) {
    // We only need to store the cache state for pos > 0
    if (pos_to_checkpoint) {
      cache_state state;
      state.with_rbr = flags.with_rbr;
      state.with_sbr = flags.with_sbr;
      state.with_start = flags.with_start;
      state.with_end = flags.with_end;
      state.with_content = flags.with_content;
      state.event_counter = m_event_counter;
      cache_state_map[pos_to_checkpoint] = state;
    }
  }

  void cache_state_rollback(my_off_t pos_to_rollback) {
    if (pos_to_rollback) {
      std::map<my_off_t, cache_state>::iterator it;
      it = cache_state_map.find(pos_to_rollback);
      if (it != cache_state_map.end()) {
        flags.with_rbr = it->second.with_rbr;
        flags.with_sbr = it->second.with_sbr;
        flags.with_start = it->second.with_start;
        flags.with_end = it->second.with_end;
        flags.with_content = it->second.with_content;
        m_event_counter = it->second.event_counter;
      } else
        assert(it == cache_state_map.end());
    }
    // Rolling back to pos == 0 means cleaning up the cache.
    else {
      flags.with_rbr = false;
      flags.with_sbr = false;
      flags.with_start = false;
      flags.with_end = false;
      flags.with_content = false;
      m_event_counter = 0;
    }
  }

  /**
     Reset the cache to unused state when the transaction is finished. It
     drops all data and clears the transaction flags. If the caller has
     promoted a spilled file, preserve_spilled_file retains that file while
     resetting the cache.
  */
  virtual void reset(bool preserve_spilled_file = false) {
    compute_statistics();
    remove_pending_event();

    if (m_cache.reset(preserve_spilled_file)) {
      LogErr(WARNING_LEVEL, ER_BINLOG_CANT_RESIZE_CACHE);
    }

    flags.with_xid = false;
    flags.immediate = false;
    flags.finalized = false;
    flags.with_sbr = false;
    flags.with_rbr = false;
    flags.with_start = false;
    flags.with_end = false;
    flags.with_content = false;

    /*
      The truncate function calls reinit_io_cache that calls my_b_flush_io_cache
      which may increase disk_writes. This breaks the disk_writes use by the
      binary log which aims to compute the ratio between in-memory cache usage
      and disk cache usage. To avoid this undesirable behavior, we reset the
      variable after truncating the cache.
    */
    cache_state_map.clear();
    m_event_counter = 0;
    m_checksum_trx_start = mysql::binlog::event::BINLOG_CHECKSUM_ALG_UNDEF;
    m_large_trx_optimization_latched = false;
    m_large_trx_optimization_enabled = false;
    m_large_trx_optimization_threshold = 0;
    m_terminating_event_offset = 0;
    m_terminating_event_type = mysql::binlog::event::UNKNOWN_EVENT;
    m_compressed_size = 0;
    m_decompressed_size = 0;
    m_compression_type = mysql::binlog::event::compression::NONE;
    assert(is_binlog_empty());
  }

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
  bool has_empty_transaction() {
    /*
      The empty transaction has two events in trx/stmt binlog cache
      and no changes: one is a transaction start and other is a transaction
      end (there should be no SBR changing content and no RBR events).
    */
    if (flags.with_start &&   // Has transaction start statement
        flags.with_end &&     // Has transaction end statement
        !flags.with_content)  // Has no other content than START/END
    {
      assert(m_event_counter == 2);  // Two events in the cache only
      assert(!flags.with_sbr);       // No statements changing content
      assert(!flags.with_rbr);       // No rows changing content
      assert(!flags.immediate);      // Not a DDL
      assert(!flags.with_xid);  // Not a XID trx and not an atomic DDL Query
      return true;
    }
    return false;
  }

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

  /*
    Checksum algorithm the events of this cache are serialized with,
    recorded at the transaction's first event (see write_event).
  */
  mysql::binlog::event::enum_binlog_checksum_alg m_checksum_trx_start =
      mysql::binlog::event::BINLOG_CHECKSUM_ALG_UNDEF;

  bool m_large_trx_optimization_latched = false;
  bool m_large_trx_optimization_enabled = false;
  ulonglong m_large_trx_optimization_threshold = 0;

  /*
    Offset and type of the transaction's terminating event in a promoted
    binary log file, recorded together at finalize().
  */
  my_off_t m_terminating_event_offset = 0;
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
  void truncate(my_off_t pos) {
    DBUG_PRINT("info", ("truncating to position %lu", (ulong)pos));
    remove_pending_event();

    // TODO: check the return value.
    (void)m_cache.truncate(pos);
  }

  /**
     Flush pending event to the cache buffer.
   */
  int flush_pending_event(THD *thd) {
    if (m_pending) {
      m_pending->set_flags(Rows_log_event::STMT_END_F);
      if (int error = write_event(m_pending)) return error;
      thd->clear_binlog_table_maps();
    }
    return 0;
  }

  /**
    Remove the pending event.
   */
  int remove_pending_event() {
    delete m_pending;
    m_pending = nullptr;
    return 0;
  }
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
  void compute_statistics() {
    if (!is_binlog_empty()) {
      (*ptr_binlog_cache_use)++;
      if (m_cache.disk_writes() != 0) (*ptr_binlog_cache_disk_use)++;
    }
  }

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

  void reset(bool preserve_spilled_file = false) override {
    DBUG_TRACE;
    DBUG_PRINT("enter", ("before_stmt_pos: %llu", (ulonglong)before_stmt_pos));
    m_cannot_rollback = false;
    before_stmt_pos = MY_OFF_T_UNDEF;
    binlog_cache_data::reset(preserve_spilled_file);
    DBUG_PRINT("return", ("before_stmt_pos: %llu", (ulonglong)before_stmt_pos));
    return;
  }

  bool cannot_rollback() const { return m_cannot_rollback; }

  void set_cannot_rollback() { m_cannot_rollback = true; }

  my_off_t get_prev_position() const { return before_stmt_pos; }

  void set_prev_position(my_off_t pos) {
    DBUG_TRACE;
    DBUG_PRINT("enter", ("before_stmt_pos: %llu", (ulonglong)before_stmt_pos));
    before_stmt_pos = pos;
    cache_state_checkpoint(before_stmt_pos);
    DBUG_PRINT("return", ("before_stmt_pos: %llu", (ulonglong)before_stmt_pos));
    return;
  }

  void restore_prev_position() {
    DBUG_TRACE;
    DBUG_PRINT("enter", ("before_stmt_pos: %llu", (ulonglong)before_stmt_pos));
    binlog_cache_data::truncate(before_stmt_pos);
    cache_state_rollback(before_stmt_pos);
    before_stmt_pos = MY_OFF_T_UNDEF;
    /*
      Binlog statement rollback clears with_xid now as the atomic DDL statement
      marker which can be set as early as at event creation and caching.
    */
    flags.with_xid = false;
    DBUG_PRINT("return", ("before_stmt_pos: %llu", (ulonglong)before_stmt_pos));
    return;
  }

  void restore_savepoint(my_off_t pos) {
    DBUG_TRACE;
    DBUG_PRINT("enter", ("before_stmt_pos: %llu", (ulonglong)before_stmt_pos));
    binlog_cache_data::truncate(pos);
    if (pos <= before_stmt_pos) before_stmt_pos = MY_OFF_T_UNDEF;
    cache_state_rollback(pos);
    DBUG_PRINT("return", ("before_stmt_pos: %llu", (ulonglong)before_stmt_pos));
    return;
  }

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

  bool init() {
    return stmt_cache.open(binlog_stmt_cache_size,
                           max_binlog_stmt_cache_size) ||
           trx_cache.open(binlog_cache_size, max_binlog_cache_size);
  }

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
  void reset() {
    if (!stmt_cache.is_binlog_empty()) stmt_cache.reset();
    if (!trx_cache.is_binlog_empty()) trx_cache.reset();
  }

#ifndef NDEBUG
  bool dbug_any_finalized() const {
    return stmt_cache.is_finalized() || trx_cache.is_finalized();
  }
#endif

  /*
    Convenience method to flush both caches to the binary log.

    @param bytes_written Pointer to variable that will be set to the
                         number of bytes written for the flush.
    @param wrote_xid     Pointer to variable that will be set to @c
                         true if any XID event was written to the
                         binary log. Otherwise, the variable will not
                         be touched.
    @return Error code on error, zero if no error.
   */
  int flush(THD *thd, my_off_t *bytes_written, bool *wrote_xid) {
    my_off_t stmt_bytes = 0;
    my_off_t trx_bytes = 0;
    assert(stmt_cache.has_xid() == 0);

    bool parallelization_barrier = false;
    if (has_incident()) {
      if (int error = handle_deferred_cache_write_incident(thd)) return error;
      // Request force rotate
      thd->rpl_thd_ctx.binlog_group_commit_ctx().set_force_rotate();
      // Set as parallelization_barrier so that dependency tracker marks all
      // subsequent transactions to depend on it.
      parallelization_barrier = true;
    }

    int error =
        stmt_cache.flush(thd, &stmt_bytes, wrote_xid, parallelization_barrier);
    if (error) return error;
    DEBUG_SYNC(thd, "after_flush_stm_cache_before_flush_trx_cache");
    error =
        trx_cache.flush(thd, &trx_bytes, wrote_xid, parallelization_barrier);
    if (error) return error;
    *bytes_written = stmt_bytes + trx_bytes;
    return 0;
  }

  /**
    Check if at least one of transactions and statement binlog caches
    contains an empty transaction, other one is empty or contains an
    empty transaction.

    @return true  At least one of transactions and statement binlog
                  caches an empty transaction, other one is empty
                  or contains an empty transaction.
    @return false Otherwise.
  */
  bool has_empty_transaction() {
    return (trx_cache.is_empty_or_has_empty_transaction() &&
            stmt_cache.is_empty_or_has_empty_transaction() &&
            !is_binlog_empty());
  }

  binlog_stmt_cache_data stmt_cache;
  binlog_trx_cache_data trx_cache;

 private:
  binlog_cache_mngr &operator=(const binlog_cache_mngr &info);
  binlog_cache_mngr(const binlog_cache_mngr &info);
};

#endif  // BINLOG_CACHE_DATA_H_INCLUDED

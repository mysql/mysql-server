/* Copyright (c) 2009, 2018, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   51 Franklin Street, Suite 500, Boston, MA 02110-1335 USA */

#include "binlog.h"

#include "my_stacktrace.h"                  // my_safe_print_system_time
#include "debug_sync.h"                     // DEBUG_SYNC
#include "log.h"                            // sql_print_warning
#include "log_event.h"                      // Rows_log_event
#include "mysqld_thd_manager.h"             // Global_THD_manager
#include "rpl_handler.h"                    // RUN_HOOK
#include "rpl_mi.h"                         // Master_info
#include "rpl_rli.h"                        // Relay_log_info
#include "rpl_rli_pdb.h"                    // Slave_worker
#include "rpl_slave_commit_order_manager.h" // Commit_order_manager
#include "rpl_trx_boundary_parser.h"        // Transaction_boundary_parser
#include "rpl_context.h"
#include "sql_class.h"                      // THD
#include "sql_parse.h"                      // sqlcom_can_generate_row_events
#include "sql_show.h"                       // append_identifier

#include "pfs_file_provider.h"
#include "mysql/psi/mysql_file.h"

#include <pfs_transaction_provider.h>
#include <mysql/psi/mysql_transaction.h>
#include "xa.h"

#include <list>
#include <string>

using std::max;
using std::min;
using std::string;
using std::list;
using binary_log::checksum_crc32;
#define FLAGSTR(V,F) ((V)&(F)?#F" ":"")

#define LOG_PREFIX	"ML"

/**
  @defgroup Binary_Log Binary Log
  @{
 */

#define MY_OFF_T_UNDEF (~(my_off_t)0UL)

/*
  Constants required for the limit unsafe warnings suppression
 */
//seconds after which the limit unsafe warnings suppression will be activated
#define LIMIT_UNSAFE_WARNING_ACTIVATION_TIMEOUT 50
//number of limit unsafe warnings after which the suppression will be activated
#define LIMIT_UNSAFE_WARNING_ACTIVATION_THRESHOLD_COUNT 50
#define MAX_SESSION_ATTACH_TRIES 10

static ulonglong limit_unsafe_suppression_start_time= 0;
static bool unsafe_warning_suppression_is_activated= false;
static int limit_unsafe_warning_count= 0;

static handlerton *binlog_hton;
bool opt_binlog_order_commits= true;

const char *log_bin_index= 0;
const char *log_bin_basename= 0;

MYSQL_BIN_LOG mysql_bin_log(&sync_binlog_period, WRITE_CACHE);

static int binlog_init(void *p);
static int binlog_start_trans_and_stmt(THD *thd, Log_event *start_event);
static int binlog_close_connection(handlerton *hton, THD *thd);
static int binlog_savepoint_set(handlerton *hton, THD *thd, void *sv);
static int binlog_savepoint_rollback(handlerton *hton, THD *thd, void *sv);
static bool binlog_savepoint_rollback_can_release_mdl(handlerton *hton,
                                                      THD *thd);
static int binlog_commit(handlerton *hton, THD *thd, bool all);
static int binlog_rollback(handlerton *hton, THD *thd, bool all);
static int binlog_prepare(handlerton *hton, THD *thd, bool all);
static int binlog_xa_commit(handlerton *hton,  XID *xid);
static int binlog_xa_rollback(handlerton *hton,  XID *xid);
static void exec_binlog_error_action_abort(const char* err_string);

/**
  Helper class to hold a mutex for the duration of the
  block.

  Eliminates the need for explicit unlocking of mutexes on, e.g.,
  error returns.  On passing a null pointer, the sentry will not do
  anything.
 */
class Mutex_sentry
{
public:
  Mutex_sentry(mysql_mutex_t *mutex)
    : m_mutex(mutex)
  {
    if (m_mutex)
      mysql_mutex_lock(mutex);
  }

  ~Mutex_sentry()
  {
    if (m_mutex)
      mysql_mutex_unlock(m_mutex);
#ifndef DBUG_OFF
    m_mutex= 0;
#endif
  }

private:
  mysql_mutex_t *m_mutex;

  // It's not allowed to copy this object in any way
  Mutex_sentry(Mutex_sentry const&);
  void operator=(Mutex_sentry const&);
};


/**
  Helper class to switch to a new thread and then go back to the previous one,
  when the object is destroyed using RAII.

  This class is used to temporarily switch to another session (THD
  structure). It will set up thread specific "globals" correctly
  so that the POSIX thread looks exactly like the session attached to.
  However, PSI_thread info is not touched as it is required to show
  the actual physial view in PFS instrumentation i.e., it should
  depict as the real thread doing the work instead of thread it switched
  to.

  On destruction, the original session (which is supplied to the
  constructor) will be re-attached automatically. For example, with
  this code, the value of @c current_thd will be the same before and
  after execution of the code.

  @code
  {
    for (int i = 0 ; i < count ; ++i)
    {
      // here we are attached to current_thd
      // [...]
      Thd_backup_and_restore switch_thd(current_thd, other_thd[i]);
      // [...]
      // here we are attached to other_thd[i]
      // [...]
    }
    // here we are attached to current_thd
  }
  @endcode

  @warning The class is not designed to be inherited from.
 */

#ifndef EMBEDDED_LIBRARY

class Thd_backup_and_restore
{
public:
  /**
    Try to attach the POSIX thread to a session.
    - This function attaches the POSIX thread to a session
    in MAX_SESSION_ATTACH_TRIES tries when encountering
    'out of memory' error, and terminates the server after
    failed in MAX_SESSION_ATTACH_TRIES tries.

    @param[in] backup_thd    The thd to restore to when object is destructed.
    @param[in] new_thd       The thd to attach to.
   */

  Thd_backup_and_restore(THD *backup_thd, THD *new_thd)
    : m_backup_thd(backup_thd), m_new_thd(new_thd),
      m_new_thd_old_real_id(new_thd->real_id)
  {
    DBUG_ASSERT(m_backup_thd != NULL && m_new_thd != NULL);
    // Reset the state of the current thd.
    m_backup_thd->restore_globals();
    int i= 0;
    /*
      Attach the POSIX thread to a session in MAX_SESSION_ATTACH_TRIES
      tries when encountering 'out of memory' error.
    */
    while (i < MAX_SESSION_ATTACH_TRIES)
    {
      /*
        Currently attach_to(...) returns ER_OUTOFMEMORY or 0. So
        we continue to attach the POSIX thread when encountering
        the ER_OUTOFMEMORY error. Please take care other error
        returned from attach_to(...) in future.
      */
      if (!attach_to(new_thd))
      {
        if (i > 0)
          sql_print_warning("Server overcomes the temporary 'out of memory' "
                            "in '%d' tries while attaching to session thread "
                            "during the group commit phase.\n", i + 1);
        break;
      }
      /* Sleep 1 microsecond per try to avoid temporary 'out of memory' */
      my_sleep(1);
      i++;
    }
    /*
      Terminate the server after failed to attach the POSIX thread
      to a session in MAX_SESSION_ATTACH_TRIES tries.
    */
    if (MAX_SESSION_ATTACH_TRIES == i)
    {
      my_safe_print_system_time();
      my_safe_printf_stderr("%s", "[Fatal] Out of memory while attaching to "
                            "session thread during the group commit phase. "
                            "Data consistency between master and slave can "
                            "be guaranteed after server restarts.\n");
      _exit(MYSQLD_FAILURE_EXIT);
    }
  }

  /**
      Restores to previous thd.
   */
  ~Thd_backup_and_restore()
  {
    /*
      Restore the global variables of the thd we previously attached to,
      to its original state. In other words, detach the m_new_thd.
    */
    m_new_thd->restore_globals();
    m_new_thd->real_id= m_new_thd_old_real_id;

    // Reset the global variables to the original state.
    if (unlikely(m_backup_thd->store_globals()))
      DBUG_ASSERT(0);                           // Out of memory?!
  }

private:

  /**
    Attach the POSIX thread to a session.
   */
  int attach_to(THD *thd)
  {
    if (DBUG_EVALUATE_IF("simulate_session_attach_error", 1, 0)
        || unlikely(thd->store_globals()))
    {
      /*
        Indirectly uses pthread_setspecific, which can only return
        ENOMEM or EINVAL. Since store_globals are using correct keys,
        the only alternative is out of memory.
      */
      return ER_OUTOFMEMORY;
    }
    return 0;
  }

  THD *m_backup_thd;
  THD *m_new_thd;
  my_thread_t m_new_thd_old_real_id;
};

#endif /* !EMBEDDED_LIBRARY */

/**
  Caches for non-transactional and transactional data before writing
  it to the binary log.

  @todo All the access functions for the flags suggest that the
  encapsuling is not done correctly, so try to move any logic that
  requires access to the flags into the cache.
*/
class binlog_cache_data
{
public:

  binlog_cache_data(bool trx_cache_arg,
                    my_off_t max_binlog_cache_size_arg,
                    ulong *ptr_binlog_cache_use_arg,
                    ulong *ptr_binlog_cache_disk_use_arg,
                    const IO_CACHE &cache_log_arg)
  : cache_log(cache_log_arg),
    m_pending(0),
    saved_max_binlog_cache_size(max_binlog_cache_size_arg),
    ptr_binlog_cache_use(ptr_binlog_cache_use_arg),
    ptr_binlog_cache_disk_use(ptr_binlog_cache_disk_use_arg)
  {
    reset();
    flags.transactional= trx_cache_arg;
    cache_log.end_of_file= saved_max_binlog_cache_size;
  }

  int finalize(THD *thd, Log_event *end_event);
  int finalize(THD *thd, Log_event *end_event, XID_STATE *xs);
  int flush(THD *thd, my_off_t *bytes, bool *wrote_xid);
  int write_event(THD *thd, Log_event *event);

  virtual ~binlog_cache_data()
  {
    DBUG_ASSERT(is_binlog_empty());
    close_cached_file(&cache_log);
  }

  bool is_binlog_empty() const
  {
    my_off_t pos= my_b_tell(&cache_log);
    DBUG_PRINT("debug", ("%s_cache - pending: 0x%llx, bytes: %llu",
                         (flags.transactional ? "trx" : "stmt"),
                         (ulonglong) pending(), (ulonglong) pos));
    return pending() == NULL && pos == 0;
  }

  bool is_finalized() const {
    return flags.finalized;
  }

  Rows_log_event *pending() const
  {
    return m_pending;
  }

  void set_pending(Rows_log_event *const pending)
  {
    m_pending= pending;
  }

  void set_incident(void)
  {
    flags.incident= true;
  }

  bool has_incident(void) const
  {
    return flags.incident;
  }

  /**
    Sets the binlog_cache_data::Flags::flush_error flag if there
    is an error while flushing cache to the file.

    @param thd  The client thread that is executing the transaction.
  */
  void set_flush_error(THD *thd)
  {
    flags.flush_error= true;
    if(is_trx_cache())
    {
      /*
         If the cache is a transactional cache and if the write
         has failed due to ENOSPC, then my_write() would have
         set EE_WRITE error, so clear the error and create an
         equivalent server error.
      */
      if (thd->is_error())
        thd->clear_error();
      char errbuf[MYSYS_STRERROR_SIZE];
      my_error(ER_ERROR_ON_WRITE, MYF(MY_WME), my_filename(cache_log.file),
          errno, my_strerror(errbuf, sizeof(errbuf), errno));
    }
  }

  bool get_flush_error(void) const
  {
    return flags.flush_error;
  }

  bool has_xid() const {
    // There should only be an XID event if we are transactional
    DBUG_ASSERT((flags.transactional && flags.with_xid) || !flags.with_xid);
    return flags.with_xid;
  }

  bool is_trx_cache() const
  {
    return flags.transactional;
  }

  my_off_t get_byte_position() const
  {
    return my_b_tell(&cache_log);
  }

  void cache_state_rollback(my_off_t pos_to_rollback)
  {
    if (pos_to_rollback)
    {
      std::map<my_off_t,cache_state>::iterator it;
      it = cache_state_map.find(pos_to_rollback);
      if (it != cache_state_map.end())
      {
        flags.with_rbr= it->second.with_rbr;
        flags.with_sbr= it->second.with_sbr;
        flags.with_start= it->second.with_start;
        flags.with_end= it->second.with_end;
        flags.with_content= it->second.with_content;
      }
      else
        DBUG_ASSERT(it == cache_state_map.end());
    }
    // Rolling back to pos == 0 means cleaning up the cache.
    else
    {
      flags.with_rbr= false;
      flags.with_sbr= false;
      flags.with_start= false;
      flags.with_end= false;
      flags.with_content= false;
    }
  }

  void cache_state_checkpoint(my_off_t pos_to_checkpoint)
  {
    // We only need to store the cache state for pos > 0
    if (pos_to_checkpoint)
    {
      cache_state state;
      state.with_rbr= flags.with_rbr;
      state.with_sbr= flags.with_sbr;
      state.with_start= flags.with_start;
      state.with_end= flags.with_end;
      state.with_content= flags.with_content;
      cache_state_map[pos_to_checkpoint]= state;
    }
  }

  virtual void reset()
  {
    compute_statistics();
    truncate(0);

    /*
      If IOCACHE has a file associated, change its size to 0.
      It is safer to do it here, since we are certain that one
      asked the cache to go to position 0 with truncate.
    */
    if(cache_log.file != -1)
    {
      int error= 0;
      if((error= my_chsize(cache_log.file, 0, 0, MYF(MY_WME))))
        sql_print_warning("Unable to resize binlog IOCACHE auxilary file");

      DBUG_EXECUTE_IF("show_io_cache_size",
                      {
                        my_off_t file_size= my_seek(cache_log.file,
                                                    0L,MY_SEEK_END,MYF(MY_WME+MY_FAE));
                        sql_print_error("New size:%llu",
                                        static_cast<ulonglong>(file_size));
                      });
    }

    flags.incident= false;
    flags.with_xid= false;
    flags.immediate= false;
    flags.finalized= false;
    flags.with_sbr= false;
    flags.with_rbr= false;
    flags.with_start= false;
    flags.with_end= false;
    flags.with_content= false;
    flags.flush_error= false;

    /*
      The truncate function calls reinit_io_cache that calls my_b_flush_io_cache
      which may increase disk_writes. This breaks the disk_writes use by the
      binary log which aims to compute the ratio between in-memory cache usage
      and disk cache usage. To avoid this undesirable behavior, we reset the
      variable after truncating the cache.
    */
    cache_log.disk_writes= 0;
    cache_state_map.clear();
    DBUG_ASSERT(is_binlog_empty());
  }

  /*
    Sets the write position to point at the position given. If the
    cache has swapped to a file, it reinitializes it, so that the
    proper data is added to the IO_CACHE buffer. Otherwise, it just
    does a my_b_seek.

    my_b_seek will not work if the cache has swapped, that's why
    we do this workaround.

    @param[IN]  pos the new write position.
    @param[IN]  use_reinit if the position should be reset resorting
                to reset_io_cache (which may issue a flush_io_cache 
                inside)

    @return The previous write position.
   */
  my_off_t reset_write_pos(my_off_t pos, bool use_reinit)
  {
    DBUG_ENTER("reset_write_pos");
    DBUG_ASSERT(cache_log.type == WRITE_CACHE);

    my_off_t oldpos= get_byte_position();

    if (use_reinit)
      reinit_io_cache(&cache_log, WRITE_CACHE, pos, 0, 0);
    else
      my_b_seek(&cache_log, pos);

    DBUG_RETURN(oldpos);
  }

  /*
    Cache to store data before copying it to the binary log.
  */
  IO_CACHE cache_log;

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
  bool may_have_sbr_stmts()
  {
    return flags.with_sbr || !flags.with_rbr;
  }

  /**
    Check if the binlog cache contains an empty transaction, which has
    two binlog events "BEGIN" and "COMMIT".

    @return true  The binlog cache contains an empty transaction.
    @return false Otherwise.
  */
  bool has_empty_transaction()
  {
    /*
      The empty transaction has two events in trx/stmt binlog cache
      and no changes (no SBR changing content and no RBR events).
      Other transaction should not have two events. So we can identify
      if this is an empty transaction by the event counter and the
      cache flags.
    */
    if (flags.with_start &&     // Has transaction start statement
            flags.with_end &&   // Has transaction end statement
            !flags.with_sbr &&  // No statements changing content
            !flags.with_rbr &&  // No rows changing content
            !flags.immediate && // Not a DDL
            !flags.with_xid &&  // Not a XID transaction and not an atomic DDL Query
            !flags.with_content)// Does not have any content
    {
      DBUG_ASSERT(!flags.with_sbr); // No statements changing content
      DBUG_ASSERT(!flags.with_rbr); // No rows changing content
      DBUG_ASSERT(!flags.immediate);// Not a DDL
      DBUG_ASSERT(!flags.with_xid); // Not a XID trx and not an atomic DDL Query

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
  bool is_empty_or_has_empty_transaction()
  {
    return is_binlog_empty() || has_empty_transaction();
  }

protected:
  /*
    This structure should have all cache variables/flags that should be restored
    when a ROLLBACK TO SAVEPOINT statement be executed.
  */
  struct cache_state
  {
    bool with_sbr;
    bool with_rbr;
    bool with_start;
    bool with_end;
    bool with_content;
  };
  /*
    For every SAVEPOINT used, we will store a cache_state for the current
    binlog cache position. So, if a ROLLBACK TO SAVEPOINT is used, we can
    restore the cache_state values after truncating the binlog cache.
  */
  std::map<my_off_t, cache_state> cache_state_map;

  /*
    It truncates the cache to a certain position. This includes deleting the
    pending event.
   */
  void truncate(my_off_t pos)
  {
    DBUG_PRINT("info", ("truncating to position %lu", (ulong) pos));
    remove_pending_event();
    /*
      Whenever there is an error while flushing cache to file,
      the local cache will not be in a normal state and the same
      cache cannot be used without facing an assert.
      So, clear the cache if there is a flush error.
    */
    reinit_io_cache(&cache_log, WRITE_CACHE, pos, 0, get_flush_error());
    cache_log.end_of_file= saved_max_binlog_cache_size;
  }

  /**
     Flush pending event to the cache buffer.
   */
  int flush_pending_event(THD *thd) {
    if (m_pending)
    {
      m_pending->set_flags(Rows_log_event::STMT_END_F);
      if (int error= write_event(thd, m_pending))
        return error;
      thd->clear_binlog_table_maps();
    }
    return 0;
  }

  /**
    Remove the pending event.
   */
  int remove_pending_event() {
    delete m_pending;
    m_pending= NULL;
    return 0;
  }
  struct Flags {
    /*
      Defines if this is either a trx-cache or stmt-cache, respectively, a
      transactional or non-transactional cache.
    */
    bool transactional:1;

    /*
      This indicates that some events did not get into the cache and most likely
      it is corrupted.
    */
    bool incident:1;

    /*
      This indicates that the cache should be written without BEGIN/END.
    */
    bool immediate:1;

    /*
      This flag indicates that the buffer was finalized and has to be
      flushed to disk.
     */
    bool finalized:1;

    /*
      This indicates that the cache contain an XID event.
     */
    bool with_xid:1;

    /*
      This indicates that the cache contain statements changing content.
    */
    bool with_sbr:1;

    /*
      This indicates that the cache contain RBR event changing content.
    */
    bool with_rbr:1;

    /*
      This indicates that the cache contain s transaction start statement.
    */
    bool with_start:1;

    /*
      This indicates that the cache contain a transaction end event.
    */
    bool with_end:1;

    /*
      This indicates that the cache contain content other than START/END.
    */
    bool with_content:1;

    /*
      This flag is set to 'true' when there is an error while flushing the
      I/O cache to file.
    */
    bool flush_error:1;
  } flags;

private:
  /*
    Pending binrows event. This event is the event where the rows are currently
    written.
   */
  Rows_log_event *m_pending;

  /**
    This function computes binlog cache and disk usage.
  */
  void compute_statistics()
  {
    if (!is_binlog_empty())
    {
      (*ptr_binlog_cache_use)++;
      if (cache_log.disk_writes != 0)
        (*ptr_binlog_cache_disk_use)++;
    }
  }

  /*
    Stores the values of maximum size of the cache allowed when this cache
    is configured. This corresponds to either
      . max_binlog_cache_size or max_binlog_stmt_cache_size.
  */
  my_off_t saved_max_binlog_cache_size;

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

  binlog_cache_data& operator=(const binlog_cache_data& info);
  binlog_cache_data(const binlog_cache_data& info);
};


class binlog_stmt_cache_data
  : public binlog_cache_data
{
public:
  binlog_stmt_cache_data(bool trx_cache_arg,
                        my_off_t max_binlog_cache_size_arg,
                        ulong *ptr_binlog_cache_use_arg,
                        ulong *ptr_binlog_cache_disk_use_arg,
                        const IO_CACHE &cache_log)
    : binlog_cache_data(trx_cache_arg,
                        max_binlog_cache_size_arg,
                        ptr_binlog_cache_use_arg,
                        ptr_binlog_cache_disk_use_arg,
                        cache_log)
  {
  }

  using binlog_cache_data::finalize;

  int finalize(THD *thd);
};


int
binlog_stmt_cache_data::finalize(THD *thd)
{
  if (flags.immediate)
  {
    if (int error= finalize(thd, NULL))
      return error;
  }
  else
  {
    Query_log_event
      end_evt(thd, STRING_WITH_LEN("COMMIT"), false, false, true, 0, true);
    if (int error= finalize(thd, &end_evt))
      return error;
  }
  return 0;
}


class binlog_trx_cache_data : public binlog_cache_data
{
public:
  binlog_trx_cache_data(bool trx_cache_arg,
                        my_off_t max_binlog_cache_size_arg,
                        ulong *ptr_binlog_cache_use_arg,
                        ulong *ptr_binlog_cache_disk_use_arg,
                        const IO_CACHE &cache_log)
  : binlog_cache_data(trx_cache_arg,
                      max_binlog_cache_size_arg,
                      ptr_binlog_cache_use_arg,
                      ptr_binlog_cache_disk_use_arg,
                      cache_log),
    m_cannot_rollback(FALSE), before_stmt_pos(MY_OFF_T_UNDEF)
  {   }

  void reset()
  {
    DBUG_ENTER("reset");
    DBUG_PRINT("enter", ("before_stmt_pos: %llu", (ulonglong) before_stmt_pos));
    m_cannot_rollback= FALSE;
    before_stmt_pos= MY_OFF_T_UNDEF;
    binlog_cache_data::reset();
    DBUG_PRINT("return", ("before_stmt_pos: %llu", (ulonglong) before_stmt_pos));
    DBUG_VOID_RETURN;
  }

  bool cannot_rollback() const
  {
    return m_cannot_rollback;
  }

  void set_cannot_rollback()
  {
    m_cannot_rollback= TRUE;
  }

  my_off_t get_prev_position() const
  {
     return before_stmt_pos;
  }

  void set_prev_position(my_off_t pos)
  {
    DBUG_ENTER("set_prev_position");
    DBUG_PRINT("enter", ("before_stmt_pos: %llu", (ulonglong) before_stmt_pos));
    before_stmt_pos= pos;
    cache_state_checkpoint(before_stmt_pos);
    DBUG_PRINT("return", ("before_stmt_pos: %llu", (ulonglong) before_stmt_pos));
    DBUG_VOID_RETURN;
  }

  void restore_prev_position()
  {
    DBUG_ENTER("restore_prev_position");
    DBUG_PRINT("enter", ("before_stmt_pos: %llu", (ulonglong) before_stmt_pos));
    binlog_cache_data::truncate(before_stmt_pos);
    cache_state_rollback(before_stmt_pos);
    before_stmt_pos= MY_OFF_T_UNDEF;
    DBUG_PRINT("return", ("before_stmt_pos: %llu", (ulonglong) before_stmt_pos));
    DBUG_VOID_RETURN;
  }

  void restore_savepoint(my_off_t pos)
  {
    DBUG_ENTER("restore_savepoint");
    DBUG_PRINT("enter", ("before_stmt_pos: %llu", (ulonglong) before_stmt_pos));
    binlog_cache_data::truncate(pos);
    if (pos <= before_stmt_pos)
      before_stmt_pos= MY_OFF_T_UNDEF;
    cache_state_rollback(pos);
    DBUG_PRINT("return", ("before_stmt_pos: %llu", (ulonglong) before_stmt_pos));
    DBUG_VOID_RETURN;
  }

  using binlog_cache_data::truncate;

  int truncate(THD *thd, bool all);

private:
  /*
    It will be set TRUE if any statement which cannot be rolled back safely
    is put in trx_cache.
  */
  bool m_cannot_rollback;

  /*
    Binlog position before the start of the current statement.
  */
  my_off_t before_stmt_pos;

  binlog_trx_cache_data& operator=(const binlog_trx_cache_data& info);
  binlog_trx_cache_data(const binlog_trx_cache_data& info);
};

class binlog_cache_mngr {
public:
  binlog_cache_mngr(my_off_t max_binlog_stmt_cache_size_arg,
                    ulong *ptr_binlog_stmt_cache_use_arg,
                    ulong *ptr_binlog_stmt_cache_disk_use_arg,
                    my_off_t max_binlog_cache_size_arg,
                    ulong *ptr_binlog_cache_use_arg,
                    ulong *ptr_binlog_cache_disk_use_arg,
                    const IO_CACHE &stmt_cache_log,
                    const IO_CACHE &trx_cache_log)
  : stmt_cache(FALSE, max_binlog_stmt_cache_size_arg,
               ptr_binlog_stmt_cache_use_arg,
               ptr_binlog_stmt_cache_disk_use_arg,
               stmt_cache_log),
    trx_cache(TRUE, max_binlog_cache_size_arg,
              ptr_binlog_cache_use_arg,
              ptr_binlog_cache_disk_use_arg,
              trx_cache_log),
    has_logged_xid(NULL)
  {  }

  binlog_cache_data* get_binlog_cache_data(bool is_transactional)
  {
    if (is_transactional)
      return &trx_cache;
    else
      return &stmt_cache;
  }

  IO_CACHE* get_binlog_cache_log(bool is_transactional)
  {
    return (is_transactional ? &trx_cache.cache_log : &stmt_cache.cache_log);
  }

  /**
    Convenience method to check if both caches are empty.
   */
  bool is_binlog_empty() const {
    return stmt_cache.is_binlog_empty() && trx_cache.is_binlog_empty();
  }

  /*
    clear stmt_cache and trx_cache if they are not empty
  */
  void reset()
  {
    if (!stmt_cache.is_binlog_empty())
      stmt_cache.reset();
    if (!trx_cache.is_binlog_empty())
      trx_cache.reset();
  }

#ifndef DBUG_OFF
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
  int flush(THD *thd, my_off_t *bytes_written, bool *wrote_xid)
  {
    my_off_t stmt_bytes= 0;
    my_off_t trx_bytes= 0;
    DBUG_ASSERT(stmt_cache.has_xid() == 0);
    int error= stmt_cache.flush(thd, &stmt_bytes, wrote_xid);
    if (error)
      return error;
    DEBUG_SYNC(thd, "after_flush_stm_cache_before_flush_trx_cache");
    if (int error= trx_cache.flush(thd, &trx_bytes, wrote_xid))
      return error;
    *bytes_written= stmt_bytes + trx_bytes;
    return 0;
  }

  /**
    Check if at least one of transacaction and statement binlog caches
    contains an empty transaction, other one is empty or contains an
    empty transaction.

    @return true  At least one of transacaction and statement binlog
                  caches an empty transaction, other one is emptry
                  or contains an empty transaction.
    @return false Otherwise.
  */
  bool has_empty_transaction()
  {
    return (trx_cache.is_empty_or_has_empty_transaction() &&
            stmt_cache.is_empty_or_has_empty_transaction() &&
            !is_binlog_empty());
  }

  binlog_stmt_cache_data stmt_cache;
  binlog_trx_cache_data trx_cache;
  /*
    The bool flag is for preventing do_binlog_xa_commit_rollback()
    execution twice which can happen for "external" xa commit/rollback.
  */
  bool has_logged_xid;
private:

  binlog_cache_mngr& operator=(const binlog_cache_mngr& info);
  binlog_cache_mngr(const binlog_cache_mngr& info);
};


static binlog_cache_mngr *thd_get_cache_mngr(const THD *thd)
{
  /*
    If opt_bin_log is not set, binlog_hton->slot == -1 and hence
    thd_get_ha_data(thd, hton) segfaults.
  */
  DBUG_ASSERT(opt_bin_log);
  return (binlog_cache_mngr *)thd_get_ha_data(thd, binlog_hton);
}


/**
  Checks if the BINLOG_CACHE_SIZE's value is greater than MAX_BINLOG_CACHE_SIZE.
  If this happens, the BINLOG_CACHE_SIZE is set to MAX_BINLOG_CACHE_SIZE.
*/
void check_binlog_cache_size(THD *thd)
{
  if (binlog_cache_size > max_binlog_cache_size)
  {
    if (thd)
    {
      push_warning_printf(thd, Sql_condition::SL_WARNING,
                          ER_BINLOG_CACHE_SIZE_GREATER_THAN_MAX,
                          ER(ER_BINLOG_CACHE_SIZE_GREATER_THAN_MAX),
                          (ulong) binlog_cache_size,
                          (ulong) max_binlog_cache_size);
    }
    else
    {
      sql_print_warning(ER_DEFAULT(ER_BINLOG_CACHE_SIZE_GREATER_THAN_MAX),
                        binlog_cache_size,
                        (ulong) max_binlog_cache_size);
    }
    binlog_cache_size= static_cast<ulong>(max_binlog_cache_size);
  }
}

/**
  Checks if the BINLOG_STMT_CACHE_SIZE's value is greater than MAX_BINLOG_STMT_CACHE_SIZE.
  If this happens, the BINLOG_STMT_CACHE_SIZE is set to MAX_BINLOG_STMT_CACHE_SIZE.
*/
void check_binlog_stmt_cache_size(THD *thd)
{
  if (binlog_stmt_cache_size > max_binlog_stmt_cache_size)
  {
    if (thd)
    {
      push_warning_printf(thd, Sql_condition::SL_WARNING,
                          ER_BINLOG_STMT_CACHE_SIZE_GREATER_THAN_MAX,
                          ER(ER_BINLOG_STMT_CACHE_SIZE_GREATER_THAN_MAX),
                          (ulong) binlog_stmt_cache_size,
                          (ulong) max_binlog_stmt_cache_size);
    }
    else
    {
      sql_print_warning(ER_DEFAULT(ER_BINLOG_STMT_CACHE_SIZE_GREATER_THAN_MAX),
                        binlog_stmt_cache_size,
                        (ulong) max_binlog_stmt_cache_size);
    }
    binlog_stmt_cache_size= static_cast<ulong>(max_binlog_stmt_cache_size);
  }
}

/**
 Check whether binlog_hton has valid slot and enabled
*/
bool binlog_enabled()
{
	return(binlog_hton && binlog_hton->slot != HA_SLOT_UNDEF);
}

 /*
  Save position of binary log transaction cache.

  SYNPOSIS
    binlog_trans_log_savepos()

    thd      The thread to take the binlog data from
    pos      Pointer to variable where the position will be stored

  DESCRIPTION

    Save the current position in the binary log transaction cache into
    the variable pointed to by 'pos'
 */

static void
binlog_trans_log_savepos(THD *thd, my_off_t *pos)
{
  DBUG_ENTER("binlog_trans_log_savepos");
  DBUG_ASSERT(pos != NULL);
  binlog_cache_mngr *const cache_mngr= thd_get_cache_mngr(thd);
  DBUG_ASSERT(mysql_bin_log.is_open());
  *pos= cache_mngr->trx_cache.get_byte_position();
  DBUG_PRINT("return", ("position: %lu", (ulong) *pos));
  cache_mngr->trx_cache.cache_state_checkpoint(*pos);
  DBUG_VOID_RETURN;
}

static int binlog_dummy_recover(handlerton *hton, XID *xid, uint len)
{
  return 0;
}

/**
  Auxiliary class to copy serialized events to the binary log and
  correct some of the fields that are not known until just before
  writing the event.

  This class allows feeding events in parts, so it is practical to use
  in do_write_cache() which reads events from an IO_CACHE where events
  may span mutiple cache pages.

  The following fields are fixed before writing the event:
  - end_log_pos is set
  - the checksum is computed if checksums are enabled
  - the length is incremented by the checksum size if checksums are enabled
*/
class Binlog_event_writer
{
  IO_CACHE *output_cache;
  bool have_checksum;
  ha_checksum initial_checksum;
  ha_checksum checksum;
  uint32 end_log_pos;

public:
  /**
    Constructs a new Binlog_event_writer. Should be called once before
    starting to flush the transaction or statement cache to the
    binlog.

    @param output_cache_arg IO_CACHE to write to.
    @param have_checksum_al
  */
  Binlog_event_writer(IO_CACHE *output_cache_arg)
    : output_cache(output_cache_arg),
      have_checksum(binlog_checksum_options !=
                    binary_log::BINLOG_CHECKSUM_ALG_OFF),
      initial_checksum(my_checksum(0L, NULL, 0)),
      checksum(initial_checksum),
      end_log_pos(my_b_tell(output_cache))
  {
    // Simulate checksum error
    if (DBUG_EVALUATE_IF("fault_injection_crc_value", 1, 0))
      checksum--;
  }

  /**
    Write part of an event to disk.

    @param buf_p[IN,OUT] Points to buffer with data to write.  The
    caller must set this initially, and it will be increased by the
    number of bytes written.

    @param buf_len_p[IN,OUT] Points to the remaining length of the
    buffer, i.e., from buf_p to the end of the buffer.  The caller
    must set this initially, and it will be decreased by the number of
    written bytes.

    @param event_len_p[IN,OUT] Points to the remaining length of the
    event, i.e., the size of the event minus what was already written.
    This must be initialized to zero by the caller, must be remembered
    by the caller between calls, and is updated by this function: when
    an event begins it is set to the length of the event, and for each
    call it is decreased by the number of written bytes.

    It is allowed that buf_len_p is less than event_len_p (i.e., event
    is only partial) and that event_len_p is less than buf_len_p
    (i.e., there is more than this event in the buffer).  This
    function will write as much as is available of one event, but
    never more than one.  It is required that buf_len_p >=
    LOG_EVENT_HEADER_LEN.

    @retval true Error, i.e., my_b_write failed.
    @retval false Success.
  */
  bool write_event_part(uchar **buf_p, uint32 *buf_len_p, uint32 *event_len_p)
  {
    DBUG_ENTER("Binlog_event_writer::write_event_part");

    if (*buf_len_p == 0)
      DBUG_RETURN(false);

    // This is the beginning of an event
    if (*event_len_p == 0)
    {
      // Caller must ensure that the first part of the event contains
      // a full event header.
      DBUG_ASSERT(*buf_len_p >= LOG_EVENT_HEADER_LEN);

      // Read event length
      *event_len_p= uint4korr(*buf_p + EVENT_LEN_OFFSET);

      // Increase end_log_pos
      end_log_pos+= *event_len_p;

      // Change event length if checksum is enabled
      if (have_checksum)
      {
        int4store(*buf_p + EVENT_LEN_OFFSET,
                  *event_len_p + BINLOG_CHECKSUM_LEN);
        // end_log_pos is shifted by the checksum length
        end_log_pos+= BINLOG_CHECKSUM_LEN;
      }

      // Store end_log_pos
      int4store(*buf_p + LOG_POS_OFFSET, end_log_pos);
    }

    // write the buffer
    uint32 write_bytes= std::min<uint32>(*buf_len_p, *event_len_p);
    DBUG_ASSERT(write_bytes > 0);
    if (my_b_write(output_cache, *buf_p, write_bytes))
      DBUG_RETURN(true);

    // update the checksum
    if (have_checksum)
      checksum= my_checksum(checksum, *buf_p, write_bytes);

    // Step positions.
    *buf_p+= write_bytes;
    *buf_len_p-= write_bytes;
    *event_len_p-= write_bytes;

    if (have_checksum)
    {
      // store checksum
      if (*event_len_p == 0)
      {
        char checksum_buf[BINLOG_CHECKSUM_LEN];
        int4store(checksum_buf, checksum);
        if (my_b_write(output_cache, checksum_buf, BINLOG_CHECKSUM_LEN))
          DBUG_RETURN(true);
        checksum= initial_checksum;
      }
    }

    DBUG_RETURN(false);
  }

  /**
    Write a full event to disk.

    This is a wrapper around write_event_part, which handles the
    special case where you have a complete event in the buffer.

    @param buf Buffer to write.
    @param buf_len Number of bytes to write.

    @retval true Error, i.e., my_b_write failed.
    @retval false Success.
  */
  bool write_full_event(uchar *buf, uint32 buf_len)
  {
    uint32 event_len_unused= 0;
    bool ret= write_event_part(&buf, &buf_len, &event_len_unused);
    DBUG_ASSERT(buf_len == 0);
    DBUG_ASSERT(event_len_unused == 0);
    return ret;
  }

};


/*
  this function is mostly a placeholder.
  conceptually, binlog initialization (now mostly done in MYSQL_BIN_LOG::open)
  should be moved here.
*/

static int binlog_init(void *p)
{
  binlog_hton= (handlerton *)p;
  binlog_hton->state=opt_bin_log ? SHOW_OPTION_YES : SHOW_OPTION_NO;
  binlog_hton->db_type=DB_TYPE_BINLOG;
  binlog_hton->savepoint_offset= sizeof(my_off_t);
  binlog_hton->close_connection= binlog_close_connection;
  binlog_hton->savepoint_set= binlog_savepoint_set;
  binlog_hton->savepoint_rollback= binlog_savepoint_rollback;
  binlog_hton->savepoint_rollback_can_release_mdl=
                                     binlog_savepoint_rollback_can_release_mdl;
  binlog_hton->commit= binlog_commit;
  binlog_hton->commit_by_xid= binlog_xa_commit;
  binlog_hton->rollback= binlog_rollback;
  binlog_hton->rollback_by_xid= binlog_xa_rollback;
  binlog_hton->prepare= binlog_prepare;
  binlog_hton->recover=binlog_dummy_recover;
  binlog_hton->flags= HTON_NOT_USER_SELECTABLE | HTON_HIDDEN;
  return 0;
}


static int binlog_deinit(void *p)
{
  /* Using binlog as TC after the binlog has been unloaded, won't work */
  if (tc_log == &mysql_bin_log)
    tc_log= NULL;
  binlog_hton= NULL;
  return 0;
}


static int binlog_close_connection(handlerton *hton, THD *thd)
{
  DBUG_ENTER("binlog_close_connection");
  binlog_cache_mngr *const cache_mngr= thd_get_cache_mngr(thd);
  DBUG_ASSERT(cache_mngr->is_binlog_empty());
  DBUG_PRINT("debug", ("Set ha_data slot %d to 0x%llx", binlog_hton->slot, (ulonglong) NULL));
  thd_set_ha_data(thd, binlog_hton, NULL);
  cache_mngr->~binlog_cache_mngr();
  my_free(cache_mngr);
  DBUG_RETURN(0);
}

int binlog_cache_data::write_event(THD *thd, Log_event *ev)
{
  DBUG_ENTER("binlog_cache_data::write_event");

  if (ev != NULL)
  {
    DBUG_EXECUTE_IF("simulate_disk_full_at_flush_pending",
                  {DBUG_SET("+d,simulate_file_write_error");});

    DBUG_EXECUTE_IF("simulate_tmpdir_partition_full",
                  {
                  static int count= -1;
                  count++;
                  if(count %4 == 3 && ev->get_type_code() ==
                      binary_log::WRITE_ROWS_EVENT)
                    DBUG_SET("+d,simulate_temp_file_write_error");
                  });
    if (ev->write(&cache_log) != 0)
    {
      DBUG_EXECUTE_IF("simulate_disk_full_at_flush_pending",
                      {
                        DBUG_SET("-d,simulate_file_write_error");
                        DBUG_SET("-d,simulate_disk_full_at_flush_pending");
                        /* 
                           after +d,simulate_file_write_error the local cache
                           is in unsane state. Since -d,simulate_file_write_error
                           revokes the first simulation do_write_cache()
                           can't be run without facing an assert.
                           So it's blocked with the following 2nd simulation:
                        */
                        DBUG_SET("+d,simulate_do_write_cache_failure");
                      });

      DBUG_EXECUTE_IF("simulate_temp_file_write_error",
                      {
                        DBUG_SET("-d,simulate_temp_file_write_error");
                      });
      /*
        If the flush has failed due to ENOSPC error, set the
        flush_error flag.
      */
      if (thd->is_error() && my_errno() == ENOSPC)
      {
        set_flush_error(thd);
      }
      DBUG_RETURN(1);
    }
    if (ev->get_type_code() == binary_log::XID_EVENT)
      flags.with_xid= true;
    if (ev->is_using_immediate_logging())
      flags.immediate= true;
    /* With respect to the event type being written */
    if (ev->is_sbr_logging_format())
      flags.with_sbr= true;
    if (ev->is_rbr_logging_format())
      flags.with_rbr= true;
#ifndef EMBEDDED_LIBRARY
    /* With respect to empty transactions */
    if (ev->starts_group())
      flags.with_start= true;
    if (ev->ends_group())
      flags.with_end= true;
    if ((!ev->starts_group() && !ev->ends_group())
        ||ev->get_type_code() == binary_log::VIEW_CHANGE_EVENT)
      flags.with_content= true;
#endif
  }
  DBUG_RETURN(0);
}

bool MYSQL_BIN_LOG::assign_automatic_gtids_to_flush_group(THD *first_seen)
{
  DBUG_ENTER("MYSQL_BIN_LOG::assign_automatic_gtids_to_flush_group");
  bool error= false;
  bool is_global_sid_locked= false;
  rpl_sidno locked_sidno= 0;

  for (THD *head= first_seen ; head ; head = head->next_to_commit)
  {
    DBUG_ASSERT(head->variables.gtid_next.type != UNDEFINED_GROUP);

    /* Generate GTID */
    if (head->variables.gtid_next.type == AUTOMATIC_GROUP)
    {
      if (!is_global_sid_locked)
      {
        global_sid_lock->rdlock();
        is_global_sid_locked= true;
      }
      if (gtid_state->generate_automatic_gtid(head,
              head->get_transaction()->get_rpl_transaction_ctx()->get_sidno(),
              head->get_transaction()->get_rpl_transaction_ctx()->get_gno(),
              &locked_sidno)
              != RETURN_STATUS_OK)
      {
        head->commit_error= THD::CE_FLUSH_ERROR;
        error= true;
      }
    }
    else
    {
      DBUG_PRINT("info", ("thd->variables.gtid_next.type=%d "
                          "thd->owned_gtid.sidno=%d",
                          head->variables.gtid_next.type,
                          head->owned_gtid.sidno));
      if (head->variables.gtid_next.type == GTID_GROUP)
        DBUG_ASSERT(head->owned_gtid.sidno > 0);
      else
      {
        DBUG_ASSERT(head->variables.gtid_next.type == ANONYMOUS_GROUP);
        DBUG_ASSERT(head->owned_gtid.sidno == THD::OWNED_SIDNO_ANONYMOUS);
      }
    }
  }

  if (locked_sidno > 0)
    gtid_state->unlock_sidno(locked_sidno);

  if (is_global_sid_locked)
    global_sid_lock->unlock();

  DBUG_RETURN(error);
}


/**
  Write the Gtid_log_event to the binary log (prior to writing the
  statement or transaction cache).

  @param thd Thread that is committing.
  @param cache_data The cache that is flushing.
  @param writer The event will be written to this Binlog_event_writer object.

  @retval false Success.
  @retval true Error.
*/
bool MYSQL_BIN_LOG::write_gtid(THD *thd, binlog_cache_data *cache_data,
                               Binlog_event_writer *writer)
{
  DBUG_ENTER("MYSQL_BIN_LOG::write_gtid");

  /*
    The GTID for the THD was assigned at
    assign_automatic_gtids_to_flush_group()
  */
  DBUG_ASSERT(thd->owned_gtid.sidno == THD::OWNED_SIDNO_ANONYMOUS ||
              thd->owned_gtid.sidno > 0);

  int64 sequence_number, last_committed;
  /* Generate logical timestamps for MTS */
  m_dependency_tracker.get_dependency(thd, sequence_number, last_committed);

  /*
    In case both the transaction cache and the statement cache are
    non-empty, both will be flushed in sequence and logged as
    different transactions. Then the second transaction must only
    be executed after the first one has committed. Therefore, we
    need to set last_committed for the second transaction equal to
    last_committed for the first transaction. This is done in
    binlog_cache_data::flush. binlog_cache_data::flush uses the
    condition trn_ctx->last_committed==SEQ_UNINIT to detect this
    situation, hence the need to set it here.
  */
  thd->get_transaction()->last_committed= SEQ_UNINIT;


  /*
    Generate and write the Gtid_log_event.
  */
  Gtid_log_event gtid_event(thd, cache_data->is_trx_cache(),
                            last_committed, sequence_number,
                            cache_data->may_have_sbr_stmts());
  uchar buf[Gtid_log_event::MAX_EVENT_LENGTH];
  uint32 buf_len= gtid_event.write_to_memory(buf);
  bool ret= writer->write_full_event(buf, buf_len);

  DBUG_RETURN(ret);
}


int MYSQL_BIN_LOG::gtid_end_transaction(THD *thd)
{
  DBUG_ENTER("MYSQL_BIN_LOG::gtid_end_transaction");

  DBUG_PRINT("info", ("query=%s", thd->query().str));

  if (thd->owned_gtid.sidno > 0)
  {
    DBUG_ASSERT(thd->variables.gtid_next.type == GTID_GROUP);

    if (!opt_bin_log || (thd->slave_thread && !opt_log_slave_updates))
    {
      /*
        If the binary log is disabled for this thread (either by
        log_bin=0 or sql_log_bin=0 or by log_slave_updates=0 for a
        slave thread), then the statement must not be written to the
        binary log.  In this case, we just save the GTID into the
        table directly.

        (This only happens for DDL, since DML will save the GTID into
        table and release ownership inside ha_commit_trans.)
      */
      if (gtid_state->save(thd) != 0)
      {
        gtid_state->update_on_rollback(thd);
        DBUG_RETURN(1);
      }
      else
        gtid_state->update_on_commit(thd);
    }
    else
    {
      /*
        If statement is supposed to be written to binlog, we write it
        to the binary log.  Inserting into table and releasing
        ownership will be done in the binlog commit handler.
      */

      /*
        thd->cache_mngr may be uninitialized if the first transaction
        executed by the client is empty.
      */
      if (thd->binlog_setup_trx_data())
        DBUG_RETURN(1);
      binlog_cache_data *cache_data= &thd_get_cache_mngr(thd)->trx_cache;

      // Generate BEGIN event
      Query_log_event qinfo(thd, STRING_WITH_LEN("BEGIN"), TRUE,
                            FALSE, TRUE, 0, TRUE);
      DBUG_ASSERT(!qinfo.is_using_immediate_logging());

      /*
        Write BEGIN event and then commit (which will generate commit
        event and Gtid_log_event)
      */
      DBUG_PRINT("debug", ("Writing to trx_cache"));
      if (cache_data->write_event(thd, &qinfo) ||
          mysql_bin_log.commit(thd, true))
        DBUG_RETURN(1);
    }
  }
  else if (thd->owned_gtid.sidno == THD::OWNED_SIDNO_ANONYMOUS ||
           /*
             A transaction with an empty owned gtid should call
             end_gtid_violating_transaction(...) to clear the
             flag thd->has_gtid_consistency_violatoin in case
             it is set. It missed the clear in ordered_commit,
             because its binlog transaction cache is empty.
           */
           thd->has_gtid_consistency_violation)

  {
    gtid_state->update_on_commit(thd);
  }
  else if (thd->variables.gtid_next.type == GTID_GROUP &&
           thd->owned_gtid.is_empty())
  {
    DBUG_ASSERT(thd->has_gtid_consistency_violation == false);
    gtid_state->update_on_commit(thd);
  }

  DBUG_RETURN(0);
}

/**
  This function finalizes the cache preparing for commit or rollback.

  The function just writes all the necessary events to the cache but
  does not flush the data to the binary log file. That is the role of
  the binlog_cache_data::flush function.

  @see binlog_cache_data::flush

  @param thd                The thread whose transaction should be flushed
  @param cache_data         Pointer to the cache
  @param end_ev             The end event either commit/rollback

  @return
    nonzero if an error pops up when flushing the cache.
*/
int
binlog_cache_data::finalize(THD *thd, Log_event *end_event)
{
  DBUG_ENTER("binlog_cache_data::finalize");
  if (!is_binlog_empty())
  {
    DBUG_ASSERT(!flags.finalized);
    if (int error= flush_pending_event(thd))
      DBUG_RETURN(error);
    if (int error= write_event(thd, end_event))
      DBUG_RETURN(error);
    flags.finalized= true;
    DBUG_PRINT("debug", ("flags.finalized: %s", YESNO(flags.finalized)));
  }
  DBUG_RETURN(0);
}


/**
   The method writes XA END query to XA-prepared transaction's cache
   and calls the "basic" finalize().

   @return error code, 0 success
*/

int binlog_cache_data::finalize(THD *thd, Log_event *end_event, XID_STATE *xs)
{
  int error= 0;
  char buf[XID::ser_buf_size];
  char query[sizeof("XA END") + 1 + sizeof(buf)];
  int qlen= sprintf(query, "XA END %s", xs->get_xid()->serialize(buf));
  Query_log_event qev(thd, query, qlen, true, false, true, 0);

  if ((error= write_event(thd, &qev)))
    return error;

  return finalize(thd, end_event);
}


/**
  Flush caches to the binary log.

  If the cache is finalized, the cache will be flushed to the binary
  log file. If the cache is not finalized, nothing will be done.

  If flushing fails for any reason, an error will be reported and the
  cache will be reset. Flushing can fail in two circumstances:

  - It was not possible to write the cache to the file. In this case,
    it does not make sense to keep the cache.

  - The cache was successfully written to disk but post-flush actions
    (such as binary log rotation) failed. In this case, the cache is
    already written to disk and there is no reason to keep it.

  @see binlog_cache_data::finalize
 */
int
binlog_cache_data::flush(THD *thd, my_off_t *bytes_written, bool *wrote_xid)
{
  /*
    Doing a commit or a rollback including non-transactional tables,
    i.e., ending a transaction where we might write the transaction
    cache to the binary log.

    We can always end the statement when ending a transaction since
    transactions are not allowed inside stored functions. If they
    were, we would have to ensure that we're not ending a statement
    inside a stored function.
  */
  DBUG_ENTER("binlog_cache_data::flush");
  DBUG_PRINT("debug", ("flags.finalized: %s", YESNO(flags.finalized)));
  int error= 0;
  if (flags.finalized)
  {
    my_off_t bytes_in_cache= my_b_tell(&cache_log);
    Transaction_ctx *trn_ctx= thd->get_transaction();

    DBUG_PRINT("debug", ("bytes_in_cache: %llu", bytes_in_cache));

    trn_ctx->sequence_number= mysql_bin_log.m_dependency_tracker.step();
    /*
      In case of two caches the transaction is split into two groups.
      The 2nd group is considered to be a successor of the 1st rather
      than to have a common commit parent with it.
      Notice that due to a simple method of detection that the current is
      the 2nd cache being flushed, the very first few transactions may be logged
      sequentially (a next one is tagged as if a preceding one is its
      commit parent).
    */
    if (trn_ctx->last_committed == SEQ_UNINIT)
      trn_ctx->last_committed= trn_ctx->sequence_number - 1;

    /*
      The GTID is written prior to flushing the statement cache, if
      the transaction has written to the statement cache; and prior to
      flushing the transaction cache if the transaction has written to
      the transaction cache.  If GTIDs are enabled, then transactional
      and non-transactional updates cannot be mixed, so at most one of
      the caches can be non-empty, so just one GTID will be
      generated. If GTIDs are disabled, then no GTID is generated at
      all; if both the transactional cache and the statement cache are
      non-empty then we get two Anonymous_gtid_log_events, which is
      correct.
    */
    Binlog_event_writer writer(mysql_bin_log.get_log_file());

    /* The GTID ownership process might set the commit_error */
    error= (thd->commit_error == THD::CE_FLUSH_ERROR);

    DBUG_EXECUTE_IF("simulate_binlog_flush_error",
                    {
                      if (rand() % 3 == 0)
                      {
                        thd->commit_error= THD::CE_FLUSH_ERROR;
                      }
                    };);

    if (!error)
      if ((error= mysql_bin_log.write_gtid(thd, this, &writer)))
        thd->commit_error= THD::CE_FLUSH_ERROR;
    if (!error)
      error= mysql_bin_log.write_cache(thd, this, &writer);

    if (flags.with_xid && error == 0)
      *wrote_xid= true;

    /*
      Reset have to be after the if above, since it clears the
      with_xid flag
    */
    reset();
    if (bytes_written)
      *bytes_written= bytes_in_cache;
  }
  DBUG_ASSERT(!flags.finalized);
  DBUG_RETURN(error);
}

/**
  This function truncates the transactional cache upon committing or rolling
  back either a transaction or a statement.

  @param thd        The thread whose transaction should be flushed
  @param cache_mngr Pointer to the cache data to be flushed
  @param all        @c true means truncate the transaction, otherwise the
                    statement must be truncated.

  @return
    nonzero if an error pops up when truncating the transactional cache.
*/
int
binlog_trx_cache_data::truncate(THD *thd, bool all)
{
  DBUG_ENTER("binlog_trx_cache_data::truncate");
  int error=0;

  DBUG_PRINT("info", ("thd->options={ %s %s}, transaction: %s",
                      FLAGSTR(thd->variables.option_bits, OPTION_NOT_AUTOCOMMIT),
                      FLAGSTR(thd->variables.option_bits, OPTION_BEGIN),
                      all ? "all" : "stmt"));

  remove_pending_event();

  /*
    If rolling back an entire transaction or a single statement not
    inside a transaction, we reset the transaction cache.
  */
  if (ending_trans(thd, all))
  {
    if (has_incident())
    {
      const char* err_msg= "Error happend while resetting the transaction "
                           "cache for a rolled back transaction or a single "
                           "statement not inside a transaction.";
      error= mysql_bin_log.write_incident(thd, true/*need_lock_log=true*/,
                                          err_msg);
    }
    reset();
  }
  /*
    If rolling back a statement in a transaction, we truncate the
    transaction cache to remove the statement.
  */
  else if (get_prev_position() != MY_OFF_T_UNDEF)
    restore_prev_position();

  thd->clear_binlog_table_maps();

  DBUG_RETURN(error);
}


inline enum xa_option_words get_xa_opt(THD *thd)
{
  enum xa_option_words xa_opt= XA_NONE;
  switch(thd->lex->sql_command)
  {
  case SQLCOM_XA_COMMIT:
    xa_opt= static_cast<Sql_cmd_xa_commit*>(thd->lex->m_sql_cmd)->get_xa_opt();
    break;
  default:
    break;
  }

  return xa_opt;
}


/**
   Predicate function yields true when XA transaction is
   being logged having a proper state ready for prepare or
   commit in one phase.

   @param thd    THD pointer of running transaction
   @return true  When the being prepared transaction should be binlogged,
           false otherwise.
*/

inline bool is_loggable_xa_prepare(THD *thd)
{
  /*
    simulate_commit_failure is doing a trick with XID_STATE while
    the ongoing transaction is not XA, and therefore to be errored out,
    asserted below. In that case because of the
    latter fact the function returns @c false.
  */
  DBUG_EXECUTE_IF("simulate_commit_failure",
                  {
                    XID_STATE *xs= thd->get_transaction()->xid_state();
                    DBUG_ASSERT((thd->is_error() &&
                                 xs->get_state() == XID_STATE::XA_IDLE) ||
                                xs->get_state() == XID_STATE::XA_NOTR);
                  });

  return DBUG_EVALUATE_IF("simulate_commit_failure",
                          false,
                          thd->get_transaction()->xid_state()->
                          has_state(XID_STATE::XA_IDLE));
}

static int binlog_prepare(handlerton *hton, THD *thd, bool all)
{
  DBUG_ENTER("binlog_prepare");
  if (!all)
  {
    thd->get_transaction()->store_commit_parent(mysql_bin_log.
      m_dependency_tracker.get_max_committed_timestamp());

  }

  DBUG_RETURN(all && is_loggable_xa_prepare(thd) ?
              mysql_bin_log.commit(thd, true) : 0);
}


/**
   Logging XA commit/rollback of a prepared transaction.

   The function is called at XA-commit or XA-rollback logging via
   two paths: the recovered-or-slave-applier or immediately through
   the  XA-prepared transaction connection itself.
   It fills in appropiate event in the statement cache whenever
   xid state is marked with is_binlogged() flag that indicates
   the prepared part of the transaction must've been logged.

   About early returns from the function.
   In the recovered-or-slave-applier case the function may be called
   for the 2nd time, which has_logged_xid monitors.
   ONE_PHASE option to XA-COMMIT is handled to skip
   writing XA-commit event now.
   And the final early return check is for the read-only XA that is
   not to be logged.

   @param thd          THD handle
   @param xid          a pointer to XID object that is serialized
   @param commit       when @c true XA-COMMIT is to be logged,
                       and @c false when it's XA-ROLLBACK.
   @return error code, 0 success
*/

inline int do_binlog_xa_commit_rollback(THD *thd, XID *xid, bool commit)
{
  DBUG_ASSERT(thd->lex->sql_command == SQLCOM_XA_COMMIT ||
              thd->lex->sql_command == SQLCOM_XA_ROLLBACK);

  XID_STATE *xid_state= thd->get_transaction()->xid_state();
  binlog_cache_mngr *cache_mngr= thd_get_cache_mngr(thd);

  if (cache_mngr != NULL && cache_mngr->has_logged_xid)
    return 0;

  if (get_xa_opt(thd) == XA_ONE_PHASE)
    return 0;
  if (!xid_state->is_binlogged())
    return 0; // nothing was really logged at prepare
  if (thd->is_error() && DBUG_EVALUATE_IF("simulate_xa_rm_error", 0, 1))
    return 0; // don't binlog if there are some errors.

  DBUG_ASSERT(!xid->is_null() ||
              !(thd->variables.option_bits & OPTION_BIN_LOG));

  char buf[XID::ser_buf_size];
  char query[(sizeof("XA ROLLBACK")) + 1 + sizeof(buf)];
  int qlen= sprintf(query, "XA %s %s", commit ? "COMMIT" : "ROLLBACK",
                    xid->serialize(buf));
  Query_log_event qinfo(thd, query, qlen, false, true, true, 0, false);
  return mysql_bin_log.write_event(&qinfo);
}


/**
   Logging XA commit/rollback of a prepared transaction in the case
   it was disconnected and resumed (recovered), or executed by a slave applier.

   @param thd         THD handle
   @param xid         a pointer to XID object
   @param commit      when @c true XA-COMMIT is logged, otherwise XA-ROLLBACK

   @return error code, 0 success
*/

inline int binlog_xa_commit_or_rollback(THD *thd, XID *xid, bool commit)
{
  int error= 0;

#ifndef DBUG_OFF
  binlog_cache_mngr *cache_mngr= thd_get_cache_mngr(thd);
  DBUG_ASSERT(!cache_mngr || !cache_mngr->has_logged_xid);
#endif
  if (!(error= do_binlog_xa_commit_rollback(thd, xid, commit)))
  {
    /*
      Error can't be propagated naturally via result.
      A grand-caller has to access to it through thd's da.
      todo:
      Bug #20488921 ERROR PROPAGATION DOES FULLY WORK IN XA
      stands in the way of implementing a failure simulation
      for XA PREPARE/COMMIT/ROLLBACK.
    */
    binlog_cache_mngr *cache_mngr= thd_get_cache_mngr(thd);

    if (cache_mngr)
      cache_mngr->has_logged_xid= true;
    if (commit)
      (void) mysql_bin_log.commit(thd, true);
    else
      (void) mysql_bin_log.rollback(thd, true);
    if (cache_mngr)
      cache_mngr->has_logged_xid= false;
  }
  return error;
}


static int binlog_xa_commit(handlerton *hton,  XID *xid)
{
  (void) binlog_xa_commit_or_rollback(current_thd, xid, true);

  return 0;
}


static int binlog_xa_rollback(handlerton *hton,  XID *xid)
{
  (void) binlog_xa_commit_or_rollback(current_thd, xid, false);

  return 0;
}

/**
  When a fatal error occurs due to which binary logging becomes impossible and
  the user specified binlog_error_action= ABORT_SERVER the following function is
  invoked. This function pushes the appropriate error message to client and logs
  the same to server error log and then aborts the server.

  @param err_string          Error string which specifies the exact error
                             message from the caller.

  @retval
    none
*/
static void exec_binlog_error_action_abort(const char* err_string)
{
  THD *thd= current_thd;
  /*
    When the code enters here it means that there was an error at higher layer
    and my_error function could have been invoked to let the client know what
    went wrong during the execution.

    But these errors will not let the client know that the server is going to
    abort. Even if we add an additional my_error function call at this point
    client will be able to see only the first error message that was set
    during the very first invocation of my_error function call.

    The advantage of having multiple my_error function calls are visible when
    the server is up and running and user issues SHOW WARNINGS or SHOW ERROR
    calls. In this special scenario server will be immediately aborted and
    user will not be able execute the above SHOW commands.

    Hence we clear the previous errors and push one critical error message to
    clients.
   */
  if (thd)
  {
    if (thd->is_error())
      thd->clear_error();
    /*
      Adding ME_ERRORLOG flag will ensure that the error is sent to both
      client and to the server error log as well.
    */
    my_error(ER_BINLOG_LOGGING_IMPOSSIBLE, MYF(ME_ERRORLOG + ME_FATALERROR),
             err_string);
    thd->send_statement_status();
  }
  else
    sql_print_error("%s",err_string);
  abort();
}



/**
  This function is called once after each statement.

  @todo This function is currently not used any more and will
  eventually be eliminated. The real commit job is done in the
  MYSQL_BIN_LOG::commit function.

  @see MYSQL_BIN_LOG::commit

  @param hton  The binlog handlerton.
  @param thd   The client thread that executes the transaction.
  @param all   This is @c true if this is a real transaction commit, and
               @false otherwise.

  @see handlerton::commit
*/
static int binlog_commit(handlerton *hton, THD *thd, bool all)
{
  DBUG_ENTER("binlog_commit");
  /*
    Nothing to do (any more) on commit.
   */
  DBUG_RETURN(0);
}

/**
  This function is called when a transaction or a statement is rolled back.

  @internal It is necessary to execute a rollback here if the
  transaction was rolled back because of executing a ROLLBACK TO
  SAVEPOINT command, but it is not used for normal rollback since
  MYSQL_BIN_LOG::rollback is called in that case.

  @todo Refactor code to introduce a <code>MYSQL_BIN_LOG::rollback(THD
  *thd, SAVEPOINT *sv)</code> function in @c TC_LOG and have that
  function execute the necessary work to rollback to a savepoint.

  @param hton  The binlog handlerton.
  @param thd   The client thread that executes the transaction.
  @param all   This is @c true if this is a real transaction rollback, and
               @false otherwise.

  @see handlerton::rollback
*/
static int binlog_rollback(handlerton *hton, THD *thd, bool all)
{
  DBUG_ENTER("binlog_rollback");
  int error= 0;
  if (thd->lex->sql_command == SQLCOM_ROLLBACK_TO_SAVEPOINT)
    error= mysql_bin_log.rollback(thd, all);
  DBUG_RETURN(error);
}


bool
Stage_manager::Mutex_queue::append(THD *first)
{
  DBUG_ENTER("Stage_manager::Mutex_queue::append");
  lock();
  DBUG_PRINT("enter", ("first: 0x%llx", (ulonglong) first));
  DBUG_PRINT("info", ("m_first: 0x%llx, &m_first: 0x%llx, m_last: 0x%llx",
                       (ulonglong) m_first, (ulonglong) &m_first,
                       (ulonglong) m_last));
  int32 count= 1;
  bool empty= (m_first == NULL);
  *m_last= first;
  DBUG_PRINT("info", ("m_first: 0x%llx, &m_first: 0x%llx, m_last: 0x%llx",
                       (ulonglong) m_first, (ulonglong) &m_first,
                       (ulonglong) m_last));
  /*
    Go to the last THD instance of the list. We expect lists to be
    moderately short. If they are not, we need to track the end of
    the queue as well.
  */

  while (first->next_to_commit)
  {
    count++;
    first= first->next_to_commit;
  }
  my_atomic_add32(&m_size, count);

  m_last= &first->next_to_commit;
  DBUG_PRINT("info", ("m_first: 0x%llx, &m_first: 0x%llx, m_last: 0x%llx",
                        (ulonglong) m_first, (ulonglong) &m_first,
                        (ulonglong) m_last));
  DBUG_ASSERT(m_first || m_last == &m_first);
  DBUG_PRINT("return", ("empty: %s", YESNO(empty)));
  unlock();
  DBUG_RETURN(empty);
}


std::pair<bool, THD*>
Stage_manager::Mutex_queue::pop_front()
{
  DBUG_ENTER("Stage_manager::Mutex_queue::pop_front");
  lock();
  THD *result= m_first;
  bool more= true;
  /*
    We do not set next_to_commit to NULL here since this is only used
    in the flush stage. We will have to call fetch_queue last here,
    and will then "cut" the linked list by setting the end of that
    queue to NULL.
  */
  if (result)
    m_first= result->next_to_commit;
  if (m_first == NULL)
  {
    more= false;
    m_last = &m_first;
  }
  DBUG_ASSERT(my_atomic_load32(&m_size) > 0);
  my_atomic_add32(&m_size, -1);
  DBUG_ASSERT(m_first || m_last == &m_first);
  unlock();
  DBUG_PRINT("return", ("result: 0x%llx, more: %s",
                        (ulonglong) result, YESNO(more)));
  DBUG_RETURN(std::make_pair(more, result));
}


bool
Stage_manager::enroll_for(StageID stage, THD *thd, mysql_mutex_t *stage_mutex)
{
  // If the queue was empty: we're the leader for this batch
  DBUG_PRINT("debug", ("Enqueue 0x%llx to queue for stage %d",
                       (ulonglong) thd, stage));
  bool leader= m_queue[stage].append(thd);

#ifdef HAVE_REPLICATION
  if (stage == FLUSH_STAGE && has_commit_order_manager(thd))
  {
    Slave_worker *worker= dynamic_cast<Slave_worker *>(thd->rli_slave);
    Commit_order_manager *mngr= worker->get_commit_order_manager();

    mngr->unregister_trx(worker);
  }
#endif

  /*
    We do not need to unlock the stage_mutex if it is LOCK_log when rotating
    binlog caused by logging incident log event, since it should be held
    always during rotation.
  */
  bool need_unlock_stage_mutex=
    !(mysql_bin_log.is_rotating_caused_by_incident &&
      stage_mutex == mysql_bin_log.get_log_lock());

  /*
    The stage mutex can be NULL if we are enrolling for the first
    stage.
  */
  if (stage_mutex && need_unlock_stage_mutex)
    mysql_mutex_unlock(stage_mutex);

#ifndef DBUG_OFF
  DBUG_PRINT("info", ("This is a leader thread: %d (0=n 1=y)", leader));

  DEBUG_SYNC(thd, "after_enrolling_for_stage");

  switch (stage)
  {
  case Stage_manager::FLUSH_STAGE:
    DEBUG_SYNC(thd, "bgc_after_enrolling_for_flush_stage");
    break;
  case Stage_manager::SYNC_STAGE:
    DEBUG_SYNC(thd, "bgc_after_enrolling_for_sync_stage");
    break;
  case Stage_manager::COMMIT_STAGE:
    DEBUG_SYNC(thd, "bgc_after_enrolling_for_commit_stage");
    break;
  default:
    // not reached
    DBUG_ASSERT(0);
  }

  DBUG_EXECUTE_IF("assert_leader", DBUG_ASSERT(leader););
  DBUG_EXECUTE_IF("assert_follower", DBUG_ASSERT(!leader););
#endif

  /*
    If the queue was not empty, we're a follower and wait for the
    leader to process the queue. If we were holding a mutex, we have
    to release it before going to sleep.
  */
  if (!leader)
  {
    mysql_mutex_lock(&m_lock_done);
#ifndef DBUG_OFF
    /*
      Leader can be awaiting all-clear to preempt follower's execution.
      With setting the status the follower ensures it won't execute anything
      including thread-specific code.
    */
    thd->get_transaction()->m_flags.ready_preempt= 1;
    if (leader_await_preempt_status)
      mysql_cond_signal(&m_cond_preempt);
#endif
    while (thd->get_transaction()->m_flags.pending)
      mysql_cond_wait(&m_cond_done, &m_lock_done);
    mysql_mutex_unlock(&m_lock_done);
  }
  return leader;
}


THD *Stage_manager::Mutex_queue::fetch_and_empty()
{
  DBUG_ENTER("Stage_manager::Mutex_queue::fetch_and_empty");
  lock();
  DBUG_PRINT("enter", ("m_first: 0x%llx, &m_first: 0x%llx, m_last: 0x%llx",
                       (ulonglong) m_first, (ulonglong) &m_first,
                       (ulonglong) m_last));
  THD *result= m_first;
  m_first= NULL;
  m_last= &m_first;
  DBUG_PRINT("info", ("m_first: 0x%llx, &m_first: 0x%llx, m_last: 0x%llx",
                       (ulonglong) m_first, (ulonglong) &m_first,
                       (ulonglong) m_last));
  DBUG_PRINT("info", ("fetched queue of %d transactions", my_atomic_load32(&m_size)));
  DBUG_PRINT("return", ("result: 0x%llx", (ulonglong) result));
  DBUG_ASSERT(my_atomic_load32(&m_size) >= 0);
  my_atomic_store32(&m_size, 0);
  unlock();
  DBUG_RETURN(result);
}

void Stage_manager::wait_count_or_timeout(ulong count, long usec, StageID stage)
{
  long to_wait=
    DBUG_EVALUATE_IF("bgc_set_infinite_delay", LONG_MAX, usec);
  /*
    For testing purposes while waiting for inifinity
    to arrive, we keep checking the queue size at regular,
    small intervals. Otherwise, waiting 0.1 * infinite
    is too long.
   */
  long delta=
    DBUG_EVALUATE_IF("bgc_set_infinite_delay", 100000,
                     max<long>(1, (to_wait * 0.1)));

  while (to_wait > 0 && (count == 0 || static_cast<ulong>(m_queue[stage].get_size()) < count))
  {
#ifndef DBUG_OFF
    if (current_thd)
      DEBUG_SYNC(current_thd, "bgc_wait_count_or_timeout");
#endif
    my_sleep(delta);
    to_wait -= delta;
  }
}

void Stage_manager::signal_done(THD *queue)
{
  mysql_mutex_lock(&m_lock_done);
  for (THD *thd= queue ; thd ; thd = thd->next_to_commit)
    thd->get_transaction()->m_flags.pending= false;
  mysql_mutex_unlock(&m_lock_done);
  mysql_cond_broadcast(&m_cond_done);
}

#ifndef DBUG_OFF
void Stage_manager::clear_preempt_status(THD *head)
{
  DBUG_ASSERT(head);

  mysql_mutex_lock(&m_lock_done);
  while(!head->get_transaction()->m_flags.ready_preempt)
  {
    leader_await_preempt_status= true;
    mysql_cond_wait(&m_cond_preempt, &m_lock_done);
  }
  leader_await_preempt_status= false;
  mysql_mutex_unlock(&m_lock_done);
}
#endif

/**
  Write a rollback record of the transaction to the binary log.

  For binary log group commit, the rollback is separated into three
  parts:

  1. First part consists of filling the necessary caches and
     finalizing them (if they need to be finalized). After a cache is
     finalized, nothing can be added to the cache.

  2. Second part execute an ordered flush and commit. This will be
     done using the group commit functionality in @c ordered_commit.

     Since we roll back the transaction early, we call @c
     ordered_commit with the @c skip_commit flag set. The @c
     ha_commit_low call inside @c ordered_commit will then not be
     called.

  3. Third part checks any errors resulting from the flush and handles
     them appropriately.

  @see MYSQL_BIN_LOG::ordered_commit
  @see ha_commit_low
  @see ha_rollback_low

  @param thd Session to commit
  @param all This is @c true if this is a real transaction rollback, and
             @false otherwise.

  @return Error code, or zero if there were no error.
 */

int MYSQL_BIN_LOG::rollback(THD *thd, bool all)
{
  int error= 0;
  bool stuff_logged= false;
  binlog_cache_mngr *cache_mngr= thd_get_cache_mngr(thd);

  DBUG_ENTER("MYSQL_BIN_LOG::rollback(THD *thd, bool all)");
  DBUG_PRINT("enter", ("all: %s, cache_mngr: 0x%llx, thd->is_error: %s",
                       YESNO(all), (ulonglong) cache_mngr,
                       YESNO(thd->is_error())));
  /*
    Defer XA-transaction rollback until its XA-rollback event is recorded.
    When we are executing a ROLLBACK TO SAVEPOINT, we
    should only clear the caches since this function is called as part
    of the engine rollback.
    In other cases we roll back the transaction in the engines early
    since this will release locks and allow other transactions to
    start executing.
  */
  if (thd->lex->sql_command == SQLCOM_XA_ROLLBACK)
  {
    XID_STATE *xs= thd->get_transaction()->xid_state();

    DBUG_ASSERT(all || !xs->is_binlogged() ||
                (!xs->is_in_recovery() && thd->is_error()));
    /*
      Whenever cache_mngr is not initialized, the xa prepared
      transaction's binary logging status must not be set, unless the
      transaction is rolled back through an external connection which
      has binlogging switched off.
    */
    DBUG_ASSERT(cache_mngr || !xs->is_binlogged()
                || !(is_open() && thd->variables.option_bits & OPTION_BIN_LOG));

    if ((error= do_binlog_xa_commit_rollback(thd, xs->get_xid(), false)))
      goto end;
    cache_mngr= thd_get_cache_mngr(thd);
  }
  else if (thd->lex->sql_command != SQLCOM_ROLLBACK_TO_SAVEPOINT)
    if ((error= ha_rollback_low(thd, all)))
      goto end;

  /*
    If there is no cache manager, or if there is nothing in the
    caches, there are no caches to roll back, so we're trivially done
    unless XA-ROLLBACK that yet to run rollback_low().
  */
  if (cache_mngr == NULL || cache_mngr->is_binlog_empty())
  {
    goto end;
  }

  DBUG_PRINT("debug",
             ("all.cannot_safely_rollback(): %s, trx_cache_empty: %s",
              YESNO(thd->get_transaction()->cannot_safely_rollback(
                  Transaction_ctx::SESSION)),
              YESNO(cache_mngr->trx_cache.is_binlog_empty())));
  DBUG_PRINT("debug",
             ("stmt.cannot_safely_rollback(): %s, stmt_cache_empty: %s",
              YESNO(thd->get_transaction()->cannot_safely_rollback(
                  Transaction_ctx::STMT)),
              YESNO(cache_mngr->stmt_cache.is_binlog_empty())));

  /*
    If an incident event is set we do not flush the content of the statement
    cache because it may be corrupted.
  */
  if (cache_mngr->stmt_cache.has_incident())
  {
    const char* err_msg= "The content of the statement cache is corrupted "
                         "while writing a rollback record of the transaction "
                         "to the binary log.";
    error= write_incident(thd, true/*need_lock_log=true*/, err_msg);
    cache_mngr->stmt_cache.reset();
  }
  else if (!cache_mngr->stmt_cache.is_binlog_empty())
  {
    if (thd->lex->sql_command == SQLCOM_CREATE_TABLE &&
        thd->lex->select_lex->item_list.elements && /* With select */
        !(thd->lex->create_info.options & HA_LEX_CREATE_TMP_TABLE) &&
        thd->is_current_stmt_binlog_format_row())
    {
      /*
        In row based binlog format, we reset the binlog statement cache
        when rolling back a single statement 'CREATE...SELECT' transaction,
        since the 'CREATE TABLE' event was put in the binlog statement cache.
      */
      cache_mngr->stmt_cache.reset();
    }
    else
    {
      if ((error= cache_mngr->stmt_cache.finalize(thd)))
        goto end;
      stuff_logged= true;
    }
  }

  if (ending_trans(thd, all))
  {
    if (trans_cannot_safely_rollback(thd))
    {
      const char xa_rollback_str[]= "XA ROLLBACK";
      /*
        sizeof(xa_rollback_str) and XID::ser_buf_size both allocate `\0',
        so one of the two is used for necessary in the xa case `space' char
      */
      char query[sizeof(xa_rollback_str) + XID::ser_buf_size]= "ROLLBACK";
      XID_STATE *xs= thd->get_transaction()->xid_state();

      if (thd->lex->sql_command == SQLCOM_XA_ROLLBACK)
      {
        /* this block is relevant only for not prepared yet and "local" xa trx */
        DBUG_ASSERT(thd->get_transaction()->xid_state()->
                    has_state(XID_STATE::XA_IDLE));
        DBUG_ASSERT(!cache_mngr->has_logged_xid);

        sprintf(query, "%s ", xa_rollback_str);
        xs->get_xid()->serialize(query + sizeof(xa_rollback_str));
      }
      /*
        If the transaction is being rolled back and contains changes that
        cannot be rolled back, the trx-cache's content is flushed.
      */
      Query_log_event
        end_evt(thd, query, strlen(query), true, false, true, 0, true);
      error= thd->lex->sql_command != SQLCOM_XA_ROLLBACK ?
        cache_mngr->trx_cache.finalize(thd, &end_evt) :
        cache_mngr->trx_cache.finalize(thd, &end_evt, xs);
      stuff_logged= true;
    }
    else
    {
      /*
        If the transaction is being rolled back and its changes can be
        rolled back, the trx-cache's content is truncated.
      */
      error= cache_mngr->trx_cache.truncate(thd, all);
    }
  }
  else
  {
    /*
      If a statement is being rolled back, it is necessary to know
      exactly why a statement may not be safely rolled back as in
      some specific situations the trx-cache can be truncated.

      If a temporary table is created or dropped, the trx-cache is not
      truncated. Note that if the stmt-cache is used, there is nothing
      to truncate in the trx-cache.

      If a non-transactional table is updated and the binlog format is
      statement, the trx-cache is not truncated. The trx-cache is used
      when the direct option is off and a transactional table has been
      updated before the current statement in the context of the
      current transaction. Note that if the stmt-cache is used there is
      nothing to truncate in the trx-cache.

      If other binlog formats are used, updates to non-transactional
      tables are written to the stmt-cache and trx-cache can be safely
      truncated, if necessary.
    */
    if (thd->get_transaction()->has_dropped_temp_table(
          Transaction_ctx::STMT) ||
        thd->get_transaction()->has_created_temp_table(
          Transaction_ctx::STMT) ||
        (thd->get_transaction()->has_modified_non_trans_table(
          Transaction_ctx::STMT) &&
        thd->variables.binlog_format == BINLOG_FORMAT_STMT))
    {
      /*
        If the statement is being rolled back and dropped or created a
        temporary table or modified a non-transactional table and the
        statement-based replication is in use, the statement's changes
        in the trx-cache are preserved.
      */
      cache_mngr->trx_cache.set_prev_position(MY_OFF_T_UNDEF);
    }
    else
    {
      /*
        Otherwise, the statement's changes in the trx-cache are
        truncated.
      */
      error= cache_mngr->trx_cache.truncate(thd, all);
    }
  }
  if (stuff_logged)
  {
    Transaction_ctx *trn_ctx= thd->get_transaction();
    trn_ctx->store_commit_parent(m_dependency_tracker.get_max_committed_timestamp());
  }

  DBUG_PRINT("debug", ("error: %d", error));
  if (error == 0 && stuff_logged)
  {
    if (RUN_HOOK(transaction,
                 before_commit,
                 (thd, all,
                  thd_get_cache_mngr(thd)->get_binlog_cache_log(true),
                  thd_get_cache_mngr(thd)->get_binlog_cache_log(false),
                  max<my_off_t>(max_binlog_cache_size,
                                max_binlog_stmt_cache_size))))
    {
      //Reset the thread OK status before changing the outcome.
      if (thd->get_stmt_da()->is_ok())
        thd->get_stmt_da()->reset_diagnostics_area();
      my_error(ER_RUN_HOOK_ERROR, MYF(0), "before_commit");
      DBUG_RETURN(RESULT_ABORTED);
    }
#ifndef DBUG_OFF
    /*
      XA rollback is always accepted.
    */
    if (thd->get_transaction()->get_rpl_transaction_ctx()->is_transaction_rollback())
      DBUG_ASSERT(0);
#endif

    error= ordered_commit(thd, all, /* skip_commit */ true);
  }

  if (check_write_error(thd))
  {
    /*
      "all == true" means that a "rollback statement" triggered the error and
      this function was called. However, this must not happen as a rollback
      is written directly to the binary log. And in auto-commit mode, a single
      statement that is rolled back has the flag all == false.
    */
    DBUG_ASSERT(!all);
    /*
      We reach this point if the effect of a statement did not properly get into
      a cache and need to be rolled back.
    */
    error|= cache_mngr->trx_cache.truncate(thd, all);
  }

end:
  /* Deferred xa rollback to engines */
  if (!error && thd->lex->sql_command == SQLCOM_XA_ROLLBACK)
  {
    error= ha_rollback_low(thd, all);
    /* Successful XA-rollback commits the new gtid_state */
    gtid_state->update_on_commit(thd);
  }
  /*
    When a statement errors out on auto-commit mode it is rollback
    implicitly, so the same should happen to its GTID.
  */
  if (!thd->in_active_multi_stmt_transaction())
    gtid_state->update_on_rollback(thd);

  /*
    TODO: some errors are overwritten, which may cause problem,
    fix it later.
  */
  DBUG_PRINT("return", ("error: %d", error));
  DBUG_RETURN(error);
}

/**
  @note
  How do we handle this (unlikely but legal) case:
  @verbatim
    [transaction] + [update to non-trans table] + [rollback to savepoint] ?
  @endverbatim
  The problem occurs when a savepoint is before the update to the
  non-transactional table. Then when there's a rollback to the savepoint, if we
  simply truncate the binlog cache, we lose the part of the binlog cache where
  the update is. If we want to not lose it, we need to write the SAVEPOINT
  command and the ROLLBACK TO SAVEPOINT command to the binlog cache. The latter
  is easy: it's just write at the end of the binlog cache, but the former
  should be *inserted* to the place where the user called SAVEPOINT. The
  solution is that when the user calls SAVEPOINT, we write it to the binlog
  cache (so no need to later insert it). As transactions are never intermixed
  in the binary log (i.e. they are serialized), we won't have conflicts with
  savepoint names when using mysqlbinlog or in the slave SQL thread.
  Then when ROLLBACK TO SAVEPOINT is called, if we updated some
  non-transactional table, we don't truncate the binlog cache but instead write
  ROLLBACK TO SAVEPOINT to it; otherwise we truncate the binlog cache (which
  will chop the SAVEPOINT command from the binlog cache, which is good as in
  that case there is no need to have it in the binlog).
*/

static int binlog_savepoint_set(handlerton *hton, THD *thd, void *sv)
{
  DBUG_ENTER("binlog_savepoint_set");
  int error= 1;

  String log_query;
  if (log_query.append(STRING_WITH_LEN("SAVEPOINT ")))
    DBUG_RETURN(error);
  else
    append_identifier(thd, &log_query, thd->lex->ident.str,
                      thd->lex->ident.length);

  int errcode= query_error_code(thd, thd->killed == THD::NOT_KILLED);
  Query_log_event qinfo(thd, log_query.c_ptr_safe(), log_query.length(),
                        TRUE, FALSE, TRUE, errcode);
  /* 
    We cannot record the position before writing the statement
    because a rollback to a savepoint (.e.g. consider it "S") would
    prevent the savepoint statement (i.e. "SAVEPOINT S") from being
    written to the binary log despite the fact that the server could
    still issue other rollback statements to the same savepoint (i.e. 
    "S"). 
    Given that the savepoint is valid until the server releases it,
    ie, until the transaction commits or it is released explicitly,
    we need to log it anyway so that we don't have "ROLLBACK TO S"
    or "RELEASE S" without the preceding "SAVEPOINT S" in the binary
    log.
  */
  if (!(error= mysql_bin_log.write_event(&qinfo)))
    binlog_trans_log_savepos(thd, (my_off_t*) sv);

  DBUG_RETURN(error);
}

static int binlog_savepoint_rollback(handlerton *hton, THD *thd, void *sv)
{
  DBUG_ENTER("binlog_savepoint_rollback");
  binlog_cache_mngr *const cache_mngr= thd_get_cache_mngr(thd);
  my_off_t pos= *(my_off_t*) sv;
  DBUG_ASSERT(pos != ~(my_off_t) 0);

  /*
    Write ROLLBACK TO SAVEPOINT to the binlog cache if we have updated some
    non-transactional table. Otherwise, truncate the binlog cache starting
    from the SAVEPOINT command.
  */
  if (trans_cannot_safely_rollback(thd))
  {
    String log_query;
    if (log_query.append(STRING_WITH_LEN("ROLLBACK TO ")) ||
        log_query.append("`") ||
        log_query.append(thd->lex->ident.str, thd->lex->ident.length) ||
        log_query.append("`"))
      DBUG_RETURN(1);
    int errcode= query_error_code(thd, thd->killed == THD::NOT_KILLED);
    Query_log_event qinfo(thd, log_query.c_ptr_safe(), log_query.length(),
                          TRUE, FALSE, TRUE, errcode);
    DBUG_RETURN(mysql_bin_log.write_event(&qinfo));
  }
  // Otherwise, we truncate the cache
  cache_mngr->trx_cache.restore_savepoint(pos);
  /*
    When a SAVEPOINT is executed inside a stored function/trigger we force the
    pending event to be flushed with a STMT_END_F flag and clear the table maps
    as well to ensure that following DMLs will have a clean state to start
    with. ROLLBACK inside a stored routine has to finalize possibly existing
    current row-based pending event with cleaning up table maps. That ensures
    that following DMLs will have a clean state to start with.
   */
  if (thd->in_sub_stmt)
    thd->clear_binlog_table_maps();
  DBUG_RETURN(0);
}

/**
   purge logs, master and slave sides both, related error code
   convertor.
   Called from @c purge_error_message(), @c MYSQL_BIN_LOG::reset_logs()

   @param  res  an error code as used by purging routines

   @return the user level error code ER_*
*/
static uint purge_log_get_error_code(int res)
{
  uint errcode= 0;

  switch (res)  {
  case 0: break;
  case LOG_INFO_EOF:	errcode= ER_UNKNOWN_TARGET_BINLOG; break;
  case LOG_INFO_IO:	errcode= ER_IO_ERR_LOG_INDEX_READ; break;
  case LOG_INFO_INVALID:errcode= ER_BINLOG_PURGE_PROHIBITED; break;
  case LOG_INFO_SEEK:	errcode= ER_FSEEK_FAIL; break;
  case LOG_INFO_MEM:	errcode= ER_OUT_OF_RESOURCES; break;
  case LOG_INFO_FATAL:	errcode= ER_BINLOG_PURGE_FATAL_ERR; break;
  case LOG_INFO_IN_USE: errcode= ER_LOG_IN_USE; break;
  case LOG_INFO_EMFILE: errcode= ER_BINLOG_PURGE_EMFILE; break;
  default:		errcode= ER_LOG_PURGE_UNKNOWN_ERR; break;
  }

  return errcode;
}

/**
  Check whether binlog state allows to safely release MDL locks after
  rollback to savepoint.

  @param hton  The binlog handlerton.
  @param thd   The client thread that executes the transaction.

  @return true  - It is safe to release MDL locks.
          false - If it is not.
*/
static bool binlog_savepoint_rollback_can_release_mdl(handlerton *hton,
                                                      THD *thd)
{
  DBUG_ENTER("binlog_savepoint_rollback_can_release_mdl");
  /**
    If we have not updated any non-transactional tables rollback
    to savepoint will simply truncate binlog cache starting from
    SAVEPOINT command. So it should be safe to release MDL acquired
    after SAVEPOINT command in this case.
  */
  DBUG_RETURN(!trans_cannot_safely_rollback(thd));
}

#ifdef HAVE_REPLICATION
/**
  Adjust log offset in the binary log file for all running slaves
  This class implements call back function for do_for_all_thd().
  It is called for each thd in thd list to adjust offset.
*/
class Adjust_offset : public Do_THD_Impl
{
public:
  Adjust_offset(my_off_t value) : m_purge_offset(value) {}
  virtual void operator()(THD *thd)
  {
    LOG_INFO* linfo;
    mysql_mutex_lock(&thd->LOCK_thd_data);
    if ((linfo= thd->current_linfo))
    {
      /*
        Index file offset can be less that purge offset only if
        we just started reading the index file. In that case
        we have nothing to adjust.
      */
      if (linfo->index_file_offset < m_purge_offset)
        linfo->fatal = (linfo->index_file_offset != 0);
      else
        linfo->index_file_offset -= m_purge_offset;
    }
    mysql_mutex_unlock(&thd->LOCK_thd_data);
  }
private:
  my_off_t m_purge_offset;
};

/*
  Adjust the position pointer in the binary log file for all running slaves.

  SYNOPSIS
    adjust_linfo_offsets()
    purge_offset	Number of bytes removed from start of log index file

  NOTES
    - This is called when doing a PURGE when we delete lines from the
      index log file.

  REQUIREMENTS
    - Before calling this function, we have to ensure that no threads are
      using any binary log file before purge_offset.

  TODO
    - Inform the slave threads that they should sync the position
      in the binary log file with flush_relay_log_info.
      Now they sync is done for next read.
*/
static void adjust_linfo_offsets(my_off_t purge_offset)
{
  Adjust_offset adjust_offset(purge_offset);
  Global_THD_manager::get_instance()->do_for_all_thd(&adjust_offset);
}

/**
  This class implements Call back function for do_for_all_thd().
  It is called for each thd in thd list to count
  threads using bin log file
*/

class Log_in_use : public Do_THD_Impl
{
public:
  Log_in_use(const char* value) : m_log_name(value), m_count(0)
  {
    m_log_name_len = strlen(m_log_name) + 1;
  }
  virtual void operator()(THD *thd)
  {
    LOG_INFO* linfo;
    mysql_mutex_lock(&thd->LOCK_thd_data);
    if ((linfo = thd->current_linfo))
    {
      if(!memcmp(m_log_name, linfo->log_file_name, m_log_name_len))
      {
        sql_print_warning("file %s was not purged because it was being read"
                          "by thread number %u", m_log_name, thd->thread_id());
        m_count++;
      }
    }
    mysql_mutex_unlock(&thd->LOCK_thd_data);
  }
  int get_count() { return m_count; }
private:
  const char* m_log_name;
  size_t m_log_name_len;
  int m_count;
};

static int log_in_use(const char* log_name)
{
  Log_in_use log_in_use(log_name);
#ifndef DBUG_OFF
  if (current_thd)
    DEBUG_SYNC(current_thd,"purge_logs_after_lock_index_before_thread_count");
#endif
  Global_THD_manager::get_instance()->do_for_all_thd(&log_in_use);
  return log_in_use.get_count();
}

static bool purge_error_message(THD* thd, int res)
{
  uint errcode;

  if ((errcode= purge_log_get_error_code(res)) != 0)
  {
    my_message(errcode, ER(errcode), MYF(0));
    return TRUE;
  }
  my_ok(thd);
  return FALSE;
}

#endif /* HAVE_REPLICATION */

int check_binlog_magic(IO_CACHE* log, const char** errmsg)
{
  char magic[4];
  DBUG_ASSERT(my_b_tell(log) == 0);

  if (my_b_read(log, (uchar*) magic, sizeof(magic)))
  {
    *errmsg = "I/O error reading the header from the binary log";
    sql_print_error("%s, errno=%d, io cache code=%d", *errmsg, my_errno(),
		    log->error);
    return 1;
  }
  if (memcmp(magic, BINLOG_MAGIC, sizeof(magic)))
  {
    *errmsg = "Binlog has bad magic number;  It's not a binary log file that can be used by this version of MySQL";
    return 1;
  }
  return 0;
}


File open_binlog_file(IO_CACHE *log, const char *log_file_name, const char **errmsg)
{
  File file;
  DBUG_ENTER("open_binlog_file");

  if ((file= mysql_file_open(key_file_binlog,
                             log_file_name, O_RDONLY | O_BINARY | O_SHARE,
                             MYF(MY_WME))) < 0)
  {
    sql_print_error("Failed to open log (file '%s', errno %d)",
                    log_file_name, my_errno());
    *errmsg = "Could not open log file";
    goto err;
  }
  if (init_io_cache_ext(log, file, IO_SIZE*2, READ_CACHE, 0, 0,
                        MYF(MY_WME|MY_DONT_CHECK_FILESIZE), key_file_binlog_cache))
  {
    sql_print_error("Failed to create a cache on log (file '%s')",
                    log_file_name);
    *errmsg = "Could not open log file";
    goto err;
  }
  if (check_binlog_magic(log,errmsg))
    goto err;
  DBUG_RETURN(file);

err:
  if (file >= 0)
  {
    mysql_file_close(file, MYF(0));
    end_io_cache(log);
  }
  DBUG_RETURN(-1);
}


bool is_empty_transaction_in_binlog_cache(const THD* thd)
{
  DBUG_ENTER("is_empty_transaction_in_binlog_cache");

  binlog_cache_mngr *const cache_mngr= thd_get_cache_mngr(thd);
  if (cache_mngr != NULL && cache_mngr->has_empty_transaction())
  {
    DBUG_RETURN(true);
  }

  DBUG_RETURN(false);
}


/** 
  This function checks if a transactional table was updated by the
  current transaction.

  @param thd The client thread that executed the current statement.
  @return
    @c true if a transactional table was updated, @c false otherwise.
*/
bool
trans_has_updated_trans_table(const THD* thd)
{
  binlog_cache_mngr *const cache_mngr= thd_get_cache_mngr(thd);

  return (cache_mngr ? !cache_mngr->trx_cache.is_binlog_empty() : 0);
}

/** 
  This function checks if a transactional table was updated by the
  current statement.

  @param ha_list Registered storage engine handler list.
  @return
    @c true if a transactional table was updated, @c false otherwise.
*/
bool
stmt_has_updated_trans_table(Ha_trx_info* ha_list)
{
  const Ha_trx_info *ha_info;
  for (ha_info= ha_list; ha_info; ha_info= ha_info->next())
  {
    if (ha_info->is_trx_read_write() && ha_info->ht() != binlog_hton)
      return (TRUE);
  }
  return (FALSE);
}

/**
  This function checks if a transaction, either a multi-statement
  or a single statement transaction is about to commit or not.

  @param thd The client thread that executed the current statement.
  @param all Committing a transaction (i.e. TRUE) or a statement
             (i.e. FALSE).
  @return
    @c true if committing a transaction, otherwise @c false.
*/
bool ending_trans(THD* thd, const bool all)
{
  return (all || ending_single_stmt_trans(thd, all));
}

/**
  This function checks if a single statement transaction is about
  to commit or not.

  @param thd The client thread that executed the current statement.
  @param all Committing a transaction (i.e. TRUE) or a statement
             (i.e. FALSE).
  @return
    @c true if committing a single statement transaction, otherwise
    @c false.
*/
bool ending_single_stmt_trans(THD* thd, const bool all)
{
  return (!all && !thd->in_multi_stmt_transaction_mode());
}

/**
  This function checks if a transaction cannot be rolled back safely.

  @param thd The client thread that executed the current statement.
  @return
    @c true if cannot be safely rolled back, @c false otherwise.
*/
bool trans_cannot_safely_rollback(const THD* thd)
{
  binlog_cache_mngr *const cache_mngr= thd_get_cache_mngr(thd);

  return cache_mngr->trx_cache.cannot_rollback();
}

/**
  This function checks if current statement cannot be rollded back safely.

  @param thd The client thread that executed the current statement.
  @return
    @c true if cannot be safely rolled back, @c false otherwise.
*/
bool stmt_cannot_safely_rollback(const THD* thd)
{
  return thd->get_transaction()->cannot_safely_rollback(Transaction_ctx::STMT);
}

#ifndef EMBEDDED_LIBRARY
/**
  Execute a PURGE BINARY LOGS TO <log> command.

  @param thd Pointer to THD object for the client thread executing the
  statement.

  @param to_log Name of the last log to purge.

  @retval FALSE success
  @retval TRUE failure
*/
bool purge_master_logs(THD* thd, const char* to_log)
{
  char search_file_name[FN_REFLEN];
  if (!mysql_bin_log.is_open())
  {
    my_ok(thd);
    return FALSE;
  }

  mysql_bin_log.make_log_name(search_file_name, to_log);
  return purge_error_message(thd,
                             mysql_bin_log.purge_logs(search_file_name, false,
                                                      true/*need_lock_index=true*/,
                                                      true/*need_update_threads=true*/,
                                                      NULL, false));
}


/**
  Execute a PURGE BINARY LOGS BEFORE <date> command.

  @param thd Pointer to THD object for the client thread executing the
  statement.

  @param purge_time Date before which logs should be purged.

  @retval FALSE success
  @retval TRUE failure
*/
bool purge_master_logs_before_date(THD* thd, time_t purge_time)
{
  if (!mysql_bin_log.is_open())
  {
    my_ok(thd);
    return 0;
  }
  return purge_error_message(thd,
                             mysql_bin_log.purge_logs_before_date(purge_time,
                                                                  false));
}
#endif /* EMBEDDED_LIBRARY */

/*
  Helper function to get the error code of the query to be binlogged.
 */
int query_error_code(THD *thd, bool not_killed)
{
  int error;
  
  if (not_killed || (thd->killed == THD::KILL_BAD_DATA))
  {
    error= thd->is_error() ? thd->get_stmt_da()->mysql_errno() : 0;

    /* thd->get_stmt_da()->sql_errno() might be ER_SERVER_SHUTDOWN or
       ER_QUERY_INTERRUPTED, So here we need to make sure that error
       is not set to these errors when specified not_killed by the
       caller.
    */
    if (error == ER_SERVER_SHUTDOWN || error == ER_QUERY_INTERRUPTED)
      error= 0;
  }
  else
    error= thd->killed_errno();

  return error;
}


/**
  Copy content of 'from' file from offset to 'to' file.

  - We do the copy outside of the IO_CACHE as the cache
  buffers would just make things slower and more complicated.
  In most cases the copy loop should only do one read.

  @param from          File to copy.
  @param to            File to copy to.
  @param offset        Offset in 'from' file.


  @retval
    0    ok
  @retval
    -1    error
*/
static bool copy_file(IO_CACHE *from, IO_CACHE *to, my_off_t offset)
{
  int bytes_read;
  uchar io_buf[IO_SIZE*2];
  DBUG_ENTER("copy_file");

  mysql_file_seek(from->file, offset, MY_SEEK_SET, MYF(0));
  while(TRUE)
  {
    if ((bytes_read= (int) mysql_file_read(from->file, io_buf, sizeof(io_buf),
                                           MYF(MY_WME)))
        < 0)
      goto err;
    if (DBUG_EVALUATE_IF("fault_injection_copy_part_file", 1, 0))
      bytes_read= bytes_read/2;
    if (!bytes_read)
      break;                                    // end of file
    if (mysql_file_write(to->file, io_buf, bytes_read, MYF(MY_WME | MY_NABP)))
      goto err;
  }

  DBUG_RETURN(0);

err:
  DBUG_RETURN(1);
}


#ifdef HAVE_REPLICATION
/**
   Load data's io cache specific hook to be executed
   before a chunk of data is being read into the cache's buffer
   The fuction instantianates and writes into the binlog
   replication events along LOAD DATA processing.
   
   @param file  pointer to io-cache
   @retval 0 success
   @retval 1 failure
*/
int log_loaded_block(IO_CACHE* file)
{
  DBUG_ENTER("log_loaded_block");
  LOAD_FILE_INFO *lf_info;
  uint block_len;
  /* buffer contains position where we started last read */
  uchar* buffer= (uchar*) my_b_get_buffer_start(file);
  uint max_event_size= current_thd->variables.max_allowed_packet;
  lf_info= (LOAD_FILE_INFO*) file->arg;
  if (lf_info->thd->is_current_stmt_binlog_format_row())
    DBUG_RETURN(0);
  if (lf_info->last_pos_in_file != HA_POS_ERROR &&
      lf_info->last_pos_in_file >= my_b_get_pos_in_file(file))
    DBUG_RETURN(0);
  
  for (block_len= (uint) (my_b_get_bytes_in_buffer(file)); block_len > 0;
       buffer += min(block_len, max_event_size),
       block_len -= min(block_len, max_event_size))
  {
    lf_info->last_pos_in_file= my_b_get_pos_in_file(file);
    if (lf_info->wrote_create_file)
    {
      Append_block_log_event a(lf_info->thd, lf_info->thd->db().str, buffer,
                               min(block_len, max_event_size),
                               lf_info->log_delayed);
      if (mysql_bin_log.write_event(&a))
        DBUG_RETURN(1);
    }
    else
    {
      Begin_load_query_log_event b(lf_info->thd, lf_info->thd->db().str,
                                   buffer,
                                   min(block_len, max_event_size),
                                   lf_info->log_delayed);
      if (mysql_bin_log.write_event(&b))
        DBUG_RETURN(1);
      lf_info->wrote_create_file= 1;
    }
  }
  DBUG_RETURN(0);
}

/* Helper function for SHOW BINLOG/RELAYLOG EVENTS */
bool show_binlog_events(THD *thd, MYSQL_BIN_LOG *binary_log)
{
  Protocol *protocol= thd->get_protocol();
  List<Item> field_list;
  const char *errmsg = 0;
  bool ret = TRUE;
  IO_CACHE log;
  File file = -1;
  int old_max_allowed_packet= thd->variables.max_allowed_packet;
  LOG_INFO linfo;

  DBUG_ENTER("show_binlog_events");

  DBUG_ASSERT(thd->lex->sql_command == SQLCOM_SHOW_BINLOG_EVENTS ||
              thd->lex->sql_command == SQLCOM_SHOW_RELAYLOG_EVENTS);

  Format_description_log_event *description_event= new
    Format_description_log_event(3); /* MySQL 4.0 by default */

  if (binary_log->is_open())
  {
    LEX_MASTER_INFO *lex_mi= &thd->lex->mi;
    SELECT_LEX_UNIT *unit= thd->lex->unit;
    ha_rows event_count, limit_start, limit_end;
    my_off_t pos = max<my_off_t>(BIN_LOG_HEADER_SIZE, lex_mi->pos); // user-friendly
    char search_file_name[FN_REFLEN], *name;
    const char *log_file_name = lex_mi->log_file_name;
    mysql_mutex_t *log_lock = binary_log->get_log_lock();
    Log_event* ev;

    unit->set_limit(thd->lex->current_select());
    limit_start= unit->offset_limit_cnt;
    limit_end= unit->select_limit_cnt;

    name= search_file_name;
    if (log_file_name)
      binary_log->make_log_name(search_file_name, log_file_name);
    else
      name=0;					// Find first log

    linfo.index_file_offset = 0;

    if (binary_log->find_log_pos(&linfo, name, true/*need_lock_index=true*/))
    {
      errmsg = "Could not find target log";
      goto err;
    }

    mysql_mutex_lock(&thd->LOCK_thd_data);
    thd->current_linfo = &linfo;
    mysql_mutex_unlock(&thd->LOCK_thd_data);

    if ((file=open_binlog_file(&log, linfo.log_file_name, &errmsg)) < 0)
      goto err;

    my_off_t end_pos;
    /*
      Acquire LOCK_log only for the duration to calculate the
      log's end position. LOCK_log should be acquired even while
      we are checking whether the log is active log or not.
    */
    mysql_mutex_lock(log_lock);
    if (binary_log->is_active(linfo.log_file_name))
    {
      LOG_INFO li;
      binary_log->get_current_log(&li, false /*LOCK_log is already acquired*/);
      end_pos= li.pos;
    }
    else
    {
      end_pos= my_b_filelength(&log);
    }
    mysql_mutex_unlock(log_lock);

    /*
      to account binlog event header size
    */
    thd->variables.max_allowed_packet += MAX_LOG_EVENT_HEADER;

    DEBUG_SYNC(thd, "after_show_binlog_event_found_file");

    /*
      open_binlog_file() sought to position 4.
      Read the first event in case it's a Format_description_log_event, to
      know the format. If there's no such event, we are 3.23 or 4.x. This
      code, like before, can't read 3.23 binlogs.
      This code will fail on a mixed relay log (one which has Format_desc then
      Rotate then Format_desc).
    */
    ev= Log_event::read_log_event(&log, (mysql_mutex_t*)0, description_event,
                                   opt_master_verify_checksum);
    if (ev)
    {
      if (ev->get_type_code() == binary_log::FORMAT_DESCRIPTION_EVENT)
      {
        delete description_event;
        description_event= (Format_description_log_event*) ev;
      }
      else
        delete ev;
    }

    my_b_seek(&log, pos);

    if (!description_event->is_valid())
    {
      errmsg="Invalid Format_description event; could be out of memory";
      goto err;
    }

    for (event_count = 0;
         (ev = Log_event::read_log_event(&log, (mysql_mutex_t*) 0,
                                         description_event,
                                         opt_master_verify_checksum)); )
    {
      DEBUG_SYNC(thd, "wait_in_show_binlog_events_loop");
      if (ev->get_type_code() == binary_log::FORMAT_DESCRIPTION_EVENT)
        description_event->common_footer->checksum_alg=
                           ev->common_footer->checksum_alg;
      if (event_count >= limit_start &&
	  ev->net_send(protocol, linfo.log_file_name, pos))
      {
	errmsg = "Net error";
	delete ev;
	goto err;
      }

      pos = my_b_tell(&log);
      delete ev;

      if (++event_count >= limit_end || pos >= end_pos)
	break;
    }

    if (event_count < limit_end && log.error)
    {
      errmsg = "Wrong offset or I/O error";
      goto err;
    }

  }
  // Check that linfo is still on the function scope.
  DEBUG_SYNC(thd, "after_show_binlog_events");

  ret= FALSE;

err:
  delete description_event;
  if (file >= 0)
  {
    end_io_cache(&log);
    mysql_file_close(file, MYF(MY_WME));
  }

  if (errmsg)
  {
    if(thd->lex->sql_command == SQLCOM_SHOW_RELAYLOG_EVENTS)
      my_error(ER_ERROR_WHEN_EXECUTING_COMMAND, MYF(0),
             "SHOW RELAYLOG EVENTS", errmsg);
    else
      my_error(ER_ERROR_WHEN_EXECUTING_COMMAND, MYF(0),
             "SHOW BINLOG EVENTS", errmsg);
  }
  else
    my_eof(thd);

  mysql_mutex_lock(&thd->LOCK_thd_data);
  thd->current_linfo = 0;
  mysql_mutex_unlock(&thd->LOCK_thd_data);
  thd->variables.max_allowed_packet= old_max_allowed_packet;
  DBUG_RETURN(ret);
}

/**
  Execute a SHOW BINLOG EVENTS statement.

  @param thd Pointer to THD object for the client thread executing the
  statement.

  @retval FALSE success
  @retval TRUE failure
*/
bool mysql_show_binlog_events(THD* thd)
{
  List<Item> field_list;
  DBUG_ENTER("mysql_show_binlog_events");

  DBUG_ASSERT(thd->lex->sql_command == SQLCOM_SHOW_BINLOG_EVENTS);

  Log_event::init_show_field_list(&field_list);
  if (thd->send_result_metadata(&field_list,
                                Protocol::SEND_NUM_ROWS | Protocol::SEND_EOF))
    DBUG_RETURN(TRUE);

  /*
    Wait for handlers to insert any pending information
    into the binlog.  For e.g. ndb which updates the binlog asynchronously
    this is needed so that the uses sees all its own commands in the binlog
  */
  ha_binlog_wait(thd);
  
  DBUG_RETURN(show_binlog_events(thd, &mysql_bin_log));
}

#endif /* HAVE_REPLICATION */


MYSQL_BIN_LOG::MYSQL_BIN_LOG(uint *sync_period,
                             enum cache_type io_cache_type_arg)
  :name(NULL), write_error(false), inited(false),
   io_cache_type(io_cache_type_arg),
#ifdef HAVE_PSI_INTERFACE
   m_key_LOCK_log(key_LOG_LOCK_log),
#endif
   bytes_written(0), file_id(1), open_count(1),
   sync_period_ptr(sync_period), sync_counter(0),
   is_relay_log(0), signal_cnt(0),
   checksum_alg_reset(binary_log::BINLOG_CHECKSUM_ALG_UNDEF),
   relay_log_checksum_alg(binary_log::BINLOG_CHECKSUM_ALG_UNDEF),
   previous_gtid_set_relaylog(0), is_rotating_caused_by_incident(false)
{
  log_state.atomic_set(LOG_CLOSED);
  /*
    We don't want to initialize locks here as such initialization depends on
    safe_mutex (when using safe_mutex) which depends on MY_INIT(), which is
    called only in main(). Doing initialization here would make it happen
    before main().
  */
  m_prep_xids.atomic_set(0);
  memset(&log_file, 0, sizeof(log_file));
  index_file_name[0] = 0;
  memset(&index_file, 0, sizeof(index_file));
  memset(&purge_index_file, 0, sizeof(purge_index_file));
  memset(&crash_safe_index_file, 0, sizeof(crash_safe_index_file));
}


/* this is called only once */

void MYSQL_BIN_LOG::cleanup()
{
  DBUG_ENTER("cleanup");
  if (inited)
  {
    inited= 0;
    close(LOG_CLOSE_INDEX|LOG_CLOSE_STOP_EVENT, true /*need_lock_log=true*/,
          true /*need_lock_index=true*/);
    mysql_mutex_destroy(&LOCK_log);
    mysql_mutex_destroy(&LOCK_index);
    mysql_mutex_destroy(&LOCK_commit);
    mysql_mutex_destroy(&LOCK_sync);
    mysql_mutex_destroy(&LOCK_binlog_end_pos);
    mysql_mutex_destroy(&LOCK_xids);
    mysql_cond_destroy(&update_cond);
    mysql_cond_destroy(&m_prep_xids_cond);
    stage_manager.deinit();
  }
  DBUG_VOID_RETURN;
}


void MYSQL_BIN_LOG::init_pthread_objects()
{
  DBUG_ASSERT(inited == 0);
  inited= 1;
  mysql_mutex_init(m_key_LOCK_log, &LOCK_log, MY_MUTEX_INIT_SLOW);
  mysql_mutex_init(m_key_LOCK_index, &LOCK_index, MY_MUTEX_INIT_SLOW);
  mysql_mutex_init(m_key_LOCK_commit, &LOCK_commit, MY_MUTEX_INIT_FAST);
  mysql_mutex_init(m_key_LOCK_sync, &LOCK_sync, MY_MUTEX_INIT_FAST);
  mysql_mutex_init(m_key_LOCK_binlog_end_pos, &LOCK_binlog_end_pos,
                   MY_MUTEX_INIT_FAST);
  mysql_mutex_init(m_key_LOCK_xids, &LOCK_xids, MY_MUTEX_INIT_FAST);
  mysql_cond_init(m_key_update_cond, &update_cond);
  mysql_cond_init(m_key_prep_xids_cond, &m_prep_xids_cond);
  stage_manager.init(
#ifdef HAVE_PSI_INTERFACE
                   m_key_LOCK_flush_queue,
                   m_key_LOCK_sync_queue,
                   m_key_LOCK_commit_queue,
                   m_key_LOCK_done, m_key_COND_done
#endif
                   );
}


/**
  Check if a string is a valid number.

  @param str			String to test
  @param res			Store value here
  @param allow_wildcards	Set to 1 if we should ignore '%' and '_'

  @note
    For the moment the allow_wildcards argument is not used
    Should be moved to some other file.

  @retval
    1	String is a number
  @retval
    0	String is not a number
*/

static bool is_number(const char *str,
                      ulong *res, bool allow_wildcards)
{
  int flag;
  const char *start;
  DBUG_ENTER("is_number");

  flag=0; start=str;
  while (*str++ == ' ') ;
  if (*--str == '-' || *str == '+')
    str++;
  while (my_isdigit(files_charset_info,*str) ||
	 (allow_wildcards && (*str == wild_many || *str == wild_one)))
  {
    flag=1;
    str++;
  }
  if (*str == '.')
  {
    for (str++ ;
	 my_isdigit(files_charset_info,*str) ||
	   (allow_wildcards && (*str == wild_many || *str == wild_one)) ;
	 str++, flag=1) ;
  }
  if (*str != 0 || flag == 0)
    DBUG_RETURN(0);
  if (res)
    *res=atol(start);
  DBUG_RETURN(1);			/* Number ok */
} /* is_number */


/*
  Maximum unique log filename extension.
  Note: setting to 0x7FFFFFFF due to atol windows
        overflow/truncate.
 */
#define MAX_LOG_UNIQUE_FN_EXT 0x7FFFFFFF

/*
   Number of warnings that will be printed to error log
   before extension number is exhausted.
*/
#define LOG_WARN_UNIQUE_FN_EXT_LEFT 1000

/**
  Find a unique filename for 'filename.#'.

  Set '#' to the highest existing log file extension plus one.

  This function will return nonzero if: (i) the generated name
  exceeds FN_REFLEN; (ii) if the number of extensions is exhausted;
  or (iii) some other error happened while examining the filesystem.

  @return
    nonzero if not possible to get unique filename.
*/

static int find_uniq_filename(char *name)
{
  uint                  i;
  char                  buff[FN_REFLEN], ext_buf[FN_REFLEN];
  struct st_my_dir     *dir_info;
  struct fileinfo *file_info;
  ulong                 max_found= 0, next= 0, number= 0;
  size_t		buf_length, length;
  char			*start, *end;
  int                   error= 0;
  DBUG_ENTER("find_uniq_filename");

  length= dirname_part(buff, name, &buf_length);
  start=  name + length;
  end=    strend(start);

  *end='.';
  length= (size_t) (end - start + 1);

  if ((DBUG_EVALUATE_IF("error_unique_log_filename", 1, 
      !(dir_info= my_dir(buff,MYF(MY_DONT_SORT))))))
  {						// This shouldn't happen
    my_stpcpy(end,".1");				// use name+1
    DBUG_RETURN(1);
  }
  file_info= dir_info->dir_entry;
  for (i= dir_info->number_off_files ; i-- ; file_info++)
  {
    if (strncmp(file_info->name, start, length) == 0 &&
	is_number(file_info->name+length, &number,0))
    {
      set_if_bigger(max_found, number);
    }
  }
  my_dirend(dir_info);

  /* check if reached the maximum possible extension number */
  if (max_found == MAX_LOG_UNIQUE_FN_EXT)
  {
    sql_print_error("Log filename extension number exhausted: %06lu. \
Please fix this by archiving old logs and \
updating the index files.", max_found);
    error= 1;
    goto end;
  }

  next= max_found + 1;
  if (sprintf(ext_buf, "%06lu", next)<0)
  {
    error= 1;
    goto end;
  }
  *end++='.';

  /* 
    Check if the generated extension size + the file name exceeds the
    buffer size used. If one did not check this, then the filename might be
    truncated, resulting in error.
   */
  if (((strlen(ext_buf) + (end - name)) >= FN_REFLEN))
  {
    sql_print_error("Log filename too large: %s%s (%zu). \
Please fix this by archiving old logs and updating the \
index files.", name, ext_buf, (strlen(ext_buf) + (end - name)));
    error= 1;
    goto end;
  }

  if (sprintf(end, "%06lu", next)<0)
  {
    error= 1;
    goto end;
  }

  /* print warning if reaching the end of available extensions. */
  if ((next > (MAX_LOG_UNIQUE_FN_EXT - LOG_WARN_UNIQUE_FN_EXT_LEFT)))
    sql_print_warning("Next log extension: %lu. \
Remaining log filename extensions: %lu. \
Please consider archiving some logs.", next, (MAX_LOG_UNIQUE_FN_EXT - next));

end:
  DBUG_RETURN(error);
}


int MYSQL_BIN_LOG::generate_new_name(char *new_name, const char *log_name)
{
  fn_format(new_name, log_name, mysql_data_home, "", 4);
  if (!fn_ext(log_name)[0])
  {
    if (find_uniq_filename(new_name))
    {
      my_printf_error(ER_NO_UNIQUE_LOGFILE, ER(ER_NO_UNIQUE_LOGFILE),
                      MYF(ME_FATALERROR), log_name);
      sql_print_error(ER(ER_NO_UNIQUE_LOGFILE), log_name);
      return 1;
    }
  }
  return 0;
}


/**
  @todo
  The following should be using fn_format();  We just need to
  first change fn_format() to cut the file name if it's too long.
*/
const char *MYSQL_BIN_LOG::generate_name(const char *log_name,
                                         const char *suffix,
                                         char *buff)
{
  if (!log_name || !log_name[0])
  {
    strmake(buff, default_logfile_name, FN_REFLEN - strlen(suffix) - 1);
    return (const char *)
      fn_format(buff, buff, "", suffix, MYF(MY_REPLACE_EXT|MY_REPLACE_DIR));
  }
  // get rid of extension to avoid problems

  char *p= fn_ext(log_name);
  uint length= (uint) (p - log_name);
  strmake(buff, log_name, min<size_t>(length, FN_REFLEN-1));
  return (const char*)buff;
}


bool MYSQL_BIN_LOG::init_and_set_log_file_name(const char *log_name,
                                               const char *new_name)
{
  if (new_name && !my_stpcpy(log_file_name, new_name))
    return TRUE;
  else if (!new_name && generate_new_name(log_file_name, log_name))
    return TRUE;

  return FALSE;
}


/**
  Open the logfile and init IO_CACHE.

  @param log_name            The name of the log to open
  @param new_name            The new name for the logfile.
                             NULL forces generate_new_name() to be called.

  @return true if error, false otherwise.
*/

bool MYSQL_BIN_LOG::open(
#ifdef HAVE_PSI_INTERFACE
                     PSI_file_key log_file_key,
#endif
                     const char *log_name,
                     const char *new_name)
{
  File file= -1;
  my_off_t pos= 0;
  int open_flags= O_CREAT | O_BINARY;
  DBUG_ENTER("MYSQL_BIN_LOG::open");

  write_error= 0;

  if (!(name= my_strdup(key_memory_MYSQL_LOG_name,
                        log_name, MYF(MY_WME))))
  {
    name= (char *)log_name; // for the error message
    goto err;
  }

  if (init_and_set_log_file_name(name, new_name) ||
      DBUG_EVALUATE_IF("fault_injection_init_name", 1, 0))
    goto err;

  if (io_cache_type == SEQ_READ_APPEND)
    open_flags |= O_RDWR | O_APPEND;
  else
    open_flags |= O_WRONLY;

  db[0]= 0;

#ifdef HAVE_PSI_INTERFACE
  /* Keep the key for reopen */
  m_log_file_key= log_file_key;
#endif

  if ((file= mysql_file_open(log_file_key,
                             log_file_name, open_flags,
                             MYF(MY_WME))) < 0)
    goto err;

  if ((pos= mysql_file_tell(file, MYF(MY_WME))) == MY_FILEPOS_ERROR)
  {
    if (my_errno() == ESPIPE)
      pos= 0;
    else
      goto err;
  }

  if (init_io_cache(&log_file, file, IO_SIZE, io_cache_type, pos, 0,
                    MYF(MY_WME | MY_NABP | MY_WAIT_IF_FULL)))
    goto err;

  log_state.atomic_set(LOG_OPENED);
  DBUG_RETURN(0);

err:
  if (binlog_error_action == ABORT_SERVER)
  {
    exec_binlog_error_action_abort("Either disk is full or file system is read "
                                   "only while opening the binlog. Aborting the"
                                   " server.");
  }
  else
    sql_print_error("Could not open %s for logging (error %d). "
                    "Turning logging off for the whole duration "
                    "of the MySQL server process. To turn it on "
                    "again: fix the cause, shutdown the MySQL "
                    "server and restart it.",
                    name, errno);
  if (file >= 0)
    mysql_file_close(file, MYF(0));
  end_io_cache(&log_file);
  my_free(name);
  name= NULL;
  log_state.atomic_set(LOG_CLOSED);
  DBUG_RETURN(1);
}


bool MYSQL_BIN_LOG::open_index_file(const char *index_file_name_arg,
                                    const char *log_name, bool need_lock_index)
{
  bool error= false;
  File index_file_nr= -1;
  if (need_lock_index)
    mysql_mutex_lock(&LOCK_index);
  else
    mysql_mutex_assert_owner(&LOCK_index);

  /*
    First open of this class instance
    Create an index file that will hold all file names uses for logging.
    Add new entries to the end of it.
  */
  myf opt= MY_UNPACK_FILENAME;

  if (my_b_inited(&index_file))
    goto end;

  if (!index_file_name_arg)
  {
    index_file_name_arg= log_name;    // Use same basename for index file
    opt= MY_UNPACK_FILENAME | MY_REPLACE_EXT;
  }
  fn_format(index_file_name, index_file_name_arg, mysql_data_home,
            ".index", opt);

  if (set_crash_safe_index_file_name(index_file_name_arg))
  {
    sql_print_error("MYSQL_BIN_LOG::set_crash_safe_index_file_name failed.");
    error= true;
    goto end;
  }

  /*
    We need move crash_safe_index_file to index_file if the index_file
    does not exist and crash_safe_index_file exists when mysqld server
    restarts.
  */
  if (my_access(index_file_name, F_OK) &&
      !my_access(crash_safe_index_file_name, F_OK) &&
      my_rename(crash_safe_index_file_name, index_file_name, MYF(MY_WME)))
  {
    sql_print_error("MYSQL_BIN_LOG::open_index_file failed to "
                    "move crash_safe_index_file to index file.");
    error= true;
    goto end;
  }

  if ((index_file_nr= mysql_file_open(m_key_file_log_index,
                                      index_file_name,
                                      O_RDWR | O_CREAT | O_BINARY,
                                      MYF(MY_WME))) < 0 ||
       mysql_file_sync(index_file_nr, MYF(MY_WME)) ||
       init_io_cache_ext(&index_file, index_file_nr,
                         IO_SIZE, READ_CACHE,
                         mysql_file_seek(index_file_nr, 0L, MY_SEEK_END, MYF(0)),
                                         0, MYF(MY_WME | MY_WAIT_IF_FULL),
                         m_key_file_log_index_cache) ||
      DBUG_EVALUATE_IF("fault_injection_openning_index", 1, 0))
  {
    /*
      TODO: all operations creating/deleting the index file or a log, should
      call my_sync_dir() or my_sync_dir_by_file() to be durable.
      TODO: file creation should be done with mysql_file_create()
      not mysql_file_open().
    */
    if (index_file_nr >= 0)
      mysql_file_close(index_file_nr, MYF(0));
    error= true;
    goto end;
  }

#ifdef HAVE_REPLICATION
  /*
    Sync the index by purging any binary log file that is not registered.
    In other words, either purge binary log files that were removed from
    the index but not purged from the file system due to a crash or purge
    any binary log file that was created but not register in the index
    due to a crash.
  */

  if (set_purge_index_file_name(index_file_name_arg) ||
      open_purge_index_file(FALSE) ||
      purge_index_entry(NULL, NULL, false) ||
      close_purge_index_file() ||
      DBUG_EVALUATE_IF("fault_injection_recovering_index", 1, 0))
  {
    sql_print_error("MYSQL_BIN_LOG::open_index_file failed to sync the index "
                    "file.");
    error= true;
    goto end;
  }
#endif

end:
  if (need_lock_index)
    mysql_mutex_unlock(&LOCK_index);
  return error;
}

/**
  Add the GTIDs from the given relaylog file and also
  update the IO thread transaction parser.

  @param filename Relaylog file to read from.
  @param retrieved_set Gtid_set to store the GTIDs found on the relaylog file.
  @param verify_checksum Set to true to verify event checksums.
  @param trx_parser The transaction boundary parser to be used in order to
  only add a GTID to the gtid_set after ensuring the transaction is fully
  stored on the relay log.
  @param gtid_partial_trx The gtid of the last incomplete transaction
  found in the relay log.

  @retval false The file was successfully read and all GTIDs from
  Previous_gtids and Gtid_log_event from complete transactions were added to
  the retrieved_set.
  @retval true There was an error during the procedure.
*/
static bool
read_gtids_and_update_trx_parser_from_relaylog(
  const char *filename,
  Gtid_set *retrieved_gtids,
  bool verify_checksum,
  Transaction_boundary_parser *trx_parser,
  Gtid *gtid_partial_trx)
{
  DBUG_ENTER("read_gtids_and_update_trx_parser_from_relaylog");
  DBUG_PRINT("info", ("Opening file %s", filename));

  DBUG_ASSERT(retrieved_gtids != NULL);
  DBUG_ASSERT(trx_parser != NULL);
#ifndef DBUG_OFF
  unsigned long event_counter= 0;
#endif

  /*
    Create a Format_description_log_event that is used to read the
    first event of the log.
  */
  Format_description_log_event fd_ev(BINLOG_VERSION), *fd_ev_p= &fd_ev;
  if (!fd_ev.is_valid())
    DBUG_RETURN(true);

  File file;
  IO_CACHE log;

  const char *errmsg= NULL;
  if ((file= open_binlog_file(&log, filename, &errmsg)) < 0)
  {
    sql_print_error("%s", errmsg);
    /*
      As read_gtids_from_binlog() will not throw error on truncated
      relaylog files, we should do the same here in order to keep the
      current behavior.
    */
    DBUG_RETURN(false);
  }

  /*
    Seek for Previous_gtids_log_event and Gtid_log_event events to
    gather information what has been processed so far.
  */
  my_b_seek(&log, BIN_LOG_HEADER_SIZE);
  Log_event *ev= NULL;
  bool error= false;
  bool seen_prev_gtids= false;
  ulong data_len= 0;

  while (!error &&
         (ev= Log_event::read_log_event(&log, 0, fd_ev_p, verify_checksum)) !=
         NULL)
  {
    DBUG_PRINT("info", ("Read event of type %s", ev->get_type_str()));
#ifndef DBUG_OFF
    event_counter++;
#endif

    data_len= uint4korr(ev->temp_buf + EVENT_LEN_OFFSET);
    if (trx_parser->feed_event(ev->temp_buf, data_len, fd_ev_p, false))
    {
      /*
        The transaction boundary parser found an error while parsing a
        sequence of events from the relaylog. As we don't know if the
        parsing has started from a reliable point (it might started in
        a relay log file that begins with the rest of a transaction
        that started in a previous relay log file), it is better to do
        nothing in this case. The boundary parser will fix itself once
        finding an event that represent a transaction boundary.

        Suppose the following relaylog:

         rl-bin.000011 | rl-bin.000012 | rl-bin.000013 | rl-bin-000014
        ---------------+---------------+---------------+---------------
         PREV_GTIDS    | PREV_GTIDS    | PREV_GTIDS    | PREV_GTIDS
         (empty)       | (UUID:1-2)    | (UUID:1-2)    | (UUID:1-2)
        ---------------+---------------+---------------+---------------
         XID           | QUERY(INSERT) | QUERY(INSERT) | XID
        ---------------+---------------+---------------+---------------
         GTID(UUID:2)  |
        ---------------+
         QUERY(CREATE  |
         TABLE t1 ...) |
        ---------------+
         GTID(UUID:3)  |
        ---------------+
         QUERY(BEGIN)  |
        ---------------+

        As it is impossible to determine the current Retrieved_Gtid_Set by only
        looking to the PREVIOUS_GTIDS on the last relay log file, and scanning
        events on it, we tried to find a relay log file that contains at least
        one GTID event during the backwards search.

        In the example, we will find a GTID only in rl-bin.000011, as the
        UUID:3 transaction was spanned across 4 relay log files.

        The transaction spanning can be caused by "FLUSH RELAY LOGS" commands
        on slave while it is queuing the transaction.

        So, in order to correctly add UUID:3 into Retrieved_Gtid_Set, we need
        to parse the relay log starting on the file we found the last GTID
        queued to know if the transaction was fully retrieved or not.

        Start scanning rl-bin.000011 after resetting the transaction parser
        will generate an error, as XID event is only expected inside a DML,
        but in this case, we can ignore this error and reset the parser.
      */
      trx_parser->reset();
      /*
        We also have to discard the GTID of the partial transaction that was
        not finished if there is one. This is needed supposing that an
        incomplete transaction was replicated with a GTID.

        GTID(1), QUERY(BEGIN), QUERY(INSERT), ANONYMOUS_GTID, QUERY(DROP ...)

        In the example above, without cleaning the gtid_partial_trx,
        the GTID(1) would be added to the Retrieved_Gtid_Set after the
        QUERY(DROP ...) event.

        GTID(1), QUERY(BEGIN), QUERY(INSERT), GTID(2), QUERY(DROP ...)

        In the example above the GTID(1) will also be discarded as the
        GTID(1) transaction is not complete.
      */
      if (!gtid_partial_trx->is_empty())
      {
        DBUG_PRINT("info", ("Discarding Gtid(%d, %lld) as the transaction "
                            "wasn't complete and we found an error in the"
                            "transaction boundary parser.",
                            gtid_partial_trx->sidno,
                            gtid_partial_trx->gno));
        gtid_partial_trx->clear();
      }
    }

    switch (ev->get_type_code())
    {
    case binary_log::FORMAT_DESCRIPTION_EVENT:
      if (fd_ev_p != &fd_ev)
        delete fd_ev_p;
      fd_ev_p= (Format_description_log_event *)ev;
      break;
    case binary_log::ROTATE_EVENT:
      // do nothing; just accept this event and go to next
      break;
    case binary_log::PREVIOUS_GTIDS_LOG_EVENT:
    {
      seen_prev_gtids= true;
      // add events to sets
      Previous_gtids_log_event *prev_gtids_ev= (Previous_gtids_log_event *)ev;
      if (prev_gtids_ev->add_to_set(retrieved_gtids) != 0)
      {
        error= true;
        break;
      }
#ifndef DBUG_OFF
      char* prev_buffer= prev_gtids_ev->get_str(NULL, NULL);
      DBUG_PRINT("info", ("Got Previous_gtids from file '%s': Gtid_set='%s'.",
                          filename, prev_buffer));
      my_free(prev_buffer);
#endif
      break;
    }
    case binary_log::GTID_LOG_EVENT:
    {
      /* If we didn't find any PREVIOUS_GTIDS in this file */
      if (!seen_prev_gtids)
      {
        my_error(ER_BINLOG_LOGICAL_CORRUPTION, MYF(0), filename,
                 "The first global transaction identifier was read, but "
                 "no other information regarding identifiers existing "
                 "on the previous log files was found.");
        error= true;
        break;
      }

      Gtid_log_event *gtid_ev= (Gtid_log_event *)ev;
      rpl_sidno sidno= gtid_ev->get_sidno(retrieved_gtids->get_sid_map());
      if (sidno < 0)
      {
        error= true;
        break;
      }
      else
      {
        if (retrieved_gtids->ensure_sidno(sidno) != RETURN_STATUS_OK)
        {
          error= true;
          break;
        }
        else
        {
          /*
            As are updating the transaction boundary parser while reading
            GTIDs from relay log files to fill the Retrieved_Gtid_Set, we
            should not add the GTID here as we don't know if the transaction
            is complete on the relay log yet.
          */
          gtid_partial_trx->set(sidno, gtid_ev->get_gno());
        }
        DBUG_PRINT("info", ("Found Gtid in relaylog file '%s': Gtid(%d, %lld).",
                            filename, sidno, gtid_ev->get_gno()));
      }
      break;
    }
    case binary_log::ANONYMOUS_GTID_LOG_EVENT:
    default:
      /*
        If we reached the end of a transaction after storing it's GTID
        in gtid_partial_trx variable, it is time to add this GTID to the
        retrieved_gtids set because the transaction is complete and there is no
        need for asking this transaction again.
      */
      if (trx_parser->is_not_inside_transaction())
      {
        if (!gtid_partial_trx->is_empty())
        {
          DBUG_PRINT("info", ("Adding Gtid to Retrieved_Gtid_Set as the "
                              "transaction was completed at "
                              "relaylog file '%s': Gtid(%d, %lld).",
                              filename, gtid_partial_trx->sidno,
                              gtid_partial_trx->gno));
          retrieved_gtids->_add_gtid(gtid_partial_trx->sidno,
                                     gtid_partial_trx->gno);
          gtid_partial_trx->clear();
        }
      }
      break;
    }
    if (ev != fd_ev_p)
      delete ev;
  }

  if (log.error < 0)
  {
    // This is not a fatal error; the log may just be truncated.
    // @todo but what other errors could happen? IO error?
    sql_print_warning("Error reading GTIDs from relaylog: %d", log.error);
  }

  if (fd_ev_p != &fd_ev)
  {
    delete fd_ev_p;
    fd_ev_p= &fd_ev;
  }

  mysql_file_close(file, MYF(MY_WME));
  end_io_cache(&log);

#ifndef DBUG_OFF
  sql_print_information("%lu events read in relaylog file '%s' for updating "
                        "Retrieved_Gtid_Set and/or IO thread transaction "
                        "parser state.",
                        event_counter, filename);
#endif

  DBUG_RETURN(error);
}

/**
  Reads GTIDs from the given binlog file.

  @param filename File to read from.
  @param all_gtids If not NULL, then the GTIDs from the
  Previous_gtids_log_event and from all Gtid_log_events are stored in
  this object.
  @param prev_gtids If not NULL, then the GTIDs from the
  Previous_gtids_log_events are stored in this object.
  @param first_gtid If not NULL, then the first GTID information from the
  file will be stored in this object.
  @param sid_map The sid_map object to use in the rpl_sidno generation
  of the Gtid_log_event. If lock is needed in the sid_map, the caller
  must hold it.
  @param verify_checksum Set to true to verify event checksums.

  @retval GOT_GTIDS The file was successfully read and it contains
  both Gtid_log_events and Previous_gtids_log_events.
  This is only possible if either all_gtids or first_gtid are not null.
  @retval GOT_PREVIOUS_GTIDS The file was successfully read and it
  contains Previous_gtids_log_events but no Gtid_log_events.
  For binary logs, if no all_gtids and no first_gtid are specified,
  this function will be done right after reading the PREVIOUS_GTIDS
  regardless of the rest of the content of the binary log file.
  @retval NO_GTIDS The file was successfully read and it does not
  contain GTID events.
  @retval ERROR Out of memory, or IO error, or malformed event
  structure, or the file is malformed (e.g., contains Gtid_log_events
  but no Previous_gtids_log_event).
  @retval TRUNCATED The file was truncated before the end of the
  first Previous_gtids_log_event.
*/
enum enum_read_gtids_from_binlog_status
{ GOT_GTIDS, GOT_PREVIOUS_GTIDS, NO_GTIDS, ERROR, TRUNCATED };
static enum_read_gtids_from_binlog_status
read_gtids_from_binlog(const char *filename, Gtid_set *all_gtids,
                       Gtid_set *prev_gtids, Gtid *first_gtid,
                       Sid_map* sid_map,
                       bool verify_checksum, bool is_relay_log)
{
  DBUG_ENTER("read_gtids_from_binlog");
  DBUG_PRINT("info", ("Opening file %s", filename));

  /*
    Create a Format_description_log_event that is used to read the
    first event of the log.
  */
  Format_description_log_event fd_ev(BINLOG_VERSION), *fd_ev_p= &fd_ev;
  if (!fd_ev.is_valid())
    DBUG_RETURN(ERROR);

  File file;
  IO_CACHE log;

#ifndef DBUG_OFF
  unsigned long event_counter= 0;
  /*
    We assert here that both all_gtids and prev_gtids, if specified,
    uses the same sid_map as the one passed as a parameter. This is just
    to ensure that, if the sid_map needed some lock and was locked by
    the caller, the lock applies to all the GTID sets this function is
    dealing with.
  */
  if (all_gtids)
    DBUG_ASSERT(all_gtids->get_sid_map() == sid_map);
  if (prev_gtids)
    DBUG_ASSERT(prev_gtids->get_sid_map() == sid_map);
#endif

  const char *errmsg= NULL;
  if ((file= open_binlog_file(&log, filename, &errmsg)) < 0)
  {
    sql_print_error("%s", errmsg);
    /*
      We need to revisit the recovery procedure for relay log
      files. Currently, it is called after this routine.
      /Alfranio
    */
    DBUG_RETURN(TRUNCATED);
  }

  /*
    Seek for Previous_gtids_log_event and Gtid_log_event events to
    gather information what has been processed so far.
  */
  my_b_seek(&log, BIN_LOG_HEADER_SIZE);
  Log_event *ev= NULL;
  enum_read_gtids_from_binlog_status ret= NO_GTIDS;
  bool done= false;
  bool seen_first_gtid= false;
  while (!done &&
         (ev= Log_event::read_log_event(&log, 0, fd_ev_p, verify_checksum)) !=
         NULL)
  {
#ifndef DBUG_OFF
    event_counter++;
#endif
    DBUG_PRINT("info", ("Read event of type %s", ev->get_type_str()));
    switch (ev->get_type_code())
    {
    case binary_log::FORMAT_DESCRIPTION_EVENT:
      if (fd_ev_p != &fd_ev)
        delete fd_ev_p;
      fd_ev_p= (Format_description_log_event *)ev;
      break;
    case binary_log::ROTATE_EVENT:
      // do nothing; just accept this event and go to next
      break;
    case binary_log::PREVIOUS_GTIDS_LOG_EVENT:
    {
      ret= GOT_PREVIOUS_GTIDS;
      // add events to sets
      Previous_gtids_log_event *prev_gtids_ev=
        (Previous_gtids_log_event *)ev;
      if (all_gtids != NULL && prev_gtids_ev->add_to_set(all_gtids) != 0)
        ret= ERROR, done= true;
      else if (prev_gtids != NULL && prev_gtids_ev->add_to_set(prev_gtids) != 0)
        ret= ERROR, done= true;
#ifndef DBUG_OFF
      char* prev_buffer= prev_gtids_ev->get_str(NULL, NULL);
      DBUG_PRINT("info", ("Got Previous_gtids from file '%s': Gtid_set='%s'.",
                          filename, prev_buffer));
      my_free(prev_buffer);
#endif
      /*
        If this is not a relay log, the previous_gtids were asked and no
        all_gtids neither first_gtid were asked, it is fine to consider the
        job as done.
      */
      if (!is_relay_log && prev_gtids != NULL &&
          all_gtids == NULL && first_gtid == NULL)
        done= true;
      DBUG_EXECUTE_IF("inject_fault_bug16502579", {
                      DBUG_PRINT("debug", ("PREVIOUS_GTIDS_LOG_EVENT found. "
                                           "Injected ret=NO_GTIDS."));
                      if (ret == GOT_PREVIOUS_GTIDS)
                      {
                        ret=NO_GTIDS;
                        done= false;
                      }
                      });
      break;
    }
    case binary_log::GTID_LOG_EVENT:
    {
      if (ret != GOT_GTIDS)
      {
        if (ret != GOT_PREVIOUS_GTIDS)
        {
          /*
            Since this routine is run on startup, there may not be a
            THD instance. Therefore, ER(X) cannot be used.
           */
          const char* msg_fmt= (current_thd != NULL) ?
                               ER(ER_BINLOG_LOGICAL_CORRUPTION) :
                               ER_DEFAULT(ER_BINLOG_LOGICAL_CORRUPTION);
          my_printf_error(ER_BINLOG_LOGICAL_CORRUPTION,
                          msg_fmt, MYF(0),
                          filename,
                          "The first global transaction identifier was read, but "
                          "no other information regarding identifiers existing "
                          "on the previous log files was found.");
          ret= ERROR, done= true;
          break;
        }
        else
          ret= GOT_GTIDS;
      }
      /*
        When this is a relaylog, we just check if the relay log contains at
        least one Gtid_log_event, so that we can distinguish the return values
        GOT_GTID and GOT_PREVIOUS_GTIDS. We don't need to read anything else
        from the relay log.
        When this is a binary log, if all_gtids is requested (i.e., NOT NULL),
        we should continue to read all gtids. If just first_gtid was requested,
        we will be done after storing this Gtid_log_event info on it.
      */
      if (is_relay_log)
      {
        ret= GOT_GTIDS, done= true;
      }
      else
      {
        Gtid_log_event *gtid_ev= (Gtid_log_event *)ev;
        rpl_sidno sidno= gtid_ev->get_sidno(sid_map);
        if (sidno < 0)
          ret= ERROR, done= true;
        else
        {
          if (all_gtids)
          {
            if (all_gtids->ensure_sidno(sidno) != RETURN_STATUS_OK)
              ret= ERROR, done= true;
            all_gtids->_add_gtid(sidno, gtid_ev->get_gno());
            DBUG_PRINT("info", ("Got Gtid from file '%s': Gtid(%d, %lld).",
                                filename, sidno, gtid_ev->get_gno()));
          }

          /* If the first GTID was requested, stores it */
          if (first_gtid && !seen_first_gtid)
          {
            first_gtid->set(sidno, gtid_ev->get_gno());
            seen_first_gtid= true;
            /* If the first_gtid was the only thing requested, we are done */
            if (all_gtids == NULL)
              ret= GOT_GTIDS, done= true;
          }
        }
      }
      break;
    }
    case binary_log::ANONYMOUS_GTID_LOG_EVENT:
    {
      /*
        When this is a relaylog, we just check if it contains
        at least one Anonymous_gtid_log_event after initialization
        (FDs, Rotates and PREVIOUS_GTIDS), so that we can distinguish the
        return values GOT_GTID and GOT_PREVIOUS_GTIDS.
        We don't need to read anything else from the relay log.
      */
      if (is_relay_log)
      {
        ret= GOT_GTIDS;
        done= true;
        break;
      }
      DBUG_ASSERT(prev_gtids == NULL ? true : all_gtids != NULL ||
                                              first_gtid != NULL);
    }
    // Fall through.
    default:
      // if we found any other event type without finding a
      // previous_gtids_log_event, then the rest of this binlog
      // cannot contain gtids
      if (ret != GOT_GTIDS && ret != GOT_PREVIOUS_GTIDS)
        done= true;
      /*
        The GTIDs of the relaylog files will be handled later
        because of the possibility of transactions be spanned
        along distinct relaylog files.
        So, if we found an ordinary event without finding the
        GTID but we already found the PREVIOUS_GTIDS, this probably
        means that the event is from a transaction that started on
        previous relaylog file.
      */
      if (ret == GOT_PREVIOUS_GTIDS && is_relay_log)
        done= true;
      break;
    }
    if (ev != fd_ev_p)
      delete ev;
    DBUG_PRINT("info", ("done=%d", done));
  }

  if (log.error < 0)
  {
    // This is not a fatal error; the log may just be truncated.

    // @todo but what other errors could happen? IO error?
    sql_print_warning("Error reading GTIDs from binary log: %d", log.error);
  }

  if (fd_ev_p != &fd_ev)
  {
    delete fd_ev_p;
    fd_ev_p= &fd_ev;
  }

  mysql_file_close(file, MYF(MY_WME));
  end_io_cache(&log);

  if (all_gtids)
    all_gtids->dbug_print("all_gtids");
  else
    DBUG_PRINT("info", ("all_gtids==NULL"));
  if (prev_gtids)
    prev_gtids->dbug_print("prev_gtids");
  else
    DBUG_PRINT("info", ("prev_gtids==NULL"));
  if (first_gtid == NULL)
    DBUG_PRINT("info", ("first_gtid==NULL"));
  else if (first_gtid->sidno == 0)
    DBUG_PRINT("info", ("first_gtid.sidno==0"));
  else
    first_gtid->dbug_print(sid_map, "first_gtid");

  DBUG_PRINT("info", ("returning %d", ret));
#ifndef DBUG_OFF
  if (!is_relay_log && prev_gtids != NULL &&
      all_gtids == NULL && first_gtid == NULL)
    sql_print_information("Read %lu events from binary log file '%s' to "
                          "determine the GTIDs purged from binary logs.",
                          event_counter, filename);
#endif
  DBUG_RETURN(ret);
}

bool MYSQL_BIN_LOG::find_first_log_not_in_gtid_set(char *binlog_file_name,
                                                   const Gtid_set *gtid_set,
                                                   Gtid *first_gtid,
                                                   const char **errmsg)
{
  DBUG_ENTER("MYSQL_BIN_LOG::gtid_read_start_binlog");
  /*
    Gather the set of files to be accessed.
  */
  list<string> filename_list;
  LOG_INFO linfo;
  int error;

  list<string>::reverse_iterator rit;
  Gtid_set binlog_previous_gtid_set(gtid_set->get_sid_map());

  mysql_mutex_lock(&LOCK_index);
  for (error= find_log_pos(&linfo, NULL, false/*need_lock_index=false*/);
       !error; error= find_next_log(&linfo, false/*need_lock_index=false*/))
  {
    DBUG_PRINT("info", ("read log filename '%s'", linfo.log_file_name));
    filename_list.push_back(string(linfo.log_file_name));
  }
  mysql_mutex_unlock(&LOCK_index);
  if (error != LOG_INFO_EOF)
  {
    *errmsg= "Failed to read the binary log index file while "
      "looking for the oldest binary log that contains any GTID "
      "that is not in the given gtid set";
    error= -1;
    goto end;
  }

  if (filename_list.empty())
  {
    *errmsg= "Could not find first log file name in binary log index file "
      "while looking for the oldest binary log that contains any GTID "
      "that is not in the given gtid set";
    error= -2;
    goto end;
  }

  /*
    Iterate over all the binary logs in reverse order, and read only
    the Previous_gtids_log_event, to find the first one, that is the
    subset of the given gtid set. Since every binary log begins with
    a Previous_gtids_log_event, that contains all GTIDs in all
    previous binary logs.
    We also ask for the first GTID in the binary log to know if we
    should send the FD event with the "created" field cleared or not.
  */
  DBUG_PRINT("info", ("Iterating backwards through binary logs, and reading "
                      "only the Previous_gtids_log_event, to find the first "
                      "one, that is the subset of the given gtid set."));
  rit= filename_list.rbegin();
  error= 0;
  while (rit != filename_list.rend())
  {
    const char *filename= rit->c_str();
    DBUG_PRINT("info", ("Read Previous_gtids_log_event from filename='%s'",
                        filename));
    switch (read_gtids_from_binlog(filename, NULL, &binlog_previous_gtid_set,
                                   first_gtid,
                                   binlog_previous_gtid_set.get_sid_map(),
                                   opt_master_verify_checksum, is_relay_log))
    {
    case ERROR:
      *errmsg= "Error reading header of binary log while looking for "
        "the oldest binary log that contains any GTID that is not in "
        "the given gtid set";
      error= -3;
      goto end;
    case NO_GTIDS:
      *errmsg= "Found old binary log without GTIDs while looking for "
        "the oldest binary log that contains any GTID that is not in "
        "the given gtid set";
      error= -4;
      goto end;
    case GOT_GTIDS:
    case GOT_PREVIOUS_GTIDS:
      if (binlog_previous_gtid_set.is_subset(gtid_set))
      {
        strcpy(binlog_file_name, filename);
        /*
          Verify that the selected binlog is not the first binlog,
        */
        DBUG_EXECUTE_IF("slave_reconnect_with_gtid_set_executed",
                        DBUG_ASSERT(strcmp(filename_list.begin()->c_str(),
                                           binlog_file_name) != 0););
        goto end;
      }
    case TRUNCATED:
      break;
    }
    binlog_previous_gtid_set.clear();

    rit++;
  }

  if (rit == filename_list.rend())
  {
    *errmsg= ER(ER_MASTER_HAS_PURGED_REQUIRED_GTIDS);
    error= -5;
  }

end:
  if (error)
    DBUG_PRINT("error", ("'%s'", *errmsg));
  filename_list.clear();
  DBUG_PRINT("info", ("returning %d", error));
  DBUG_RETURN(error != 0 ? true : false);
}

bool MYSQL_BIN_LOG::init_gtid_sets(Gtid_set *all_gtids, Gtid_set *lost_gtids,
                                   bool verify_checksum, bool need_lock,
                                   Transaction_boundary_parser *trx_parser,
                                   Gtid *gtid_partial_trx,
                                   bool is_server_starting)
{
  DBUG_ENTER("MYSQL_BIN_LOG::init_gtid_sets");
  DBUG_PRINT("info", ("lost_gtids=%p; so we are recovering a %s log; is_relay_log=%d",
                      lost_gtids, lost_gtids == NULL ? "relay" : "binary",
                      is_relay_log));

  /*
    If this is a relay log, we must have the IO thread Master_info trx_parser
    in order to correctly feed it with relay log events.
  */
#ifndef DBUG_OFF
  if (is_relay_log)
  {
    DBUG_ASSERT(trx_parser != NULL);
    DBUG_ASSERT(lost_gtids == NULL);
  }
#endif

  /*
    Acquires the necessary locks to ensure that logs are not either
    removed or updated when we are reading from it.
  */
  if (need_lock)
  {
    // We don't need LOCK_log if we are only going to read the initial
    // Prevoius_gtids_log_event and ignore the Gtid_log_events.
    if (all_gtids != NULL)
      mysql_mutex_lock(&LOCK_log);
    mysql_mutex_lock(&LOCK_index);
    global_sid_lock->wrlock();
  }
  else
  {
    if (all_gtids != NULL)
      mysql_mutex_assert_owner(&LOCK_log);
    mysql_mutex_assert_owner(&LOCK_index);
    global_sid_lock->assert_some_wrlock();
  }

  // Gather the set of files to be accessed.
  list<string> filename_list;
  LOG_INFO linfo;
  int error;

  list<string>::iterator it;
  list<string>::reverse_iterator rit;
  bool reached_first_file= false;

  /* Initialize the sid_map to be used in read_gtids_from_binlog */
  Sid_map *sid_map= NULL;
  if (all_gtids)
    sid_map= all_gtids->get_sid_map();
  else if (lost_gtids)
    sid_map= lost_gtids->get_sid_map();

  for (error= find_log_pos(&linfo, NULL, false/*need_lock_index=false*/); !error;
       error= find_next_log(&linfo, false/*need_lock_index=false*/))
  {
    DBUG_PRINT("info", ("read log filename '%s'", linfo.log_file_name));
    filename_list.push_back(string(linfo.log_file_name));
  }
  if (error != LOG_INFO_EOF)
  {
    DBUG_PRINT("error", ("Error reading %s index",
                         is_relay_log ? "relaylog" : "binlog"));
    goto end;
  }
  /*
    On server starting, one new empty binlog file is created and
    its file name is put into index file before initializing
    GLOBAL.GTID_EXECUTED AND GLOBAL.GTID_PURGED, it is not the
    last binlog file before the server restarts, so we remove
    its file name from filename_list.
  */
  if (is_server_starting && !is_relay_log && !filename_list.empty())
    filename_list.pop_back();

  error= 0;

  if (all_gtids != NULL)
  {
    DBUG_PRINT("info", ("Iterating backwards through %s logs, "
                        "looking for the last %s log that contains "
                        "a Previous_gtids_log_event.",
                        is_relay_log ? "relay" : "binary",
                        is_relay_log ? "relay" : "binary"));
    // Iterate over all files in reverse order until we find one that
    // contains a Previous_gtids_log_event.
    rit= filename_list.rbegin();
    bool can_stop_reading= false;
    reached_first_file= (rit == filename_list.rend());
    DBUG_PRINT("info", ("filename='%s' reached_first_file=%d",
                        reached_first_file ? "" : rit->c_str(),
                        reached_first_file));
    while (!can_stop_reading && !reached_first_file)
    {
      const char *filename= rit->c_str();
      DBUG_ASSERT(rit != filename_list.rend());
      rit++;
      reached_first_file= (rit == filename_list.rend());
      DBUG_PRINT("info", ("filename='%s' can_stop_reading=%d "
                          "reached_first_file=%d, ",
                          filename, can_stop_reading, reached_first_file));
      switch (read_gtids_from_binlog(filename, all_gtids,
                                     reached_first_file ? lost_gtids : NULL,
                                     NULL/* first_gtid */,
                                     sid_map, verify_checksum, is_relay_log))
      {
        case ERROR:
        {
          error= 1;
          goto end;
        }
        case GOT_GTIDS:
        {
          can_stop_reading= true;
          break;
        }
        case GOT_PREVIOUS_GTIDS:
        {
          /*
            If this is a binlog file, it is enough to have GOT_PREVIOUS_GTIDS.
            If this is a relaylog file, we need to find at least one GTID to
            start parsing the relay log to add GTID of transactions that might
            have spanned in distinct relaylog files.
          */
          if (!is_relay_log)
            can_stop_reading= true;
          break;
        }
        case NO_GTIDS:
        {
          /*
            Mysql server iterates backwards through binary logs, looking for
            the last binary log that contains a Previous_gtids_log_event for
            gathering the set of gtid_executed on server start. This may take
            very long time if it has many binary logs and almost all of them
            are out of filesystem cache. So if the binlog_gtid_simple_recovery
            is enabled, and the last binary log does not contain any GTID
            event, do not read any more binary logs, GLOBAL.GTID_EXECUTED and
            GLOBAL.GTID_PURGED should be empty in the case.
          */
          if (binlog_gtid_simple_recovery && is_server_starting &&
              !is_relay_log)
          {
            DBUG_ASSERT(all_gtids->is_empty());
            DBUG_ASSERT(lost_gtids->is_empty());
            goto end;
          }
          /*FALLTHROUGH*/
        }
        case TRUNCATED:
        {
          break;
        }
      }
    }

    /*
      If we use GTIDs and have partial transactions on the relay log,
      must check if it ends on next relay log files.
      We also need to feed the boundary parser with the rest of the
      relay log to put it in the correct state before receiving new
      events from the master in the case of GTID auto positioning be
      disabled.
    */
    if (is_relay_log && filename_list.size() > 0)
    {
      /*
        Suppose the following relaylog:

         rl-bin.000001 | rl-bin.000002 | rl-bin.000003 | rl-bin-000004
        ---------------+---------------+---------------+---------------
         PREV_GTIDS    | PREV_GTIDS    | PREV_GTIDS    | PREV_GTIDS
         (empty)       | (UUID:1)      | (UUID:1)      | (UUID:1)
        ---------------+---------------+---------------+---------------
         GTID(UUID:1)  | QUERY(INSERT) | QUERY(INSERT) | XID
        ---------------+---------------+---------------+---------------
         QUERY(CREATE  |
         TABLE t1 ...) |
        ---------------+
         GTID(UUID:2)  |
        ---------------+
         QUERY(BEGIN)  |
        ---------------+

        As it is impossible to determine the current Retrieved_Gtid_Set by only
        looking to the PREVIOUS_GTIDS on the last relay log file, and scanning
        events on it, we tried to find a relay log file that contains at least
        one GTID event during the backwards search.

        In the example, we will find a GTID only in rl-bin.000001, as the
        UUID:2 transaction was spanned across 4 relay log files.

        The transaction spanning can be caused by "FLUSH RELAY LOGS" commands
        on slave while it is queuing the transaction.

        So, in order to correctly add UUID:2 into Retrieved_Gtid_Set, we need
        to parse the relay log starting on the file we found the last GTID
        queued to know if the transaction was fully retrieved or not.
      */

      /*
        Adjust the reverse iterator to point to the relaylog file we
        need to start parsing, as it was incremented after generating
        the relay log file name.
      */
      DBUG_ASSERT(rit != filename_list.rbegin());
      rit--;
      DBUG_ASSERT(rit != filename_list.rend());
      /* Reset the transaction parser before feeding it with events */
      trx_parser->reset();
      gtid_partial_trx->clear();

      DBUG_PRINT("info", ("Iterating forwards through relay logs, "
                          "updating the Retrieved_Gtid_Set and updating "
                          "IO thread trx parser before start."));
      for (it= find(filename_list.begin(), filename_list.end(), *rit);
           it != filename_list.end(); it++)
      {
        const char *filename= it->c_str();
        DBUG_PRINT("info", ("filename='%s'", filename));
        if (read_gtids_and_update_trx_parser_from_relaylog(filename, all_gtids,
                                                           true, trx_parser,
                                                           gtid_partial_trx))
        {
          error= 1;
          goto end;
        }
      }
    }
  }
  if (lost_gtids != NULL && !reached_first_file)
  {
    /*
      This branch is only reacheable by a binary log. The relay log
      don't need to get lost_gtids information.

      A 5.6 server sets GTID_PURGED by rotating the binary log.

      A 5.6 server that had recently enabled GTIDs and set GTID_PURGED
      would have a sequence of binary logs like:

      master-bin.N  : No PREVIOUS_GTIDS (GTID wasn't enabled)
      master-bin.N+1: Has an empty PREVIOUS_GTIDS and a ROTATE
                      (GTID was enabled on startup)
      master-bin.N+2: Has a PREVIOUS_GTIDS with the content set by a
                      SET @@GLOBAL.GTID_PURGED + has GTIDs of some
                      transactions.

      If this 5.6 server be upgraded to 5.7 keeping its binary log files,
      this routine will have to find the first binary log that contains a
      PREVIOUS_GTIDS + a GTID event to ensure that the content of the
      GTID_PURGED will be correctly set (assuming binlog_gtid_simple_recovery
      is not enabled).
    */
    DBUG_PRINT("info", ("Iterating forwards through binary logs, looking for "
                        "the first binary log that contains both a "
                        "Previous_gtids_log_event and a Gtid_log_event."));
    DBUG_ASSERT(!is_relay_log);
    for (it= filename_list.begin(); it != filename_list.end(); it++)
    {
      /*
        We should pass a first_gtid to read_gtids_from_binlog when
        binlog_gtid_simple_recovery is disabled, or else it will return
        right after reading the PREVIOUS_GTIDS event to avoid stall on
        reading the whole binary log.
      */
      Gtid first_gtid= {0, 0};
      const char *filename= it->c_str();
      DBUG_PRINT("info", ("filename='%s'", filename));
      switch (read_gtids_from_binlog(filename, NULL, lost_gtids,
                                     binlog_gtid_simple_recovery ? NULL :
                                                                   &first_gtid,
                                     sid_map, verify_checksum, is_relay_log))
      {
        case ERROR:
        {
          error= 1;
          /*FALLTHROUGH*/
        }
        case GOT_GTIDS:
        {
          goto end;
        }
        case NO_GTIDS:
        case GOT_PREVIOUS_GTIDS:
        {
          /*
            Mysql server iterates forwards through binary logs, looking for
            the first binary log that contains both Previous_gtids_log_event
            and gtid_log_event for gathering the set of gtid_purged on server
            start. It also iterates forwards through binary logs, looking for
            the first binary log that contains both Previous_gtids_log_event
            and gtid_log_event for gathering the set of gtid_purged when
            purging binary logs. This may take very long time if it has many
            binary logs and almost all of them are out of filesystem cache.
            So if the binlog_gtid_simple_recovery is enabled, we just
            initialize GLOBAL.GTID_PURGED from the first binary log, do not
            read any more binary logs.
          */
          if (binlog_gtid_simple_recovery)
            goto end;
          /*FALLTHROUGH*/
        }
        case TRUNCATED:
        {
          break;
        }
      }
    }
  }
end:
  if (all_gtids)
    all_gtids->dbug_print("all_gtids");
  if (lost_gtids)
    lost_gtids->dbug_print("lost_gtids");
  if (need_lock)
  {
    global_sid_lock->unlock();
    mysql_mutex_unlock(&LOCK_index);
    if (all_gtids != NULL)
      mysql_mutex_unlock(&LOCK_log);
  }
  filename_list.clear();
  DBUG_PRINT("info", ("returning %d", error));
  DBUG_RETURN(error != 0 ? true : false);
}


/**
  Open a (new) binlog file.

  - Open the log file and the index file. Register the new
  file name in it
  - When calling this when the file is in use, you must have a locks
  on LOCK_log and LOCK_index.

  @retval
    0	ok
  @retval
    1	error
*/

bool MYSQL_BIN_LOG::open_binlog(const char *log_name,
                                const char *new_name,
                                ulong max_size_arg,
                                bool null_created_arg,
                                bool need_lock_index,
                                bool need_sid_lock,
                                Format_description_log_event *extra_description_event)
{
  // lock_index must be acquired *before* sid_lock.
  DBUG_ASSERT(need_sid_lock || !need_lock_index);
  DBUG_ENTER("MYSQL_BIN_LOG::open_binlog(const char *, ...)");
  DBUG_PRINT("enter",("base filename: %s", log_name));

  mysql_mutex_assert_owner(get_log_lock());

  if (init_and_set_log_file_name(log_name, new_name))
  {
    sql_print_error("MYSQL_BIN_LOG::open failed to generate new file name.");
    DBUG_RETURN(1);
  }

  DBUG_PRINT("info", ("generated filename: %s", log_file_name));

  DEBUG_SYNC(current_thd, "after_log_file_name_initialized");

#ifdef HAVE_REPLICATION
  if (open_purge_index_file(TRUE) ||
      register_create_index_entry(log_file_name) ||
      sync_purge_index_file() ||
      DBUG_EVALUATE_IF("fault_injection_registering_index", 1, 0))
  {
    /**
      @todo: although this was introduced to appease valgrind
      when injecting emulated faults using fault_injection_registering_index
      it may be good to consider what actually happens when
      open_purge_index_file succeeds but register or sync fails.

      Perhaps we might need the code below in MYSQL_BIN_LOG::cleanup
      for "real life" purposes as well? 
    */
    DBUG_EXECUTE_IF("fault_injection_registering_index", {
      if (my_b_inited(&purge_index_file))
      {
        end_io_cache(&purge_index_file);
        my_close(purge_index_file.file, MYF(0));
      }
    });

    sql_print_error("MYSQL_BIN_LOG::open failed to sync the index file.");
    DBUG_RETURN(1);
  }
  DBUG_EXECUTE_IF("crash_create_non_critical_before_update_index", DBUG_SUICIDE(););
#endif

  write_error= 0;

  /* open the main log file */
  if (open(
#ifdef HAVE_PSI_INTERFACE
                      m_key_file_log,
#endif
                      log_name, new_name))
  {
#ifdef HAVE_REPLICATION
    close_purge_index_file();
#endif
    DBUG_RETURN(1);                            /* all warnings issued */
  }

  max_size= max_size_arg;

  open_count++;

  bool write_file_name_to_index_file=0;

  /* This must be before goto err. */
#ifndef DBUG_OFF
  binary_log_debug::debug_pretend_version_50034_in_binlog=
    DBUG_EVALUATE_IF("pretend_version_50034_in_binlog", true, false);
#endif
  Format_description_log_event s(BINLOG_VERSION);

  if (!my_b_filelength(&log_file))
  {
    /*
      The binary log file was empty (probably newly created)
      This is the normal case and happens when the user doesn't specify
      an extension for the binary log files.
      In this case we write a standard header to it.
    */
    if (my_b_safe_write(&log_file, (uchar*) BINLOG_MAGIC,
                        BIN_LOG_HEADER_SIZE))
      goto err;
    bytes_written+= BIN_LOG_HEADER_SIZE;
    write_file_name_to_index_file= 1;
  }

  /*
    don't set LOG_EVENT_BINLOG_IN_USE_F for SEQ_READ_APPEND io_cache
    as we won't be able to reset it later
  */
  if (io_cache_type == WRITE_CACHE)
  {
    s.common_header->flags|= LOG_EVENT_BINLOG_IN_USE_F;
  }

  if (is_relay_log)
  {
    /* relay-log */
    if (relay_log_checksum_alg == binary_log::BINLOG_CHECKSUM_ALG_UNDEF)
    {
      /* inherit master's A descriptor if one has been received */
      if (opt_slave_sql_verify_checksum == 0)
        /* otherwise use slave's local preference of RL events verification */
        relay_log_checksum_alg= binary_log::BINLOG_CHECKSUM_ALG_OFF;
      else
        relay_log_checksum_alg= static_cast<enum_binlog_checksum_alg>
                                (binlog_checksum_options);
    }
    s.common_footer->checksum_alg= relay_log_checksum_alg;
  }
  else
    /* binlog */
    s.common_footer->checksum_alg= static_cast<enum_binlog_checksum_alg>
                                     (binlog_checksum_options);

  DBUG_ASSERT((s.common_footer)->checksum_alg !=
               binary_log::BINLOG_CHECKSUM_ALG_UNDEF);
  if (!s.is_valid())
    goto err;
  s.dont_set_created= null_created_arg;
  /* Set LOG_EVENT_RELAY_LOG_F flag for relay log's FD */
  if (is_relay_log)
    s.set_relay_log_event();
  if (s.write(&log_file))
    goto err;
  bytes_written+= s.common_header->data_written;
  /*
    We need to revisit this code and improve it.
    See further comments in the mysqld.
    /Alfranio
  */
  if (current_thd)
  {
    Gtid_set logged_gtids_binlog(global_sid_map, global_sid_lock);
    Gtid_set* previous_logged_gtids;

    if (is_relay_log)
      previous_logged_gtids= previous_gtid_set_relaylog;
    else
      previous_logged_gtids= &logged_gtids_binlog;

    if (need_sid_lock)
      global_sid_lock->wrlock();
    else
      global_sid_lock->assert_some_wrlock();

    if (!is_relay_log)
    {
      const Gtid_set *executed_gtids= gtid_state->get_executed_gtids();
      const Gtid_set *gtids_only_in_table=
        gtid_state->get_gtids_only_in_table();
      /* logged_gtids_binlog= executed_gtids - gtids_only_in_table */
      if (logged_gtids_binlog.add_gtid_set(executed_gtids) !=
          RETURN_STATUS_OK)
      {
        if (need_sid_lock)
          global_sid_lock->unlock();
        goto err;
      }
      logged_gtids_binlog.remove_gtid_set(gtids_only_in_table);
    }
    DBUG_PRINT("info",("Generating PREVIOUS_GTIDS for %s file.",
                       is_relay_log ? "relaylog" : "binlog"));
    Previous_gtids_log_event prev_gtids_ev(previous_logged_gtids);
    if (is_relay_log)
      prev_gtids_ev.set_relay_log_event();
    if (need_sid_lock)
      global_sid_lock->unlock();
    prev_gtids_ev.common_footer->checksum_alg=
                                   (s.common_footer)->checksum_alg;
    if (prev_gtids_ev.write(&log_file))
      goto err;
    bytes_written+= prev_gtids_ev.common_header->data_written;
  }
  else // !(current_thd)
  {
    /*
      If the slave was configured before server restart, the server will
      generate a new relay log file without having current_thd, but this
      new relay log file must have a PREVIOUS_GTIDS event as we now
      generate the PREVIOUS_GTIDS event always.

      This is only needed for relay log files because the server will add
      the PREVIOUS_GTIDS of binary logs (when current_thd==NULL) after
      server's GTID initialization.

      During server's startup at mysqld_main(), from the binary/relay log
      initialization point of view, it will:
      1) Call init_server_components() that will generate a new binary log
         file but won't write the PREVIOUS_GTIDS event yet;
      2) Initialize server's GTIDs;
      3) Write the binary log PREVIOUS_GTIDS;
      4) Call init_slave() in where the new relay log file will be created
         after initializing relay log's Retrieved_Gtid_Set;
    */
    if (is_relay_log)
    {
      if (need_sid_lock)
        global_sid_lock->wrlock();
      else
        global_sid_lock->assert_some_wrlock();

      DBUG_PRINT("info",("Generating PREVIOUS_GTIDS for relaylog file."));
      Previous_gtids_log_event prev_gtids_ev(previous_gtid_set_relaylog);
      prev_gtids_ev.set_relay_log_event();

      if (need_sid_lock)
        global_sid_lock->unlock();

      prev_gtids_ev.common_footer->checksum_alg=
                                   (s.common_footer)->checksum_alg;
      if (prev_gtids_ev.write(&log_file))
        goto err;
      bytes_written+= prev_gtids_ev.common_header->data_written;
    }
  }
  if (extra_description_event &&
      extra_description_event->binlog_version>=4)
  {
    /*
      This is a relay log written to by the I/O slave thread.
      Write the event so that others can later know the format of this relay
      log.
      Note that this event is very close to the original event from the
      master (it has binlog version of the master, event types of the
      master), so this is suitable to parse the next relay log's event. It
      has been produced by
      Format_description_log_event::Format_description_log_event(char* buf,).
      Why don't we want to write the mi_description_event if this
      event is for format<4 (3.23 or 4.x): this is because in that case, the
      mi_description_event describes the data received from the
      master, but not the data written to the relay log (*conversion*),
      which is in format 4 (slave's).
    */
    /*
      Set 'created' to 0, so that in next relay logs this event does not
      trigger cleaning actions on the slave in
      Format_description_log_event::apply_event_impl().
    */
    extra_description_event->created= 0;
    /* Don't set log_pos in event header */
    extra_description_event->set_artificial_event();

    if (extra_description_event->write(&log_file))
      goto err;
    bytes_written+= extra_description_event->common_header->data_written;
  }
  if (flush_io_cache(&log_file) ||
      mysql_file_sync(log_file.file, MYF(MY_WME)))
    goto err;
  
  if (write_file_name_to_index_file)
  {
#ifdef HAVE_REPLICATION
    DBUG_EXECUTE_IF("crash_create_critical_before_update_index", DBUG_SUICIDE(););
#endif

    DBUG_ASSERT(my_b_inited(&index_file) != 0);

    /*
      The new log file name is appended into crash safe index file after
      all the content of index file is copyed into the crash safe index
      file. Then move the crash safe index file to index file.
    */
    DBUG_EXECUTE_IF("simulate_disk_full_on_open_binlog",
                    {DBUG_SET("+d,simulate_no_free_space_error");});
    if (DBUG_EVALUATE_IF("fault_injection_updating_index", 1, 0) ||
        add_log_to_index((uchar*) log_file_name, strlen(log_file_name),
                         need_lock_index))
    {
      DBUG_EXECUTE_IF("simulate_disk_full_on_open_binlog",
                      {
                        DBUG_SET("-d,simulate_file_write_error");
                        DBUG_SET("-d,simulate_no_free_space_error");
                        DBUG_SET("-d,simulate_disk_full_on_open_binlog");
                      });
      goto err;
    }

#ifdef HAVE_REPLICATION
    DBUG_EXECUTE_IF("crash_create_after_update_index", DBUG_SUICIDE(););
#endif
  }

  log_state.atomic_set(LOG_OPENED);
  /*
    At every rotate memorize the last transaction counter state to use it as
    offset at logging the transaction logical timestamps.
  */
  m_dependency_tracker.rotate();
#ifdef HAVE_REPLICATION
  close_purge_index_file();
#endif

  update_binlog_end_pos();
  DBUG_RETURN(0);

err:
#ifdef HAVE_REPLICATION
  if (is_inited_purge_index_file())
    purge_index_entry(NULL, NULL, need_lock_index);
  close_purge_index_file();
#endif
  if (binlog_error_action == ABORT_SERVER)
  {
    exec_binlog_error_action_abort("Either disk is full or file system is read "
                                   "only while opening the binlog. Aborting the"
                                   " server.");
  }
  else
  {
    sql_print_error("Could not use %s for logging (error %d). "
                    "Turning logging off for the whole duration of the MySQL "
                    "server process. To turn it on again: fix the cause, "
                    "shutdown the MySQL server and restart it.",
                    (new_name) ? new_name : name, errno);
    close(LOG_CLOSE_INDEX, false, need_lock_index);
  }
  DBUG_RETURN(1);
}


/**
  Move crash safe index file to index file.

  @param need_lock_index If true, LOCK_index will be acquired;
  otherwise it should already be held.

  @retval 0 ok
  @retval -1 error
*/
int MYSQL_BIN_LOG::move_crash_safe_index_file_to_index_file(bool need_lock_index)
{
  int error= 0;
  File fd= -1;
  DBUG_ENTER("MYSQL_BIN_LOG::move_crash_safe_index_file_to_index_file");

  if (need_lock_index)
    mysql_mutex_lock(&LOCK_index);
  else
    mysql_mutex_assert_owner(&LOCK_index);

  if (my_b_inited(&index_file))
  {
    end_io_cache(&index_file);
    if (mysql_file_close(index_file.file, MYF(0)) < 0)
    {
      error= -1;
      sql_print_error("While rebuilding index file %s: "
                      "Failed to close the index file.", index_file_name);
      /*
        Delete Crash safe index file here and recover the binlog.index
        state(index_file io_cache) from old binlog.index content.
       */
      mysql_file_delete(key_file_binlog_index, crash_safe_index_file_name,
                        MYF(0));

      goto recoverable_err;
    }
    if (DBUG_EVALUATE_IF("force_index_file_delete_failure", 1, 0) ||
        mysql_file_delete(key_file_binlog_index, index_file_name, MYF(MY_WME)))
    {
      error= -1;
      sql_print_error("While rebuilding index file %s: "
                      "Failed to delete the existing index file. It could be "
                      "that file is being used by some other process.",
                      index_file_name);
      /*
        Delete Crash safe file index file here and recover the binlog.index
        state(index_file io_cache) from old binlog.index content.
       */
      mysql_file_delete(key_file_binlog_index, crash_safe_index_file_name,
                        MYF(0));

      goto recoverable_err;
    }
  }

  DBUG_EXECUTE_IF("crash_create_before_rename_index_file", DBUG_SUICIDE(););
  if (my_rename(crash_safe_index_file_name, index_file_name, MYF(MY_WME)))
  {
    error= -1;
    sql_print_error("While rebuilding index file %s: "
                    "Failed to rename the new index file to the existing "
                    "index file.", index_file_name);
    goto fatal_err;
  }
  DBUG_EXECUTE_IF("crash_create_after_rename_index_file", DBUG_SUICIDE(););

recoverable_err:
  if ((fd= mysql_file_open(key_file_binlog_index,
                           index_file_name,
                           O_RDWR | O_CREAT | O_BINARY,
                           MYF(MY_WME))) < 0 ||
           mysql_file_sync(fd, MYF(MY_WME)) ||
           init_io_cache_ext(&index_file, fd, IO_SIZE, READ_CACHE,
                             mysql_file_seek(fd, 0L, MY_SEEK_END, MYF(0)),
                                             0, MYF(MY_WME | MY_WAIT_IF_FULL),
                             key_file_binlog_index_cache))
  {
    sql_print_error("After rebuilding the index file %s: "
                    "Failed to open the index file.", index_file_name);
    goto fatal_err;
  }

  if (need_lock_index)
    mysql_mutex_unlock(&LOCK_index);
  DBUG_RETURN(error);

fatal_err:
  /*
    This situation is very very rare to happen (unless there is some serious
    memory related issues like OOM) and should be treated as fatal error.
    Hence it is better to bring down the server without respecting
    'binlog_error_action' value here.
  */
  exec_binlog_error_action_abort("MySQL server failed to update the "
                                 "binlog.index file's content properly. "
                                 "It might not be in sync with available "
                                 "binlogs and the binlog.index file state is in "
                                 "unrecoverable state. Aborting the server.");
  /*
    Server is aborted in the above function.
    This is dead code to make compiler happy.
   */
  DBUG_RETURN(error);
}


/**
  Append log file name to index file.

  - To make crash safe, we copy all the content of index file
  to crash safe index file firstly and then append the log
  file name to the crash safe index file. Finally move the
  crash safe index file to index file.

  @retval
    0   ok
  @retval
    -1   error
*/
int MYSQL_BIN_LOG::add_log_to_index(uchar* log_name,
                                    size_t log_name_len, bool need_lock_index)
{
  DBUG_ENTER("MYSQL_BIN_LOG::add_log_to_index");

  if (open_crash_safe_index_file())
  {
    sql_print_error("MYSQL_BIN_LOG::add_log_to_index failed to "
                    "open the crash safe index file.");
    goto err;
  }

  if (copy_file(&index_file, &crash_safe_index_file, 0))
  {
    sql_print_error("MYSQL_BIN_LOG::add_log_to_index failed to "
                    "copy index file to crash safe index file.");
    goto err;
  }

  if (my_b_write(&crash_safe_index_file, log_name, log_name_len) ||
      my_b_write(&crash_safe_index_file, (uchar*) "\n", 1) ||
      flush_io_cache(&crash_safe_index_file) ||
      mysql_file_sync(crash_safe_index_file.file, MYF(MY_WME)))
  {
    sql_print_error("MYSQL_BIN_LOG::add_log_to_index failed to "
                    "append log file name: %s, to crash "
                    "safe index file.", log_name);
    goto err;
  }

  if (close_crash_safe_index_file())
  {
    sql_print_error("MYSQL_BIN_LOG::add_log_to_index failed to "
                    "close the crash safe index file.");
    goto err;
  }

  if (move_crash_safe_index_file_to_index_file(need_lock_index))
  {
    sql_print_error("MYSQL_BIN_LOG::add_log_to_index failed to "
                    "move crash safe index file to index file.");
    goto err;
  }

  DBUG_RETURN(0);

err:
  DBUG_RETURN(-1);
}

int MYSQL_BIN_LOG::get_current_log(LOG_INFO* linfo, bool need_lock_log/*true*/)
{
  if (need_lock_log)
    mysql_mutex_lock(&LOCK_log);
  int ret = raw_get_current_log(linfo);
  if (need_lock_log)
    mysql_mutex_unlock(&LOCK_log);
  return ret;
}

int MYSQL_BIN_LOG::raw_get_current_log(LOG_INFO* linfo)
{
  strmake(linfo->log_file_name, log_file_name, sizeof(linfo->log_file_name)-1);
  linfo->pos = my_b_safe_tell(&log_file);
  return 0;
}

bool MYSQL_BIN_LOG::check_write_error(THD *thd)
{
  DBUG_ENTER("MYSQL_BIN_LOG::check_write_error");

  bool checked= FALSE;

  if (!thd->is_error())
    DBUG_RETURN(checked);

  switch (thd->get_stmt_da()->mysql_errno())
  {
    case ER_TRANS_CACHE_FULL:
    case ER_STMT_CACHE_FULL:
    case ER_ERROR_ON_WRITE:
    case ER_BINLOG_LOGGING_IMPOSSIBLE:
      checked= TRUE;
    break;
  }
  DBUG_PRINT("return", ("checked: %s", YESNO(checked)));
  DBUG_RETURN(checked);
}

void MYSQL_BIN_LOG::set_write_error(THD *thd, bool is_transactional)
{
  DBUG_ENTER("MYSQL_BIN_LOG::set_write_error");

  write_error= 1;

  if (check_write_error(thd))
    DBUG_VOID_RETURN;

  if (my_errno() == EFBIG)
  {
    if (is_transactional)
    {
      my_message(ER_TRANS_CACHE_FULL, ER(ER_TRANS_CACHE_FULL), MYF(MY_WME));
    }
    else
    {
      my_message(ER_STMT_CACHE_FULL, ER(ER_STMT_CACHE_FULL), MYF(MY_WME));
    }
  }
  else
  {
    char errbuf[MYSYS_STRERROR_SIZE];
    my_error(ER_ERROR_ON_WRITE, MYF(MY_WME), name,
             errno, my_strerror(errbuf, sizeof(errbuf), errno));
  }

  DBUG_VOID_RETURN;
}

/**
  Find the position in the log-index-file for the given log name.

  @param[out] linfo The found log file name will be stored here, along
  with the byte offset of the next log file name in the index file.
  @param log_name Filename to find in the index file, or NULL if we
  want to read the first entry.
  @param need_lock_index If false, this function acquires LOCK_index;
  otherwise the lock should already be held by the caller.

  @note
    On systems without the truncate function the file will end with one or
    more empty lines.  These will be ignored when reading the file.

  @retval
    0			ok
  @retval
    LOG_INFO_EOF	        End of log-index-file found
  @retval
    LOG_INFO_IO		Got IO error while reading file
*/

int MYSQL_BIN_LOG::find_log_pos(LOG_INFO *linfo, const char *log_name,
                                bool need_lock_index)
{
  int error= 0;
  char *full_fname= linfo->log_file_name;
  char full_log_name[FN_REFLEN], fname[FN_REFLEN];
  size_t log_name_len= 0, fname_len= 0;
  DBUG_ENTER("find_log_pos");
  full_log_name[0]= full_fname[0]= 0;

  /*
    Mutex needed because we need to make sure the file pointer does not
    move from under our feet
  */
  if (need_lock_index)
    mysql_mutex_lock(&LOCK_index);
  else
    mysql_mutex_assert_owner(&LOCK_index);

  if (!my_b_inited(&index_file))
  {
      error= LOG_INFO_IO;
      goto end;
  }

  // extend relative paths for log_name to be searched
  if (log_name)
  {
    if(normalize_binlog_name(full_log_name, log_name, is_relay_log))
    {
      error= LOG_INFO_EOF;
      goto end;
    }
  }

  log_name_len= log_name ? strlen(full_log_name) : 0;
  DBUG_PRINT("enter", ("log_name: %s, full_log_name: %s", 
                       log_name ? log_name : "NULL", full_log_name));

  /* As the file is flushed, we can't get an error here */
  my_b_seek(&index_file, (my_off_t) 0);

  for (;;)
  {
    size_t length;
    my_off_t offset= my_b_tell(&index_file);

    DBUG_EXECUTE_IF("simulate_find_log_pos_error",
                    error=  LOG_INFO_EOF; break;);
    /* If we get 0 or 1 characters, this is the end of the file */
    if ((length= my_b_gets(&index_file, fname, FN_REFLEN)) <= 1)
    {
      /* Did not find the given entry; Return not found or error */
      error= !index_file.error ? LOG_INFO_EOF : LOG_INFO_IO;
      break;
    }

    // extend relative paths and match against full path
    if (normalize_binlog_name(full_fname, fname, is_relay_log))
    {
      error= LOG_INFO_EOF;
      break;
    }
    fname_len= strlen(full_fname);

    // if the log entry matches, null string matching anything
    if (!log_name ||
       (log_name_len == fname_len &&
       !memcmp(full_fname, full_log_name, log_name_len)))
    {
      DBUG_PRINT("info", ("Found log file entry"));
      linfo->index_file_start_offset= offset;
      linfo->index_file_offset = my_b_tell(&index_file);
      break;
    }
    linfo->entry_index++;
  }

end:  
  if (need_lock_index)
    mysql_mutex_unlock(&LOCK_index);
  DBUG_RETURN(error);
}


/**
  Find the position in the log-index-file for the given log name.

  @param[out] linfo The filename will be stored here, along with the
  byte offset of the next filename in the index file.

  @param need_lock_index If true, LOCK_index will be acquired;
  otherwise it should already be held by the caller.

  @note
    - Before calling this function, one has to call find_log_pos()
    to set up 'linfo'
    - Mutex needed because we need to make sure the file pointer does not move
    from under our feet

  @retval 0 ok
  @retval LOG_INFO_EOF End of log-index-file found
  @retval LOG_INFO_IO Got IO error while reading file
*/
int MYSQL_BIN_LOG::find_next_log(LOG_INFO* linfo, bool need_lock_index)
{
  int error= 0;
  size_t length;
  char fname[FN_REFLEN];
  char *full_fname= linfo->log_file_name;

  if (need_lock_index)
    mysql_mutex_lock(&LOCK_index);
  else
    mysql_mutex_assert_owner(&LOCK_index);

  if (!my_b_inited(&index_file))
  {
      error= LOG_INFO_IO;
      goto err;
  }
  /* As the file is flushed, we can't get an error here */
  my_b_seek(&index_file, linfo->index_file_offset);

  linfo->index_file_start_offset= linfo->index_file_offset;
  if ((length=my_b_gets(&index_file, fname, FN_REFLEN)) <= 1)
  {
    error = !index_file.error ? LOG_INFO_EOF : LOG_INFO_IO;
    goto err;
  }

  if (fname[0] != 0)
  {
    if(normalize_binlog_name(full_fname, fname, is_relay_log))
    {
      error= LOG_INFO_EOF;
      goto err;
    }
    length= strlen(full_fname);
  }

  linfo->index_file_offset= my_b_tell(&index_file);

err:
  if (need_lock_index)
    mysql_mutex_unlock(&LOCK_index);
  return error;
}

/**
  Find the relay log name following the given name from relay log index file.

  @param[in|out] log_name  The name is full path name.

  @return return 0 if it finds next relay log. Otherwise return the error code.
*/
int MYSQL_BIN_LOG::find_next_relay_log(char log_name[FN_REFLEN+1])
{
  LOG_INFO info;
  int error;
  char relative_path_name[FN_REFLEN+1];

  if (fn_format(relative_path_name, log_name+dirname_length(log_name),
                mysql_data_home, "", 0)
      == NullS)
    return 1;

  mysql_mutex_lock(&LOCK_index);

  error= find_log_pos(&info, relative_path_name, false);
  if (error == 0)
  {
    error= find_next_log(&info, false);
    if (error == 0)
      strcpy(log_name, info.log_file_name);
  }

  mysql_mutex_unlock(&LOCK_index);
  return error;
}

/**
  Removes files, as part of a RESET MASTER or RESET SLAVE statement,
  by deleting all logs refered to in the index file. Then, it starts
  writing to a new log file.

  The new index file will only contain this file.

  @param thd Thread

  @note
    If not called from slave thread, write start event to new log

  @retval
    0	ok
  @retval
    1   error
*/
bool MYSQL_BIN_LOG::reset_logs(THD* thd, bool delete_only)
{
  LOG_INFO linfo;
  bool error=0;
  int err;
  const char* save_name;
  DBUG_ENTER("reset_logs");

  /*
    Flush logs for storage engines, so that the last transaction
    is fsynced inside storage engines.
  */
  if (ha_flush_logs(NULL))
    DBUG_RETURN(1);

  ha_reset_logs(thd);

  /*
    We need to get both locks to be sure that no one is trying to
    write to the index log file.
  */
  mysql_mutex_lock(&LOCK_log);
  mysql_mutex_lock(&LOCK_index);

  global_sid_lock->wrlock();

  /* Save variables so that we can reopen the log */
  save_name=name;
  name=0;					// Protect against free
  close(LOG_CLOSE_TO_BE_OPENED, false/*need_lock_log=false*/,
        false/*need_lock_index=false*/);

  /*
    First delete all old log files and then update the index file.
    As we first delete the log files and do not use sort of logging,
    a crash may lead to an inconsistent state where the index has
    references to non-existent files.

    We need to invert the steps and use the purge_index_file methods
    in order to make the operation safe.
  */

  if ((err= find_log_pos(&linfo, NullS, false/*need_lock_index=false*/)) != 0)
  {
    uint errcode= purge_log_get_error_code(err);
    sql_print_error("Failed to locate old binlog or relay log files");
    my_message(errcode, ER(errcode), MYF(0));
    error= 1;
    goto err;
  }

  for (;;)
  {
    if ((error= my_delete_allow_opened(linfo.log_file_name, MYF(0))) != 0)
    {
      if (my_errno() == ENOENT) 
      {
        push_warning_printf(current_thd, Sql_condition::SL_WARNING,
                            ER_LOG_PURGE_NO_FILE, ER(ER_LOG_PURGE_NO_FILE),
                            linfo.log_file_name);
        sql_print_information("Failed to delete file '%s'",
                              linfo.log_file_name);
        set_my_errno(0);
        error= 0;
      }
      else
      {
        push_warning_printf(current_thd, Sql_condition::SL_WARNING,
                            ER_BINLOG_PURGE_FATAL_ERR,
                            "a problem with deleting %s; "
                            "consider examining correspondence "
                            "of your binlog index file "
                            "to the actual binlog files",
                            linfo.log_file_name);
        error= 1;
        goto err;
      }
    }
    if (find_next_log(&linfo, false/*need_lock_index=false*/))
      break;
  }

  /* Start logging with a new file */
  close(LOG_CLOSE_INDEX | LOG_CLOSE_TO_BE_OPENED,
        false/*need_lock_log=false*/,
        false/*need_lock_index=false*/);
  if ((error= my_delete_allow_opened(index_file_name, MYF(0))))	// Reset (open will update)
  {
    if (my_errno() == ENOENT)
    {
      push_warning_printf(current_thd, Sql_condition::SL_WARNING,
                          ER_LOG_PURGE_NO_FILE, ER(ER_LOG_PURGE_NO_FILE),
                          index_file_name);
      sql_print_information("Failed to delete file '%s'",
                            index_file_name);
      set_my_errno(0);
      error= 0;
    }
    else
    {
      push_warning_printf(current_thd, Sql_condition::SL_WARNING,
                          ER_BINLOG_PURGE_FATAL_ERR,
                          "a problem with deleting %s; "
                          "consider examining correspondence "
                          "of your binlog index file "
                          "to the actual binlog files",
                          index_file_name);
      error= 1;
      goto err;
    }
  }

#ifdef HAVE_REPLICATION
  /*
    For relay logs we clear the gtid state associated per channel(i.e rli)
    in the purge_relay_logs()
  */
  if (!is_relay_log)
  {
    if(gtid_state->clear(thd))
    {
      error= 1;
      goto err;
    }
    // don't clear global_sid_map because it's used by the relay log too
    if (gtid_state->init() != 0)
      goto err;
  }
#endif

  if (!delete_only)
  {
    if (!open_index_file(index_file_name, 0, false/*need_lock_index=false*/))
    if ((error= open_binlog(save_name, 0,
                            max_size, false,
                            false/*need_lock_index=false*/,
                            false/*need_sid_lock=false*/,
                            NULL)))
      goto err;
  }
  my_free((void *) save_name);

err:
  if (error == 1)
    name= const_cast<char*>(save_name);
  global_sid_lock->unlock();
  mysql_mutex_unlock(&LOCK_index);
  mysql_mutex_unlock(&LOCK_log);
  DBUG_RETURN(error);
}


/**
  Set the name of crash safe index file.

  @retval
    0   ok
  @retval
    1   error
*/
int MYSQL_BIN_LOG::set_crash_safe_index_file_name(const char *base_file_name)
{
  int error= 0;
  DBUG_ENTER("MYSQL_BIN_LOG::set_crash_safe_index_file_name");
  if (fn_format(crash_safe_index_file_name, base_file_name, mysql_data_home,
                ".index_crash_safe", MYF(MY_UNPACK_FILENAME | MY_SAFE_PATH |
                                         MY_REPLACE_EXT)) == NULL)
  {
    error= 1;
    sql_print_error("MYSQL_BIN_LOG::set_crash_safe_index_file_name failed "
                    "to set file name.");
  }
  DBUG_RETURN(error);
}


/**
  Open a (new) crash safe index file.

  @note
    The crash safe index file is a special file
    used for guaranteeing index file crash safe.
  @retval
    0   ok
  @retval
    1   error
*/
int MYSQL_BIN_LOG::open_crash_safe_index_file()
{
  int error= 0;
  File file= -1;

  DBUG_ENTER("MYSQL_BIN_LOG::open_crash_safe_index_file");

  if (!my_b_inited(&crash_safe_index_file))
  {
    if ((file= my_open(crash_safe_index_file_name, O_RDWR | O_CREAT | O_BINARY,
                       MYF(MY_WME))) < 0  ||
        init_io_cache(&crash_safe_index_file, file, IO_SIZE, WRITE_CACHE,
                      0, 0, MYF(MY_WME | MY_NABP | MY_WAIT_IF_FULL)))
    {
      error= 1;
      sql_print_error("MYSQL_BIN_LOG::open_crash_safe_index_file failed "
                      "to open temporary index file.");
    }
  }
  DBUG_RETURN(error);
}


/**
  Close the crash safe index file.

  @note
    The crash safe file is just closed, is not deleted.
    Because it is moved to index file later on.
  @retval
    0   ok
  @retval
    1   error
*/
int MYSQL_BIN_LOG::close_crash_safe_index_file()
{
  int error= 0;

  DBUG_ENTER("MYSQL_BIN_LOG::close_crash_safe_index_file");

  if (my_b_inited(&crash_safe_index_file))
  {
    end_io_cache(&crash_safe_index_file);
    error= my_close(crash_safe_index_file.file, MYF(0));
  }
  memset(&crash_safe_index_file, 0, sizeof(crash_safe_index_file));

  DBUG_RETURN(error);
}


/**
  Delete relay log files prior to rli->group_relay_log_name
  (i.e. all logs which are not involved in a non-finished group
  (transaction)), remove them from the index file and start on next
  relay log.

  IMPLEMENTATION

  - You must hold rli->data_lock before calling this function, since
    it writes group_relay_log_pos and similar fields of
    Relay_log_info.
  - Protects index file with LOCK_index
  - Delete relevant relay log files
  - Copy all file names after these ones to the front of the index file
  - If the OS has truncate, truncate the file, else fill it with \n'
  - Read the next file name from the index file and store in rli->linfo

  @param rli	       Relay log information
  @param included     If false, all relay logs that are strictly before
                      rli->group_relay_log_name are deleted ; if true, the
                      latter is deleted too (i.e. all relay logs
                      read by the SQL slave thread are deleted).

  @note
    - This is only called from the slave SQL thread when it has read
    all commands from a relay log and want to switch to a new relay log.
    - When this happens, we can be in an active transaction as
    a transaction can span over two relay logs
    (although it is always written as a single block to the master's binary
    log, hence cannot span over two master's binary logs).

  @retval
    0			ok
  @retval
    LOG_INFO_EOF	        End of log-index-file found
  @retval
    LOG_INFO_SEEK	Could not allocate IO cache
  @retval
    LOG_INFO_IO		Got IO error while reading file
*/

#ifdef HAVE_REPLICATION

int MYSQL_BIN_LOG::purge_first_log(Relay_log_info* rli, bool included)
{
  int error;
  char *to_purge_if_included= NULL;
  DBUG_ENTER("purge_first_log");

  DBUG_ASSERT(current_thd->system_thread == SYSTEM_THREAD_SLAVE_SQL);
  DBUG_ASSERT(is_relay_log);
  DBUG_ASSERT(is_open());
  DBUG_ASSERT(rli->slave_running == 1);
  DBUG_ASSERT(!strcmp(rli->linfo.log_file_name,rli->get_event_relay_log_name()));

  mysql_mutex_assert_owner(&rli->data_lock);

  mysql_mutex_lock(&LOCK_index);
  to_purge_if_included= my_strdup(key_memory_Relay_log_info_group_relay_log_name,
                                  rli->get_group_relay_log_name(), MYF(0));

  /*
    Read the next log file name from the index file and pass it back to
    the caller.
  */
  if((error=find_log_pos(&rli->linfo, rli->get_event_relay_log_name(),
                         false/*need_lock_index=false*/)) ||
     (error=find_next_log(&rli->linfo, false/*need_lock_index=false*/)))
  {
    char buff[22];
    sql_print_error("next log error: %d  offset: %s  log: %s included: %d",
                    error,
                    llstr(rli->linfo.index_file_offset,buff),
                    rli->get_event_relay_log_name(),
                    included);
    goto err;
  }

  /*
    Reset rli's coordinates to the current log.
  */
  rli->set_event_relay_log_pos(BIN_LOG_HEADER_SIZE);
  rli->set_event_relay_log_name(rli->linfo.log_file_name);

  /*
    If we removed the rli->group_relay_log_name file,
    we must update the rli->group* coordinates, otherwise do not touch it as the
    group's execution is not finished (e.g. COMMIT not executed)
  */
  if (included)
  {
    rli->set_group_relay_log_pos(BIN_LOG_HEADER_SIZE);
    rli->set_group_relay_log_name(rli->linfo.log_file_name);
    rli->notify_group_relay_log_name_update();
  }
  /*
    Store where we are in the new file for the execution thread.
    If we are in the middle of a transaction, then we
    should not store the position in the repository, instead in
    that case set a flag to true which indicates that a 'forced flush'
    is postponed due to transaction split across the relaylogs.
  */
  if (!rli->is_in_group())
    rli->flush_info(TRUE);
  else
    rli->force_flush_postponed_due_to_split_trans= true;

  DBUG_EXECUTE_IF("crash_before_purge_logs", DBUG_SUICIDE(););

  mysql_mutex_lock(&rli->log_space_lock);
  rli->relay_log.purge_logs(to_purge_if_included, included,
                            false/*need_lock_index=false*/,
                            false/*need_update_threads=false*/,
                            &rli->log_space_total, true);
  // Tell the I/O thread to take the relay_log_space_limit into account
  rli->ignore_log_space_limit= 0;
  mysql_mutex_unlock(&rli->log_space_lock);

  /*
    Ok to broadcast after the critical region as there is no risk of
    the mutex being destroyed by this thread later - this helps save
    context switches
  */
  mysql_cond_broadcast(&rli->log_space_cond);

  /*
   * Need to update the log pos because purge logs has been called 
   * after fetching initially the log pos at the begining of the method.
   */
  if((error=find_log_pos(&rli->linfo, rli->get_event_relay_log_name(),
                         false/*need_lock_index=false*/)))
  {
    char buff[22];
    sql_print_error("next log error: %d  offset: %s  log: %s included: %d",
                    error,
                    llstr(rli->linfo.index_file_offset,buff),
                    rli->get_group_relay_log_name(),
                    included);
    goto err;
  }

  /* If included was passed, rli->linfo should be the first entry. */
  DBUG_ASSERT(!included || rli->linfo.index_file_start_offset == 0);

err:
  my_free(to_purge_if_included);
  mysql_mutex_unlock(&LOCK_index);
  DBUG_RETURN(error);
}


/**
  Remove logs from index file.

  - To make crash safe, we copy the content of index file
  from index_file_start_offset recored in log_info to
  crash safe index file firstly and then move the crash
  safe index file to index file.

  @param linfo                  Store here the found log file name and
                                position to the NEXT log file name in
                                the index file.

  @param need_update_threads    If we want to update the log coordinates
                                of all threads. False for relay logs,
                                true otherwise.

  @retval
    0    ok
  @retval
    LOG_INFO_IO    Got IO error while reading/writing file
*/
int MYSQL_BIN_LOG::remove_logs_from_index(LOG_INFO* log_info, bool need_update_threads)
{
  if (open_crash_safe_index_file())
  {
    sql_print_error("MYSQL_BIN_LOG::remove_logs_from_index failed to "
                    "open the crash safe index file.");
    goto err;
  }

  if (copy_file(&index_file, &crash_safe_index_file,
                log_info->index_file_start_offset))
  {
    sql_print_error("MYSQL_BIN_LOG::remove_logs_from_index failed to "
                    "copy index file to crash safe index file.");
    goto err;
  }

  if (close_crash_safe_index_file())
  {
    sql_print_error("MYSQL_BIN_LOG::remove_logs_from_index failed to "
                    "close the crash safe index file.");
    goto err;
  }
  DBUG_EXECUTE_IF("fault_injection_copy_part_file", DBUG_SUICIDE(););

  if (move_crash_safe_index_file_to_index_file(false/*need_lock_index=false*/))
  {
    sql_print_error("MYSQL_BIN_LOG::remove_logs_from_index failed to "
                    "move crash safe index file to index file.");
    goto err;
  }

  // now update offsets in index file for running threads
  if (need_update_threads)
    adjust_linfo_offsets(log_info->index_file_start_offset);
  return 0;

err:
  return LOG_INFO_IO;
}

/**
  Remove all logs before the given log from disk and from the index file.

  @param to_log	      Delete all log file name before this file.
  @param included            If true, to_log is deleted too.
  @param need_lock_index
  @param need_update_threads If we want to update the log coordinates of
                             all threads. False for relay logs, true otherwise.
  @param freed_log_space     If not null, decrement this variable of
                             the amount of log space freed
  @param auto_purge          True if this is an automatic purge.

  @note
    If any of the logs before the deleted one is in use,
    only purge logs up to this one.

  @retval
    0			ok
  @retval
    LOG_INFO_EOF		to_log not found
    LOG_INFO_EMFILE             too many files opened
    LOG_INFO_FATAL              if any other than ENOENT error from
                                mysql_file_stat() or mysql_file_delete()
*/

int MYSQL_BIN_LOG::purge_logs(const char *to_log,
                              bool included,
                              bool need_lock_index,
                              bool need_update_threads,
                              ulonglong *decrease_log_space,
                              bool auto_purge)
{
  int error= 0, no_of_log_files_to_purge= 0, no_of_log_files_purged= 0;
  int no_of_threads_locking_log= 0;
  bool exit_loop= 0;
  LOG_INFO log_info;
  THD *thd= current_thd;
  DBUG_ENTER("purge_logs");
  DBUG_PRINT("info",("to_log= %s",to_log));

  if (need_lock_index)
    mysql_mutex_lock(&LOCK_index);
  else
    mysql_mutex_assert_owner(&LOCK_index);
  if ((error=find_log_pos(&log_info, to_log, false/*need_lock_index=false*/))) 
  {
    sql_print_error("MYSQL_BIN_LOG::purge_logs was called with file %s not "
                    "listed in the index.", to_log);
    goto err;
  }

  no_of_log_files_to_purge= log_info.entry_index;

  if ((error= open_purge_index_file(TRUE)))
  {
    sql_print_error("MYSQL_BIN_LOG::purge_logs failed to sync the index file.");
    goto err;
  }

  /*
    File name exists in index file; delete until we find this file
    or a file that is used.
  */
  if ((error=find_log_pos(&log_info, NullS, false/*need_lock_index=false*/)))
    goto err;

  while ((strcmp(to_log,log_info.log_file_name) || (exit_loop=included)))
  {
    if(is_active(log_info.log_file_name))
    {
      if(!auto_purge)
        push_warning_printf(thd, Sql_condition::SL_WARNING,
                            ER_WARN_PURGE_LOG_IS_ACTIVE,
                            ER(ER_WARN_PURGE_LOG_IS_ACTIVE),
                            log_info.log_file_name);
      break;
    }

    if ((no_of_threads_locking_log= log_in_use(log_info.log_file_name)))
    {
      if(!auto_purge)
        push_warning_printf(thd, Sql_condition::SL_WARNING,
                            ER_WARN_PURGE_LOG_IN_USE,
                            ER(ER_WARN_PURGE_LOG_IN_USE),
                            log_info.log_file_name,  no_of_threads_locking_log,
                            no_of_log_files_purged, no_of_log_files_to_purge);
      break;
    }
    no_of_log_files_purged++;

    if ((error= register_purge_index_entry(log_info.log_file_name)))
    {
      sql_print_error("MYSQL_BIN_LOG::purge_logs failed to copy %s to register file.",
                      log_info.log_file_name);
      goto err;
    }

    if (find_next_log(&log_info, false/*need_lock_index=false*/) || exit_loop)
      break;
  }

  DBUG_EXECUTE_IF("crash_purge_before_update_index", DBUG_SUICIDE(););

  if ((error= sync_purge_index_file()))
  {
    sql_print_error("MYSQL_BIN_LOG::purge_logs failed to flush register file.");
    goto err;
  }

  /* We know how many files to delete. Update index file. */
  if ((error=remove_logs_from_index(&log_info, need_update_threads)))
  {
    sql_print_error("MYSQL_BIN_LOG::purge_logs failed to update the index file");
    goto err;
  }

  // Update gtid_state->lost_gtids
  if (!is_relay_log)
  {
    global_sid_lock->wrlock();
    error= init_gtid_sets(NULL,
                          const_cast<Gtid_set *>(gtid_state->get_lost_gtids()),
                          opt_master_verify_checksum,
                          false/*false=don't need lock*/,
                          NULL/*trx_parser*/, NULL/*gtid_partial_trx*/);
    global_sid_lock->unlock();
    if (error)
      goto err;
  }

  DBUG_EXECUTE_IF("crash_purge_critical_after_update_index", DBUG_SUICIDE(););

err:

  int error_index= 0, close_error_index= 0;
  /* Read each entry from purge_index_file and delete the file. */
  if (!error && is_inited_purge_index_file() &&
      (error_index= purge_index_entry(thd, decrease_log_space, false/*need_lock_index=false*/)))
    sql_print_error("MYSQL_BIN_LOG::purge_logs failed to process registered files"
                    " that would be purged.");

  close_error_index= close_purge_index_file();

  DBUG_EXECUTE_IF("crash_purge_non_critical_after_update_index", DBUG_SUICIDE(););

  if (need_lock_index)
    mysql_mutex_unlock(&LOCK_index);

  /*
    Error codes from purge logs take precedence.
    Then error codes from purging the index entry.
    Finally, error codes from closing the purge index file.
  */
  error= error ? error : (error_index ? error_index :
                          close_error_index);

  DBUG_RETURN(error);
}

int MYSQL_BIN_LOG::set_purge_index_file_name(const char *base_file_name)
{
  int error= 0;
  DBUG_ENTER("MYSQL_BIN_LOG::set_purge_index_file_name");
  if (fn_format(purge_index_file_name, base_file_name, mysql_data_home,
                ".~rec~", MYF(MY_UNPACK_FILENAME | MY_SAFE_PATH |
                              MY_REPLACE_EXT)) == NULL)
  {
    error= 1;
    sql_print_error("MYSQL_BIN_LOG::set_purge_index_file_name failed to set "
                      "file name.");
  }
  DBUG_RETURN(error);
}

int MYSQL_BIN_LOG::open_purge_index_file(bool destroy)
{
  int error= 0;
  File file= -1;

  DBUG_ENTER("MYSQL_BIN_LOG::open_purge_index_file");

  if (destroy)
    close_purge_index_file();

  if (!my_b_inited(&purge_index_file))
  {
    if ((file= my_open(purge_index_file_name, O_RDWR | O_CREAT | O_BINARY,
                       MYF(MY_WME))) < 0  ||
        init_io_cache(&purge_index_file, file, IO_SIZE,
                      (destroy ? WRITE_CACHE : READ_CACHE),
                      0, 0, MYF(MY_WME | MY_NABP | MY_WAIT_IF_FULL)))
    {
      error= 1;
      sql_print_error("MYSQL_BIN_LOG::open_purge_index_file failed to open register "
                      " file.");
    }
  }
  DBUG_RETURN(error);
}

int MYSQL_BIN_LOG::close_purge_index_file()
{
  int error= 0;

  DBUG_ENTER("MYSQL_BIN_LOG::close_purge_index_file");

  if (my_b_inited(&purge_index_file))
  {
    end_io_cache(&purge_index_file);
    error= my_close(purge_index_file.file, MYF(0));
  }
  my_delete(purge_index_file_name, MYF(0));
  memset(&purge_index_file, 0, sizeof(purge_index_file));

  DBUG_RETURN(error);
}

bool MYSQL_BIN_LOG::is_inited_purge_index_file()
{
  DBUG_ENTER("MYSQL_BIN_LOG::is_inited_purge_index_file");
  DBUG_RETURN (my_b_inited(&purge_index_file));
}

int MYSQL_BIN_LOG::sync_purge_index_file()
{
  int error= 0;
  DBUG_ENTER("MYSQL_BIN_LOG::sync_purge_index_file");

  if ((error= flush_io_cache(&purge_index_file)) ||
      (error= my_sync(purge_index_file.file, MYF(MY_WME))))
    DBUG_RETURN(error);

  DBUG_RETURN(error);
}

int MYSQL_BIN_LOG::register_purge_index_entry(const char *entry)
{
  int error= 0;
  DBUG_ENTER("MYSQL_BIN_LOG::register_purge_index_entry");

  if ((error=my_b_write(&purge_index_file, (const uchar*)entry, strlen(entry))) ||
      (error=my_b_write(&purge_index_file, (const uchar*)"\n", 1)))
    DBUG_RETURN (error);

  DBUG_RETURN(error);
}

int MYSQL_BIN_LOG::register_create_index_entry(const char *entry)
{
  DBUG_ENTER("MYSQL_BIN_LOG::register_create_index_entry");
  DBUG_RETURN(register_purge_index_entry(entry));
}

int MYSQL_BIN_LOG::purge_index_entry(THD *thd, ulonglong *decrease_log_space,
                                     bool need_lock_index)
{
  MY_STAT s;
  int error= 0;
  LOG_INFO log_info;
  LOG_INFO check_log_info;

  DBUG_ENTER("MYSQL_BIN_LOG:purge_index_entry");

  DBUG_ASSERT(my_b_inited(&purge_index_file));

  if ((error=reinit_io_cache(&purge_index_file, READ_CACHE, 0, 0, 0)))
  {
    sql_print_error("MYSQL_BIN_LOG::purge_index_entry failed to reinit register file "
                    "for read");
    goto err;
  }

  for (;;)
  {
    size_t length;

    if ((length=my_b_gets(&purge_index_file, log_info.log_file_name,
                          FN_REFLEN)) <= 1)
    {
      if (purge_index_file.error)
      {
        error= purge_index_file.error;
        sql_print_error("MYSQL_BIN_LOG::purge_index_entry error %d reading from "
                        "register file.", error);
        goto err;
      }

      /* Reached EOF */
      break;
    }

    /* Get rid of the trailing '\n' */
    log_info.log_file_name[length-1]= 0;

    if (!mysql_file_stat(m_key_file_log, log_info.log_file_name, &s, MYF(0)))
    {
      if (my_errno() == ENOENT) 
      {
        /*
          It's not fatal if we can't stat a log file that does not exist;
          If we could not stat, we won't delete.
        */
        if (thd)
        {
          push_warning_printf(thd, Sql_condition::SL_WARNING,
                              ER_LOG_PURGE_NO_FILE, ER(ER_LOG_PURGE_NO_FILE),
                              log_info.log_file_name);
        }
        sql_print_information("Failed to execute mysql_file_stat on file '%s'",
			      log_info.log_file_name);
        set_my_errno(0);
      }
      else
      {
        /*
          Other than ENOENT are fatal
        */
        if (thd)
        {
          push_warning_printf(thd, Sql_condition::SL_WARNING,
                              ER_BINLOG_PURGE_FATAL_ERR,
                              "a problem with getting info on being purged %s; "
                              "consider examining correspondence "
                              "of your binlog index file "
                              "to the actual binlog files",
                              log_info.log_file_name);
        }
        else
        {
          sql_print_information("Failed to delete log file '%s'; "
                                "consider examining correspondence "
                                "of your binlog index file "
                                "to the actual binlog files",
                                log_info.log_file_name);
        }
        error= LOG_INFO_FATAL;
        goto err;
      }
    }
    else
    {
      if ((error= find_log_pos(&check_log_info, log_info.log_file_name,
                               need_lock_index)))
      {
        if (error != LOG_INFO_EOF)
        {
          if (thd)
          {
            push_warning_printf(thd, Sql_condition::SL_WARNING,
                                ER_BINLOG_PURGE_FATAL_ERR,
                                "a problem with deleting %s and "
                                "reading the binlog index file",
                                log_info.log_file_name);
          }
          else
          {
            sql_print_information("Failed to delete file '%s' and "
                                  "read the binlog index file",
                                  log_info.log_file_name);
          }
          goto err;
        }
           
        error= 0;
        if (!need_lock_index)
        {
          /*
            This is to avoid triggering an error in NDB.

            @todo: This is weird, what does NDB errors have to do with
            need_lock_index? Explain better or refactor /Sven
          */
          ha_binlog_index_purge_file(current_thd, log_info.log_file_name);
        }

        DBUG_PRINT("info",("purging %s",log_info.log_file_name));
        if (!mysql_file_delete(key_file_binlog, log_info.log_file_name, MYF(0)))
        {
          if (decrease_log_space)
            *decrease_log_space-= s.st_size;
        }
        else
        {
          if (my_errno() == ENOENT)
          {
            if (thd)
            {
              push_warning_printf(thd, Sql_condition::SL_WARNING,
                                  ER_LOG_PURGE_NO_FILE, ER(ER_LOG_PURGE_NO_FILE),
                                  log_info.log_file_name);
            }
            sql_print_information("Failed to delete file '%s'",
                                  log_info.log_file_name);
            set_my_errno(0);
          }
          else
          {
            if (thd)
            {
              push_warning_printf(thd, Sql_condition::SL_WARNING,
                                  ER_BINLOG_PURGE_FATAL_ERR,
                                  "a problem with deleting %s; "
                                  "consider examining correspondence "
                                  "of your binlog index file "
                                  "to the actual binlog files",
                                  log_info.log_file_name);
            }
            else
            {
              sql_print_information("Failed to delete file '%s'; "
                                    "consider examining correspondence "
                                    "of your binlog index file "
                                    "to the actual binlog files",
                                    log_info.log_file_name);
            }
            if (my_errno() == EMFILE)
            {
              DBUG_PRINT("info",
                         ("my_errno: %d, set ret = LOG_INFO_EMFILE", my_errno()));
              error= LOG_INFO_EMFILE;
              goto err;
            }
            error= LOG_INFO_FATAL;
            goto err;
          }
        }
      }
    }
  }

err:
  DBUG_RETURN(error);
}

/**
  Remove all logs before the given file date from disk and from the
  index file.

  @param thd		Thread pointer
  @param purge_time	Delete all log files before given date.
  @param auto_purge     True if this is an automatic purge.

  @note
    If any of the logs before the deleted one is in use,
    only purge logs up to this one.

  @retval
    0				ok
  @retval
    LOG_INFO_PURGE_NO_ROTATE	Binary file that can't be rotated
    LOG_INFO_FATAL              if any other than ENOENT error from
                                mysql_file_stat() or mysql_file_delete()
*/

int MYSQL_BIN_LOG::purge_logs_before_date(time_t purge_time, bool auto_purge)
{
  int error;
  int no_of_threads_locking_log= 0, no_of_log_files_purged= 0;
  bool log_is_active= false, log_is_in_use= false;
  char to_log[FN_REFLEN], copy_log_in_use[FN_REFLEN];
  LOG_INFO log_info;
  MY_STAT stat_area;
  THD *thd= current_thd;

  DBUG_ENTER("purge_logs_before_date");

  mysql_mutex_lock(&LOCK_index);
  to_log[0]= 0;

  if ((error=find_log_pos(&log_info, NullS, false/*need_lock_index=false*/)))
    goto err;

  while (!(log_is_active= is_active(log_info.log_file_name)))
  {
    if (!mysql_file_stat(m_key_file_log,
                         log_info.log_file_name, &stat_area, MYF(0)))
    {
      if (my_errno() == ENOENT)
      {
        /*
          It's not fatal if we can't stat a log file that does not exist.
        */
        set_my_errno(0);
      }
      else
      {
        /*
          Other than ENOENT are fatal
        */
        if (thd)
        {
          push_warning_printf(thd, Sql_condition::SL_WARNING,
                              ER_BINLOG_PURGE_FATAL_ERR,
                              "a problem with getting info on being purged %s; "
                              "consider examining correspondence "
                              "of your binlog index file "
                              "to the actual binlog files",
                              log_info.log_file_name);
        }
        else
        {
          sql_print_information("Failed to delete log file '%s'",
                                log_info.log_file_name);
        }
        error= LOG_INFO_FATAL;
        goto err;
      }
    }
    /* check if the binary log file is older than the purge_time
       if yes check if it is in use, if not in use then add
       it in the list of binary log files to be purged.
    */
    else if (stat_area.st_mtime < purge_time)
    {
      if ((no_of_threads_locking_log= log_in_use(log_info.log_file_name)))
      {
        if (!auto_purge)
        {
          log_is_in_use= true;
          strcpy(copy_log_in_use, log_info.log_file_name);
        }
        break;
      }
      strmake(to_log,
              log_info.log_file_name,
              sizeof(log_info.log_file_name) - 1);
      no_of_log_files_purged++;
    }
    else
      break;
    if (find_next_log(&log_info, false/*need_lock_index=false*/))
      break;
  }

  if (log_is_active)
  {
    if(!auto_purge)
      push_warning_printf(thd, Sql_condition::SL_WARNING,
                          ER_WARN_PURGE_LOG_IS_ACTIVE,
                          ER(ER_WARN_PURGE_LOG_IS_ACTIVE),
                          log_info.log_file_name);

  }

  if (log_is_in_use)
  {
    int no_of_log_files_to_purge= no_of_log_files_purged+1;
    while (strcmp(log_file_name, log_info.log_file_name))
    {
      if (mysql_file_stat(m_key_file_log, log_info.log_file_name,
                          &stat_area, MYF(0)))
      {
        if (stat_area.st_mtime < purge_time)
          no_of_log_files_to_purge++;
        else
          break;
      }
      if (find_next_log(&log_info, false/*need_lock_index=false*/))
      {
        no_of_log_files_to_purge++;
        break;
      }
    }

    push_warning_printf(thd, Sql_condition::SL_WARNING,
                        ER_WARN_PURGE_LOG_IN_USE,
                        ER(ER_WARN_PURGE_LOG_IN_USE),
                        copy_log_in_use, no_of_threads_locking_log,
                        no_of_log_files_purged, no_of_log_files_to_purge);
  }

  error= (to_log[0] ? purge_logs(to_log, true,
                                 false/*need_lock_index=false*/,
                                 true/*need_update_threads=true*/,
                                 (ulonglong *) 0, auto_purge) : 0);

err:
  mysql_mutex_unlock(&LOCK_index);
  DBUG_RETURN(error);
}
#endif /* HAVE_REPLICATION */


/**
  Create a new log file name.

  @param buf		buf of at least FN_REFLEN where new name is stored

  @note
    If file name will be longer then FN_REFLEN it will be truncated
*/

void MYSQL_BIN_LOG::make_log_name(char* buf, const char* log_ident)
{
  size_t dir_len = dirname_length(log_file_name); 
  if (dir_len >= FN_REFLEN)
    dir_len=FN_REFLEN-1;
  my_stpnmov(buf, log_file_name, dir_len);
  strmake(buf+dir_len, log_ident, FN_REFLEN - dir_len -1);
}


/**
  Check if we are writing/reading to the given log file.
*/

bool MYSQL_BIN_LOG::is_active(const char *log_file_name_arg)
{
  return !strcmp(log_file_name, log_file_name_arg);
}


void MYSQL_BIN_LOG::inc_prep_xids(THD *thd)
{
  DBUG_ENTER("MYSQL_BIN_LOG::inc_prep_xids");
#ifndef DBUG_OFF
  int result= m_prep_xids.atomic_add(1);
  DBUG_PRINT("debug", ("m_prep_xids: %d", result + 1));
#else
  (void) m_prep_xids.atomic_add(1);
#endif
  thd->get_transaction()->m_flags.xid_written= true;
  DBUG_VOID_RETURN;
}


void MYSQL_BIN_LOG::dec_prep_xids(THD *thd)
{
  DBUG_ENTER("MYSQL_BIN_LOG::dec_prep_xids");
  int32 result= m_prep_xids.atomic_add(-1);
  DBUG_PRINT("debug", ("m_prep_xids: %d", result - 1));
  thd->get_transaction()->m_flags.xid_written= false;
  /* If the old value was 1, it is zero now. */
  if (result == 1)
  {
    mysql_mutex_lock(&LOCK_xids);
    mysql_cond_signal(&m_prep_xids_cond);
    mysql_mutex_unlock(&LOCK_xids);
  }
  DBUG_VOID_RETURN;
}


/*
  Wrappers around new_file_impl to avoid using argument
  to control locking. The argument 1) less readable 2) breaks
  incapsulation 3) allows external access to the class without
  a lock (which is not possible with private new_file_without_locking
  method).
  
  @retval
    nonzero - error

*/

int MYSQL_BIN_LOG::new_file(Format_description_log_event *extra_description_event)
{
  return new_file_impl(true/*need_lock_log=true*/, extra_description_event);
}

/*
  @retval
    nonzero - error
*/
int MYSQL_BIN_LOG::new_file_without_locking(Format_description_log_event *extra_description_event)
{
  return new_file_impl(false/*need_lock_log=false*/, extra_description_event);
}


/**
  Start writing to a new log file or reopen the old file.

  @param need_lock_log If true, this function acquires LOCK_log;
  otherwise the caller should already have acquired it.

  @retval 0 success
  @retval nonzero - error

  @note The new file name is stored last in the index file
*/
int MYSQL_BIN_LOG::new_file_impl(bool need_lock_log, Format_description_log_event *extra_description_event)
{
  int error= 0;
  bool close_on_error= false;
  char new_name[FN_REFLEN], *new_name_ptr= NULL, *old_name, *file_to_open;

  DBUG_ENTER("MYSQL_BIN_LOG::new_file_impl");
  if (!is_open())
  {
    DBUG_PRINT("info",("log is closed"));
    DBUG_RETURN(error);
  }

  if (need_lock_log)
    mysql_mutex_lock(&LOCK_log);
  else
    mysql_mutex_assert_owner(&LOCK_log);
  DBUG_EXECUTE_IF("semi_sync_3-way_deadlock",
                  DEBUG_SYNC(current_thd, "before_rotate_binlog"););
  mysql_mutex_lock(&LOCK_xids);
  /*
    We need to ensure that the number of prepared XIDs are 0.

    If m_prep_xids is not zero:
    - We wait for storage engine commit, hence decrease m_prep_xids
    - We keep the LOCK_log to block new transactions from being
      written to the binary log.
   */
  while (get_prep_xids() > 0)
  {
    DEBUG_SYNC(current_thd, "before_rotate_binlog_file");
    mysql_cond_wait(&m_prep_xids_cond, &LOCK_xids);
  }
  mysql_mutex_unlock(&LOCK_xids);

  mysql_mutex_lock(&LOCK_index);

  mysql_mutex_assert_owner(&LOCK_log);
  mysql_mutex_assert_owner(&LOCK_index);


  if (DBUG_EVALUATE_IF("expire_logs_always", 0, 1)
      && (error= ha_flush_logs(NULL)))
    goto end;

  if (!is_relay_log)
  {
    /* Save set of GTIDs of the last binlog into table on binlog rotation */
    if ((error= gtid_state->save_gtids_of_last_binlog_into_table(true)))
    {
      close_on_error= true;
      goto end;
    }
  }

  /*
    If user hasn't specified an extension, generate a new log name
    We have to do this here and not in open as we want to store the
    new file name in the current binary log file.
  */
  new_name_ptr= new_name;
  if ((error= generate_new_name(new_name, name)))
  {
    // Use the old name if generation of new name fails.
    strcpy(new_name, name);
    close_on_error= TRUE;
    goto end;
  }
  else
  {
    /*
      We log the whole file name for log file as the user may decide
      to change base names at some point.
    */
    Rotate_log_event r(new_name+dirname_length(new_name), 0, LOG_EVENT_OFFSET,
                       is_relay_log ? Rotate_log_event::RELAY_LOG : 0);
    /* 
      The current relay-log's closing Rotate event must have checksum
      value computed with an algorithm of the last relay-logged FD event.
    */
    if (is_relay_log)
      (r.common_footer)->checksum_alg= relay_log_checksum_alg;
    DBUG_ASSERT(!is_relay_log || relay_log_checksum_alg !=
                binary_log::BINLOG_CHECKSUM_ALG_UNDEF);
    if(DBUG_EVALUATE_IF("fault_injection_new_file_rotate_event",
                        (error=1), FALSE) ||
       (error= r.write(&log_file)))
    {
      char errbuf[MYSYS_STRERROR_SIZE];
      DBUG_EXECUTE_IF("fault_injection_new_file_rotate_event", errno=2;);
      close_on_error= true;
      my_printf_error(ER_ERROR_ON_WRITE, ER(ER_CANT_OPEN_FILE),
                      MYF(ME_FATALERROR), name,
                      errno, my_strerror(errbuf, sizeof(errbuf), errno));
      goto end;
    }
    bytes_written += r.common_header->data_written;
  }

  if ((error= flush_io_cache(&log_file)))
  {
    close_on_error= true;
    goto end;
  }

  DEBUG_SYNC(current_thd, "after_rotate_event_appended");

  old_name=name;
  name=0;				// Don't free name
  close(LOG_CLOSE_TO_BE_OPENED | LOG_CLOSE_INDEX,
        false/*need_lock_log=false*/,
        false/*need_lock_index=false*/);

  if (checksum_alg_reset != binary_log::BINLOG_CHECKSUM_ALG_UNDEF)
  {
    DBUG_ASSERT(!is_relay_log);
    DBUG_ASSERT(binlog_checksum_options != checksum_alg_reset);
    binlog_checksum_options= checksum_alg_reset;
  }
  /*
     Note that at this point, log_state != LOG_CLOSED (important for is_open()).
  */

  DEBUG_SYNC(current_thd, "before_rotate_binlog_file");
  /*
     new_file() is only used for rotation (in FLUSH LOGS or because size >
     max_binlog_size or max_relay_log_size).
     If this is a binary log, the Format_description_log_event at the beginning of
     the new file should have created=0 (to distinguish with the
     Format_description_log_event written at server startup, which should
     trigger temp tables deletion on slaves.
  */

  /* reopen index binlog file, BUG#34582 */
  file_to_open= index_file_name;
  error= open_index_file(index_file_name, 0, false/*need_lock_index=false*/);
  if (!error)
  {
    /* reopen the binary log file. */
    file_to_open= new_name_ptr;
    error= open_binlog(old_name, new_name_ptr,
                       max_size, true/*null_created_arg=true*/,
                       false/*need_lock_index=false*/,
                       true/*need_sid_lock=true*/,
                       extra_description_event);
  }

  /* handle reopening errors */
  if (error)
  {
    char errbuf[MYSYS_STRERROR_SIZE];
    my_printf_error(ER_CANT_OPEN_FILE, ER(ER_CANT_OPEN_FILE), 
                    MYF(ME_FATALERROR), file_to_open,
                    error, my_strerror(errbuf, sizeof(errbuf), error));
    close_on_error= true;
  }
  my_free(old_name);

end:

  if (error && close_on_error /* rotate, flush or reopen failed */)
  {
    /* 
      Close whatever was left opened.

      We are keeping the behavior as it exists today, ie,
      we disable logging and move on (see: BUG#51014).

      TODO: as part of WL#1790 consider other approaches:
       - kill mysql (safety);
       - try multiple locations for opening a log file;
       - switch server to protected/readonly mode
       - ...
    */
    if (binlog_error_action == ABORT_SERVER)
    {
      exec_binlog_error_action_abort("Either disk is full or file system is"
                                     " read only while rotating the binlog."
                                     " Aborting the server.");
    }
    else
      sql_print_error("Could not open %s for logging (error %d). "
                      "Turning logging off for the whole duration "
                      "of the MySQL server process. To turn it on "
                      "again: fix the cause, shutdown the MySQL "
                      "server and restart it.",
                      new_name_ptr, errno);
    close(LOG_CLOSE_INDEX, false /*need_lock_log=false*/,
          false/*need_lock_index=false*/);
  }

  mysql_mutex_unlock(&LOCK_index);
  if (need_lock_log)
    mysql_mutex_unlock(&LOCK_log);
  DEBUG_SYNC(current_thd, "after_disable_binlog");
  DBUG_RETURN(error);
}


#ifdef HAVE_REPLICATION
/**
  Called after an event has been written to the relay log by the IO
  thread.  This flushes and possibly syncs the file (according to the
  sync options), rotates the file if it has grown over the limit, and
  finally calls signal_update().

  @note The caller must hold LOCK_log before invoking this function.

  @param mi Master_info for the IO thread.
  @param need_data_lock If true, mi->data_lock will be acquired if a
  rotation is needed.  Otherwise, mi->data_lock must be held by the
  caller.

  @retval false success
  @retval true error
*/
bool MYSQL_BIN_LOG::after_append_to_relay_log(Master_info *mi)
{
  DBUG_ENTER("MYSQL_BIN_LOG::after_append_to_relay_log");
  DBUG_PRINT("info",("max_size: %lu",max_size));

  // Check pre-conditions
  mysql_mutex_assert_owner(&LOCK_log);
  mysql_mutex_assert_owner(&mi->data_lock);
  DBUG_ASSERT(is_relay_log);
  DBUG_ASSERT(current_thd->system_thread == SYSTEM_THREAD_SLAVE_IO);

  /*
    We allow the relay log rotation by relay log size
    only if the trx parser is not inside a transaction.
  */
  bool can_rotate= mi->transaction_parser.is_not_inside_transaction();

#ifndef DBUG_OFF
  if ((uint) my_b_append_tell(&log_file) >
      DBUG_EVALUATE_IF("rotate_slave_debug_group", 500, max_size) &&
      !can_rotate)
  {
    DBUG_PRINT("info",("Postponing the rotation by size waiting for "
                       "the end of the current transaction."));
  }
#endif

  // Flush and sync
  bool error= false;
  if (flush_and_sync(0) == 0 && can_rotate)
  {
    /*
      If the last event of the transaction has been flushed, we can add
      the GTID (if it is not empty) to the logged set, or else it will
      not be available in the Previous GTIDs of the next relay log file
      if we are going to rotate the relay log.
    */
    Gtid *last_gtid_queued= mi->get_last_gtid_queued();
    if (!last_gtid_queued->is_empty())
    {
      global_sid_lock->rdlock();
      mi->rli->add_logged_gtid(last_gtid_queued->sidno,
                               last_gtid_queued->gno);
      global_sid_lock->unlock();
      mi->clear_last_gtid_queued();
    }

    /*
      If relay log is too big, rotate. But only if not in the middle of a
      transaction when GTIDs are enabled.
      We now try to mimic the following master binlog behavior: "A transaction
      is written in one chunk to the binary log, so it is never split between
      several binary logs. Therefore, if you have big transactions, you might
      see binary log files larger than max_binlog_size."
    */
    if ((uint) my_b_append_tell(&log_file) >
        DBUG_EVALUATE_IF("rotate_slave_debug_group", 500, max_size))
    {
      error= new_file_without_locking(mi->get_mi_description_event());
    }
  }

  signal_update();

  DBUG_RETURN(error);
}


bool MYSQL_BIN_LOG::append_event(Log_event* ev, Master_info *mi)
{
  DBUG_ENTER("MYSQL_BIN_LOG::append");

  // check preconditions
  DBUG_ASSERT(log_file.type == SEQ_READ_APPEND);
  DBUG_ASSERT(is_relay_log);

  // acquire locks
  mysql_mutex_lock(&LOCK_log);

  // write data
  bool error = false;
  if (ev->write(&log_file) == 0)
  {
    bytes_written+= ev->common_header->data_written;
    error= after_append_to_relay_log(mi);
  }
  else
    error= true;

  mysql_mutex_unlock(&LOCK_log);
  DBUG_RETURN(error);
}


bool MYSQL_BIN_LOG::append_buffer(const char* buf, uint len, Master_info *mi)
{
  DBUG_ENTER("MYSQL_BIN_LOG::append_buffer");

  // check preconditions
  DBUG_ASSERT(log_file.type == SEQ_READ_APPEND);
  DBUG_ASSERT(is_relay_log);
  mysql_mutex_assert_owner(&LOCK_log);

  // write data
  bool error= false;
  if (my_b_append(&log_file,(uchar*) buf,len) == 0)
  {
    bytes_written += len;
    error= after_append_to_relay_log(mi);
  }
  else
    error= true;

  DBUG_RETURN(error);
}
#endif // ifdef HAVE_REPLICATION

bool MYSQL_BIN_LOG::flush_and_sync(const bool force)
{
  mysql_mutex_assert_owner(&LOCK_log);

  if (flush_io_cache(&log_file))
    return 1;

  std::pair<bool, bool> result= sync_binlog_file(force);

  return result.first;
}

void MYSQL_BIN_LOG::start_union_events(THD *thd, query_id_t query_id_param)
{
  DBUG_ASSERT(!thd->binlog_evt_union.do_union);
  thd->binlog_evt_union.do_union= TRUE;
  thd->binlog_evt_union.unioned_events= FALSE;
  thd->binlog_evt_union.unioned_events_trans= FALSE;
  thd->binlog_evt_union.first_query_id= query_id_param;
}

void MYSQL_BIN_LOG::stop_union_events(THD *thd)
{
  DBUG_ASSERT(thd->binlog_evt_union.do_union);
  thd->binlog_evt_union.do_union= FALSE;
}

bool MYSQL_BIN_LOG::is_query_in_union(THD *thd, query_id_t query_id_param)
{
  return (thd->binlog_evt_union.do_union && 
          query_id_param >= thd->binlog_evt_union.first_query_id);
}

/*
  Updates thd's position-of-next-event variables
  after a *real* write a file.
 */
void MYSQL_BIN_LOG::update_thd_next_event_pos(THD* thd)
{
  if (likely(thd != NULL))
  {
    thd->set_next_event_pos(log_file_name,
                            my_b_tell(&log_file));
  }
}

/*
  Moves the last bunch of rows from the pending Rows event to a cache (either
  transactional cache if is_transaction is @c true, or the non-transactional
  cache otherwise. Sets a new pending event.

  @param thd               a pointer to the user thread.
  @param evt               a pointer to the row event.
  @param is_transactional  @c true indicates a transactional cache,
                           otherwise @c false a non-transactional.
*/
int
MYSQL_BIN_LOG::flush_and_set_pending_rows_event(THD *thd,
                                                Rows_log_event* event,
                                                bool is_transactional)
{
  DBUG_ENTER("MYSQL_BIN_LOG::flush_and_set_pending_rows_event(event)");
  DBUG_ASSERT(mysql_bin_log.is_open());
  DBUG_PRINT("enter", ("event: 0x%lx", (long) event));

  int error= 0;
  binlog_cache_mngr *const cache_mngr= thd_get_cache_mngr(thd);

  DBUG_ASSERT(cache_mngr);

  binlog_cache_data *cache_data=
    cache_mngr->get_binlog_cache_data(is_transactional);

  DBUG_PRINT("info", ("cache_mngr->pending(): 0x%lx", (long) cache_data->pending()));

  if (Rows_log_event* pending= cache_data->pending())
  {
    /*
      Write pending event to the cache.
    */
    if (cache_data->write_event(thd, pending))
    {
      set_write_error(thd, is_transactional);
      if (check_write_error(thd) && cache_data &&
          stmt_cannot_safely_rollback(thd))
        cache_data->set_incident();
      delete pending;
      cache_data->set_pending(NULL);
      DBUG_RETURN(1);
    }

    delete pending;
  }

  cache_data->set_pending(event);

  DBUG_RETURN(error);
}

/**
  Write an event to the binary log.
*/

bool MYSQL_BIN_LOG::write_event(Log_event *event_info)
{
  THD *thd= event_info->thd;
  bool error= 1;
  DBUG_ENTER("MYSQL_BIN_LOG::write_event(Log_event *)");

  if (thd->binlog_evt_union.do_union)
  {
    /*
      In Stored function; Remember that function call caused an update.
      We will log the function call to the binary log on function exit
    */
    thd->binlog_evt_union.unioned_events= TRUE;
    thd->binlog_evt_union.unioned_events_trans |=
      event_info->is_using_trans_cache();
    DBUG_RETURN(0);
  }

  /*
    We only end the statement if we are in a top-level statement.  If
    we are inside a stored function, we do not end the statement since
    this will close all tables on the slave. But there can be a special case
    where we are inside a stored function/trigger and a SAVEPOINT is being
    set in side the stored function/trigger. This SAVEPOINT execution will
    force the pending event to be flushed without an STMT_END_F flag. This
    will result in a case where following DMLs will be considered as part of
    same statement and result in data loss on slave. Hence in this case we
    force the end_stmt to be true.
  */
  bool const end_stmt= (thd->in_sub_stmt && thd->lex->sql_command ==
                        SQLCOM_SAVEPOINT)? true:
    (thd->locked_tables_mode && thd->lex->requires_prelocking());
  if (thd->binlog_flush_pending_rows_event(end_stmt,
                                           event_info->is_using_trans_cache()))
    DBUG_RETURN(error);

  /*
     In most cases this is only called if 'is_open()' is true; in fact this is
     mostly called if is_open() *was* true a few instructions before, but it
     could have changed since.
  */
  if (likely(is_open()))
  {
#ifdef HAVE_REPLICATION
    /*
      In the future we need to add to the following if tests like
      "do the involved tables match (to be implemented)
      binlog_[wild_]{do|ignore}_table?" (WL#1049)"
    */
    const char *local_db= event_info->get_db();
    if ((thd && !(thd->variables.option_bits & OPTION_BIN_LOG)) ||
	(thd->lex->sql_command != SQLCOM_ROLLBACK_TO_SAVEPOINT &&
         thd->lex->sql_command != SQLCOM_SAVEPOINT &&
         (!event_info->is_no_filter_event() && 
          !binlog_filter->db_ok(local_db))))
      DBUG_RETURN(0);
#endif /* HAVE_REPLICATION */

    DBUG_ASSERT(event_info->is_using_trans_cache() || event_info->is_using_stmt_cache());
    
    if (binlog_start_trans_and_stmt(thd, event_info))
      DBUG_RETURN(error);

    bool is_trans_cache= event_info->is_using_trans_cache();
    binlog_cache_mngr *cache_mngr= thd_get_cache_mngr(thd);
    binlog_cache_data *cache_data= cache_mngr->get_binlog_cache_data(is_trans_cache);
    
    DBUG_PRINT("info",("event type: %d",event_info->get_type_code()));

    /*
       No check for auto events flag here - this write method should
       never be called if auto-events are enabled.

       Write first log events which describe the 'run environment'
       of the SQL command. If row-based binlogging, Insert_id, Rand
       and other kind of "setting context" events are not needed.
    */
    if (thd)
    {
      if (!thd->is_current_stmt_binlog_format_row())
      {
        if (thd->stmt_depends_on_first_successful_insert_id_in_prev_stmt)
        {
          Intvar_log_event e(thd,(uchar) binary_log::Intvar_event::LAST_INSERT_ID_EVENT,
                             thd->first_successful_insert_id_in_prev_stmt_for_binlog,
                             event_info->event_cache_type, event_info->event_logging_type);
          if (cache_data->write_event(thd, &e))
            goto err;
        }
        if (thd->auto_inc_intervals_in_cur_stmt_for_binlog.nb_elements() > 0)
        {
          DBUG_PRINT("info",("number of auto_inc intervals: %u",
                             thd->auto_inc_intervals_in_cur_stmt_for_binlog.
                             nb_elements()));
          Intvar_log_event e(thd, (uchar) binary_log::Intvar_event::INSERT_ID_EVENT,
                             thd->auto_inc_intervals_in_cur_stmt_for_binlog.
                             minimum(), event_info->event_cache_type,
                             event_info->event_logging_type);
          if (cache_data->write_event(thd, &e))
            goto err;
        }
        if (thd->rand_used)
        {
          Rand_log_event e(thd,thd->rand_saved_seed1,thd->rand_saved_seed2,
                           event_info->event_cache_type,
                           event_info->event_logging_type);
          if (cache_data->write_event(thd, &e))
            goto err;
        }
        if (!thd->user_var_events.empty())
        {
          for (size_t i= 0; i < thd->user_var_events.size(); i++)
          {
            BINLOG_USER_VAR_EVENT *user_var_event= thd->user_var_events[i];

            /* setting flags for user var log event */
            uchar flags= User_var_log_event::UNDEF_F;
            if (user_var_event->unsigned_flag)
              flags|= User_var_log_event::UNSIGNED_F;

            User_var_log_event e(thd,
                                 user_var_event->user_var_event->entry_name.ptr(),
                                 user_var_event->user_var_event->entry_name.length(),
                                 user_var_event->value,
                                 user_var_event->length,
                                 user_var_event->type,
                                 user_var_event->charset_number, flags,
                                 event_info->event_cache_type,
                                 event_info->event_logging_type);
            if (cache_data->write_event(thd, &e))
              goto err;
          }
        }
      }
    }

    /*
      Write the event.
    */
    if (cache_data->write_event(thd, event_info))
      goto err;

    if (DBUG_EVALUATE_IF("injecting_fault_writing", 1, 0))
      goto err;

    /*
      After writing the event, if the trx-cache was used and any unsafe
      change was written into it, the cache is marked as cannot safely
      roll back.
    */
    if (is_trans_cache && stmt_cannot_safely_rollback(thd))
      cache_mngr->trx_cache.set_cannot_rollback();

    error= 0;

err:
    if (error)
    {
      set_write_error(thd, is_trans_cache);
      if (check_write_error(thd) && cache_data &&
          stmt_cannot_safely_rollback(thd))
        cache_data->set_incident();
    }
  }

  DBUG_RETURN(error);
}

/**
  The method executes rotation when LOCK_log is already acquired
  by the caller.

  @param force_rotate  caller can request the log rotation
  @param check_purge   is set to true if rotation took place

  @note
    If rotation fails, for instance the server was unable 
    to create a new log file, we still try to write an 
    incident event to the current log.

  @note The caller must hold LOCK_log when invoking this function.

  @retval
    nonzero - error in rotating routine.
*/
int MYSQL_BIN_LOG::rotate(bool force_rotate, bool* check_purge)
{
  int error= 0;
  DBUG_ENTER("MYSQL_BIN_LOG::rotate");

  DBUG_ASSERT(!is_relay_log);
  mysql_mutex_assert_owner(&LOCK_log);

  *check_purge= false;

  if (DBUG_EVALUATE_IF("force_rotate", 1, 0) || force_rotate ||
      (my_b_tell(&log_file) >= (my_off_t) max_size))
  {
    error= new_file_without_locking(NULL);
    *check_purge= true;
  }
  DBUG_RETURN(error);
}

/**
  The method executes logs purging routine.

  @retval
    nonzero - error in rotating routine.
*/
void MYSQL_BIN_LOG::purge()
{
#ifdef HAVE_REPLICATION
  if (expire_logs_days)
  {
    DEBUG_SYNC(current_thd, "at_purge_logs_before_date");
    time_t purge_time= my_time(0) - expire_logs_days*24*60*60;
    DBUG_EXECUTE_IF("expire_logs_always",
                    { purge_time= my_time(0);});
    if (purge_time >= 0)
    {
      /*
        Flush logs for storage engines, so that the last transaction
        is fsynced inside storage engines.
      */
      ha_flush_logs(NULL);
      purge_logs_before_date(purge_time, true);
    }
  }
#endif
}

/**
  Execute a FLUSH LOGS statement.

  The method is a shortcut of @c rotate() and @c purge().
  LOCK_log is acquired prior to rotate and is released after it.

  @param force_rotate  caller can request the log rotation

  @retval
    nonzero - error in rotating routine.
*/
int MYSQL_BIN_LOG::rotate_and_purge(THD* thd, bool force_rotate)
{
  int error= 0;
  DBUG_ENTER("MYSQL_BIN_LOG::rotate_and_purge");
  bool check_purge= false;

  /*
    FLUSH BINARY LOGS command should ignore 'read-only' and 'super_read_only'
    options so that it can update 'mysql.gtid_executed' replication repository
    table.
  */
  thd->set_skip_readonly_check();
  /*
    Wait for handlerton to insert any pending information into the binlog.
    For e.g. ha_ndbcluster which updates the binlog asynchronously this is
    needed so that the user see its own commands in the binlog.
  */
  ha_binlog_wait(thd);

  DBUG_ASSERT(!is_relay_log);
  mysql_mutex_lock(&LOCK_log);
  error= rotate(force_rotate, &check_purge);
  /*
    NOTE: Run purge_logs wo/ holding LOCK_log because it does not need
          the mutex. Otherwise causes various deadlocks.
  */
  mysql_mutex_unlock(&LOCK_log);

  if (!error && check_purge)
    purge();

  DBUG_RETURN(error);
}

uint MYSQL_BIN_LOG::next_file_id()
{
  uint res;
  mysql_mutex_lock(&LOCK_log);
  res = file_id++;
  mysql_mutex_unlock(&LOCK_log);
  return res;
}


int MYSQL_BIN_LOG::get_gtid_executed(Sid_map *sid_map, Gtid_set *gtid_set)
{
  DBUG_ENTER("MYSQL_BIN_LOG::get_gtid_executed");
  int error= 0;

  mysql_mutex_lock(&mysql_bin_log.LOCK_commit);
  global_sid_lock->wrlock();

  enum_return_status return_status= global_sid_map->copy(sid_map);
  if (return_status != RETURN_STATUS_OK)
  {
    error= 1;
    goto end;
  }

  return_status= gtid_set->add_gtid_set(gtid_state->get_executed_gtids());
  if (return_status != RETURN_STATUS_OK)
    error= 1;

end:
  global_sid_lock->unlock();
  mysql_mutex_unlock(&mysql_bin_log.LOCK_commit);

  DBUG_RETURN(error);
}


/**
  Auxiliary function to read a page from the cache and set the given
  buffer pointer to point to the beginning of the page and the given
  length pointer to point to the end of it.

  @param cache IO_CACHE to read from
  @param[OUT] buf_p Will be set to point to the beginning of the page.
  @param[OUT] buf_len_p Will be set to the length of the buffer.

  @retval false Success
  @retval true Error reading from the cache.
*/
static bool read_cache_page(IO_CACHE *cache, uchar **buf_p, uint32 *buf_len_p)
{
  DBUG_ASSERT(*buf_len_p == 0);
  cache->read_pos= cache->read_end;
  *buf_len_p= my_b_fill(cache);
  *buf_p= cache->read_pos;
  return cache->error ? true : false;
}


/**
  Write the contents of the given IO_CACHE to the binary log.

  The cache will be reset as a READ_CACHE to be able to read the
  contents from it.

  The data will be post-processed: see class Binlog_event_writer for
  details.

  @param cache Events will be read from this IO_CACHE.
  @param writer Events will be written to this Binlog_event_writer.

  @retval true IO error.
  @retval false Success.

  @see MYSQL_BIN_LOG::write_cache
*/
bool MYSQL_BIN_LOG::do_write_cache(IO_CACHE *cache, Binlog_event_writer *writer)
{
  DBUG_ENTER("MYSQL_BIN_LOG::do_write_cache");

  DBUG_EXECUTE_IF("simulate_do_write_cache_failure",
                  {
                    /*
                       see binlog_cache_data::write_event() that reacts on
                       @c simulate_disk_full_at_flush_pending.
                    */
                    DBUG_SET("-d,simulate_do_write_cache_failure");
                    DBUG_RETURN(true);
                  });

#ifndef DBUG_OFF
  uint64 expected_total_len= my_b_tell(cache);
#endif

  DBUG_EXECUTE_IF("simulate_tmpdir_partition_full",
                  {
                    DBUG_SET("+d,simulate_file_write_error");
                  });

  if (reinit_io_cache(cache, READ_CACHE, 0, 0, 0))
  {
    DBUG_EXECUTE_IF("simulate_tmpdir_partition_full",
                    {
                      DBUG_SET("-d,simulate_file_write_error");
                    });
    DBUG_RETURN(true);
  }

  uchar *buf= cache->read_pos;
  uint32 buf_len= my_b_bytes_in_cache(cache);
  uint32 event_len= 0;
  uchar header[LOG_EVENT_HEADER_LEN];
  uint32 header_len= 0;

  /*
    Each iteration of this loop processes all or a part of
    1) an event header or 2) an event body from the IO_CACHE.
  */
  while (true)
  {
    /**
      Nothing in cache: try to refill, and if cache was ended here,
      return success.  This code is needed even on the first iteration
      of the loop, because reinit_io_cache may or may not fill the
      first page.
    */
    if (buf_len == 0)
    {
      if (read_cache_page(cache, &buf, &buf_len))
      {
        /**
          @todo: this can happen in case of disk corruption in the
          IO_CACHE.  We may have written a half transaction (even half
          event) to the binlog.  We should rollback the transaction
          and truncate the binlog.  /Sven
        */
        DBUG_ASSERT(0);
      }
      if (buf_len == 0)
      {
        /**
          @todo: this can happen in case of disk corruption in the
          IO_CACHE.  We may have written a half transaction (even half
          event) to the binlog.  We should rollback the transaction
          and truncate the binlog.  /Sven
        */
        DBUG_ASSERT(my_b_tell(cache) == expected_total_len);
        /* Arrive the end of the cache */
        DBUG_RETURN(false);
      }
    }

    /* Write event header into binlog */
    if (event_len == 0)
    {
      /* data in the buf may be smaller than header size.*/
      uint32 header_incr =
        std::min<uint32>(LOG_EVENT_HEADER_LEN - header_len, buf_len);

      memcpy(header + header_len, buf, header_incr);
      header_len += header_incr;
      buf += header_incr;
      buf_len -= header_incr;

      if (header_len == LOG_EVENT_HEADER_LEN)
      {
        // Flush event header.
        uchar *header_p= header;
        if (writer->write_event_part(&header_p, &header_len, &event_len))
          DBUG_RETURN(true);
        DBUG_ASSERT(header_len == 0);
      }
    }
    else
    {
      /* Write all or part of the event body to binlog */
      if (writer->write_event_part(&buf, &buf_len, &event_len))
        DBUG_RETURN(true);
    }
  }
}

/**
  Writes an incident event to stmt_cache.

  @param ev Incident event to be written
  @param thd Thread variable
  @param need_lock_log If true, will acquire LOCK_log; otherwise the
  caller should already have acquired LOCK_log.
  @param err_msg Error message written to log file for the incident.
  @do_flush_and_sync If true, will call flush_and_sync(), rotate() and
  purge().

  @retval false error
  @retval true success
*/
bool MYSQL_BIN_LOG::write_incident(Incident_log_event *ev, THD *thd,
                                   bool need_lock_log, const char* err_msg,
                                   bool do_flush_and_sync)
{
  uint error= 0;
  DBUG_ENTER("MYSQL_BIN_LOG::write_incident");
  DBUG_ASSERT(err_msg);

  if (!is_open())
    DBUG_RETURN(error);

  // @todo make this work with the group log. /sven
  binlog_cache_mngr *const cache_mngr= thd_get_cache_mngr(thd);

#ifndef DBUG_OFF
  if (DBUG_EVALUATE_IF("simulate_write_incident_event_into_binlog_directly",
                       1, 0) && !cache_mngr->stmt_cache.is_binlog_empty())
  {
    /* The stmt_cache contains corruption data, so we can reset it. */
    cache_mngr->stmt_cache.reset();
  }
#endif

  /*
    If there is no binlog cache then we write incidents directly
    into the binlog. If caller needs GTIDs it has to setup the
    binlog cache (for the injector thread).
  */
  if (cache_mngr == NULL ||
      DBUG_EVALUATE_IF("simulate_write_incident_event_into_binlog_directly",
                       1, 0))
  {
    if (need_lock_log)
      mysql_mutex_lock(&LOCK_log);
    else
      mysql_mutex_assert_owner(&LOCK_log);
    /* Write an incident event into binlog directly. */
    error= ev->write(&log_file);
    /*
      Write an error to log. So that user might have a chance
      to be alerted and explore incident details.
    */
    if (!error)
      sql_print_error("%s An incident event has been written to the binary "
                      "log which will stop the slaves.", err_msg);
  }
  else // (cache_mngr != NULL)
  {
    if (!cache_mngr->stmt_cache.is_binlog_empty())
    {
      /* The stmt_cache contains corruption data, so we can reset it. */
      cache_mngr->stmt_cache.reset();
    }
    if (!cache_mngr->trx_cache.is_binlog_empty())
    {
      /* The trx_cache contains corruption data, so we can reset it. */
      cache_mngr->trx_cache.reset();
    }
    /*
      Write the incident event into stmt_cache, so that a GTID is generated and
      written for it prior to flushing the stmt_cache.
    */
    binlog_cache_data *cache_data= cache_mngr->get_binlog_cache_data(false);
    if ((error= cache_data->write_event(thd, ev)))
    {
      sql_print_error("Failed to write an incident event into stmt_cache.");
      cache_mngr->stmt_cache.reset();
      DBUG_RETURN(error);
    }

    if (need_lock_log)
      mysql_mutex_lock(&LOCK_log);
    else
      mysql_mutex_assert_owner(&LOCK_log);
  }

  if (do_flush_and_sync)
  {
    if (!error && !(error= flush_and_sync()))
    {
      bool check_purge= false;
      update_binlog_end_pos();
      is_rotating_caused_by_incident= true;
      error= rotate(true, &check_purge);
      is_rotating_caused_by_incident= false;
      if (!error && check_purge)
        purge();
    }
  }

  if (need_lock_log)
    mysql_mutex_unlock(&LOCK_log);

  /*
    Write an error to log. So that user might have a chance
    to be alerted and explore incident details.
  */
  if (!error && cache_mngr != NULL)
    sql_print_error("%s An incident event has been written to the binary "
                    "log which will stop the slaves.", err_msg);

  DBUG_RETURN(error);
}

bool MYSQL_BIN_LOG::write_dml_directly(THD* thd, const char *stmt, size_t stmt_len)
{
  bool ret= false;
  /* backup the original command */
  enum_sql_command save_sql_command= thd->lex->sql_command;

  /* Fake it as a DELETE statement, so it can be binlogged correctly */
  thd->lex->sql_command= SQLCOM_DELETE;

  if (thd->binlog_query(THD::STMT_QUERY_TYPE, stmt, stmt_len,
                        FALSE, FALSE, FALSE, 0) ||
      commit(thd, false) != TC_LOG::RESULT_SUCCESS)
  {
    ret= true;
  }

  thd->lex->sql_command= save_sql_command;
  return ret;
}


/**
  Creates an incident event and writes it to the binary log.

  @param thd  Thread variable
  @param ev   Incident event to be written
  @param err_msg Error message written to log file for the incident.
  @param lock If the binary lock should be locked or not

  @retval
    0    error
  @retval
    1    success
*/
bool MYSQL_BIN_LOG::write_incident(THD *thd, bool need_lock_log,
                                   const char* err_msg,
                                   bool do_flush_and_sync)
{
  DBUG_ENTER("MYSQL_BIN_LOG::write_incident");

  if (!is_open())
    DBUG_RETURN(0);

  LEX_STRING write_error_msg= {(char*) err_msg, strlen(err_msg)};
  binary_log::Incident_event::enum_incident incident=
                              binary_log::Incident_event::INCIDENT_LOST_EVENTS;
  Incident_log_event ev(thd, incident, write_error_msg);

  DBUG_RETURN(write_incident(&ev, thd, need_lock_log, err_msg,
                             do_flush_and_sync));
}


/**
  Write the contents of the statement or transaction cache to the binary log.

  Comparison with do_write_cache:

  - do_write_cache is a lower-level function that only performs the
    actual write.

  - write_cache is a higher-level function that calls do_write_cache
    and additionally performs some maintenance tasks, including:
    - report any errors that occurred
    - write incident event if needed
    - update gtid_state
    - update thd.binlog_next_event_pos

  @param thd Thread variable

  @param cache_data Events will be read from the IO_CACHE of this
  cache_data object.

  @param writer Events will be written to this Binlog_event_writer.

  @retval true IO error.
  @retval false Success.

  @note We only come here if there is something in the cache.
  @note Whatever is in the cache is always a complete transaction.
  @note 'cache' needs to be reinitialized after this functions returns.
*/
bool MYSQL_BIN_LOG::write_cache(THD *thd, binlog_cache_data *cache_data,
                                Binlog_event_writer *writer)
{
  DBUG_ENTER("MYSQL_BIN_LOG::write_cache(THD *, binlog_cache_data *, bool)");

  IO_CACHE *cache= &cache_data->cache_log;
  bool incident= cache_data->has_incident();

  mysql_mutex_assert_owner(&LOCK_log);

  DBUG_ASSERT(is_open());
  if (likely(is_open()))                       // Should always be true
  {
    /*
      We only bother to write to the binary log if there is anything
      to write.

      @todo Is this check redundant? Probably this is only called if
      there is anything in the cache (see @note in comment above this
      function). Check if we can replace this by an assertion. /Sven
    */
    if (my_b_tell(cache) > 0)
    {
      DBUG_EXECUTE_IF("crash_before_writing_xid",
                      {
                        if ((write_error= do_write_cache(cache, writer)))
                          DBUG_PRINT("info", ("error writing binlog cache: %d",
                                              write_error));
                        flush_and_sync(true);
                        DBUG_PRINT("info", ("crashing before writing xid"));
                        DBUG_SUICIDE();
                      });
      if ((write_error= do_write_cache(cache, writer)))
        goto err;

      const char* err_msg= "Non-transactional changes did not get into "
                           "the binlog.";
      if (incident && write_incident(thd, false/*need_lock_log=false*/,
                                     err_msg,
                                     false/*do_flush_and_sync==false*/))
        goto err;

      DBUG_EXECUTE_IF("half_binlogged_transaction", DBUG_SUICIDE(););
      if (cache->error)				// Error on read
      {
        char errbuf[MYSYS_STRERROR_SIZE];
        sql_print_error(ER(ER_ERROR_ON_READ), cache->file_name,
                        errno, my_strerror(errbuf, sizeof(errbuf), errno));
        write_error= true; // Don't give more errors
        goto err;
      }
    }
    update_thd_next_event_pos(thd);
  }

  DBUG_RETURN(false);

err:
  if (!write_error)
  {
    char errbuf[MYSYS_STRERROR_SIZE];
    write_error= true;
    sql_print_error(ER(ER_ERROR_ON_WRITE), name,
                    errno, my_strerror(errbuf, sizeof(errbuf), errno));
  }

  /*
    If the flush has failed due to ENOSPC, set the flush_error flag.
  */
  if (cache->error && thd->is_error() && my_errno() == ENOSPC)
  {
    cache_data->set_flush_error(thd);
  }
  thd->commit_error= THD::CE_FLUSH_ERROR;

  DBUG_RETURN(true);
}


/**
  Wait until we get a signal that the relay log has been updated.

  @param[in] thd        Thread variable
  @param[in] timeout    a pointer to a timespec;
                        NULL means to wait w/o timeout.

  @retval    0          if got signalled on update
  @retval    non-0      if wait timeout elapsed

  @note
    One must have a lock on LOCK_log before calling this function.
*/

int MYSQL_BIN_LOG::wait_for_update_relay_log(THD* thd, const struct timespec *timeout)
{
  int ret= 0;
  PSI_stage_info old_stage;
  DBUG_ENTER("wait_for_update_relay_log");

  thd->ENTER_COND(&update_cond, &LOCK_log,
                  &stage_slave_has_read_all_relay_log,
                  &old_stage);

  if (!timeout)
    mysql_cond_wait(&update_cond, &LOCK_log);
  else
    ret= mysql_cond_timedwait(&update_cond, &LOCK_log,
                              const_cast<struct timespec *>(timeout));
  mysql_mutex_unlock(&LOCK_log);
  thd->EXIT_COND(&old_stage);

  DBUG_RETURN(ret);
}

/**
  Wait until we get a signal that the binary log has been updated.
  Applies to master only.
     
  NOTES
  @param[in] thd        a THD struct
  @param[in] timeout    a pointer to a timespec;
                        NULL means to wait w/o timeout.
  @retval    0          if got signalled on update
  @retval    non-0      if wait timeout elapsed
  @note
    LOCK_log must be taken before calling this function.
    LOCK_log is being released while the thread is waiting.
    LOCK_log is released by the caller.
*/

int MYSQL_BIN_LOG::wait_for_update_bin_log(THD* thd,
                                           const struct timespec *timeout)
{
  int ret= 0;
  DBUG_ENTER("wait_for_update_bin_log");

  if (!timeout)
    mysql_cond_wait(&update_cond, &LOCK_binlog_end_pos);
  else
    ret= mysql_cond_timedwait(&update_cond, &LOCK_binlog_end_pos,
                              const_cast<struct timespec *>(timeout));
  DBUG_RETURN(ret);
}


/**
  Close the log file.

  @param exiting     Bitmask for one or more of the following bits:
          - LOG_CLOSE_INDEX : if we should close the index file
          - LOG_CLOSE_TO_BE_OPENED : if we intend to call open
                                     at once after close.
          - LOG_CLOSE_STOP_EVENT : write a 'stop' event to the log

  @param need_lock_log If true, this function acquires LOCK_log;
  otherwise the caller should already have acquired it.

  @param need_lock_index If true, this function acquires LOCK_index;
  otherwise the caller should already have acquired it.

  @note
    One can do an open on the object at once after doing a close.
    The internal structures are not freed until cleanup() is called
*/

void MYSQL_BIN_LOG::close(uint exiting, bool need_lock_log,
                          bool need_lock_index)
{					// One can't set log_type here!
  DBUG_ENTER("MYSQL_BIN_LOG::close");
  DBUG_PRINT("enter",("exiting: %d", (int) exiting));
  if (need_lock_log)
    mysql_mutex_lock(&LOCK_log);
  else
    mysql_mutex_assert_owner(&LOCK_log);

  if (log_state.atomic_get() == LOG_OPENED)
  {
#ifdef HAVE_REPLICATION
    if ((exiting & LOG_CLOSE_STOP_EVENT) != 0)
    {
      /**
        TODO(WL#7546): Change the implementation to Stop_event after write() is
        moved into libbinlogevents
      */
      Stop_log_event s;
      // the checksumming rule for relay-log case is similar to Rotate
        s.common_footer->checksum_alg= is_relay_log ? relay_log_checksum_alg :
                                       static_cast<enum_binlog_checksum_alg>
                                       (binlog_checksum_options);
      DBUG_ASSERT(!is_relay_log ||
                  relay_log_checksum_alg != binary_log::BINLOG_CHECKSUM_ALG_UNDEF);
      s.write(&log_file);
      bytes_written+= s.common_header->data_written;
      flush_io_cache(&log_file);
      update_binlog_end_pos();
    }
#endif /* HAVE_REPLICATION */

    /* don't pwrite in a file opened with O_APPEND - it doesn't work */
    if (log_file.type == WRITE_CACHE)
    {
      my_off_t offset= BIN_LOG_HEADER_SIZE + FLAGS_OFFSET;
      my_off_t org_position= mysql_file_tell(log_file.file, MYF(0));
      uchar flags= 0;            // clearing LOG_EVENT_BINLOG_IN_USE_F
      mysql_file_pwrite(log_file.file, &flags, 1, offset, MYF(0));
      /*
        Restore position so that anything we have in the IO_cache is written
        to the correct position.
        We need the seek here, as mysql_file_pwrite() is not guaranteed to keep the
        original position on system that doesn't support pwrite().
      */
      mysql_file_seek(log_file.file, org_position, MY_SEEK_SET, MYF(0));
    }

    /* this will cleanup IO_CACHE, sync and close the file */
    if (log_state.atomic_get() == LOG_OPENED)
    {
      end_io_cache(&log_file);

      if (mysql_file_sync(log_file.file, MYF(MY_WME)) && ! write_error)
      {
        char errbuf[MYSYS_STRERROR_SIZE];
        write_error= 1;
        sql_print_error(ER_DEFAULT(ER_ERROR_ON_WRITE), name, errno,
                        my_strerror(errbuf, sizeof(errbuf), errno));
      }

      if (mysql_file_close(log_file.file, MYF(MY_WME)) && ! write_error)
      {
        char errbuf[MYSYS_STRERROR_SIZE];
        write_error= 1;
        sql_print_error(ER_DEFAULT(ER_ERROR_ON_WRITE), name, errno,
                        my_strerror(errbuf, sizeof(errbuf), errno));
      }
    }

    log_state.atomic_set((exiting & LOG_CLOSE_TO_BE_OPENED) ? LOG_TO_BE_OPENED : LOG_CLOSED);
    my_free(name);
    name= NULL;
  }

  /*
    The following test is needed even if is_open() is not set, as we may have
    called a not complete close earlier and the index file is still open.
  */

  if (need_lock_index)
    mysql_mutex_lock(&LOCK_index);
  else
    mysql_mutex_assert_owner(&LOCK_index);

  if ((exiting & LOG_CLOSE_INDEX) && my_b_inited(&index_file))
  {
    end_io_cache(&index_file);
    if (mysql_file_close(index_file.file, MYF(0)) < 0 && ! write_error)
    {
      char errbuf[MYSYS_STRERROR_SIZE];
      write_error= 1;
      sql_print_error(ER(ER_ERROR_ON_WRITE), index_file_name,
                      errno, my_strerror(errbuf, sizeof(errbuf), errno));
    }
  }

  if (need_lock_index)
    mysql_mutex_unlock(&LOCK_index);

  log_state.atomic_set((exiting & LOG_CLOSE_TO_BE_OPENED) ? LOG_TO_BE_OPENED : LOG_CLOSED);
  my_free(name);
  name= NULL;

  if (need_lock_log)
    mysql_mutex_unlock(&LOCK_log);

  DBUG_VOID_RETURN;
}


void MYSQL_BIN_LOG::set_max_size(ulong max_size_arg)
{
  /*
    We need to take locks, otherwise this may happen:
    new_file() is called, calls open(old_max_size), then before open() starts,
    set_max_size() sets max_size to max_size_arg, then open() starts and
    uses the old_max_size argument, so max_size_arg has been overwritten and
    it's like if the SET command was never run.
  */
  DBUG_ENTER("MYSQL_BIN_LOG::set_max_size");
  mysql_mutex_lock(&LOCK_log);
  if (is_open())
    max_size= max_size_arg;
  mysql_mutex_unlock(&LOCK_log);
  DBUG_VOID_RETURN;
}

/****** transaction coordinator log for 2pc - binlog() based solution ******/

/**
  @todo
  keep in-memory list of prepared transactions
  (add to list in log(), remove on unlog())
  and copy it to the new binlog if rotated
  but let's check the behaviour of tc_log_page_waits first!
*/

int MYSQL_BIN_LOG::open_binlog(const char *opt_name)
{
  LOG_INFO log_info;
  int      error= 1;

  /*
    This function is used for 2pc transaction coordination.  Hence, it
    is never used for relay logs.
  */
  DBUG_ASSERT(!is_relay_log);
  DBUG_ASSERT(total_ha_2pc > 1 || (1 == total_ha_2pc && opt_bin_log));
  DBUG_ASSERT(opt_name && opt_name[0]);

  if (!my_b_inited(&index_file))
  {
    /* There was a failure to open the index file, can't open the binlog */
    cleanup();
    return 1;
  }

  if (using_heuristic_recover())
  {
    /* generate a new binlog to mask a corrupted one */
    mysql_mutex_lock(&LOCK_log);
    open_binlog(opt_name, 0, max_binlog_size, false,
                true/*need_lock_index=true*/,
                true/*need_sid_lock=true*/,
                NULL);
    mysql_mutex_unlock(&LOCK_log);
    cleanup();
    return 1;
  }

  if ((error= find_log_pos(&log_info, NullS, true/*need_lock_index=true*/)))
  {
    if (error != LOG_INFO_EOF)
      sql_print_error("find_log_pos() failed (error: %d)", error);
    else
      error= 0;
    goto err;
  }

  {
    const char *errmsg;
    IO_CACHE    log;
    File        file;
    Log_event  *ev=0;
    Format_description_log_event fdle(BINLOG_VERSION);
    char        log_name[FN_REFLEN];
    my_off_t    valid_pos= 0;
    my_off_t    binlog_size;
    MY_STAT     s;

    if (! fdle.is_valid())
      goto err;

    do
    {
      strmake(log_name, log_info.log_file_name, sizeof(log_name)-1);
    } while (!(error= find_next_log(&log_info, true/*need_lock_index=true*/)));

    if (error !=  LOG_INFO_EOF)
    {
      sql_print_error("find_log_pos() failed (error: %d)", error);
      goto err;
    }

    if ((file= open_binlog_file(&log, log_name, &errmsg)) < 0)
    {
      sql_print_error("%s", errmsg);
      goto err;
    }

    my_stat(log_name, &s, MYF(0));
    binlog_size= s.st_size;

    /*
      If the binary log was not properly closed it means that the server
      may have crashed. In that case, we need to call MYSQL_BIN_LOG::recover
      to:

        a) collect logged XIDs;
        b) complete the 2PC of the pending XIDs;
        c) collect the last valid position.

      Therefore, we do need to iterate over the binary log, even if
      total_ha_2pc == 1, to find the last valid group of events written.
      Later we will take this value and truncate the log if need be.
    */
    if ((ev= Log_event::read_log_event(&log, 0, &fdle,
                                       opt_master_verify_checksum)) &&
        ev->get_type_code() == binary_log::FORMAT_DESCRIPTION_EVENT &&
        (ev->common_header->flags & LOG_EVENT_BINLOG_IN_USE_F ||
         DBUG_EVALUATE_IF("eval_force_bin_log_recovery", true, false)))
    {
      sql_print_information("Recovering after a crash using %s", opt_name);
      valid_pos= my_b_tell(&log);
      error= recover(&log, (Format_description_log_event *)ev, &valid_pos);
    }
    else
      error=0;

    delete ev;
    end_io_cache(&log);
    mysql_file_close(file, MYF(MY_WME));

    if (error)
      goto err;

    /* Trim the crashed binlog file to last valid transaction
      or event (non-transaction) base on valid_pos. */
    if (valid_pos > 0)
    {
      if ((file= mysql_file_open(key_file_binlog, log_name,
                                 O_RDWR | O_BINARY, MYF(MY_WME))) < 0)
      {
        sql_print_error("Failed to open the crashed binlog file "
                        "when master server is recovering it.");
        return -1;
      }

      /* Change binlog file size to valid_pos */
      if (valid_pos < binlog_size)
      {
        if (my_chsize(file, valid_pos, 0, MYF(MY_WME)))
        {
          sql_print_error("Failed to trim the crashed binlog file "
                          "when master server is recovering it.");
          mysql_file_close(file, MYF(MY_WME));
          return -1;
        }
        else
        {
          sql_print_information("Crashed binlog file %s size is %llu, "
                                "but recovered up to %llu. Binlog trimmed to %llu bytes.",
                                log_name, binlog_size, valid_pos, valid_pos);
        }
      }

      /* Clear LOG_EVENT_BINLOG_IN_USE_F */
      my_off_t offset= BIN_LOG_HEADER_SIZE + FLAGS_OFFSET;
      uchar flags= 0;
      if (mysql_file_pwrite(file, &flags, 1, offset, MYF(0)) != 1)
      {
        sql_print_error("Failed to clear LOG_EVENT_BINLOG_IN_USE_F "
                        "for the crashed binlog file when master "
                        "server is recovering it.");
        mysql_file_close(file, MYF(MY_WME));
        return -1;
      }

      mysql_file_close(file, MYF(MY_WME));
    } //end if
  }

err:
  return error;
}

/** This is called on shutdown, after ha_panic. */
void MYSQL_BIN_LOG::close()
{
}

/*
  Prepare the transaction in the transaction coordinator.

  This function will prepare the transaction in the storage engines
  (by calling @c ha_prepare_low) what will write a prepare record
  to the log buffers.

  @retval 0    success
  @retval 1    error
*/
int MYSQL_BIN_LOG::prepare(THD *thd, bool all)
{
  DBUG_ENTER("MYSQL_BIN_LOG::prepare");

  DBUG_ASSERT(opt_bin_log);
  /*
    The applier thread explicitly overrides the value of sql_log_bin
    with the value of log_slave_updates.
  */
  DBUG_ASSERT(thd->slave_thread ?
              opt_log_slave_updates : thd->variables.sql_log_bin);

  /*
    Set HA_IGNORE_DURABILITY to not flush the prepared record of the
    transaction to the log of storage engine (for example, InnoDB
    redo log) during the prepare phase. So that we can flush prepared
    records of transactions to the log of storage engine in a group
    right before flushing them to binary log during binlog group
    commit flush stage. Reset to HA_REGULAR_DURABILITY at the
    beginning of parsing next command.
  */
  thd->durability_property= HA_IGNORE_DURABILITY;

  int error= ha_prepare_low(thd, all);

  DBUG_RETURN(error);
}

/**
  Commit the transaction in the transaction coordinator.

  This function will commit the sessions transaction in the binary log
  and in the storage engines (by calling @c ha_commit_low). If the
  transaction was successfully logged (or not successfully unlogged)
  but the commit in the engines did not succed, there is a risk of
  inconsistency between the engines and the binary log.

  For binary log group commit, the commit is separated into three
  parts:

  1. First part consists of filling the necessary caches and
     finalizing them (if they need to be finalized). After this,
     nothing is added to any of the caches.

  2. Second part execute an ordered flush and commit. This will be
     done using the group commit functionality in ordered_commit.

  3. Third part checks any errors resulting from the ordered commit
     and handles them appropriately.

  @retval RESULT_SUCCESS   success
  @retval RESULT_ABORTED   error, transaction was neither logged nor committed
  @retval RESULT_INCONSISTENT  error, transaction was logged but not committed
*/
TC_LOG::enum_result MYSQL_BIN_LOG::commit(THD *thd, bool all)
{
  DBUG_ENTER("MYSQL_BIN_LOG::commit");
  DBUG_PRINT("info", ("query='%s'",
                      thd == current_thd ? thd->query().str : NULL));
  binlog_cache_mngr *cache_mngr= thd_get_cache_mngr(thd);
  Transaction_ctx *trn_ctx= thd->get_transaction();
  my_xid xid= trn_ctx->xid_state()->get_xid()->get_my_xid();
  bool stmt_stuff_logged= false;
  bool trx_stuff_logged= false;
  bool skip_commit= is_loggable_xa_prepare(thd);

  DBUG_PRINT("enter", ("thd: 0x%llx, all: %s, xid: %llu, cache_mngr: 0x%llx",
                       (ulonglong) thd, YESNO(all), (ulonglong) xid,
                       (ulonglong) cache_mngr));

  /*
    No cache manager means nothing to log, but we still have to commit
    the transaction.
   */
  if (cache_mngr == NULL)
  {
    if (!skip_commit && ha_commit_low(thd, all))
      DBUG_RETURN(RESULT_ABORTED);
    DBUG_RETURN(RESULT_SUCCESS);
  }

  Transaction_ctx::enum_trx_scope trx_scope=  all ? Transaction_ctx::SESSION :
                                                    Transaction_ctx::STMT;

  DBUG_PRINT("debug", ("in_transaction: %s, no_2pc: %s, rw_ha_count: %d",
                       YESNO(thd->in_multi_stmt_transaction_mode()),
                       YESNO(trn_ctx->no_2pc(trx_scope)),
                       trn_ctx->rw_ha_count(trx_scope)));
  DBUG_PRINT("debug",
             ("all.cannot_safely_rollback(): %s, trx_cache_empty: %s",
              YESNO(trn_ctx->cannot_safely_rollback(Transaction_ctx::SESSION)),
              YESNO(cache_mngr->trx_cache.is_binlog_empty())));
  DBUG_PRINT("debug",
             ("stmt.cannot_safely_rollback(): %s, stmt_cache_empty: %s",
              YESNO(trn_ctx->cannot_safely_rollback(Transaction_ctx::STMT)),
              YESNO(cache_mngr->stmt_cache.is_binlog_empty())));


  /*
    If there are no handlertons registered, there is nothing to
    commit. Note that DDLs are written earlier in this case (inside
    binlog_query).

    TODO: This can be a problem in those cases that there are no
    handlertons registered. DDLs are one example, but the other case
    is MyISAM. In this case, we could register a dummy handlerton to
    trigger the commit.

    Any statement that requires logging will call binlog_query before
    trans_commit_stmt, so an alternative is to use the condition
    "binlog_query called or stmt.ha_list != 0".
   */
  if (!all && !trn_ctx->is_active(trx_scope) &&
      cache_mngr->stmt_cache.is_binlog_empty())
    DBUG_RETURN(RESULT_SUCCESS);

  if (thd->lex->sql_command == SQLCOM_XA_COMMIT)
  {
    /* The Commit phase of the XA two phase logging. */

    bool one_phase= get_xa_opt(thd) == XA_ONE_PHASE;
    DBUG_ASSERT(all);
    DBUG_ASSERT(!skip_commit || one_phase);

    int err= 0;
    XID_STATE *xs= thd->get_transaction()->xid_state();
    /*
      XA COMMIT ONE PHASE statement which has not gone through the binary log
      prepare phase, has to end the active XA transaction with appropriate XA
      END followed by XA COMMIT ONE PHASE.

      The state of XA transaction is changed to PREPARED after the prepare
      phase, intermediately in ha_commit_trans code for the interest of
      binlogger. Hence check that the XA COMMIT ONE PHASE is set to 'PREPARE'
      and it has not already been written to binary log. For such transaction
      write the appropriate XA END statement.
    */
    if (!(is_loggable_xa_prepare(thd))
        && one_phase
        && !(xs->is_binlogged())
        && !cache_mngr->trx_cache.is_binlog_empty())
    {
      XA_prepare_log_event end_evt(thd, xs->get_xid(), one_phase);
      err= cache_mngr->trx_cache.finalize(thd, &end_evt, xs);
      if (err)
      {
        DBUG_RETURN(RESULT_ABORTED);
      }
      trx_stuff_logged= true;
      thd->get_transaction()->xid_state()->set_binlogged();
    }
    if (DBUG_EVALUATE_IF("simulate_xa_commit_log_failure", true,
                         do_binlog_xa_commit_rollback(thd, xs->get_xid(),
                                                      true)))
      DBUG_RETURN(RESULT_ABORTED);
  }

  /*
    If there is anything in the stmt cache, and GTIDs are enabled,
    then this is a single statement outside a transaction and it is
    impossible that there is anything in the trx cache.  Hence, we
    write any empty group(s) to the stmt cache.

    Otherwise, we write any empty group(s) to the trx cache at the end
    of the transaction.
  */
  if (!cache_mngr->stmt_cache.is_binlog_empty())
  {
    /*
      Commit parent identification of non-transactional query has
      been deferred until now, except for the mixed transaction case.
    */
    trn_ctx->store_commit_parent(m_dependency_tracker.get_max_committed_timestamp());
    if (cache_mngr->stmt_cache.finalize(thd))
      DBUG_RETURN(RESULT_ABORTED);
    stmt_stuff_logged= true;
  }

  /*
    We commit the transaction if:
     - We are not in a transaction and committing a statement, or
     - We are in a transaction and a full transaction is committed.
    Otherwise, we accumulate the changes.
  */
  if (!cache_mngr->trx_cache.is_binlog_empty() &&
      ending_trans(thd, all) && !trx_stuff_logged)
  {
    const bool real_trans=
      (all || !trn_ctx->is_active(Transaction_ctx::SESSION));

    /*
      We are committing an XA transaction if it is a "real" transaction
      and has an XID assigned (because some handlerton registered). A
      transaction is "real" if either 'all' is true or the 'all.ha_list'
      is empty.

      Note: This is kind of strange since registering the binlog
      handlerton will then make the transaction XA, which is not really
      true. This occurs for example if a MyISAM statement is executed
      with row-based replication on.
    */
    if (is_loggable_xa_prepare(thd))
    {
      /* The prepare phase of XA transaction two phase logging. */
      int err= 0;
      bool one_phase= get_xa_opt(thd) == XA_ONE_PHASE;

      DBUG_ASSERT(thd->lex->sql_command != SQLCOM_XA_COMMIT || one_phase);

      XID_STATE *xs= thd->get_transaction()->xid_state();
      XA_prepare_log_event end_evt(thd, xs->get_xid(), one_phase);

      DBUG_ASSERT(skip_commit);

      err= cache_mngr->trx_cache.finalize(thd, &end_evt, xs);
      if (err ||
          (DBUG_EVALUATE_IF("simulate_xa_prepare_failure_in_cache_finalize",
                            true, false)))
      {
        DBUG_RETURN(RESULT_ABORTED);
      }
    }
    else if (real_trans && xid && trn_ctx->rw_ha_count(trx_scope) > 1 &&
             !trn_ctx->no_2pc(trx_scope))
    {
      Xid_log_event end_evt(thd, xid);
      if (cache_mngr->trx_cache.finalize(thd, &end_evt))
        DBUG_RETURN(RESULT_ABORTED);
    }
    else
    {
      Query_log_event end_evt(thd, STRING_WITH_LEN("COMMIT"),
                              true, FALSE, TRUE, 0, TRUE);
      if (cache_mngr->trx_cache.finalize(thd, &end_evt))
        DBUG_RETURN(RESULT_ABORTED);
    }
    trx_stuff_logged= true;
  }

  /*
    This is part of the stmt rollback.
  */
  if (!all)
    cache_mngr->trx_cache.set_prev_position(MY_OFF_T_UNDEF);

  /*
    Now all the events are written to the caches, so we will commit
    the transaction in the engines. This is done using the group
    commit logic in ordered_commit, which will return when the
    transaction is committed.

    If the commit in the engines fail, we still have something logged
    to the binary log so we have to report this as a "bad" failure
    (failed to commit, but logged something).
  */
  if (stmt_stuff_logged || trx_stuff_logged)
  {
    if (RUN_HOOK(transaction,
                 before_commit,
                 (thd, all,
                  thd_get_cache_mngr(thd)->get_binlog_cache_log(true),
                  thd_get_cache_mngr(thd)->get_binlog_cache_log(false),
                  max<my_off_t>(max_binlog_cache_size,
                                max_binlog_stmt_cache_size))) ||
        DBUG_EVALUATE_IF("simulate_failure_in_before_commit_hook", true, false))
    {
      ha_rollback_low(thd, all);
      gtid_state->update_on_rollback(thd);
      thd_get_cache_mngr(thd)->reset();
      //Reset the thread OK status before changing the outcome.
      if (thd->get_stmt_da()->is_ok())
        thd->get_stmt_da()->reset_diagnostics_area();
      my_error(ER_RUN_HOOK_ERROR, MYF(0), "before_commit");
      DBUG_RETURN(RESULT_ABORTED);
    }
    /*
      Check whether the transaction should commit or abort given the
      plugin feedback.
    */
    if (thd->get_transaction()->get_rpl_transaction_ctx()->is_transaction_rollback() ||
        (DBUG_EVALUATE_IF("simulate_transaction_rollback_request", true, false)))
    {
      ha_rollback_low(thd, all);
      gtid_state->update_on_rollback(thd);
      thd_get_cache_mngr(thd)->reset();
      if (thd->get_stmt_da()->is_ok())
        thd->get_stmt_da()->reset_diagnostics_area();
      my_error(ER_TRANSACTION_ROLLBACK_DURING_COMMIT, MYF(0));
      DBUG_RETURN(RESULT_ABORTED);
    }

    if (ordered_commit(thd, all, skip_commit))
      DBUG_RETURN(RESULT_INCONSISTENT);

    /*
      Mark the flag m_is_binlogged to true only after we are done
      with checking all the error cases.
    */
    if (is_loggable_xa_prepare(thd))
      thd->get_transaction()->xid_state()->set_binlogged();
  }
  else if (!skip_commit)
  {
    if (ha_commit_low(thd, all))
      DBUG_RETURN(RESULT_INCONSISTENT);
  }

  DBUG_RETURN(RESULT_SUCCESS);
}


/**
   Flush caches for session.

   @note @c set_trans_pos is called with a pointer to the file name
   that the binary log currently use and a rotation will change the
   contents of the variable.

   The position is used when calling the after_flush, after_commit,
   and after_rollback hooks, but these have been placed so that they
   occur before a rotation is executed.

   It is the responsibility of any plugin that use this position to
   copy it if they need it after the hook has returned.

   The current "global" transaction_counter is stepped and its new value
   is assigned to the transaction.
 */
std::pair<int,my_off_t>
MYSQL_BIN_LOG::flush_thread_caches(THD *thd)
{
  binlog_cache_mngr *cache_mngr= thd_get_cache_mngr(thd);
  my_off_t bytes= 0;
  bool wrote_xid= false;
  int error= cache_mngr->flush(thd, &bytes, &wrote_xid);
  if (!error && bytes > 0)
  {
    /*
      Note that set_trans_pos does not copy the file name. See
      this function documentation for more info.
    */
    thd->set_trans_pos(log_file_name, my_b_tell(&log_file));
    if (wrote_xid)
      inc_prep_xids(thd);
  }
  DBUG_PRINT("debug", ("bytes: %llu", bytes));
  return std::make_pair(error, bytes);
}


/**
  Execute the flush stage.

  @param total_bytes_var Pointer to variable that will be set to total
  number of bytes flushed, or NULL.

  @param rotate_var Pointer to variable that will be set to true if
  binlog rotation should be performed after releasing locks. If rotate
  is not necessary, the variable will not be touched.

  @return Error code on error, zero on success
 */

int
MYSQL_BIN_LOG::process_flush_stage_queue(my_off_t *total_bytes_var,
                                         bool *rotate_var,
                                         THD **out_queue_var)
{
  DBUG_ENTER("MYSQL_BIN_LOG::process_flush_stage_queue");
  #ifndef DBUG_OFF
  // number of flushes per group.
  int no_flushes= 0;
  #endif
  DBUG_ASSERT(total_bytes_var && rotate_var && out_queue_var);
  my_off_t total_bytes= 0;
  int flush_error= 1;
  mysql_mutex_assert_owner(&LOCK_log);

  /*
    Fetch the entire flush queue and empty it, so that the next batch
    has a leader. We must do this before invoking ha_flush_logs(...)
    for guaranteeing to flush prepared records of transactions before
    flushing them to binary log, which is required by crash recovery.
  */
  THD *first_seen= stage_manager.fetch_queue_for(Stage_manager::FLUSH_STAGE);
  DBUG_ASSERT(first_seen != NULL);
  /*
    We flush prepared records of transactions to the log of storage
    engine (for example, InnoDB redo log) in a group right before
    flushing them to binary log.
  */
  ha_flush_logs(NULL, true);
  DBUG_EXECUTE_IF("crash_after_flush_engine_log", DBUG_SUICIDE(););
  assign_automatic_gtids_to_flush_group(first_seen);
  /* Flush thread caches to binary log. */
  for (THD *head= first_seen ; head ; head = head->next_to_commit)
  {
    std::pair<int,my_off_t> result= flush_thread_caches(head);
    total_bytes+= result.second;
    if (flush_error == 1)
      flush_error= result.first;
#ifndef DBUG_OFF
    no_flushes++;
#endif
  }

  *out_queue_var= first_seen;
  *total_bytes_var= total_bytes;
  if (total_bytes > 0 && my_b_tell(&log_file) >= (my_off_t) max_size)
    *rotate_var= true;
#ifndef DBUG_OFF
  DBUG_PRINT("info",("no_flushes:= %d", no_flushes));
  no_flushes= 0;
#endif
  DBUG_RETURN(flush_error);
}

/**
  Commit a sequence of sessions.

  This function commit an entire queue of sessions starting with the
  session in @c first. If there were an error in the flushing part of
  the ordered commit, the error code is passed in and all the threads
  are marked accordingly (but not committed).

  It will also add the GTIDs of the transactions to gtid_executed.

  @see MYSQL_BIN_LOG::ordered_commit

  @param thd The "master" thread
  @param first First thread in the queue of threads to commit
 */

void
MYSQL_BIN_LOG::process_commit_stage_queue(THD *thd, THD *first)
{
  mysql_mutex_assert_owner(&LOCK_commit);
#ifndef DBUG_OFF
  thd->get_transaction()->m_flags.ready_preempt= 1; // formality by the leader
#endif
  for (THD *head= first ; head ; head = head->next_to_commit)
  {
    DBUG_PRINT("debug", ("Thread ID: %u, commit_error: %d, flags.pending: %s",
                         head->thread_id(), head->commit_error,
                         YESNO(head->get_transaction()->m_flags.pending)));
    /*
      If flushing failed, set commit_error for the session, skip the
      transaction and proceed with the next transaction instead. This
      will mark all threads as failed, since the flush failed.

      If flush succeeded, attach to the session and commit it in the
      engines.
    */
#ifndef DBUG_OFF
    stage_manager.clear_preempt_status(head);
#endif
    if (head->get_transaction()->sequence_number != SEQ_UNINIT)
      m_dependency_tracker.update_max_committed(head);
    /*
      Flush/Sync error should be ignored and continue
      to commit phase. And thd->commit_error cannot be
      COMMIT_ERROR at this moment.
    */
    DBUG_ASSERT(head->commit_error != THD::CE_COMMIT_ERROR);
#ifndef EMBEDDED_LIBRARY
    Thd_backup_and_restore switch_thd(thd, head);
#endif /* !EMBEDDED_LIBRARY */
    bool all= head->get_transaction()->m_flags.real_commit;
    if (head->get_transaction()->m_flags.commit_low)
    {
      /* head is parked to have exited append() */
      DBUG_ASSERT(head->get_transaction()->m_flags.ready_preempt);
      /*
        storage engine commit
       */
      if (ha_commit_low(head, all, false))
        head->commit_error= THD::CE_COMMIT_ERROR;
    }
    DBUG_PRINT("debug", ("commit_error: %d, flags.pending: %s",
                         head->commit_error,
                         YESNO(head->get_transaction()->m_flags.pending)));
  }

  /*
    Handle the GTID of the threads.
    gtid_executed table is kept updated even though transactions fail to be
    logged. That's required by slave auto positioning.
  */
  gtid_state->update_commit_group(first);

  for (THD *head= first ; head ; head = head->next_to_commit)
  {
    /*
      Decrement the prepared XID counter after storage engine commit.
      We also need decrement the prepared XID when encountering a
      flush error or session attach error for avoiding 3-way deadlock
      among user thread, rotate thread and dump thread.
    */
    if (head->get_transaction()->m_flags.xid_written)
      dec_prep_xids(head);
  }
}

/**
  Process after commit for a sequence of sessions.

  @param thd The "master" thread
  @param first First thread in the queue of threads to commit
 */

void
MYSQL_BIN_LOG::process_after_commit_stage_queue(THD *thd, THD *first)
{
  for (THD *head= first; head; head= head->next_to_commit)
  {
    if (head->get_transaction()->m_flags.run_hooks &&
        head->commit_error != THD::CE_COMMIT_ERROR)
    {

      /*
        TODO: This hook here should probably move outside/below this
              if and be the only after_commit invocation left in the
              code.
      */
#ifndef EMBEDDED_LIBRARY
      Thd_backup_and_restore switch_thd(thd, head);
#endif /* !EMBEDDED_LIBRARY */
      bool all= head->get_transaction()->m_flags.real_commit;
      (void) RUN_HOOK(transaction, after_commit, (head, all));
      /*
        When after_commit finished for the transaction, clear the run_hooks flag.
        This allow other parts of the system to check if after_commit was called.
      */
      head->get_transaction()->m_flags.run_hooks= false;
    }
  }
}

#ifndef DBUG_OFF
/** Names for the stages. */
static const char* g_stage_name[] = {
  "FLUSH",
  "SYNC",
  "COMMIT",
};
#endif


/**
  Enter a stage of the ordered commit procedure.

  Entering is stage is done by:

  - Atomically enqueueing a queue of processes (which is just one for
    the first phase).

  - If the queue was empty, the thread is the leader for that stage
    and it should process the entire queue for that stage.

  - If the queue was not empty, the thread is a follower and can go
    waiting for the commit to finish.

  The function will lock the stage mutex if it was designated the
  leader for the phase.

  @param thd    Session structure
  @param stage  The stage to enter
  @param queue  Queue of threads to enqueue for the stage
  @param stage_mutex Mutex for the stage

  @retval true  The thread should "bail out" and go waiting for the
                commit to finish
  @retval false The thread is the leader for the stage and should do
                the processing.
*/

bool
MYSQL_BIN_LOG::change_stage(THD *thd,
                            Stage_manager::StageID stage, THD *queue,
                            mysql_mutex_t *leave_mutex,
                            mysql_mutex_t *enter_mutex)
{
  DBUG_ENTER("MYSQL_BIN_LOG::change_stage");
  DBUG_PRINT("enter", ("thd: 0x%llx, stage: %s, queue: 0x%llx",
                       (ulonglong) thd, g_stage_name[stage], (ulonglong) queue));
  DBUG_ASSERT(0 <= stage && stage < Stage_manager::STAGE_COUNTER);
  DBUG_ASSERT(enter_mutex);
  DBUG_ASSERT(queue);
  /*
    enroll_for will release the leave_mutex once the sessions are
    queued.
  */
  if (!stage_manager.enroll_for(stage, queue, leave_mutex))
  {
    DBUG_ASSERT(!thd_get_cache_mngr(thd)->dbug_any_finalized());
    DBUG_RETURN(true);
  }

  /*
    We do not lock the enter_mutex if it is LOCK_log when rotating binlog
    caused by logging incident log event, since it is already locked.
  */
  bool need_lock_enter_mutex=
    !(is_rotating_caused_by_incident && enter_mutex == &LOCK_log);

  if (need_lock_enter_mutex)
    mysql_mutex_lock(enter_mutex);
  else
    mysql_mutex_assert_owner(enter_mutex);

  DBUG_RETURN(false);
}



/**
  Flush the I/O cache to file.

  Flush the binary log to the binlog file if any byte where written
  and signal that the binary log file has been updated if the flush
  succeeds.
*/

int
MYSQL_BIN_LOG::flush_cache_to_file(my_off_t *end_pos_var)
{
  if (flush_io_cache(&log_file))
  {
    THD *thd= current_thd;
    thd->commit_error= THD::CE_FLUSH_ERROR;
    return ER_ERROR_ON_WRITE;
  }
  *end_pos_var= my_b_tell(&log_file);
  return 0;
}


/**
  Call fsync() to sync the file to disk.
*/
std::pair<bool, bool>
MYSQL_BIN_LOG::sync_binlog_file(bool force)
{
  bool synced= false;
  unsigned int sync_period= get_sync_period();
  if (force || (sync_period && ++sync_counter >= sync_period))
  {
    sync_counter= 0;

    /**
      On *pure non-transactional* workloads there is a small window
      in time where a concurrent rotate might be able to close
      the file before the sync is actually done. In that case,
      ignore the bad file descriptor errors.

      Transactional workloads (InnoDB) are not affected since the
      the rotation will not happen until all transactions have
      committed to the storage engine, thence decreased the XID
      counters.

      TODO: fix this properly even for non-transactional storage
            engines.
     */
    if (DBUG_EVALUATE_IF("simulate_error_during_sync_binlog_file", 1,
                         mysql_file_sync(log_file.file,
                                         MYF(MY_WME | MY_IGNORE_BADFD))))
    {
      THD *thd= current_thd;
      thd->commit_error= THD::CE_SYNC_ERROR;
      return std::make_pair(true, synced);
    }
    synced= true;
  }
  return std::make_pair(false, synced);
}


/**
   Helper function executed when leaving @c ordered_commit.

   This function contain the necessary code for fetching the error
   code, doing post-commit checks, and wrapping up the commit if
   necessary.

   It is typically called when enter_stage indicates that the thread
   should bail out, and also when the ultimate leader thread finishes
   executing @c ordered_commit.

   It is typically used in this manner:
   @code
   if (enter_stage(thd, Thread_queue::FLUSH_STAGE, thd, &LOCK_log))
     return finish_commit(thd);
   @endcode

   @return Error code if the session commit failed, or zero on
   success.
 */
int
MYSQL_BIN_LOG::finish_commit(THD *thd)
{
  DBUG_ENTER("MYSQL_BIN_LOG::finish_commit");
  DEBUG_SYNC(thd, "reached_finish_commit");
  /*
    In some unlikely situations, it can happen that binary
    log is closed before the thread flushes it's cache.
    In that case, clear the caches before doing commit.
  */
  if (unlikely(!is_open()))
  {
    binlog_cache_mngr *cache_mngr= thd_get_cache_mngr(thd);
    if (cache_mngr)
      cache_mngr->reset();
  }
  if (thd->get_transaction()->sequence_number != SEQ_UNINIT)
    m_dependency_tracker.update_max_committed(thd);
  if (thd->get_transaction()->m_flags.commit_low)
  {
    const bool all= thd->get_transaction()->m_flags.real_commit;
    /*
      Now flush error and sync erros are ignored and we are continuing and
      committing. And at this time, commit_error cannot be COMMIT_ERROR.
    */
    DBUG_ASSERT(thd->commit_error != THD::CE_COMMIT_ERROR);
    /*
      storage engine commit
    */
    if (ha_commit_low(thd, all, false))
      thd->commit_error= THD::CE_COMMIT_ERROR;
    /*
      Decrement the prepared XID counter after storage engine commit
    */
    if (thd->get_transaction()->m_flags.xid_written)
      dec_prep_xids(thd);
    /*
      If commit succeeded, we call the after_commit hook

      TODO: This hook here should probably move outside/below this
            if and be the only after_commit invocation left in the
            code.
    */
    if ((thd->commit_error != THD::CE_COMMIT_ERROR) &&
        thd->get_transaction()->m_flags.run_hooks)
    {
      (void) RUN_HOOK(transaction, after_commit, (thd, all));
      thd->get_transaction()->m_flags.run_hooks= false;
    }
  }
  else if (thd->get_transaction()->m_flags.xid_written)
    dec_prep_xids(thd);

  /*
    If the ordered commit didn't updated the GTIDs for this thd yet
    at process_commit_stage_queue (i.e. --binlog-order-commits=0)
    the thd still has the ownership of a GTID and we must handle it.
  */
  if (!thd->owned_gtid.is_empty())
  {
    /*
      Gtid is added to gtid_state.executed_gtids and removed from owned_gtids
      on update_on_commit().
    */
    if (thd->commit_error == THD::CE_NONE)
    {
      gtid_state->update_on_commit(thd);
    }
    else
      gtid_state->update_on_rollback(thd);
  }

  DBUG_EXECUTE_IF("leaving_finish_commit",
                  {
                    const char act[]=
                      "now SIGNAL signal_leaving_finish_commit";
                    DBUG_ASSERT(!debug_sync_set_action(current_thd,
                                                       STRING_WITH_LEN(act)));
                  };);

  DBUG_ASSERT(thd->commit_error || !thd->get_transaction()->m_flags.run_hooks);
  DBUG_ASSERT(!thd_get_cache_mngr(thd)->dbug_any_finalized());
  DBUG_PRINT("return", ("Thread ID: %u, commit_error: %d",
                        thd->thread_id(), thd->commit_error));
  /*
    flush or sync errors are handled by the leader of the group
    (using binlog_error_action). Hence treat only COMMIT_ERRORs as errors.
  */
  DBUG_RETURN(thd->commit_error == THD::CE_COMMIT_ERROR);
}

/**
   Auxiliary function used in ordered_commit.
*/
static inline int call_after_sync_hook(THD *queue_head)
{
  const char *log_file= NULL;
  my_off_t pos= 0;

  if (NO_HOOK(binlog_storage))
    return 0;

  DBUG_ASSERT(queue_head != NULL);
  for (THD *thd= queue_head; thd != NULL; thd= thd->next_to_commit)
    if (likely(thd->commit_error == THD::CE_NONE))
      thd->get_trans_fixed_pos(&log_file, &pos);

  if (DBUG_EVALUATE_IF("simulate_after_sync_hook_error", 1, 0) ||
      RUN_HOOK(binlog_storage, after_sync, (queue_head, log_file, pos)))
  {
    sql_print_error("Failed to run 'after_sync' hooks");
    return ER_ERROR_ON_WRITE;
  }
  return 0;
}

/**
  Helper function to handle flush or sync stage errors.
  If binlog_error_action= ABORT_SERVER, server will be aborted
  after reporting the error to the client.
  If binlog_error_action= IGNORE_ERROR, binlog will be closed
  for the reset of the life time of the server. close() call is protected
  with LOCK_log to avoid any parallel operations on binary log.

  @param thd Thread object that faced flush/sync error
  @param need_lock_log
                       > Indicates true if LOCk_log is needed before closing
                         binlog (happens when we are handling sync error)
                       > Indicates false if LOCK_log is already acquired
                         by the thread (happens when we are handling flush
                         error)

  @return void
*/
void MYSQL_BIN_LOG::handle_binlog_flush_or_sync_error(THD *thd,
                                                      bool need_lock_log)
{
  char errmsg[MYSQL_ERRMSG_SIZE];
  sprintf(errmsg, "An error occurred during %s stage of the commit. "
          "'binlog_error_action' is set to '%s'.",
          thd->commit_error== THD::CE_FLUSH_ERROR ? "flush" : "sync",
          binlog_error_action == ABORT_SERVER ? "ABORT_SERVER" : "IGNORE_ERROR");
  if (binlog_error_action == ABORT_SERVER)
  {
    char err_buff[MYSQL_ERRMSG_SIZE + 27];
    sprintf(err_buff, "%s Hence aborting the server.", errmsg);
    exec_binlog_error_action_abort(err_buff);
  }
  else
  {
    DEBUG_SYNC(thd, "before_binlog_closed_due_to_error");
    if (need_lock_log)
      mysql_mutex_lock(&LOCK_log);
    else
      mysql_mutex_assert_owner(&LOCK_log);
    /*
      It can happen that other group leader encountered
      error and already closed the binary log. So print
      error only if it is in open state. But we should
      call close() always just in case if the previous
      close did not close index file.
    */
    if (is_open())
    {
      sql_print_error("%s Hence turning logging off for the whole duration "
                      "of the MySQL server process. To turn it on again: fix "
                      "the cause, shutdown the MySQL server and restart it.",
                      errmsg);
    }
    close(LOG_CLOSE_INDEX|LOG_CLOSE_STOP_EVENT, false/*need_lock_log=false*/,
          true/*need_lock_index=true*/);
    /*
      If there is a write error (flush/sync stage) and if
      binlog_error_action=IGNORE_ERROR, clear the error
      and allow the commit to happen in storage engine.
    */
    if (check_write_error(thd))
      thd->clear_error();

    if (need_lock_log)
      mysql_mutex_unlock(&LOCK_log);
    DEBUG_SYNC(thd, "after_binlog_closed_due_to_error");
  }
}
/**
  Flush and commit the transaction.

  This will execute an ordered flush and commit of all outstanding
  transactions and is the main function for the binary log group
  commit logic. The function performs the ordered commit in two
  phases.

  The first phase flushes the caches to the binary log and under
  LOCK_log and marks all threads that were flushed as not pending.

  The second phase executes under LOCK_commit and commits all
  transactions in order.

  The procedure is:

  1. Queue ourselves for flushing.
  2. Grab the log lock, which might result is blocking if the mutex is
     already held by another thread.
  3. If we were not committed while waiting for the lock
     1. Fetch the queue
     2. For each thread in the queue:
        a. Attach to it
        b. Flush the caches, saving any error code
     3. Flush and sync (depending on the value of sync_binlog).
     4. Signal that the binary log was updated
  4. Release the log lock
  5. Grab the commit lock
     1. For each thread in the queue:
        a. If there were no error when flushing and the transaction shall be committed:
           - Commit the transaction, saving the result of executing the commit.
  6. Release the commit lock
  7. Call purge, if any of the committed thread requested a purge.
  8. Return with the saved error code

  @todo The use of @c skip_commit is a hack that we use since the @c
  TC_LOG Interface does not contain functions to handle
  savepoints. Once the binary log is eliminated as a handlerton and
  the @c TC_LOG interface is extended with savepoint handling, this
  parameter can be removed.

  @param thd Session to commit transaction for
  @param all   This is @c true if this is a real transaction commit, and
               @c false otherwise.
  @param skip_commit
               This is @c true if the call to @c ha_commit_low should
               be skipped (it is handled by the caller somehow) and @c
               false otherwise (the normal case).
 */
int MYSQL_BIN_LOG::ordered_commit(THD *thd, bool all, bool skip_commit)
{
  DBUG_ENTER("MYSQL_BIN_LOG::ordered_commit");
  int flush_error= 0, sync_error= 0;
  my_off_t total_bytes= 0;
  bool do_rotate= false;

  /*
    These values are used while flushing a transaction, so clear
    everything.

    Notes:

    - It would be good if we could keep transaction coordinator
      log-specific data out of the THD structure, but that is not the
      case right now.

    - Everything in the transaction structure is reset when calling
      ha_commit_low since that calls Transaction_ctx::cleanup.
  */
  thd->get_transaction()->m_flags.pending= true;
  thd->commit_error= THD::CE_NONE;
  thd->next_to_commit= NULL;
  thd->durability_property= HA_IGNORE_DURABILITY;
  thd->get_transaction()->m_flags.real_commit= all;
  thd->get_transaction()->m_flags.xid_written= false;
  thd->get_transaction()->m_flags.commit_low= !skip_commit;
  thd->get_transaction()->m_flags.run_hooks= !skip_commit;
#ifndef DBUG_OFF
  /*
     The group commit Leader may have to wait for follower whose transaction
     is not ready to be preempted. Initially the status is pessimistic.
     Preemption guarding logics is necessary only when !DBUG_OFF is set.
     It won't be required for the dbug-off case as long as the follower won't
     execute any thread-specific write access code in this method, which is
     the case as of current.
  */
  thd->get_transaction()->m_flags.ready_preempt= 0;
#endif

  DBUG_PRINT("enter", ("flags.pending: %s, commit_error: %d, thread_id: %u",
                       YESNO(thd->get_transaction()->m_flags.pending),
                       thd->commit_error, thd->thread_id()));

  DEBUG_SYNC(thd, "bgc_before_flush_stage");

  /*
    Stage #1: flushing transactions to binary log

    While flushing, we allow new threads to enter and will process
    them in due time. Once the queue was empty, we cannot reap
    anything more since it is possible that a thread entered and
    appointed itself leader for the flush phase.
  */

#ifdef HAVE_REPLICATION
  if (has_commit_order_manager(thd))
  {
    Slave_worker *worker= dynamic_cast<Slave_worker *>(thd->rli_slave);
    Commit_order_manager *mngr= worker->get_commit_order_manager();

    if (mngr->wait_for_its_turn(worker, all))
    {
      thd->commit_error= THD::CE_COMMIT_ERROR;
      DBUG_RETURN(thd->commit_error);
    }

    if (change_stage(thd, Stage_manager::FLUSH_STAGE, thd, NULL, &LOCK_log))
      DBUG_RETURN(finish_commit(thd));
  }
  else
#endif
  if (change_stage(thd, Stage_manager::FLUSH_STAGE, thd, NULL, &LOCK_log))
  {
    DBUG_PRINT("return", ("Thread ID: %u, commit_error: %d",
                          thd->thread_id(), thd->commit_error));
    DBUG_RETURN(finish_commit(thd));
  }

  THD *wait_queue= NULL, *final_queue= NULL;
  mysql_mutex_t *leave_mutex_before_commit_stage= NULL;
  my_off_t flush_end_pos= 0;
  bool update_binlog_end_pos_after_sync;
  if (unlikely(!is_open()))
  {
    final_queue= stage_manager.fetch_queue_for(Stage_manager::FLUSH_STAGE);
    leave_mutex_before_commit_stage= &LOCK_log;
    /*
      binary log is closed, flush stage and sync stage should be
      ignored. Binlog cache should be cleared, but instead of doing
      it here, do that work in 'finish_commit' function so that
      leader and followers thread caches will be cleared.
    */
    goto commit_stage;
  }
  DEBUG_SYNC(thd, "waiting_in_the_middle_of_flush_stage");
  flush_error= process_flush_stage_queue(&total_bytes, &do_rotate,
                                                 &wait_queue);

  if (flush_error == 0 && total_bytes > 0)
    flush_error= flush_cache_to_file(&flush_end_pos);
  DBUG_EXECUTE_IF("crash_after_flush_binlog", DBUG_SUICIDE(););

  update_binlog_end_pos_after_sync= (get_sync_period() == 1);

  /*
    If the flush finished successfully, we can call the after_flush
    hook. Being invoked here, we have the guarantee that the hook is
    executed before the before/after_send_hooks on the dump thread
    preventing race conditions among these plug-ins.
  */
  if (flush_error == 0)
  {
    const char *file_name_ptr= log_file_name + dirname_length(log_file_name);
    DBUG_ASSERT(flush_end_pos != 0);
    if (RUN_HOOK(binlog_storage, after_flush,
                 (thd, file_name_ptr, flush_end_pos)))
    {
      sql_print_error("Failed to run 'after_flush' hooks");
      flush_error= ER_ERROR_ON_WRITE;
    }

    if (!update_binlog_end_pos_after_sync)
      update_binlog_end_pos();
    DBUG_EXECUTE_IF("crash_commit_after_log", DBUG_SUICIDE(););
  }

  if (flush_error)
  {
    /*
      Handle flush error (if any) after leader finishes it's flush stage.
    */
    handle_binlog_flush_or_sync_error(thd, false /* need_lock_log */);
  }

  DEBUG_SYNC(thd, "bgc_after_flush_stage_before_sync_stage");

  /*
    Stage #2: Syncing binary log file to disk
  */

  if (change_stage(thd, Stage_manager::SYNC_STAGE, wait_queue, &LOCK_log, &LOCK_sync))
  {
    DBUG_PRINT("return", ("Thread ID: %u, commit_error: %d",
                          thd->thread_id(), thd->commit_error));
    DBUG_RETURN(finish_commit(thd));
  }

  /*
    Shall introduce a delay only if it is going to do sync
    in this ongoing SYNC stage. The "+1" used below in the
    if condition is to count the ongoing sync stage.
    When sync_binlog=0 (where we never do sync in BGC group),
    it is considered as a special case and delay will be executed
    for every group just like how it is done when sync_binlog= 1.
  */
  if (!flush_error && (sync_counter + 1 >= get_sync_period()))
    stage_manager.wait_count_or_timeout(opt_binlog_group_commit_sync_no_delay_count,
                                        opt_binlog_group_commit_sync_delay,
                                        Stage_manager::SYNC_STAGE);

  final_queue= stage_manager.fetch_queue_for(Stage_manager::SYNC_STAGE);

  if (flush_error == 0 && total_bytes > 0)
  {
    DEBUG_SYNC(thd, "before_sync_binlog_file");
    std::pair<bool, bool> result= sync_binlog_file(false);
    sync_error= result.first;
  }

  if (update_binlog_end_pos_after_sync)
  {
    THD *tmp_thd= final_queue;

    while (tmp_thd->next_to_commit != NULL)
      tmp_thd= tmp_thd->next_to_commit;
    if (flush_error == 0 && sync_error == 0)
      update_binlog_end_pos(tmp_thd->get_trans_pos());
  }

  DEBUG_SYNC(thd, "bgc_after_sync_stage_before_commit_stage");

  leave_mutex_before_commit_stage= &LOCK_sync;
  /*
    Stage #3: Commit all transactions in order.

    This stage is skipped if we do not need to order the commits and
    each thread have to execute the handlerton commit instead.

    Howver, since we are keeping the lock from the previous stage, we
    need to unlock it if we skip the stage.

    We must also step commit_clock before the ha_commit_low() is called
    either in ordered fashion(by the leader of this stage) or by the tread
    themselves.

    We are delaying the handling of sync error until
    all locks are released but we should not enter into
    commit stage if binlog_error_action is ABORT_SERVER.
  */
commit_stage:
  if (opt_binlog_order_commits &&
      (sync_error == 0 || binlog_error_action != ABORT_SERVER))
  {
    if (change_stage(thd, Stage_manager::COMMIT_STAGE,
                     final_queue, leave_mutex_before_commit_stage,
                     &LOCK_commit))
    {
      DBUG_PRINT("return", ("Thread ID: %u, commit_error: %d",
                            thd->thread_id(), thd->commit_error));
      DBUG_RETURN(finish_commit(thd));
    }
    THD *commit_queue= stage_manager.fetch_queue_for(Stage_manager::COMMIT_STAGE);
    DBUG_EXECUTE_IF("semi_sync_3-way_deadlock",
                    DEBUG_SYNC(thd, "before_process_commit_stage_queue"););

    if (flush_error == 0 && sync_error == 0)
      sync_error= call_after_sync_hook(commit_queue);

    /*
      process_commit_stage_queue will call update_on_commit or
      update_on_rollback for the GTID owned by each thd in the queue.

      This will be done this way to guarantee that GTIDs are added to
      gtid_executed in order, to avoid creating unnecessary temporary
      gaps and keep gtid_executed as a single interval at all times.

      If we allow each thread to call update_on_commit only when they
      are at finish_commit, the GTID order cannot be guaranteed and
      temporary gaps may appear in gtid_executed. When this happen,
      the server would have to add and remove intervals from the
      Gtid_set, and adding and removing intervals requires a mutex,
      which would reduce performance.
    */
    process_commit_stage_queue(thd, commit_queue);
    mysql_mutex_unlock(&LOCK_commit);
    /*
      Process after_commit after LOCK_commit is released for avoiding
      3-way deadlock among user thread, rotate thread and dump thread.
    */
    process_after_commit_stage_queue(thd, commit_queue);
    final_queue= commit_queue;
  }
  else
  {
    if (leave_mutex_before_commit_stage)
      mysql_mutex_unlock(leave_mutex_before_commit_stage);
    if (flush_error == 0 && sync_error == 0)
      sync_error= call_after_sync_hook(final_queue);
  }

  /*
    Handle sync error after we release all locks in order to avoid deadlocks
  */
  if (sync_error)
    handle_binlog_flush_or_sync_error(thd, true /* need_lock_log */);

  /* Commit done so signal all waiting threads */
  stage_manager.signal_done(final_queue);

  /*
    Finish the commit before executing a rotate, or run the risk of a
    deadlock. We don't need the return value here since it is in
    thd->commit_error, which is returned below.
  */
  (void) finish_commit(thd);

  /*
    If we need to rotate, we do it without commit error.
    Otherwise the thd->commit_error will be possibly reset.
   */
  if (DBUG_EVALUATE_IF("force_rotate", 1, 0) ||
      (do_rotate && thd->commit_error == THD::CE_NONE &&
       !is_rotating_caused_by_incident))
  {
    /*
      Do not force the rotate as several consecutive groups may
      request unnecessary rotations.

      NOTE: Run purge_logs wo/ holding LOCK_log because it does not
      need the mutex. Otherwise causes various deadlocks.
    */

    DEBUG_SYNC(thd, "ready_to_do_rotation");
    bool check_purge= false;
    mysql_mutex_lock(&LOCK_log);
    /*
      If rotate fails then depends on binlog_error_action variable
      appropriate action will be taken inside rotate call.
    */
    int error= rotate(false, &check_purge);
    mysql_mutex_unlock(&LOCK_log);

    if (error)
      thd->commit_error= THD::CE_COMMIT_ERROR;
    else if (check_purge)
      purge();
  }
  /*
    flush or sync errors are handled above (using binlog_error_action).
    Hence treat only COMMIT_ERRORs as errors.
  */
  DBUG_RETURN(thd->commit_error == THD::CE_COMMIT_ERROR);
}


/**
  MYSQLD server recovers from last crashed binlog.

  @param log           IO_CACHE of the crashed binlog.
  @param fdle          Format_description_log_event of the crashed binlog.
  @param valid_pos     The position of the last valid transaction or
                       event(non-transaction) of the crashed binlog.

  @retval
    0                  ok
  @retval
    1                  error
*/
int MYSQL_BIN_LOG::recover(IO_CACHE *log, Format_description_log_event *fdle,
                            my_off_t *valid_pos)
{
  Log_event  *ev;
  HASH xids;
  MEM_ROOT mem_root;
  /*
    The flag is used for handling the case that a transaction
    is partially written to the binlog.
  */
  bool in_transaction= FALSE;
  int memory_page_size= my_getpagesize();

  if (! fdle->is_valid() ||
      my_hash_init(&xids, &my_charset_bin, memory_page_size/3, 0,
                   sizeof(my_xid), 0, 0, 0,
                   key_memory_binlog_recover_exec))
    goto err1;

  init_alloc_root(key_memory_binlog_recover_exec,
                  &mem_root, memory_page_size, memory_page_size);

  while ((ev= Log_event::read_log_event(log, 0, fdle, TRUE))
         && ev->is_valid())
  {
    if (ev->get_type_code() == binary_log::QUERY_EVENT &&
        !strcmp(((Query_log_event*)ev)->query, "BEGIN"))
      in_transaction= TRUE;

    if (ev->get_type_code() == binary_log::QUERY_EVENT &&
        !strcmp(((Query_log_event*)ev)->query, "COMMIT"))
    {
      DBUG_ASSERT(in_transaction == TRUE);
      in_transaction= FALSE;
    }
    else if (ev->get_type_code() == binary_log::XID_EVENT)
    {
      DBUG_ASSERT(in_transaction == TRUE);
      in_transaction= FALSE;
      Xid_log_event *xev=(Xid_log_event *)ev;
      uchar *x= (uchar *) memdup_root(&mem_root, (uchar*) &xev->xid,
                                      sizeof(xev->xid));
      if (!x || my_hash_insert(&xids, x))
        goto err2;
    }

    /*
      Recorded valid position for the crashed binlog file
      which did not contain incorrect events. The following
      positions increase the variable valid_pos:

      1 -
        ...
        <---> HERE IS VALID <--->
        GTID 
        BEGIN
        ...
        COMMIT
        ...
         
      2 -
        ...
        <---> HERE IS VALID <--->
        GTID 
        DDL/UTILITY
        ...

      In other words, the following positions do not increase
      the variable valid_pos:

      1 -
        GTID 
        <---> HERE IS VALID <--->
        ...

      2 -
        GTID 
        BEGIN
        <---> HERE IS VALID <--->
        ...
    */
    if (!log->error && !in_transaction &&
        !is_gtid_event(ev))
      *valid_pos= my_b_tell(log);

    delete ev;
  }

  /*
    Call ha_recover if and only if there is a registered engine that
    does 2PC, otherwise in DBUG builds calling ha_recover directly
    will result in an assert. (Production builds would be safe since
    ha_recover returns right away if total_ha_2pc <= opt_log_bin.)
   */
  if (total_ha_2pc > 1 && ha_recover(&xids))
    goto err2;

  free_root(&mem_root, MYF(0));
  my_hash_free(&xids);
  return 0;

err2:
  free_root(&mem_root, MYF(0));
  my_hash_free(&xids);
err1:
  sql_print_error("Crash recovery failed. Either correct the problem "
                  "(if it's, for example, out of memory error) and restart, "
                  "or delete (or rename) binary log and start mysqld with "
                  "--tc-heuristic-recover={commit|rollback}");
  return 1;
}

bool THD::is_binlog_cache_empty(bool is_transactional)
{
  DBUG_ENTER("THD::is_binlog_cache_empty(bool)");

  // If opt_bin_log==0, it is not safe to call thd_get_cache_mngr
  // because binlog_hton has not been completely set up.
  DBUG_ASSERT(opt_bin_log);
  binlog_cache_mngr *cache_mngr= thd_get_cache_mngr(this);

  // cache_mngr is NULL until we call thd->binlog_setup_trx_data, so
  // we assert that this has been done.
  DBUG_ASSERT(cache_mngr != NULL);

  binlog_cache_data *cache_data=
    cache_mngr->get_binlog_cache_data(is_transactional);
  DBUG_ASSERT(cache_data != NULL);

  DBUG_RETURN(cache_data->is_binlog_empty());
}

/*
  These functions are placed in this file since they need access to
  binlog_hton, which has internal linkage.
*/

int THD::binlog_setup_trx_data()
{
  DBUG_ENTER("THD::binlog_setup_trx_data");
  binlog_cache_mngr *cache_mngr= thd_get_cache_mngr(this);

  if (cache_mngr)
    DBUG_RETURN(0);                             // Already set up

  IO_CACHE stmt_cache_log, trx_cache_log;
  memset(&stmt_cache_log, 0, sizeof(stmt_cache_log));
  memset(&trx_cache_log, 0, sizeof(trx_cache_log));

  cache_mngr= (binlog_cache_mngr*) my_malloc(key_memory_binlog_cache_mngr,
                                             sizeof(binlog_cache_mngr), MYF(MY_ZEROFILL));
  if (!cache_mngr)
  {
    DBUG_RETURN(1);
  }
  if (open_cached_file(&stmt_cache_log, mysql_tmpdir,
                       LOG_PREFIX, binlog_stmt_cache_size, MYF(MY_WME)))
  {
    my_free(cache_mngr);
    DBUG_RETURN(1);                      // Didn't manage to set it up
  }
  if (open_cached_file(&trx_cache_log, mysql_tmpdir,
                       LOG_PREFIX, binlog_cache_size, MYF(MY_WME)))
  {
    close_cached_file(&stmt_cache_log);
    my_free(cache_mngr);
    DBUG_RETURN(1);
  }
  DBUG_PRINT("debug", ("Set ha_data slot %d to 0x%llx", binlog_hton->slot, (ulonglong) cache_mngr));
  thd_set_ha_data(this, binlog_hton, cache_mngr);

  cache_mngr= new (thd_get_cache_mngr(this))
              binlog_cache_mngr(max_binlog_stmt_cache_size,
                                &binlog_stmt_cache_use,
                                &binlog_stmt_cache_disk_use,
                                max_binlog_cache_size,
                                &binlog_cache_use,
                                &binlog_cache_disk_use,
                                stmt_cache_log,
                                trx_cache_log);
  DBUG_RETURN(0);
}

/**

*/
void register_binlog_handler(THD *thd, bool trx)
{
  DBUG_ENTER("register_binlog_handler");
  /*
    If this is the first call to this function while processing a statement,
    the transactional cache does not have a savepoint defined. So, in what
    follows:
      . an implicit savepoint is defined;
      . callbacks are registered;
      . binary log is set as read/write.

    The savepoint allows for truncating the trx-cache transactional changes
    fail. Callbacks are necessary to flush caches upon committing or rolling
    back a statement or a transaction. However, notifications do not happen
    if the binary log is set as read/write.
  */
  binlog_cache_mngr *cache_mngr= thd_get_cache_mngr(thd);
  if (cache_mngr->trx_cache.get_prev_position() == MY_OFF_T_UNDEF)
  {
    /*
      Set an implicit savepoint in order to be able to truncate a trx-cache.
    */
    my_off_t pos= 0;
    binlog_trans_log_savepos(thd, &pos);
    cache_mngr->trx_cache.set_prev_position(pos);

    /*
      Set callbacks in order to be able to call commmit or rollback.
    */
    if (trx)
      trans_register_ha(thd, TRUE, binlog_hton, NULL);
    trans_register_ha(thd, FALSE, binlog_hton, NULL);

    /*
      Set the binary log as read/write otherwise callbacks are not called.
    */
    thd->ha_data[binlog_hton->slot].ha_info[0].set_trx_read_write();
  }
  DBUG_VOID_RETURN;
}

/**
  Function to start a statement and optionally a transaction for the
  binary log.

  This function does three things:
    - Starts a transaction if not in autocommit mode or if a BEGIN
      statement has been seen.

    - Start a statement transaction to allow us to truncate the cache.

    - Save the currrent binlog position so that we can roll back the
      statement by truncating the cache.

      We only update the saved position if the old one was undefined,
      the reason is that there are some cases (e.g., for CREATE-SELECT)
      where the position is saved twice (e.g., both in
      Query_result_create::prepare() and THD::binlog_write_table_map()), but
      we should use the first. This means that calls to this function
      can be used to start the statement before the first table map
      event, to include some extra events.

  Note however that IMMEDIATE_LOGGING implies that the statement is
  written without BEGIN/COMMIT.

  @param thd         Thread variable
  @param start_event The first event requested to be written into the
                     binary log
 */
static int binlog_start_trans_and_stmt(THD *thd, Log_event *start_event)
{
  DBUG_ENTER("binlog_start_trans_and_stmt");
 
  /*
    Initialize the cache manager if this was not done yet.
  */ 
  if (thd->binlog_setup_trx_data())
    DBUG_RETURN(1);

  /*
    Retrieve the appropriated cache.
  */
  bool is_transactional= start_event->is_using_trans_cache();
  binlog_cache_mngr *cache_mngr= thd_get_cache_mngr(thd);
  binlog_cache_data *cache_data= cache_mngr->get_binlog_cache_data(is_transactional);
 
  /*
    If the event is requesting immediatly logging, there is no need to go
    further down and set savepoint and register callbacks.
  */ 
  if (start_event->is_using_immediate_logging())
    DBUG_RETURN(0);

  register_binlog_handler(thd, thd->in_multi_stmt_transaction_mode());

  /*
    If the cache is empty log "BEGIN" at the beginning of every transaction.
    Here, a transaction is either a BEGIN..COMMIT/ROLLBACK block or a single
    statement in autocommit mode.
  */
  if (cache_data->is_binlog_empty())
  {
    static const char begin[]= "BEGIN";
    const char *query= NULL;
    char buf[XID::ser_buf_size];
    char xa_start[sizeof("XA START") + 1 + sizeof(buf)];
    XID_STATE *xs= thd->get_transaction()->xid_state();
    int qlen= sizeof(begin) - 1;

    if (is_transactional && xs->has_state(XID_STATE::XA_ACTIVE))
    {
      /*
        XA-prepare logging case.
      */
      qlen= sprintf(xa_start, "XA START %s", xs->get_xid()->serialize(buf));
      query= xa_start;
    }
    else
    {
      /*
        Regular transaction case.
      */
      query= begin;
    }

    Query_log_event qinfo(thd, query, qlen,
                          is_transactional, false, true, 0, true);
    if (cache_data->write_event(thd, &qinfo))
      DBUG_RETURN(1);
  }

  DBUG_RETURN(0);
}

/**
  This function writes a table map to the binary log. 
  Note that in order to keep the signature uniform with related methods,
  we use a redundant parameter to indicate whether a transactional table
  was changed or not.
  Sometimes it will write a Rows_query_log_event into binary log before
  the table map too.
 
  @param table             a pointer to the table.
  @param is_transactional  @c true indicates a transactional table,
                           otherwise @c false a non-transactional.
  @param binlog_rows_query @c true indicates a Rows_query log event
                           will be binlogged before table map,
                           otherwise @c false indicates it will not
                           be binlogged.
  @return
    nonzero if an error pops up when writing the table map event
    or the Rows_query log event.
*/
int THD::binlog_write_table_map(TABLE *table, bool is_transactional,
                                bool binlog_rows_query)
{
  int error;
  DBUG_ENTER("THD::binlog_write_table_map");
  DBUG_PRINT("enter", ("table: 0x%lx  (%s: #%llu)",
                       (long) table, table->s->table_name.str,
                       table->s->table_map_id.id()));

  /* Pre-conditions */
  DBUG_ASSERT(is_current_stmt_binlog_format_row() && mysql_bin_log.is_open());
  DBUG_ASSERT(table->s->table_map_id.is_valid());

  Table_map_log_event
    the_event(this, table, table->s->table_map_id, is_transactional);

  binlog_start_trans_and_stmt(this, &the_event);

  binlog_cache_mngr *const cache_mngr= thd_get_cache_mngr(this);

  binlog_cache_data *cache_data=
    cache_mngr->get_binlog_cache_data(is_transactional);

  if (binlog_rows_query && this->query().str)
  {
    /* Write the Rows_query_log_event into binlog before the table map */
    Rows_query_log_event
      rows_query_ev(this, this->query().str, this->query().length);
    if ((error= cache_data->write_event(this, &rows_query_ev)))
      DBUG_RETURN(error);
  }

  if ((error= cache_data->write_event(this, &the_event)))
    DBUG_RETURN(error);

  binlog_table_maps++;
  DBUG_RETURN(0);
}

/**
  This function retrieves a pending row event from a cache which is
  specified through the parameter @c is_transactional. Respectively, when it
  is @c true, the pending event is returned from the transactional cache.
  Otherwise from the non-transactional cache.

  @param is_transactional  @c true indicates a transactional cache,
                           otherwise @c false a non-transactional.
  @return
    The row event if any. 
*/
Rows_log_event*
THD::binlog_get_pending_rows_event(bool is_transactional) const
{
  Rows_log_event* rows= NULL;
  binlog_cache_mngr *const cache_mngr= thd_get_cache_mngr(this);

  /*
    This is less than ideal, but here's the story: If there is no cache_mngr,
    prepare_pending_rows_event() has never been called (since the cache_mngr
    is set up there). In that case, we just return NULL.
   */
  if (cache_mngr)
  {
    binlog_cache_data *cache_data=
      cache_mngr->get_binlog_cache_data(is_transactional);

    rows= cache_data->pending();
  }
  return (rows);
}

/**
   @param db    db name c-string to be inserted into alphabetically sorted
                THD::binlog_accessed_db_names list.
                
                Note, that space for both the data and the node
                struct are allocated in THD::main_mem_root.
                The list lasts for the top-level query time and is reset
                in @c THD::cleanup_after_query().
*/
void
THD::add_to_binlog_accessed_dbs(const char *db_param)
{
  char *after_db;
  /*
    binlog_accessed_db_names list is to maintain the database
    names which are referenced in a given command.
    Prior to bug 17806014 fix, 'main_mem_root' memory root used
    to store this list. The 'main_mem_root' scope is till the end
    of the query. Hence it caused increasing memory consumption
    problem in big procedures like the ones mentioned below.
    Eg: CALL p1() where p1 is having 1,00,000 create and drop tables.
    'main_mem_root' is freed only at the end of the command CALL p1()'s
    execution. But binlog_accessed_db_names list scope is only till the
    individual statements specified the procedure(create/drop statements).
    Hence the memory allocated in 'main_mem_root' was left uncleared
    until the p1's completion, even though it is not required after
    completion of individual statements.

    Instead of using 'main_mem_root' whose scope is complete query execution,
    now the memroot is changed to use 'thd->mem_root' whose scope is until the
    individual statement in CALL p1(). 'thd->mem_root' is set to 'execute_mem_root'
    in the context of procedure and it's scope is till the individual statement
    in CALL p1() and thd->memroot is equal to 'main_mem_root' in the context
    of a normal 'top level query'.

    Eg: a) create table t1(i int); => If this function is called while
           processing this statement, thd->memroot is equal to &main_mem_root
           which will be freed immediately after executing this statement.
        b) CALL p1() -> p1 contains create table t1(i int); => If this function
           is called while processing create table statement which is inside
           a stored procedure, then thd->memroot is equal to 'execute_mem_root'
           which will be freed immediately after executing this statement.
    In both a and b case, thd->memroot will be freed immediately and will not
    increase memory consumption.

    A special case(stored functions/triggers):
    Consider the following example:
    create function f1(i int) returns int
    begin
      insert into db1.t1 values (1);
      insert into db2.t1 values (2);
    end;
    When we are processing SELECT f1(), the list should contain db1, db2 names.
    Since thd->mem_root contains 'execute_mem_root' in the context of
    stored function, the mem root will be freed after adding db1 in
    the list and when we are processing the second statement and when we try
    to add 'db2' in the db1's list, it will lead to crash as db1's memory
    is already freed. To handle this special case, if in_sub_stmt is set
    (which is true incase of stored functions/triggers), we use &main_mem_root,
    if not set we will use thd->memroot which changes it's value to
    'execute_mem_root' or '&main_mem_root' depends on the context.
   */
  MEM_ROOT *db_mem_root= in_sub_stmt ? &main_mem_root : mem_root;

  if (!binlog_accessed_db_names)
    binlog_accessed_db_names= new (db_mem_root) List<char>;

  if (binlog_accessed_db_names->elements >  MAX_DBS_IN_EVENT_MTS)
  {
    push_warning_printf(this, Sql_condition::SL_WARNING,
                        ER_MTS_UPDATED_DBS_GREATER_MAX,
                        ER(ER_MTS_UPDATED_DBS_GREATER_MAX),
                        MAX_DBS_IN_EVENT_MTS);
    return;
  }

  after_db= strdup_root(db_mem_root, db_param);

  /* 
     sorted insertion is implemented with first rearranging data
     (pointer to char*) of the links and final appending of the least
     ordered data to create a new link in the list.
  */
  if (binlog_accessed_db_names->elements != 0)
  {
    List_iterator<char> it(*get_binlog_accessed_db_names());

    while (it++)
    {
      char *swap= NULL;
      char **ref_cur_db= it.ref();
      int cmp= strcmp(after_db, *ref_cur_db);

      DBUG_ASSERT(!swap || cmp < 0);
      
      if (cmp == 0)
      {
        after_db= NULL;  /* dup to ignore */
        break;
      }
      else if (swap || cmp > 0)
      {
        swap= *ref_cur_db;
        *ref_cur_db= after_db;
        after_db= swap;
      }
    }
  }
  if (after_db)
    binlog_accessed_db_names->push_back(after_db, db_mem_root);
}

/*
  Tells if two (or more) tables have auto_increment columns and we want to
  lock those tables with a write lock.

  SYNOPSIS
    has_two_write_locked_tables_with_auto_increment
      tables        Table list

  NOTES:
    Call this function only when you have established the list of all tables
    which you'll want to update (including stored functions, triggers, views
    inside your statement).
*/

static bool
has_write_table_with_auto_increment(TABLE_LIST *tables)
{
  for (TABLE_LIST *table= tables; table; table= table->next_global)
  {
    /* we must do preliminary checks as table->table may be NULL */
    if (!table->is_placeholder() &&
        table->table->found_next_number_field &&
        (table->lock_type >= TL_WRITE_ALLOW_WRITE))
      return 1;
  }

  return 0;
}

/*
   checks if we have select tables in the table list and write tables
   with auto-increment column.

  SYNOPSIS
   has_two_write_locked_tables_with_auto_increment_and_select
      tables        Table list

  RETURN VALUES

   -true if the table list has atleast one table with auto-increment column


         and atleast one table to select from.
   -false otherwise
*/

static bool
has_write_table_with_auto_increment_and_select(TABLE_LIST *tables)
{
  bool has_select= false;
  bool has_auto_increment_tables = has_write_table_with_auto_increment(tables);
  for(TABLE_LIST *table= tables; table; table= table->next_global)
  {
     if (!table->is_placeholder() &&
        (table->lock_type <= TL_READ_NO_INSERT))
      {
        has_select= true;
        break;
      }
  }
  return(has_select && has_auto_increment_tables);
}

/*
  Tells if there is a table whose auto_increment column is a part
  of a compound primary key while is not the first column in
  the table definition.

  @param tables Table list

  @return true if the table exists, fais if does not.
*/

static bool
has_write_table_auto_increment_not_first_in_pk(TABLE_LIST *tables)
{
  for (TABLE_LIST *table= tables; table; table= table->next_global)
  {
    /* we must do preliminary checks as table->table may be NULL */
    if (!table->is_placeholder() &&
        table->table->found_next_number_field &&
        (table->lock_type >= TL_WRITE_ALLOW_WRITE)
        && table->table->s->next_number_keypart != 0)
      return 1;
  }

  return 0;
}

/*
  Function to check whether the table in query uses a fulltext parser
  plugin or not.

  @param s - table share pointer.

  @retval TRUE - The table uses fulltext parser plugin.
  @retval FALSE - Otherwise.
*/
static bool inline fulltext_unsafe_set(TABLE_SHARE *s)
{
  for (unsigned int i= 0 ; i < s->keys ; i++)
  {
    if ((s->key_info[i].flags & HA_USES_PARSER) && s->keys_in_use.is_set(i))
      return TRUE;
  }
  return FALSE;
}
#ifndef DBUG_OFF
const char * get_locked_tables_mode_name(enum_locked_tables_mode locked_tables_mode)
{
   switch (locked_tables_mode)
   {
   case LTM_NONE:
     return "LTM_NONE";
   case LTM_LOCK_TABLES:
     return "LTM_LOCK_TABLES";
   case LTM_PRELOCKED:
     return "LTM_PRELOCKED";
   case LTM_PRELOCKED_UNDER_LOCK_TABLES:
     return "LTM_PRELOCKED_UNDER_LOCK_TABLES";
   default:
     return "Unknown table lock mode";
   }
}
#endif

/**
  Decide on logging format to use for the statement and issue errors
  or warnings as needed.  The decision depends on the following
  parameters:

  - The logging mode, i.e., the value of binlog_format.  Can be
    statement, mixed, or row.

  - The type of statement.  There are three types of statements:
    "normal" safe statements; unsafe statements; and row injections.
    An unsafe statement is one that, if logged in statement format,
    might produce different results when replayed on the slave (e.g.,
    queries with a LIMIT clause).  A row injection is either a BINLOG
    statement, or a row event executed by the slave's SQL thread.

  - The capabilities of tables modified by the statement.  The
    *capabilities vector* for a table is a set of flags associated
    with the table.  Currently, it only includes two flags: *row
    capability flag* and *statement capability flag*.

    The row capability flag is set if and only if the engine can
    handle row-based logging. The statement capability flag is set if
    and only if the table can handle statement-based logging.

  Decision table for logging format
  ---------------------------------

  The following table summarizes how the format and generated
  warning/error depends on the tables' capabilities, the statement
  type, and the current binlog_format.

     Row capable        N NNNNNNNNN YYYYYYYYY YYYYYYYYY
     Statement capable  N YYYYYYYYY NNNNNNNNN YYYYYYYYY

     Statement type     * SSSUUUIII SSSUUUIII SSSUUUIII

     binlog_format      * SMRSMRSMR SMRSMRSMR SMRSMRSMR

     Logged format      - SS-S----- -RR-RR-RR SRRSRR-RR
     Warning/Error      1 --2732444 5--5--6-- ---7--6--

  Legend
  ------

  Row capable:    N - Some table not row-capable, Y - All tables row-capable
  Stmt capable:   N - Some table not stmt-capable, Y - All tables stmt-capable
  Statement type: (S)afe, (U)nsafe, or Row (I)njection
  binlog_format:  (S)TATEMENT, (M)IXED, or (R)OW
  Logged format:  (S)tatement or (R)ow
  Warning/Error:  Warnings and error messages are as follows:

  1. Error: Cannot execute statement: binlogging impossible since both
     row-incapable engines and statement-incapable engines are
     involved.

  2. Error: Cannot execute statement: binlogging impossible since
     BINLOG_FORMAT = ROW and at least one table uses a storage engine
     limited to statement-logging.

  3. Error: Cannot execute statement: binlogging of unsafe statement
     is impossible when storage engine is limited to statement-logging
     and BINLOG_FORMAT = MIXED.

  4. Error: Cannot execute row injection: binlogging impossible since
     at least one table uses a storage engine limited to
     statement-logging.

  5. Error: Cannot execute statement: binlogging impossible since
     BINLOG_FORMAT = STATEMENT and at least one table uses a storage
     engine limited to row-logging.

  6. Error: Cannot execute row injection: binlogging impossible since
     BINLOG_FORMAT = STATEMENT.

  7. Warning: Unsafe statement binlogged in statement format since
     BINLOG_FORMAT = STATEMENT.

  In addition, we can produce the following error (not depending on
  the variables of the decision diagram):

  8. Error: Cannot execute statement: binlogging impossible since more
     than one engine is involved and at least one engine is
     self-logging.

  9. Error: Do not allow users to modify a gtid_executed table
     explicitly by a XA transaction.

  For each error case above, the statement is prevented from being
  logged, we report an error, and roll back the statement.  For
  warnings, we set the thd->binlog_flags variable: the warning will be
  printed only if the statement is successfully logged.

  @see THD::binlog_query

  @param[in] thd    Client thread
  @param[in] tables Tables involved in the query

  @retval 0 No error; statement can be logged.
  @retval -1 One of the error conditions above applies (1, 2, 4, 5, 6 or 9).
*/

int THD::decide_logging_format(TABLE_LIST *tables)
{
  DBUG_ENTER("THD::decide_logging_format");
  DBUG_PRINT("info", ("query: %s", query().str));
  DBUG_PRINT("info", ("variables.binlog_format: %lu",
                      variables.binlog_format));
  DBUG_PRINT("info", ("lex->get_stmt_unsafe_flags(): 0x%x",
                      lex->get_stmt_unsafe_flags()));

  DEBUG_SYNC(current_thd, "begin_decide_logging_format");

  reset_binlog_local_stmt_filter();

  /*
    We should not decide logging format if the binlog is closed or
    binlogging is off, or if the statement is filtered out from the
    binlog by filtering rules.
  */
  if (mysql_bin_log.is_open() && (variables.option_bits & OPTION_BIN_LOG) &&
      !(variables.binlog_format == BINLOG_FORMAT_STMT &&
        !binlog_filter->db_ok(m_db.str)))
  {
    /*
      Compute one bit field with the union of all the engine
      capabilities, and one with the intersection of all the engine
      capabilities.
    */
    handler::Table_flags flags_write_some_set= 0;
    handler::Table_flags flags_access_some_set= 0;
    handler::Table_flags flags_write_all_set=
      HA_BINLOG_ROW_CAPABLE | HA_BINLOG_STMT_CAPABLE;

    /* 
       If different types of engines are about to be updated.
       For example: Innodb and Falcon; Innodb and MyIsam.
    */
    my_bool multi_write_engine= FALSE;
    /*
       If different types of engines are about to be accessed 
       and any of them is about to be updated. For example:
       Innodb and Falcon; Innodb and MyIsam.
    */
    my_bool multi_access_engine= FALSE;
    /*
       Identifies if a table is changed.
    */
    my_bool is_write= FALSE;
    /*
       A pointer to a previous table that was changed.
    */
    TABLE* prev_write_table= NULL;
    /*
       A pointer to a previous table that was accessed.
    */
    TABLE* prev_access_table= NULL;
    /*
      True if at least one table is transactional.
    */
    bool write_to_some_transactional_table= false;
    /*
      True if at least one table is non-transactional.
    */
    bool write_to_some_non_transactional_table= false;
    /*
       True if all non-transactional tables that has been updated
       are temporary.
    */
    bool write_all_non_transactional_are_tmp_tables= true;
    /**
      The number of tables used in the current statement,
      that should be replicated.
    */
    uint replicated_tables_count= 0;
    /**
      The number of tables written to in the current statement,
      that should not be replicated.
      A table should not be replicated when it is considered
      'local' to a MySQL instance.
      Currently, these tables are:
      - mysql.slow_log
      - mysql.general_log
      - mysql.slave_relay_log_info
      - mysql.slave_master_info
      - mysql.slave_worker_info
      - performance_schema.*
      - TODO: information_schema.*
      In practice, from this list, only performance_schema.* tables
      are written to by user queries.
    */
    uint non_replicated_tables_count= 0;
    /**
      Indicate whether we alreadly reported a warning
      on modifying gtid_executed table.
    */
    int warned_gtid_executed_table= 0;
#ifndef DBUG_OFF
    {
      DBUG_PRINT("debug", ("prelocked_mode: %s",
                           get_locked_tables_mode_name(locked_tables_mode)));
    }
#endif

    if (variables.binlog_format != BINLOG_FORMAT_ROW && tables)
    {
      /*
        DML statements that modify a table with an auto_increment column based on
        rows selected from a table are unsafe as the order in which the rows are
        fetched fron the select tables cannot be determined and may differ on
        master and slave.
       */
      if (has_write_table_with_auto_increment_and_select(tables))
        lex->set_stmt_unsafe(LEX::BINLOG_STMT_UNSAFE_WRITE_AUTOINC_SELECT);

      if (has_write_table_auto_increment_not_first_in_pk(tables))
        lex->set_stmt_unsafe(LEX::BINLOG_STMT_UNSAFE_AUTOINC_NOT_FIRST);

      /*
        A query that modifies autoinc column in sub-statement can make the
        master and slave inconsistent.
        We can solve these problems in mixed mode by switching to binlogging
        if at least one updated table is used by sub-statement
       */
      if (lex->requires_prelocking() &&
          has_write_table_with_auto_increment(lex->first_not_own_table()))
        lex->set_stmt_unsafe(LEX::BINLOG_STMT_UNSAFE_AUTOINC_COLUMNS);
    }

    /*
      Get the capabilities vector for all involved storage engines and
      mask out the flags for the binary log.
    */
    for (TABLE_LIST *table= tables; table; table= table->next_global)
    {
      if (table->is_placeholder())
        continue;

      handler::Table_flags const flags= table->table->file->ha_table_flags();

      DBUG_PRINT("info", ("table: %s; ha_table_flags: 0x%llx",
                          table->table_name, flags));

      if (table->table->no_replicate)
      {
        if (!warned_gtid_executed_table)
        {
          warned_gtid_executed_table=
            gtid_state->warn_or_err_on_modify_gtid_table(this, table);
          /*
            Do not allow users to modify the gtid_executed table
            explicitly by a XA transaction.
          */
          if (warned_gtid_executed_table == 2)
            DBUG_RETURN(-1);
        }
        /*
          The statement uses a table that is not replicated.
          The following properties about the table:
          - persistent / transient
          - transactional / non transactional
          - temporary / permanent
          - read or write
          - multiple engines involved because of this table
          are not relevant, as this table is completely ignored.
          Because the statement uses a non replicated table,
          using STATEMENT format in the binlog is impossible.
          Either this statement will be discarded entirely,
          or it will be logged (possibly partially) in ROW format.
        */
        lex->set_stmt_unsafe(LEX::BINLOG_STMT_UNSAFE_SYSTEM_TABLE);

        if (table->lock_type >= TL_WRITE_ALLOW_WRITE)
        {
          non_replicated_tables_count++;
          continue;
        }
      }

      replicated_tables_count++;

      my_bool trans= table->table->file->has_transactions();

      if (table->lock_type >= TL_WRITE_ALLOW_WRITE)
      {
        write_to_some_transactional_table=
          write_to_some_transactional_table || trans;

        write_to_some_non_transactional_table=
          write_to_some_non_transactional_table || !trans;

        if (prev_write_table && prev_write_table->file->ht !=
            table->table->file->ht)
          multi_write_engine= TRUE;

        if (table->table->s->tmp_table)
          lex->set_stmt_accessed_table(trans ? LEX::STMT_WRITES_TEMP_TRANS_TABLE :
                                               LEX::STMT_WRITES_TEMP_NON_TRANS_TABLE);
        else
          lex->set_stmt_accessed_table(trans ? LEX::STMT_WRITES_TRANS_TABLE :
                                               LEX::STMT_WRITES_NON_TRANS_TABLE);

        /*
         Non-transactional updates are allowed when row binlog format is
         used and all non-transactional tables are temporary.
         Binlog format is checked on THD::is_dml_gtid_compatible() method.
        */
        if (!trans)
          write_all_non_transactional_are_tmp_tables=
            write_all_non_transactional_are_tmp_tables &&
            table->table->s->tmp_table;

        flags_write_all_set &= flags;
        flags_write_some_set |= flags;
        is_write= TRUE;

        prev_write_table= table->table;

        /*
          It should be marked unsafe if a table which uses a fulltext parser
          plugin is modified. See also bug#48183.
        */
        if (!lex->is_stmt_unsafe(LEX::BINLOG_STMT_UNSAFE_FULLTEXT_PLUGIN))
        {
          if (fulltext_unsafe_set(table->table->s))
            lex->set_stmt_unsafe(LEX::BINLOG_STMT_UNSAFE_FULLTEXT_PLUGIN);
        }
        /*
          INSERT...ON DUPLICATE KEY UPDATE on a table with more than one unique keys
          can be unsafe. Check for it if the flag is already not marked for the
          given statement.
        */
        if (!lex->is_stmt_unsafe(LEX::BINLOG_STMT_UNSAFE_INSERT_TWO_KEYS) &&
            lex->sql_command == SQLCOM_INSERT && lex->duplicates == DUP_UPDATE)
        {
          uint keys= table->table->s->keys, i= 0, unique_keys= 0;
          for (KEY* keyinfo= table->table->s->key_info;
               i < keys && unique_keys <= 1; i++, keyinfo++)
          {
            if (keyinfo->flags & HA_NOSAME)
              unique_keys++;
          }
          if (unique_keys > 1 )
            lex->set_stmt_unsafe(LEX::BINLOG_STMT_UNSAFE_INSERT_TWO_KEYS);
        }
      }
      if(lex->get_using_match())
      {
        if (fulltext_unsafe_set(table->table->s))
          lex->set_stmt_unsafe(LEX::BINLOG_STMT_UNSAFE_FULLTEXT_PLUGIN);
      }

      flags_access_some_set |= flags;

      if (lex->sql_command != SQLCOM_CREATE_TABLE ||
          (lex->sql_command == SQLCOM_CREATE_TABLE &&
          (lex->create_info.options & HA_LEX_CREATE_TMP_TABLE)))
      {
        if (table->table->s->tmp_table)
          lex->set_stmt_accessed_table(trans ? LEX::STMT_READS_TEMP_TRANS_TABLE :
                                               LEX::STMT_READS_TEMP_NON_TRANS_TABLE);
        else
          lex->set_stmt_accessed_table(trans ? LEX::STMT_READS_TRANS_TABLE :
                                               LEX::STMT_READS_NON_TRANS_TABLE);
      }

      if (prev_access_table && prev_access_table->file->ht !=
          table->table->file->ht)
         multi_access_engine= TRUE;

      prev_access_table= table->table;
    }
    DBUG_ASSERT(!is_write ||
                write_to_some_transactional_table ||
                write_to_some_non_transactional_table);
    /*
      write_all_non_transactional_are_tmp_tables may be true if any
      non-transactional table was not updated, so we fix its value here.
    */
    write_all_non_transactional_are_tmp_tables=
      write_all_non_transactional_are_tmp_tables &&
      write_to_some_non_transactional_table;

    DBUG_PRINT("info", ("flags_write_all_set: 0x%llx", flags_write_all_set));
    DBUG_PRINT("info", ("flags_write_some_set: 0x%llx", flags_write_some_set));
    DBUG_PRINT("info", ("flags_access_some_set: 0x%llx", flags_access_some_set));
    DBUG_PRINT("info", ("multi_write_engine: %d", multi_write_engine));
    DBUG_PRINT("info", ("multi_access_engine: %d", multi_access_engine));

    int error= 0;
    int unsafe_flags;

    bool multi_stmt_trans= in_multi_stmt_transaction_mode();
    bool trans_table= trans_has_updated_trans_table(this);
    bool binlog_direct= variables.binlog_direct_non_trans_update;

    if (lex->is_mixed_stmt_unsafe(multi_stmt_trans, binlog_direct,
                                  trans_table, tx_isolation))
      lex->set_stmt_unsafe(LEX::BINLOG_STMT_UNSAFE_MIXED_STATEMENT);
    else if (multi_stmt_trans && trans_table && !binlog_direct &&
             lex->stmt_accessed_table(LEX::STMT_WRITES_NON_TRANS_TABLE))
      lex->set_stmt_unsafe(LEX::BINLOG_STMT_UNSAFE_NONTRANS_AFTER_TRANS);

    /*
      If more than one engine is involved in the statement and at
      least one is doing it's own logging (is *self-logging*), the
      statement cannot be logged atomically, so we generate an error
      rather than allowing the binlog to become corrupt.
    */
    if (multi_write_engine &&
        (flags_write_some_set & HA_HAS_OWN_BINLOGGING))
      my_error((error= ER_BINLOG_MULTIPLE_ENGINES_AND_SELF_LOGGING_ENGINE),
               MYF(0));
    else if (multi_access_engine && flags_access_some_set & HA_HAS_OWN_BINLOGGING)
      lex->set_stmt_unsafe(LEX::BINLOG_STMT_UNSAFE_MULTIPLE_ENGINES_AND_SELF_LOGGING_ENGINE);

    /* XA is unsafe for statements */
    if (is_write &&
        !get_transaction()->xid_state()->has_state(XID_STATE::XA_NOTR))
      lex->set_stmt_unsafe(LEX::BINLOG_STMT_UNSAFE_XA);

    /* both statement-only and row-only engines involved */
    if ((flags_write_all_set & (HA_BINLOG_STMT_CAPABLE | HA_BINLOG_ROW_CAPABLE)) == 0)
    {
      /*
        1. Error: Binary logging impossible since both row-incapable
           engines and statement-incapable engines are involved
      */
      my_error((error= ER_BINLOG_ROW_ENGINE_AND_STMT_ENGINE), MYF(0));
    }
    /* statement-only engines involved */
    else if ((flags_write_all_set & HA_BINLOG_ROW_CAPABLE) == 0)
    {
      if (lex->is_stmt_row_injection())
      {
        /*
          4. Error: Cannot execute row injection since table uses
             storage engine limited to statement-logging
        */
        my_error((error= ER_BINLOG_ROW_INJECTION_AND_STMT_ENGINE), MYF(0));
      }
      else if (variables.binlog_format == BINLOG_FORMAT_ROW &&
               sqlcom_can_generate_row_events(this->lex->sql_command))
      {
        /*
          2. Error: Cannot modify table that uses a storage engine
             limited to statement-logging when BINLOG_FORMAT = ROW
        */
        my_error((error= ER_BINLOG_ROW_MODE_AND_STMT_ENGINE), MYF(0));
      }
      else if ((unsafe_flags= lex->get_stmt_unsafe_flags()) != 0)
      {
        /*
          3. Error: Cannot execute statement: binlogging of unsafe
             statement is impossible when storage engine is limited to
             statement-logging and BINLOG_FORMAT = MIXED.
        */
        for (int unsafe_type= 0;
             unsafe_type < LEX::BINLOG_STMT_UNSAFE_COUNT;
             unsafe_type++)
          if (unsafe_flags & (1 << unsafe_type))
            my_error((error= ER_BINLOG_UNSAFE_AND_STMT_ENGINE), MYF(0),
                     ER(LEX::binlog_stmt_unsafe_errcode[unsafe_type]));
      }
      /* log in statement format! */
    }
    /* no statement-only engines */
    else
    {
      /* binlog_format = STATEMENT */
      if (variables.binlog_format == BINLOG_FORMAT_STMT)
      {
        if (lex->is_stmt_row_injection())
        {
          /*
            6. Error: Cannot execute row injection since
               BINLOG_FORMAT = STATEMENT
          */
          my_error((error= ER_BINLOG_ROW_INJECTION_AND_STMT_MODE), MYF(0));
        }
        else if ((flags_write_all_set & HA_BINLOG_STMT_CAPABLE) == 0 &&
                 sqlcom_can_generate_row_events(this->lex->sql_command))
        {
          /*
            5. Error: Cannot modify table that uses a storage engine
               limited to row-logging when binlog_format = STATEMENT
          */
          my_error((error= ER_BINLOG_STMT_MODE_AND_ROW_ENGINE), MYF(0), "");
        }
        else if (is_write && (unsafe_flags= lex->get_stmt_unsafe_flags()) != 0)
        {
          /*
            7. Warning: Unsafe statement logged as statement due to
               binlog_format = STATEMENT
          */
          binlog_unsafe_warning_flags|= unsafe_flags;
          DBUG_PRINT("info", ("Scheduling warning to be issued by "
                              "binlog_query: '%s'",
                              ER(ER_BINLOG_UNSAFE_STATEMENT)));
          DBUG_PRINT("info", ("binlog_unsafe_warning_flags: 0x%x",
                              binlog_unsafe_warning_flags));
        }
        /* log in statement format! */
      }
      /* No statement-only engines and binlog_format != STATEMENT.
         I.e., nothing prevents us from row logging if needed. */
      else
      {
        if (lex->is_stmt_unsafe() || lex->is_stmt_row_injection()
            || (flags_write_all_set & HA_BINLOG_STMT_CAPABLE) == 0)
        {
#ifndef DBUG_OFF
          int flags= lex->get_stmt_unsafe_flags();
          DBUG_PRINT("info", ("setting row format for unsafe statement"));
          for (int i= 0; i < Query_tables_list::BINLOG_STMT_UNSAFE_COUNT; i++)
          {
            if (flags & (1 << i))
              DBUG_PRINT("info", ("unsafe reason: %s",
                                  ER(Query_tables_list::binlog_stmt_unsafe_errcode[i])));
          }
          DBUG_PRINT("info", ("is_row_injection=%d",
                              lex->is_stmt_row_injection()));
          DBUG_PRINT("info", ("stmt_capable=%llu",
                              (flags_write_all_set & HA_BINLOG_STMT_CAPABLE)));
#endif
          /* log in row format! */
          set_current_stmt_binlog_format_row_if_mixed();
        }
      }
    }

    if (non_replicated_tables_count > 0)
    {
      if ((replicated_tables_count == 0) || ! is_write)
      {
        DBUG_PRINT("info", ("decision: no logging, no replicated table affected"));
        set_binlog_local_stmt_filter();
      }
      else
      {
        if (! is_current_stmt_binlog_format_row())
        {
          my_error((error= ER_BINLOG_STMT_MODE_AND_NO_REPL_TABLES), MYF(0));
        }
        else
        {
          clear_binlog_local_stmt_filter();
        }
      }
    }
    else
    {
      clear_binlog_local_stmt_filter();
    }

    if (!error &&
        !is_dml_gtid_compatible(write_to_some_transactional_table,
                                write_to_some_non_transactional_table,
                                write_all_non_transactional_are_tmp_tables))
      error= 1;

    if (error) {
      DBUG_PRINT("info", ("decision: no logging since an error was generated"));
      DBUG_RETURN(-1);
    }

    if (is_write &&
        lex->sql_command != SQLCOM_END /* rows-event applying by slave */)
    {
      /*
        Master side of DML in the STMT format events parallelization.
        All involving table db:s are stored in a abc-ordered name list.
        In case the number of databases exceeds MAX_DBS_IN_EVENT_MTS maximum
        the list gathering breaks since it won't be sent to the slave.
      */
      for (TABLE_LIST *table= tables; table; table= table->next_global)
      {
        if (table->is_placeholder())
          continue;

        DBUG_ASSERT(table->table);

        if (table->table->file->referenced_by_foreign_key())
        {
          /* 
             FK-referenced dbs can't be gathered currently. The following
             event will be marked for sequential execution on slave.
          */
          binlog_accessed_db_names= NULL;
          add_to_binlog_accessed_dbs("");
          break;
        }
        if (!is_current_stmt_binlog_format_row())
          add_to_binlog_accessed_dbs(table->db);
      }
    }
    DBUG_PRINT("info", ("decision: logging in %s format",
                        is_current_stmt_binlog_format_row() ?
                        "ROW" : "STATEMENT"));

    if (variables.binlog_format == BINLOG_FORMAT_ROW &&
        (lex->sql_command == SQLCOM_UPDATE ||
         lex->sql_command == SQLCOM_UPDATE_MULTI ||
         lex->sql_command == SQLCOM_DELETE ||
         lex->sql_command == SQLCOM_DELETE_MULTI))
    {
      String table_names;
      /*
        Generate a warning for UPDATE/DELETE statements that modify a
        BLACKHOLE table, as row events are not logged in row format.
      */
      for (TABLE_LIST *table= tables; table; table= table->next_global)
      {
        if (table->is_placeholder())
          continue;
        if (table->table->file->ht->db_type == DB_TYPE_BLACKHOLE_DB &&
            table->lock_type >= TL_WRITE_ALLOW_WRITE)
        {
            table_names.append(table->table_name);
            table_names.append(",");
        }
      }
      if (!table_names.is_empty())
      {
        bool is_update= (lex->sql_command == SQLCOM_UPDATE ||
                         lex->sql_command == SQLCOM_UPDATE_MULTI);
        /*
          Replace the last ',' with '.' for table_names
        */
        table_names.replace(table_names.length()-1, 1, ".", 1);
        push_warning_printf(this, Sql_condition::SL_WARNING,
                            WARN_ON_BLOCKHOLE_IN_RBR,
                            ER(WARN_ON_BLOCKHOLE_IN_RBR),
                            is_update ? "UPDATE" : "DELETE",
                            table_names.c_ptr());
      }
    }
  }
  else
  {
    DBUG_PRINT("info", ("decision: no logging since "
                        "mysql_bin_log.is_open() = %d "
                        "and (options & OPTION_BIN_LOG) = 0x%llx "
                        "and binlog_format = %lu "
                        "and binlog_filter->db_ok(db) = %d",
                        mysql_bin_log.is_open(),
                        (variables.option_bits & OPTION_BIN_LOG),
                        variables.binlog_format,
                        binlog_filter->db_ok(m_db.str)));

    for (TABLE_LIST *table= tables; table; table= table->next_global)
    {
      if (!table->is_placeholder() && table->table->no_replicate &&
          gtid_state->warn_or_err_on_modify_gtid_table(this, table))
        break;
    }
  }

  DEBUG_SYNC(current_thd, "end_decide_logging_format");

  DBUG_RETURN(0);
}


/**
  Given that a possible violation of gtid consistency has happened,
  checks if gtid-inconsistencies are forbidden by the current value of
  ENFORCE_GTID_CONSISTENCY and GTID_MODE. If forbidden, generates
  error or warning accordingly.

  @param thd The thread that has issued the GTID-violating statement.

  @param error_code The error code to use, if error or warning is to
  be generated.

  @retval false Error was generated.
  @retval true No error was generated (possibly a warning was generated).
*/
static bool handle_gtid_consistency_violation(THD *thd, int error_code)
{
  DBUG_ENTER("handle_gtid_consistency_violation");

  enum_group_type gtid_next_type= thd->variables.gtid_next.type;
  global_sid_lock->rdlock();
  enum_gtid_consistency_mode gtid_consistency_mode=
    get_gtid_consistency_mode();
  enum_gtid_mode gtid_mode= get_gtid_mode(GTID_MODE_LOCK_SID);

  DBUG_PRINT("info", ("gtid_next.type=%d gtid_mode=%s "
                      "gtid_consistency_mode=%d error=%d query=%s",
                      gtid_next_type,
                      get_gtid_mode_string(gtid_mode),
                      gtid_consistency_mode,
                      error_code,
                      thd->query().str));

  /*
    GTID violations should generate error if:
    - GTID_MODE=ON or ON_PERMISSIVE and GTID_NEXT='AUTOMATIC' (since the
      transaction is expected to commit using a GTID), or
    - GTID_NEXT='UUID:NUMBER' (since the transaction is expected to
      commit usinga GTID), or
    - ENFORCE_GTID_CONSISTENCY=ON.
  */
  if ((gtid_next_type == AUTOMATIC_GROUP &&
       gtid_mode >= GTID_MODE_ON_PERMISSIVE) ||
      gtid_next_type == GTID_GROUP ||
      gtid_consistency_mode == GTID_CONSISTENCY_MODE_ON)
  {
    global_sid_lock->unlock();
    my_error(error_code, MYF(0));
    DBUG_RETURN(false);
  }
  else
  {
    /*
      If we are not generating an error, we must increase the counter
      of GTID-violating transactions.  This will prevent a concurrent
      client from executing a SET GTID_MODE or SET
      ENFORCE_GTID_CONSISTENCY statement that would be incompatible
      with this transaction.

      If the transaction had already been accounted as a gtid violating
      transaction, then don't increment the counters, just issue the
      warning below. This prevents calling
      begin_automatic_gtid_violating_transaction or
      begin_anonymous_gtid_violating_transaction multiple times for the
      same transaction, which would make the counter go out of sync.
    */
    if (!thd->has_gtid_consistency_violation)
    {
      if (gtid_next_type == AUTOMATIC_GROUP)
        gtid_state->begin_automatic_gtid_violating_transaction();
      else
      {
        DBUG_ASSERT(gtid_next_type == ANONYMOUS_GROUP);
        gtid_state->begin_anonymous_gtid_violating_transaction();
      }

      /*
        If a transaction generates multiple GTID violation conditions,
        it must still only update the counters once.  Hence we use
        this per-thread flag to keep track of whether the thread has a
        consistency or not.  This function must only be called if the
        transaction does not already have a GTID violation.
      */
      thd->has_gtid_consistency_violation= true;
    }

    global_sid_lock->unlock();

    // Generate warning if ENFORCE_GTID_CONSISTENCY = WARN.
    if (gtid_consistency_mode == GTID_CONSISTENCY_MODE_WARN)
    {
      // Need to print to log so that replication admin knows when users
      // have adjusted their workloads.
      sql_print_warning("%s", ER(error_code));
      // Need to print to client so that users can adjust their workload.
      push_warning(thd, Sql_condition::SL_WARNING, error_code, ER(error_code));
    }
    DBUG_RETURN(true);
  }
}


bool THD::is_ddl_gtid_compatible()
{
  DBUG_ENTER("THD::is_ddl_gtid_compatible");

  // If @@session.sql_log_bin has been manually turned off (only
  // doable by SUPER), then no problem, we can execute any statement.
  if ((variables.option_bits & OPTION_BIN_LOG) == 0 ||
      mysql_bin_log.is_open() == false)
    DBUG_RETURN(true);

  DBUG_PRINT("info",
             ("SQLCOM_CREATE:%d CREATE-TMP:%d SELECT:%d SQLCOM_DROP:%d DROP-TMP:%d trx:%d",
              lex->sql_command == SQLCOM_CREATE_TABLE,
              (lex->sql_command == SQLCOM_CREATE_TABLE &&
               (lex->create_info.options & HA_LEX_CREATE_TMP_TABLE)),
              lex->select_lex->item_list.elements,
              lex->sql_command == SQLCOM_DROP_TABLE,
              (lex->sql_command == SQLCOM_DROP_TABLE && lex->drop_temporary),
              in_multi_stmt_transaction_mode()));

  if (lex->sql_command == SQLCOM_CREATE_TABLE &&
      !(lex->create_info.options & HA_LEX_CREATE_TMP_TABLE) &&
      lex->select_lex->item_list.elements)
  {
    /*
      CREATE ... SELECT (without TEMPORARY) is unsafe because if
      binlog_format=row it will be logged as a CREATE TABLE followed
      by row events, re-executed non-atomically as two transactions,
      and then written to the slave's binary log as two separate
      transactions with the same GTID.
    */
    bool ret= handle_gtid_consistency_violation(
      this, ER_GTID_UNSAFE_CREATE_SELECT);
    DBUG_RETURN(ret);
  }
  else if ((lex->sql_command == SQLCOM_CREATE_TABLE &&
            (lex->create_info.options & HA_LEX_CREATE_TMP_TABLE) != 0) ||
           (lex->sql_command == SQLCOM_DROP_TABLE && lex->drop_temporary))
  {
    /*
      [CREATE|DROP] TEMPORARY TABLE is unsafe to execute
      inside a transaction because the table will stay and the
      transaction will be written to the slave's binary log with the
      GTID even if the transaction is rolled back.
      This includes the execution inside Functions and Triggers.
    */
    if (in_multi_stmt_transaction_mode() || in_sub_stmt)
    {
      bool ret= handle_gtid_consistency_violation(
        this, ER_GTID_UNSAFE_CREATE_DROP_TEMPORARY_TABLE_IN_TRANSACTION);
      DBUG_RETURN(ret);
    }
  }
  DBUG_RETURN(true);
}


bool
THD::is_dml_gtid_compatible(bool some_transactional_table,
                            bool some_non_transactional_table,
                            bool non_transactional_tables_are_tmp)
{
  DBUG_ENTER("THD::is_dml_gtid_compatible(bool, bool, bool)");

  // If @@session.sql_log_bin has been manually turned off (only
  // doable by SUPER), then no problem, we can execute any statement.
  if ((variables.option_bits & OPTION_BIN_LOG) == 0 ||
      mysql_bin_log.is_open() == false)
    DBUG_RETURN(true);

  /*
    Single non-transactional updates are allowed when not mixed
    together with transactional statements within a transaction.
    Furthermore, writing to transactional and non-transactional
    engines in a single statement is also disallowed.
    Multi-statement transactions on non-transactional tables are
    split into single-statement transactions when
    GTID_NEXT = "AUTOMATIC".

    Non-transactional updates are allowed when row binlog format is
    used and all non-transactional tables are temporary.

    The debug symbol "allow_gtid_unsafe_non_transactional_updates"
    disables the error.  This is useful because it allows us to run
    old tests that were not written with the restrictions of GTIDs in
    mind.
  */
  DBUG_PRINT("info", ("some_non_transactional_table=%d "
                      "some_transactional_table=%d "
                      "trans_has_updated_trans_table=%d "
                      "non_transactional_tables_are_tmp=%d "
                      "is_current_stmt_binlog_format_row=%d",
                      some_non_transactional_table,
                      some_transactional_table,
                      trans_has_updated_trans_table(this),
                      non_transactional_tables_are_tmp,
                      is_current_stmt_binlog_format_row()));
  if (some_non_transactional_table &&
      (some_transactional_table || trans_has_updated_trans_table(this)) &&
      !(non_transactional_tables_are_tmp &&
        is_current_stmt_binlog_format_row()) &&
      !DBUG_EVALUATE_IF("allow_gtid_unsafe_non_transactional_updates", 1, 0))
  {
    DBUG_RETURN(handle_gtid_consistency_violation(
      this, ER_GTID_UNSAFE_NON_TRANSACTIONAL_TABLE));
  }

  DBUG_RETURN(true);
}

/*
  Implementation of interface to write rows to the binary log through the
  thread.  The thread is responsible for writing the rows it has
  inserted/updated/deleted.
*/

#ifndef MYSQL_CLIENT

/*
  Template member function for ensuring that there is an rows log
  event of the apropriate type before proceeding.

  PRE CONDITION:
    - Events of type 'RowEventT' have the type code 'type_code'.
    
  POST CONDITION:
    If a non-NULL pointer is returned, the pending event for thread 'thd' will
    be an event of type 'RowEventT' (which have the type code 'type_code')
    will either empty or have enough space to hold 'needed' bytes.  In
    addition, the columns bitmap will be correct for the row, meaning that
    the pending event will be flushed if the columns in the event differ from
    the columns suppled to the function.

  RETURNS
    If no error, a non-NULL pending event (either one which already existed or
    the newly created one).
    If error, NULL.
 */

template <class RowsEventT> Rows_log_event* 
THD::binlog_prepare_pending_rows_event(TABLE* table, uint32 serv_id,
                                       size_t needed,
                                       bool is_transactional,
				       RowsEventT *hint MY_ATTRIBUTE((unused)),
                                       const uchar* extra_row_info)
{
  DBUG_ENTER("binlog_prepare_pending_rows_event");

  /* Fetch the type code for the RowsEventT template parameter */
  int const general_type_code= RowsEventT::TYPE_CODE;

  Rows_log_event* pending= binlog_get_pending_rows_event(is_transactional);

  if (unlikely(pending && !pending->is_valid()))
    DBUG_RETURN(NULL);

  /*
    Check if the current event is non-NULL and a write-rows
    event. Also check if the table provided is mapped: if it is not,
    then we have switched to writing to a new table.
    If there is no pending event, we need to create one. If there is a pending
    event, but it's not about the same table id, or not of the same type
    (between Write, Update and Delete), or not the same affected columns, or
    going to be too big, flush this event to disk and create a new pending
    event.
  */
  if (!pending ||
      pending->server_id != serv_id || 
      pending->get_table_id() != table->s->table_map_id ||
      pending->get_general_type_code() != general_type_code ||
      pending->get_data_size() + needed > opt_binlog_rows_event_max_size ||
      pending->read_write_bitmaps_cmp(table) == FALSE ||
      !binlog_row_event_extra_data_eq(pending->get_extra_row_data(),
                                      extra_row_info))
  {
    /* Create a new RowsEventT... */
    Rows_log_event* const
	ev= new RowsEventT(this, table, table->s->table_map_id,
                           is_transactional, extra_row_info);
    if (unlikely(!ev))
      DBUG_RETURN(NULL);
    ev->server_id= serv_id; // I don't like this, it's too easy to forget.
    /*
      flush the pending event and replace it with the newly created
      event...
    */
    if (unlikely(
        mysql_bin_log.flush_and_set_pending_rows_event(this, ev,
                                                       is_transactional)))
    {
      delete ev;
      DBUG_RETURN(NULL);
    }

    DBUG_RETURN(ev);               /* This is the new pending event */
  }
  DBUG_RETURN(pending);        /* This is the current pending event */
}

/* Declare in unnamed namespace. */
namespace {

  /**
     Class to handle temporary allocation of memory for row data.

     The responsibilities of the class is to provide memory for
     packing one or two rows of packed data (depending on what
     constructor is called).

     In order to make the allocation more efficient for "simple" rows,
     i.e., rows that do not contain any blobs, a pointer to the
     allocated memory is of memory is stored in the table structure
     for simple rows.  If memory for a table containing a blob field
     is requested, only memory for that is allocated, and subsequently
     released when the object is destroyed.

   */
  class Row_data_memory {
  public:
    /**
      Build an object to keep track of a block-local piece of memory
      for storing a row of data.

      @param table
      Table where the pre-allocated memory is stored.

      @param length
      Length of data that is needed, if the record contain blobs.
     */
    Row_data_memory(TABLE *table, size_t const len1)
      : m_memory(0)
    {
#ifndef DBUG_OFF
      m_alloc_checked= FALSE;
#endif
      allocate_memory(table, len1);
      m_ptr[0]= has_memory() ? m_memory : 0;
      m_ptr[1]= 0;
    }

    Row_data_memory(TABLE *table, size_t const len1, size_t const len2)
      : m_memory(0)
    {
#ifndef DBUG_OFF
      m_alloc_checked= FALSE;
#endif
      allocate_memory(table, len1 + len2);
      m_ptr[0]= has_memory() ? m_memory        : 0;
      m_ptr[1]= has_memory() ? m_memory + len1 : 0;
    }

    ~Row_data_memory()
    {
      if (m_memory != 0 && m_release_memory_on_destruction)
        my_free(m_memory);
    }

    /**
       Is there memory allocated?

       @retval true There is memory allocated
       @retval false Memory allocation failed
     */
    bool has_memory() const {
#ifndef DBUG_OFF
      m_alloc_checked= TRUE;
#endif
      return m_memory != 0;
    }

    uchar *slot(uint s)
    {
      DBUG_ASSERT(s < sizeof(m_ptr)/sizeof(*m_ptr));
      DBUG_ASSERT(m_ptr[s] != 0);
      DBUG_ASSERT(m_alloc_checked == TRUE);
      return m_ptr[s];
    }

  private:
    void allocate_memory(TABLE *const table, size_t const total_length)
    {
      if (table->s->blob_fields == 0)
      {
        /*
          The maximum length of a packed record is less than this
          length. We use this value instead of the supplied length
          when allocating memory for records, since we don't know how
          the memory will be used in future allocations.

          Since table->s->reclength is for unpacked records, we have
          to add two bytes for each field, which can potentially be
          added to hold the length of a packed field.
        */
        size_t const maxlen= table->s->reclength + 2 * table->s->fields;

        /*
          Allocate memory for two records if memory hasn't been
          allocated. We allocate memory for two records so that it can
          be used when processing update rows as well.
        */
        if (table->write_row_record == 0)
          table->write_row_record=
            (uchar *) alloc_root(&table->mem_root, 2 * maxlen);
        m_memory= table->write_row_record;
        m_release_memory_on_destruction= FALSE;
      }
      else
      {
        m_memory= (uchar *) my_malloc(key_memory_Row_data_memory_memory,
                                      total_length, MYF(MY_WME));
        m_release_memory_on_destruction= TRUE;
      }
    }

#ifndef DBUG_OFF
    mutable bool m_alloc_checked;
#endif
    bool m_release_memory_on_destruction;
    uchar *m_memory;
    uchar *m_ptr[2];
  };

} // namespace

int THD::binlog_write_row(TABLE* table, bool is_trans, 
                          uchar const *record,
                          const uchar* extra_row_info)
{ 
  DBUG_ASSERT(is_current_stmt_binlog_format_row() && mysql_bin_log.is_open());

  /*
    Pack records into format for transfer. We are allocating more
    memory than needed, but that doesn't matter.
  */
  Row_data_memory memory(table, max_row_length(table, record));
  if (!memory.has_memory())
    return HA_ERR_OUT_OF_MEM;

  uchar *row_data= memory.slot(0);

  size_t const len= pack_row(table, table->write_set, row_data, record);

  Rows_log_event* const ev=
    binlog_prepare_pending_rows_event(table, server_id, len, is_trans,
                                      static_cast<Write_rows_log_event*>(0),
                                      extra_row_info);

  if (unlikely(ev == 0))
    return HA_ERR_OUT_OF_MEM;

  return ev->add_row_data(row_data, len);
}

int THD::binlog_update_row(TABLE* table, bool is_trans,
                           const uchar *before_record,
                           const uchar *after_record,
                           const uchar* extra_row_info)
{ 
  DBUG_ASSERT(is_current_stmt_binlog_format_row() && mysql_bin_log.is_open());
  int error= 0;

  /**
    Save a reference to the original read and write set bitmaps.
    We will need this to restore the bitmaps at the end.
   */
  MY_BITMAP *old_read_set= table->read_set;
  MY_BITMAP *old_write_set= table->write_set;

  /** 
     This will remove spurious fields required during execution but
     not needed for binlogging. This is done according to the:
     binlog-row-image option.
   */
  binlog_prepare_row_images(table);

  size_t const before_maxlen = max_row_length(table, before_record);
  size_t const after_maxlen  = max_row_length(table, after_record);

  Row_data_memory row_data(table, before_maxlen, after_maxlen);
  if (!row_data.has_memory())
    return HA_ERR_OUT_OF_MEM;

  uchar *before_row= row_data.slot(0);
  uchar *after_row= row_data.slot(1);

  size_t const before_size= pack_row(table, table->read_set, before_row,
                                        before_record);
  size_t const after_size= pack_row(table, table->write_set, after_row,
                                       after_record);

  DBUG_DUMP("before_record", before_record, table->s->reclength);
  DBUG_DUMP("after_record",  after_record, table->s->reclength);
  DBUG_DUMP("before_row",    before_row, before_size);
  DBUG_DUMP("after_row",     after_row, after_size);

  Rows_log_event* const ev=
    binlog_prepare_pending_rows_event(table, server_id,
				      before_size + after_size, is_trans,
				      static_cast<Update_rows_log_event*>(0),
                                      extra_row_info);

  if (unlikely(ev == 0))
    return HA_ERR_OUT_OF_MEM;

  error= ev->add_row_data(before_row, before_size) ||
         ev->add_row_data(after_row, after_size);

  /* restore read/write set for the rest of execution */
  table->column_bitmaps_set_no_signal(old_read_set,
                                      old_write_set);
  
  bitmap_clear_all(&table->tmp_set);

  return error;
}

int THD::binlog_delete_row(TABLE* table, bool is_trans, 
                           uchar const *record,
                           const uchar* extra_row_info)
{ 
  DBUG_ASSERT(is_current_stmt_binlog_format_row() && mysql_bin_log.is_open());
  int error= 0;

  /**
    Save a reference to the original read and write set bitmaps.
    We will need this to restore the bitmaps at the end.
   */
  MY_BITMAP *old_read_set= table->read_set;
  MY_BITMAP *old_write_set= table->write_set;

  /** 
     This will remove spurious fields required during execution but
     not needed for binlogging. This is done according to the:
     binlog-row-image option.
   */
  binlog_prepare_row_images(table);

  /* 
     Pack records into format for transfer. We are allocating more
     memory than needed, but that doesn't matter.
  */
  Row_data_memory memory(table, max_row_length(table, record));
  if (unlikely(!memory.has_memory()))
    return HA_ERR_OUT_OF_MEM;

  uchar *row_data= memory.slot(0);

  DBUG_DUMP("table->read_set", (uchar*) table->read_set->bitmap, (table->s->fields + 7) / 8);
  size_t const len= pack_row(table, table->read_set, row_data, record);

  Rows_log_event* const ev=
    binlog_prepare_pending_rows_event(table, server_id, len, is_trans,
				      static_cast<Delete_rows_log_event*>(0),
                                      extra_row_info);

  if (unlikely(ev == 0))
    return HA_ERR_OUT_OF_MEM;

  error= ev->add_row_data(row_data, len);

  /* restore read/write set for the rest of execution */
  table->column_bitmaps_set_no_signal(old_read_set,
                                      old_write_set);

  bitmap_clear_all(&table->tmp_set);
  return error;
}

void THD::binlog_prepare_row_images(TABLE *table) 
{
  DBUG_ENTER("THD::binlog_prepare_row_images");
  /** 
    Remove from read_set spurious columns. The write_set has been
    handled before in table->mark_columns_needed_for_update. 
   */

  DBUG_PRINT_BITSET("debug", "table->read_set (before preparing): %s", table->read_set);
  THD *thd= table->in_use;

  /** 
    if there is a primary key in the table (ie, user declared PK or a
    non-null unique index) and we dont want to ship the entire image,
    and the handler involved supports this.
   */
  if (table->s->primary_key < MAX_KEY &&
      (thd->variables.binlog_row_image < BINLOG_ROW_IMAGE_FULL) &&
      !ha_check_storage_engine_flag(table->s->db_type(), HTON_NO_BINLOG_ROW_OPT))
  {
    /**
      Just to be sure that tmp_set is currently not in use as
      the read_set already.
    */
    DBUG_ASSERT(table->read_set != &table->tmp_set);
    // Verify it's not used
    DBUG_ASSERT(bitmap_is_clear_all(&table->tmp_set));

    switch(thd->variables.binlog_row_image)
    {
      case BINLOG_ROW_IMAGE_MINIMAL:
        /* MINIMAL: Mark only PK */
        table->mark_columns_used_by_index_no_reset(table->s->primary_key,
                                                   &table->tmp_set);
        break;
      case BINLOG_ROW_IMAGE_NOBLOB:
        /** 
          NOBLOB: Remove unnecessary BLOB fields from read_set 
                  (the ones that are not part of PK).
         */
        bitmap_union(&table->tmp_set, table->read_set);
        for (Field **ptr=table->field ; *ptr ; ptr++)
        {
          Field *field= (*ptr);
          if ((field->type() == MYSQL_TYPE_BLOB) &&
              !(field->flags & PRI_KEY_FLAG))
            bitmap_clear_bit(&table->tmp_set, field->field_index);
        }
        break;
      default:
        DBUG_ASSERT(0); // impossible.
    }

    /* set the temporary read_set */
    table->column_bitmaps_set_no_signal(&table->tmp_set,
                                        table->write_set);
  }

  DBUG_PRINT_BITSET("debug", "table->read_set (after preparing): %s", table->read_set);
  DBUG_VOID_RETURN;
}


int THD::binlog_flush_pending_rows_event(bool stmt_end, bool is_transactional)
{
  DBUG_ENTER("THD::binlog_flush_pending_rows_event");
  /*
    We shall flush the pending event even if we are not in row-based
    mode: it might be the case that we left row-based mode before
    flushing anything (e.g., if we have explicitly locked tables).
   */
  if (!mysql_bin_log.is_open())
    DBUG_RETURN(0);

  /*
    Mark the event as the last event of a statement if the stmt_end
    flag is set.
  */
  int error= 0;
  if (Rows_log_event *pending= binlog_get_pending_rows_event(is_transactional))
  {
    if (stmt_end)
    {
      pending->set_flags(Rows_log_event::STMT_END_F);
      binlog_table_maps= 0;
    }

    error= mysql_bin_log.flush_and_set_pending_rows_event(this, 0,
                                                          is_transactional);
  }

  DBUG_RETURN(error);
}


/**
   binlog_row_event_extra_data_eq

   Comparator for two binlog row event extra data
   pointers.

   It compares their significant bytes.

   Null pointers are acceptable

   @param a
     first pointer

   @param b
     first pointer

   @return
     true if the referenced structures are equal
*/
bool
THD::binlog_row_event_extra_data_eq(const uchar* a,
                                    const uchar* b)
{
  return ((a == b) ||
          ((a != NULL) &&
           (b != NULL) &&
           (a[EXTRA_ROW_INFO_LEN_OFFSET] ==
            b[EXTRA_ROW_INFO_LEN_OFFSET]) &&
           (memcmp(a, b,
                   a[EXTRA_ROW_INFO_LEN_OFFSET]) == 0)));
}

#if !defined(DBUG_OFF)
static const char *
show_query_type(THD::enum_binlog_query_type qtype)
{
  switch (qtype) {
  case THD::ROW_QUERY_TYPE:
    return "ROW";
  case THD::STMT_QUERY_TYPE:
    return "STMT";
  case THD::QUERY_TYPE_COUNT:
  default:
    DBUG_ASSERT(0 <= qtype && qtype < THD::QUERY_TYPE_COUNT);
  }
  static char buf[64];
  sprintf(buf, "UNKNOWN#%d", qtype);
  return buf;
}
#endif

/**
  Auxiliary function to reset the limit unsafety warning suppression.
*/
static void reset_binlog_unsafe_suppression()
{
  DBUG_ENTER("reset_binlog_unsafe_suppression");
  unsafe_warning_suppression_is_activated= false;
  limit_unsafe_warning_count= 0;
  limit_unsafe_suppression_start_time= my_getsystime()/10000000;
  DBUG_VOID_RETURN;
}

/**
  Auxiliary function to print warning in the error log.
*/
static void print_unsafe_warning_to_log(int unsafe_type, char* buf,
                                        const char* query)
{
  DBUG_ENTER("print_unsafe_warning_in_log");
  sprintf(buf, ER(ER_BINLOG_UNSAFE_STATEMENT),
          ER(LEX::binlog_stmt_unsafe_errcode[unsafe_type]));
  sql_print_warning(ER(ER_MESSAGE_AND_STATEMENT), buf, query);
  DBUG_VOID_RETURN;
}

/**
  Auxiliary function to check if the warning for limit unsafety should be
  thrown or suppressed. Details of the implementation can be found in the
  comments inline.

  @params
   buf         - buffer to hold the warning message text
   unsafe_type - The type of unsafety.
   query       - The actual query statement.

  TODO: Remove this function and implement a general service for all warnings
  that would prevent flooding the error log. => switch to log_throttle class?
*/
static void do_unsafe_limit_checkout(char* buf, int unsafe_type, const char* query)
{
  ulonglong now;
  DBUG_ENTER("do_unsafe_limit_checkout");
  DBUG_ASSERT(unsafe_type == LEX::BINLOG_STMT_UNSAFE_LIMIT);
  limit_unsafe_warning_count++;
  /*
    INITIALIZING:
    If this is the first time this function is called with log warning
    enabled, the monitoring the unsafe warnings should start.
  */
  if (limit_unsafe_suppression_start_time == 0)
  {
    limit_unsafe_suppression_start_time= my_getsystime()/10000000;
    print_unsafe_warning_to_log(unsafe_type, buf, query);
  }
  else
  {
    if (!unsafe_warning_suppression_is_activated)
      print_unsafe_warning_to_log(unsafe_type, buf, query);

    if (limit_unsafe_warning_count >=
        LIMIT_UNSAFE_WARNING_ACTIVATION_THRESHOLD_COUNT)
    {
      now= my_getsystime()/10000000;
      if (!unsafe_warning_suppression_is_activated)
      {
        /*
          ACTIVATION:
          We got LIMIT_UNSAFE_WARNING_ACTIVATION_THRESHOLD_COUNT warnings in
          less than LIMIT_UNSAFE_WARNING_ACTIVATION_TIMEOUT we activate the
          suppression.
        */
        if ((now-limit_unsafe_suppression_start_time) <=
                       LIMIT_UNSAFE_WARNING_ACTIVATION_TIMEOUT)
        {
          unsafe_warning_suppression_is_activated= true;
          DBUG_PRINT("info",("A warning flood has been detected and the limit \
unsafety warning suppression has been activated."));
        }
        else
        {
          /*
           there is no flooding till now, therefore we restart the monitoring
          */
          limit_unsafe_suppression_start_time= my_getsystime()/10000000;
          limit_unsafe_warning_count= 0;
        }
      }
      else
      {
        /*
          Print the suppression note and the unsafe warning.
        */
        sql_print_information("The following warning was suppressed %d times \
during the last %d seconds in the error log",
                              limit_unsafe_warning_count,
                              (int)
                              (now-limit_unsafe_suppression_start_time));
        print_unsafe_warning_to_log(unsafe_type, buf, query);
        /*
          DEACTIVATION: We got LIMIT_UNSAFE_WARNING_ACTIVATION_THRESHOLD_COUNT
          warnings in more than  LIMIT_UNSAFE_WARNING_ACTIVATION_TIMEOUT, the
          suppression should be deactivated.
        */
        if ((now - limit_unsafe_suppression_start_time) >
            LIMIT_UNSAFE_WARNING_ACTIVATION_TIMEOUT)
        {
          reset_binlog_unsafe_suppression();
          DBUG_PRINT("info",("The limit unsafety warning supression has been \
deactivated"));
        }
      }
      limit_unsafe_warning_count= 0;
    }
  }
  DBUG_VOID_RETURN;
}

/**
  Auxiliary method used by @c binlog_query() to raise warnings.

  The type of warning and the type of unsafeness is stored in
  THD::binlog_unsafe_warning_flags.
*/
void THD::issue_unsafe_warnings()
{
  char buf[MYSQL_ERRMSG_SIZE * 2];
  DBUG_ENTER("issue_unsafe_warnings");
  /*
    Ensure that binlog_unsafe_warning_flags is big enough to hold all
    bits.  This is actually a constant expression.
  */
  DBUG_ASSERT(LEX::BINLOG_STMT_UNSAFE_COUNT <=
              sizeof(binlog_unsafe_warning_flags) * CHAR_BIT);

  uint32 unsafe_type_flags= binlog_unsafe_warning_flags;

  /*
    For each unsafe_type, check if the statement is unsafe in this way
    and issue a warning.
  */
  for (int unsafe_type=0;
       unsafe_type < LEX::BINLOG_STMT_UNSAFE_COUNT;
       unsafe_type++)
  {
    if ((unsafe_type_flags & (1 << unsafe_type)) != 0)
    {
      push_warning_printf(this, Sql_condition::SL_NOTE,
                          ER_BINLOG_UNSAFE_STATEMENT,
                          ER(ER_BINLOG_UNSAFE_STATEMENT),
                          ER(LEX::binlog_stmt_unsafe_errcode[unsafe_type]));
      if (log_error_verbosity > 1 && opt_log_unsafe_statements)
      {
        if (unsafe_type == LEX::BINLOG_STMT_UNSAFE_LIMIT)
          do_unsafe_limit_checkout( buf, unsafe_type, query().str);
        else //cases other than LIMIT unsafety
          print_unsafe_warning_to_log(unsafe_type, buf, query().str);
      }
    }
  }
  DBUG_VOID_RETURN;
}

/**
  Log the current query.

  The query will be logged in either row format or statement format
  depending on the value of @c current_stmt_binlog_format_row field and
  the value of the @c qtype parameter.

  This function must be called:

  - After the all calls to ha_*_row() functions have been issued.

  - After any writes to system tables. Rationale: if system tables
    were written after a call to this function, and the master crashes
    after the call to this function and before writing the system
    tables, then the master and slave get out of sync.

  - Before tables are unlocked and closed.

  @see decide_logging_format

  @retval 0 Success

  @retval nonzero If there is a failure when writing the query (e.g.,
  write failure), then the error code is returned.
*/
int THD::binlog_query(THD::enum_binlog_query_type qtype, const char *query_arg,
                      size_t query_len, bool is_trans, bool direct,
                      bool suppress_use, int errcode)
{
  DBUG_ENTER("THD::binlog_query");
  DBUG_PRINT("enter", ("qtype: %s  query: '%s'",
                       show_query_type(qtype), query_arg));
  DBUG_ASSERT(query_arg && mysql_bin_log.is_open());

  if (get_binlog_local_stmt_filter() == BINLOG_FILTER_SET)
  {
    /*
      The current statement is to be ignored, and not written to
      the binlog. Do not call issue_unsafe_warnings().
    */
    DBUG_RETURN(0);
  }

  /*
    If we are not in prelocked mode, mysql_unlock_tables() will be
    called after this binlog_query(), so we have to flush the pending
    rows event with the STMT_END_F set to unlock all tables at the
    slave side as well.

    If we are in prelocked mode, the flushing will be done inside the
    top-most close_thread_tables().
  */
  if (this->locked_tables_mode <= LTM_LOCK_TABLES)
    if (int error= binlog_flush_pending_rows_event(TRUE, is_trans))
      DBUG_RETURN(error);

  /*
    Warnings for unsafe statements logged in statement format are
    printed in three places instead of in decide_logging_format().
    This is because the warnings should be printed only if the statement
    is actually logged. When executing decide_logging_format(), we cannot
    know for sure if the statement will be logged:

    1 - sp_head::execute_procedure which prints out warnings for calls to
    stored procedures.

    2 - sp_head::execute_function which prints out warnings for calls
    involving functions.

    3 - THD::binlog_query (here) which prints warning for top level
    statements not covered by the two cases above: i.e., if not insided a
    procedure and a function.
 
    Besides, we should not try to print these warnings if it is not
    possible to write statements to the binary log as it happens when
    the execution is inside a function, or generaly speaking, when
    the variables.option_bits & OPTION_BIN_LOG is false.
  */
  if ((variables.option_bits & OPTION_BIN_LOG) &&
      sp_runtime_ctx == NULL && !binlog_evt_union.do_union)
    issue_unsafe_warnings();

  switch (qtype) {
    /*
      ROW_QUERY_TYPE means that the statement may be logged either in
      row format or in statement format.  If
      current_stmt_binlog_format is row, it means that the
      statement has already been logged in row format and hence shall
      not be logged again.
    */
  case THD::ROW_QUERY_TYPE:
    DBUG_PRINT("debug",
               ("is_current_stmt_binlog_format_row: %d",
                is_current_stmt_binlog_format_row()));
    if (is_current_stmt_binlog_format_row())
      DBUG_RETURN(0);
    /* Fall through */

    /*
      STMT_QUERY_TYPE means that the query must be logged in statement
      format; it cannot be logged in row format.  This is typically
      used by DDL statements.  It is an error to use this query type
      if current_stmt_binlog_format_row is row.

      @todo Currently there are places that call this method with
      STMT_QUERY_TYPE and current_stmt_binlog_format is row.  Fix those
      places and add assert to ensure correct behavior. /Sven
    */
  case THD::STMT_QUERY_TYPE:
    /*
      The MYSQL_BIN_LOG::write() function will set the STMT_END_F flag and
      flush the pending rows event if necessary.
    */
    {
      Query_log_event qinfo(this, query_arg, query_len, is_trans, direct,
                            suppress_use, errcode);
      /*
        Binlog table maps will be irrelevant after a Query_log_event
        (they are just removed on the slave side) so after the query
        log event is written to the binary log, we pretend that no
        table maps were written.
       */
      int error= mysql_bin_log.write_event(&qinfo);
      binlog_table_maps= 0;
      DBUG_RETURN(error);
    }
    break;

  case THD::QUERY_TYPE_COUNT:
  default:
    DBUG_ASSERT(0 <= qtype && qtype < QUERY_TYPE_COUNT);
  }
  DBUG_RETURN(0);
}

#endif /* !defined(MYSQL_CLIENT) */

struct st_mysql_storage_engine binlog_storage_engine=
{ MYSQL_HANDLERTON_INTERFACE_VERSION };

/** @} */

mysql_declare_plugin(binlog)
{
  MYSQL_STORAGE_ENGINE_PLUGIN,
  &binlog_storage_engine,
  "binlog",
  "MySQL AB",
  "This is a pseudo storage engine to represent the binlog in a transaction",
  PLUGIN_LICENSE_GPL,
  binlog_init, /* Plugin Init */
  binlog_deinit, /* Plugin Deinit */
  0x0100 /* 1.0 */,
  NULL,                       /* status variables                */
  NULL,                       /* system variables                */
  NULL,                       /* config options                  */
  0,  
}
mysql_declare_plugin_end;

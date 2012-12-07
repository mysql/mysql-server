/* Copyright (c) 2009, 2011, 2012 Oracle and/or its affiliates. All rights reserved.

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


#include "my_global.h"
#include "log.h"
#include "binlog.h"
#include "log_event.h"
#include "rpl_filter.h"
#include "rpl_rli.h"
#include "sql_plugin.h"
#include "rpl_handler.h"
#include "rpl_info_factory.h"
#include "rpl_utility.h"
#include "debug_sync.h"
#include "global_threads.h"
#include "sql_show.h"
#include "sql_parse.h"
#include "rpl_mi.h"
#include <list>
#include <string>

using std::max;
using std::min;
using std::string;
using std::list;

#define FLAGSTR(V,F) ((V)&(F)?#F" ":"")

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

static ulonglong limit_unsafe_suppression_start_time= 0;
static bool unsafe_warning_suppression_is_activated= false;
static int limit_unsafe_warning_count= 0;

static handlerton *binlog_hton;
bool opt_binlog_order_commits= true;

const char *log_bin_index= 0;
const char *log_bin_basename= 0;

MYSQL_BIN_LOG mysql_bin_log(&sync_binlog_period);

static int binlog_init(void *p);
static int binlog_start_trans_and_stmt(THD *thd, Log_event *start_event);
static int binlog_close_connection(handlerton *hton, THD *thd);
static int binlog_savepoint_set(handlerton *hton, THD *thd, void *sv);
static int binlog_savepoint_rollback(handlerton *hton, THD *thd, void *sv);
static int binlog_commit(handlerton *hton, THD *thd, bool all);
static int binlog_rollback(handlerton *hton, THD *thd, bool all);
static int binlog_prepare(handlerton *hton, THD *thd, bool all);


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
  Helper class to perform a thread excursion.

  This class is used to temporarily switch to another session (THD
  structure). It will set up the PSI structures and other "globals"
  correctly (e.g., thread-specific variables) so that the POSIX thread
  looks exactly like the session attached to.

  On destruction, the original session (which is supplied to the
  constructor) will be re-attached automatically. For example, with
  this code, the value of @c current_thd will be the same before and
  after execution of the code.

  @code
  {
    Thread_excursion excursion(current_thd);
    for (int i = 0 ; i < count ; ++i)
      excursion.attach_to(other_thd[i]);
  }
  @endcode

  @warning The class is not designed to be inherited from.
 */

class Thread_excursion
{
public:
  Thread_excursion(THD *thd)
    : m_original_thd(thd)
#ifdef WITH_PERFSCHEMA_STORAGE_ENGINE
    , m_saved_psi(PSI_server ? PSI_server->get_thread() : NULL)
#endif
  {
  }

  ~Thread_excursion() {
#ifdef WITH_PERFSCHEMA_STORAGE_ENGINE
    if (PSI_server)
      PSI_server->set_thread(m_saved_psi);
#endif
#ifndef EMBEDDED_LIBRARY
    if (unlikely(setup_thread_globals(m_original_thd)))
      DBUG_ASSERT(0);                           // Out of memory?!
#endif
  }

  /**
    Attach the POSIX thread to a session.
   */
  int attach_to(THD *thd)
  {
#ifdef WITH_PERFSCHEMA_STORAGE_ENGINE
    if (PSI_server)
      PSI_server->set_thread(thd_get_psi(thd));
#endif
#ifndef EMBEDDED_LIBRARY
    if (unlikely(setup_thread_globals(thd)))
    {
#ifdef WITH_PERFSCHEMA_STORAGE_ENGINE
      if (PSI_server)
        PSI_server->set_thread(m_saved_psi);
#endif /* WITH_PERFSCHEMA_STORAGE_ENGINE */
      /*
        Indirectly uses pthread_setspecific, which can only return
        ENOMEM or EINVAL. Since store_globals are using correct keys,
        the only alternative is out of memory.
      */
      return ER_OUTOFMEMORY;
    }
#endif /* EMBEDDED_LIBRARY */
    return 0;
  }

private:

  int setup_thread_globals(THD *thd) const {
    int error= 0;
    THD *original_thd= my_pthread_getspecific(THD*, THR_THD);
    MEM_ROOT* original_mem_root= my_pthread_getspecific(MEM_ROOT*, THR_MALLOC);
    if ((error= my_pthread_setspecific_ptr(THR_THD, thd)))
      goto exit0;
    if ((error= my_pthread_setspecific_ptr(THR_MALLOC, &thd->mem_root)))
      goto exit1;
    if ((error= set_mysys_var(thd->mysys_var)))
      goto exit2;
    goto exit0;
exit2:
    error= my_pthread_setspecific_ptr(THR_MALLOC,  original_mem_root);
exit1:
    error= my_pthread_setspecific_ptr(THR_THD,  original_thd);
exit0:
    return error;
  }

  THD *m_original_thd;
  PSI_thread *m_saved_psi;
};


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
                    ulong *ptr_binlog_cache_disk_use_arg)
  : m_pending(0), saved_max_binlog_cache_size(max_binlog_cache_size_arg),
    ptr_binlog_cache_use(ptr_binlog_cache_use_arg),
    ptr_binlog_cache_disk_use(ptr_binlog_cache_disk_use_arg)
  {
    reset();
    flags.transactional= trx_cache_arg;
    cache_log.end_of_file= saved_max_binlog_cache_size;
  }

  int finalize(THD *thd, Log_event *end_event);
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

  bool is_group_cache_empty() const
  {
    return group_cache.is_empty();
  }

#ifndef DBUG_OFF
  bool dbug_is_finalized() const {
    return flags.finalized;
  }
#endif

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

  virtual void reset()
  {
    compute_statistics();
    truncate(0);
    flags.incident= false;
    flags.with_xid= false;
    flags.immediate= false;
    flags.finalized= false;
    /*
      The truncate function calls reinit_io_cache that calls my_b_flush_io_cache
      which may increase disk_writes. This breaks the disk_writes use by the
      binary log which aims to compute the ratio between in-memory cache usage
      and disk cache usage. To avoid this undesirable behavior, we reset the
      variable after truncating the cache.
    */
    cache_log.disk_writes= 0;
    group_cache.clear();
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
    The group cache for this cache.
  */
  Group_cache group_cache;

protected:
  /*
    It truncates the cache to a certain position. This includes deleting the
    pending event.
   */
  void truncate(my_off_t pos)
  {
    DBUG_PRINT("info", ("truncating to position %lu", (ulong) pos));
    remove_pending_event();
    reinit_io_cache(&cache_log, WRITE_CACHE, pos, 0, 0);
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
      statistic_increment(*ptr_binlog_cache_use, &LOCK_status);
      if (cache_log.disk_writes != 0)
        statistic_increment(*ptr_binlog_cache_disk_use, &LOCK_status);
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
                        ulong *ptr_binlog_cache_disk_use_arg)
    : binlog_cache_data(trx_cache_arg,
                        max_binlog_cache_size_arg,
                        ptr_binlog_cache_use_arg,
                        ptr_binlog_cache_disk_use_arg)
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
                        ulong *ptr_binlog_cache_disk_use_arg)
  : binlog_cache_data(trx_cache_arg,
                      max_binlog_cache_size_arg,
                      ptr_binlog_cache_use_arg,
                      ptr_binlog_cache_disk_use_arg),
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
    DBUG_PRINT("return", ("before_stmt_pos: %llu", (ulonglong) before_stmt_pos));
    DBUG_VOID_RETURN;
  }

  void restore_prev_position()
  {
    DBUG_ENTER("restore_prev_position");
    DBUG_PRINT("enter", ("before_stmt_pos: %llu", (ulonglong) before_stmt_pos));
    binlog_cache_data::truncate(before_stmt_pos);
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
                    ulong *ptr_binlog_cache_disk_use_arg)
  : stmt_cache(FALSE, max_binlog_stmt_cache_size_arg,
               ptr_binlog_stmt_cache_use_arg,
               ptr_binlog_stmt_cache_disk_use_arg),
    trx_cache(TRUE, max_binlog_cache_size_arg,
              ptr_binlog_cache_use_arg,
              ptr_binlog_cache_disk_use_arg)
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

#ifndef DBUG_OFF
  bool dbug_any_finalized() const {
    return stmt_cache.dbug_is_finalized() || trx_cache.dbug_is_finalized();
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
    DBUG_ASSERT(stmt_cache.has_xid() == 0 && trx_cache.has_xid() <= 1);
    if (int error= stmt_cache.flush(thd, &stmt_bytes, wrote_xid))
      return error;
    if (int error= trx_cache.flush(thd, &trx_bytes, wrote_xid))
      return error;
    *bytes_written= stmt_bytes + trx_bytes;
    return 0;
  }

  binlog_stmt_cache_data stmt_cache;
  binlog_trx_cache_data trx_cache;

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
      push_warning_printf(thd, Sql_condition::WARN_LEVEL_WARN,
                          ER_BINLOG_CACHE_SIZE_GREATER_THAN_MAX,
                          ER(ER_BINLOG_CACHE_SIZE_GREATER_THAN_MAX),
                          (ulong) binlog_cache_size,
                          (ulong) max_binlog_cache_size);
    }
    else
    {
      sql_print_warning(ER_DEFAULT(ER_BINLOG_CACHE_SIZE_GREATER_THAN_MAX),
                        (ulong) binlog_cache_size,
                        (ulong) max_binlog_cache_size);
    }
    binlog_cache_size= max_binlog_cache_size;
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
      push_warning_printf(thd, Sql_condition::WARN_LEVEL_WARN,
                          ER_BINLOG_STMT_CACHE_SIZE_GREATER_THAN_MAX,
                          ER(ER_BINLOG_STMT_CACHE_SIZE_GREATER_THAN_MAX),
                          (ulong) binlog_stmt_cache_size,
                          (ulong) max_binlog_stmt_cache_size);
    }
    else
    {
      sql_print_warning(ER_DEFAULT(ER_BINLOG_STMT_CACHE_SIZE_GREATER_THAN_MAX),
                        (ulong) binlog_stmt_cache_size,
                        (ulong) max_binlog_stmt_cache_size);
    }
    binlog_stmt_cache_size= max_binlog_stmt_cache_size;
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
  DBUG_VOID_RETURN;
}


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
  binlog_hton->commit= binlog_commit;
  binlog_hton->rollback= binlog_rollback;
  binlog_hton->prepare= binlog_prepare;
  binlog_hton->flags= HTON_NOT_USER_SELECTABLE | HTON_HIDDEN;
  return 0;
}

static int binlog_close_connection(handlerton *hton, THD *thd)
{
  DBUG_ENTER("binlog_close_connection");
  binlog_cache_mngr *const cache_mngr= thd_get_cache_mngr(thd);
  DBUG_ASSERT(cache_mngr->is_binlog_empty());
  DBUG_ASSERT(cache_mngr->trx_cache.is_group_cache_empty() &&
              cache_mngr->stmt_cache.is_group_cache_empty());
  DBUG_PRINT("debug", ("Set ha_data slot %d to 0x%llx", binlog_hton->slot, (ulonglong) NULL));
  thd_set_ha_data(thd, binlog_hton, NULL);
  cache_mngr->~binlog_cache_mngr();
  my_free(cache_mngr);
  DBUG_RETURN(0);
}

int binlog_cache_data::write_event(THD *thd, Log_event *ev)
{
  DBUG_ENTER("binlog_cache_data::write_event");

  if (gtid_mode > 0)
  {
    Group_cache::enum_add_group_status status= 
      group_cache.add_logged_group(thd, get_byte_position());
    if (status == Group_cache::ERROR)
      DBUG_RETURN(1);
    else if (status == Group_cache::APPEND_NEW_GROUP)
    {
      Gtid_log_event gtid_ev(thd, is_trx_cache());
      if (gtid_ev.write(&cache_log) != 0)
        DBUG_RETURN(1);
    }
  }

  if (ev != NULL)
  {
    DBUG_EXECUTE_IF("simulate_disk_full_at_flush_pending",
                  {DBUG_SET("+d,simulate_file_write_error");});
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
      DBUG_RETURN(1);
    }
    if (ev->get_type_code() == XID_EVENT)
      flags.with_xid= true;
    if (ev->is_using_immediate_logging())
      flags.immediate= true;
  }
  DBUG_RETURN(0);
}


/**
  Checks if the given GTID exists in the Group_cache. If not, add it
  as an empty group.

  @todo Move this function into the cache class?

  @param thd THD object that owns the Group_cache
  @param cache_data binlog_cache_data object for the cache
  @param gtid GTID to check
*/
static int write_one_empty_group_to_cache(THD *thd,
                                          binlog_cache_data *cache_data,
                                          Gtid gtid)
{
  DBUG_ENTER("write_one_empty_group_to_cache");
  Group_cache *group_cache= &cache_data->group_cache;
  if (group_cache->contains_gtid(gtid))
    DBUG_RETURN(0);
  /*
    Apparently this code is not being called. We need to
    investigate if this is a bug or this code is not
    necessary. /Alfranio

    Empty groups are currently being handled in the function
    gtid_empty_group_log_and_cleanup().
  */
  DBUG_ASSERT(0); /*NOTREACHED*/
#ifdef NON_ERROR_GTID
  IO_CACHE *cache= &cache_data->cache_log;
  Group_cache::enum_add_group_status status= group_cache->add_empty_group(gtid);
  if (status == Group_cache::ERROR)
    DBUG_RETURN(1);
  DBUG_ASSERT(status == Group_cache::APPEND_NEW_GROUP);
  Gtid_specification spec= { GTID_GROUP, gtid };
  Gtid_log_event gtid_ev(thd, cache_data->is_trx_cache(), &spec);
  if (gtid_ev.write(cache) != 0)
    DBUG_RETURN(1);
#endif
  DBUG_RETURN(0);
}

/**
  Writes all GTIDs that the thread owns to the stmt/trx cache, if the
  GTID is not already in the cache.

  @todo Move this function into the cache class?

  @param thd THD object for the thread that owns the cache.
  @param cache_data The cache.
*/
static int write_empty_groups_to_cache(THD *thd, binlog_cache_data *cache_data)
{
  DBUG_ENTER("write_empty_groups_to_cache");
  if (thd->owned_gtid.sidno == -1)
  {
#ifdef HAVE_NDB_BINLOG
    Gtid_set::Gtid_iterator git(&thd->owned_gtid_set);
    Gtid gtid= git.get();
    while (gtid.sidno != 0)
    {
      if (write_one_empty_group_to_cache(thd, cache_data, gtid) != 0)
        DBUG_RETURN(1);
      git.next();
      gtid= git.get();
    }
#else
    DBUG_ASSERT(0);
#endif
  }
  else if (thd->owned_gtid.sidno > 0)
    if (write_one_empty_group_to_cache(thd, cache_data, thd->owned_gtid) != 0)
      DBUG_RETURN(1);
  DBUG_RETURN(0);
}


/**

  @todo Move this function into the cache class?
 */
static int
gtid_before_write_cache(THD* thd, binlog_cache_data* cache_data)
{
  DBUG_ENTER("gtid_before_write_cache");
  int error= 0;

  DBUG_ASSERT(thd->variables.gtid_next.type != UNDEFINED_GROUP);

  if (gtid_mode == 0)
    DBUG_RETURN(0);

  Group_cache* group_cache= &cache_data->group_cache;

  global_sid_lock->rdlock();

  if (thd->variables.gtid_next.type == AUTOMATIC_GROUP)
  {
    if (group_cache->generate_automatic_gno(thd) !=
        RETURN_STATUS_OK)
    {
      global_sid_lock->unlock();
      DBUG_RETURN(1); 
    }
  }
  if (write_empty_groups_to_cache(thd, cache_data) != 0)
  {
    global_sid_lock->unlock();
    DBUG_RETURN(1);
  }

  global_sid_lock->unlock();

  /*
    If an automatic group number was generated, change the first event
    into a "real" one.
  */
  if (thd->variables.gtid_next.type == AUTOMATIC_GROUP)
  {
    DBUG_ASSERT(group_cache->get_n_groups() == 1);
    Cached_group *cached_group= group_cache->get_unsafe_pointer(0);
    DBUG_ASSERT(cached_group->spec.type != AUTOMATIC_GROUP);
    Gtid_log_event gtid_ev(thd, cache_data->is_trx_cache(),
                           &cached_group->spec);
    bool using_file= cache_data->cache_log.pos_in_file > 0;
    my_off_t saved_position= cache_data->reset_write_pos(0, using_file);
    error= gtid_ev.write(&cache_data->cache_log);
    cache_data->reset_write_pos(saved_position, using_file);
  }

  DBUG_RETURN(error);
}

/**
   The function logs an empty group with GTID and performs cleanup.
   Its logic wrt GTID is equivalent to one of binlog_commit(). 
   It's called at the end of statement execution in case binlog_commit()
   was skipped.
   Such cases are due ineffective binlogging incl an empty group
   re-execution.

   @param thd   The thread handle

   @return
    nonzero if an error pops up.
*/
int gtid_empty_group_log_and_cleanup(THD *thd)
{
  int ret= 1;
  binlog_cache_data* cache_data= NULL;
  
  DBUG_ENTER("gtid_empty_group_log_and_cleanup");

  Query_log_event qinfo(thd, STRING_WITH_LEN("BEGIN"), TRUE,
                          FALSE, TRUE, 0, TRUE);
  DBUG_ASSERT(!qinfo.is_using_immediate_logging());

  /*
    thd->cache_mngr is uninitialized on the first empty transaction.
  */
  if (thd->binlog_setup_trx_data())
    DBUG_RETURN(1);
  cache_data= &thd_get_cache_mngr(thd)->trx_cache;
  DBUG_PRINT("debug", ("Writing to trx_cache"));
  if (cache_data->write_event(thd, &qinfo) ||
      gtid_before_write_cache(thd, cache_data))
    goto err;

  ret= mysql_bin_log.commit(thd, true);

err:
  DBUG_RETURN(ret);
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
    DBUG_PRINT("debug", ("bytes_in_cache: %llu", bytes_in_cache));
    /*
      The cache is always reset since subsequent rollbacks of the
      transactions might trigger attempts to write to the binary log
      if the cache is not reset.
     */
    if (!(error= gtid_before_write_cache(thd, this)))
      error= mysql_bin_log.write_cache(thd, this);

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
      error= mysql_bin_log.write_incident(thd, true/*need_lock_log=true*/);
    reset();
  }
  /*
    If rolling back a statement in a transaction, we truncate the
    transaction cache to remove the statement.
  */
  else if (get_prev_position() != MY_OFF_T_UNDEF)
  {
    restore_prev_position();
    if (is_binlog_empty())
    {
      /*
        After restoring the previous position, we need to check if
        the cache is empty. In such case, the group cache needs to
        be cleaned up too because the GTID is removed too from the
        cache.

        So if any change happens again, the GTID must be rewritten
        and this will not happen if the group cache is not cleaned
        up.

        After integrating this with NDB, we need to check if the
        current approach is enough or the group cache needs to
        explicitly support rollback to savepoints.
      */
      group_cache.clear();
    }
  }

  thd->clear_binlog_table_maps();

  DBUG_RETURN(error);
}

static int binlog_prepare(handlerton *hton, THD *thd, bool all)
{
  /*
    do nothing.
    just pretend we can do 2pc, so that MySQL won't
    switch to 1pc.
    real work will be done in MYSQL_BIN_LOG::commit()
  */
  return 0;
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
    first= first->next_to_commit;
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

  /*
    The stage mutex can be NULL if we are enrolling for the first
    stage.
  */
  if (stage_mutex)
    mysql_mutex_unlock(stage_mutex);

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
    thd->transaction.flags.ready_preempt= 1;
    if (leader_await_preempt_status)
      mysql_cond_signal(&m_cond_preempt);
#endif
    while (thd->transaction.flags.pending)
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
  DBUG_ASSERT(m_first || m_last == &m_first);
  DBUG_PRINT("return", ("result: 0x%llx", (ulonglong) result));
  unlock();
  DBUG_RETURN(result);
}

#ifndef DBUG_OFF
void Stage_manager::clear_preempt_status(THD *head)
{
  DBUG_ASSERT(head);

  mysql_mutex_lock(&m_lock_done);
  while(!head->transaction.flags.ready_preempt)
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

  binlog_cache_mngr *const cache_mngr= thd_get_cache_mngr(thd);
  DBUG_ENTER("MYSQL_BIN_LOG::rollback(THD *thd, bool all)");
  DBUG_PRINT("enter", ("all: %s, cache_mngr: 0x%llx, thd->is_error: %s",
                       YESNO(all), (ulonglong) cache_mngr, YESNO(thd->is_error())));

  /*
    We roll back the transaction in the engines early since this will
    release locks and allow other transactions to start executing.

    If we are executing a ROLLBACK TO SAVEPOINT, we should only clear
    the caches since this function is called as part of the engine
    rollback.
   */
  if (thd->lex->sql_command != SQLCOM_ROLLBACK_TO_SAVEPOINT)
    if (int error= ha_rollback_low(thd, all))
      DBUG_RETURN(error);

  /*
    If there is no cache manager, or if there is nothing in the
    caches, there are no caches to roll back, so we're trivially done.
   */
  if (cache_mngr == NULL || cache_mngr->is_binlog_empty())
    DBUG_RETURN(0);

  DBUG_PRINT("debug",
             ("all.cannot_safely_rollback(): %s, trx_cache_empty: %s",
              YESNO(thd->transaction.all.cannot_safely_rollback()),
              YESNO(cache_mngr->trx_cache.is_binlog_empty())));
  DBUG_PRINT("debug",
             ("stmt.cannot_safely_rollback(): %s, stmt_cache_empty: %s",
              YESNO(thd->transaction.stmt.cannot_safely_rollback()),
              YESNO(cache_mngr->stmt_cache.is_binlog_empty())));

  /*
    If an incident event is set we do not flush the content of the statement
    cache because it may be corrupted.
  */
  if (cache_mngr->stmt_cache.has_incident())
  {
    error= write_incident(thd, true/*need_lock_log=true*/);
    cache_mngr->stmt_cache.reset();
  }
  else if (!cache_mngr->stmt_cache.is_binlog_empty())
  {
    if (int error= cache_mngr->stmt_cache.finalize(thd))
      DBUG_RETURN(error);
    stuff_logged= true;
  }

  if (ending_trans(thd, all))
  {
    if (trans_cannot_safely_rollback(thd))
    {
      /*
        If the transaction is being rolled back and contains changes that
        cannot be rolled back, the trx-cache's content is flushed.
      */
      Query_log_event
        end_evt(thd, STRING_WITH_LEN("ROLLBACK"), true, false, true, 0, true);
      error= cache_mngr->trx_cache.finalize(thd, &end_evt);
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
    if (thd->transaction.stmt.has_dropped_temp_table() ||
        thd->transaction.stmt.has_created_temp_table() ||
        (thd->transaction.stmt.has_modified_non_trans_table() &&
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

  DBUG_PRINT("debug", ("error: %d", error));
  if (error == 0 && stuff_logged)
    error= ordered_commit(thd, all, /* skip_commit */ true);

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
    error |= cache_mngr->trx_cache.truncate(thd, all);
    DBUG_RETURN(error);
  }

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
  if (cache_mngr->trx_cache.is_binlog_empty())
    cache_mngr->trx_cache.group_cache.clear();
  DBUG_RETURN(0);
}

#ifdef HAVE_REPLICATION

/*
  Adjust the position pointer in the binary log file for all running slaves

  SYNOPSIS
    adjust_linfo_offsets()
    purge_offset	Number of bytes removed from start of log index file

  NOTES
    - This is called when doing a PURGE when we delete lines from the
      index log file

  REQUIREMENTS
    - Before calling this function, we have to ensure that no threads are
      using any binary log file before purge_offset.a

  TODO
    - Inform the slave threads that they should sync the position
      in the binary log file with flush_relay_log_info.
      Now they sync is done for next read.
*/

static void adjust_linfo_offsets(my_off_t purge_offset)
{
  mysql_mutex_lock(&LOCK_thread_count);

  Thread_iterator it= global_thread_list_begin();
  Thread_iterator end= global_thread_list_end();
  for (; it != end; ++it)
  {
    LOG_INFO* linfo;
    if ((linfo = (*it)->current_linfo))
    {
      mysql_mutex_lock(&linfo->lock);
      /*
	Index file offset can be less that purge offset only if
	we just started reading the index file. In that case
	we have nothing to adjust
      */
      if (linfo->index_file_offset < purge_offset)
	linfo->fatal = (linfo->index_file_offset != 0);
      else
	linfo->index_file_offset -= purge_offset;
      mysql_mutex_unlock(&linfo->lock);
    }
  }
  mysql_mutex_unlock(&LOCK_thread_count);
}


static bool log_in_use(const char* log_name)
{
  size_t log_name_len = strlen(log_name) + 1;
  bool result = 0;

  mysql_mutex_lock(&LOCK_thread_count);

  Thread_iterator it= global_thread_list_begin();
  Thread_iterator end= global_thread_list_end();
  for (; it != end; ++it)
  {
    LOG_INFO* linfo;
    if ((linfo = (*it)->current_linfo))
    {
      mysql_mutex_lock(&linfo->lock);
      result = !memcmp(log_name, linfo->log_file_name, log_name_len);
      mysql_mutex_unlock(&linfo->lock);
      if (result)
	break;
    }
  }

  mysql_mutex_unlock(&LOCK_thread_count);
  return result;
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
    sql_print_error("%s, errno=%d, io cache code=%d", *errmsg, my_errno,
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
                    log_file_name, my_errno);
    *errmsg = "Could not open log file";
    goto err;
  }
  if (init_io_cache(log, file, IO_SIZE*2, READ_CACHE, 0, 0,
                    MYF(MY_WME|MY_DONT_CHECK_FILESIZE)))
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

  @param thd The client thread that executed the current statement.
  @return
    @c true if a transactional table was updated, @c false otherwise.
*/
bool
stmt_has_updated_trans_table(const THD *thd)
{
  Ha_trx_info *ha_info;

  for (ha_info= thd->transaction.stmt.ha_list; ha_info;
       ha_info= ha_info->next())
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
  return thd->transaction.stmt.cannot_safely_rollback();
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
                                                      NULL));
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
                             mysql_bin_log.purge_logs_before_date(purge_time));
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
    error= thd->is_error() ? thd->get_stmt_da()->sql_errno() : 0;

    /* thd->get_stmt_da()->sql_errno() might be ER_SERVER_SHUTDOWN or
       ER_QUERY_INTERRUPTED, So here we need to make sure that error
       is not set to these errors when specified not_killed by the
       caller.
    */
    if (error == ER_SERVER_SHUTDOWN || error == ER_QUERY_INTERRUPTED)
      error= 0;
  }
  else
  {
    /* killed status for DELAYED INSERT thread should never be used */
    DBUG_ASSERT(!(thd->system_thread & SYSTEM_THREAD_DELAYED_INSERT));
    error= thd->killed_errno();
  }

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
      Append_block_log_event a(lf_info->thd, lf_info->thd->db, buffer,
                               min(block_len, max_event_size),
                               lf_info->log_delayed);
      if (mysql_bin_log.write_event(&a))
        DBUG_RETURN(1);
    }
    else
    {
      Begin_load_query_log_event b(lf_info->thd, lf_info->thd->db,
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
  Protocol *protocol= thd->protocol;
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
    SELECT_LEX_UNIT *unit= &thd->lex->unit;
    ha_rows event_count, limit_start, limit_end;
    my_off_t pos = max<my_off_t>(BIN_LOG_HEADER_SIZE, lex_mi->pos); // user-friendly
    char search_file_name[FN_REFLEN], *name;
    const char *log_file_name = lex_mi->log_file_name;
    mysql_mutex_t *log_lock = binary_log->get_log_lock();
    Log_event* ev;

    unit->set_limit(thd->lex->current_select);
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

    mysql_mutex_lock(&LOCK_thread_count);
    thd->current_linfo = &linfo;
    mysql_mutex_unlock(&LOCK_thread_count);

    if ((file=open_binlog_file(&log, linfo.log_file_name, &errmsg)) < 0)
      goto err;

    /*
      to account binlog event header size
    */
    thd->variables.max_allowed_packet += MAX_LOG_EVENT_HEADER;

    mysql_mutex_lock(log_lock);

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
      if (ev->get_type_code() == FORMAT_DESCRIPTION_EVENT)
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
      if (ev->get_type_code() == FORMAT_DESCRIPTION_EVENT)
        description_event->checksum_alg= ev->checksum_alg;

      if (event_count >= limit_start &&
	  ev->net_send(protocol, linfo.log_file_name, pos))
      {
	errmsg = "Net error";
	delete ev;
        mysql_mutex_unlock(log_lock);
	goto err;
      }

      pos = my_b_tell(&log);
      delete ev;

      if (++event_count >= limit_end)
	break;
    }

    if (event_count < limit_end && log.error)
    {
      errmsg = "Wrong offset or I/O error";
      mysql_mutex_unlock(log_lock);
      goto err;
    }

    mysql_mutex_unlock(log_lock);
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
    my_error(ER_ERROR_WHEN_EXECUTING_COMMAND, MYF(0),
             "SHOW BINLOG EVENTS", errmsg);
  else
    my_eof(thd);

  mysql_mutex_lock(&LOCK_thread_count);
  thd->current_linfo = 0;
  mysql_mutex_unlock(&LOCK_thread_count);
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
  Protocol *protocol= thd->protocol;
  List<Item> field_list;
  DBUG_ENTER("mysql_show_binlog_events");

  DBUG_ASSERT(thd->lex->sql_command == SQLCOM_SHOW_BINLOG_EVENTS);

  Log_event::init_show_field_list(&field_list);
  if (protocol->send_result_set_metadata(&field_list,
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


MYSQL_BIN_LOG::MYSQL_BIN_LOG(uint *sync_period)
  :bytes_written(0), file_id(1), open_count(1),
   sync_period_ptr(sync_period), sync_counter(0),
   m_prep_xids(0),
   is_relay_log(0), signal_cnt(0),
   checksum_alg_reset(BINLOG_CHECKSUM_ALG_UNDEF),
   relay_log_checksum_alg(BINLOG_CHECKSUM_ALG_UNDEF),
   previous_gtid_set(0)
{
  /*
    We don't want to initialize locks here as such initialization depends on
    safe_mutex (when using safe_mutex) which depends on MY_INIT(), which is
    called only in main(). Doing initialization here would make it happen
    before main().
  */
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
    close(LOG_CLOSE_INDEX|LOG_CLOSE_STOP_EVENT);
    mysql_mutex_destroy(&LOCK_log);
    mysql_mutex_destroy(&LOCK_index);
    mysql_mutex_destroy(&LOCK_commit);
    mysql_mutex_destroy(&LOCK_sync);
    mysql_cond_destroy(&update_cond);
    my_atomic_rwlock_destroy(&m_prep_xids_lock);
    mysql_cond_destroy(&m_prep_xids_cond);
  }
  DBUG_VOID_RETURN;
}


void MYSQL_BIN_LOG::init_pthread_objects()
{
  MYSQL_LOG::init_pthread_objects();
  mysql_mutex_init(m_key_LOCK_index, &LOCK_index, MY_MUTEX_INIT_SLOW);
  mysql_mutex_init(m_key_LOCK_commit, &LOCK_commit, MY_MUTEX_INIT_FAST);
  mysql_mutex_init(m_key_LOCK_sync, &LOCK_sync, MY_MUTEX_INIT_FAST);
  mysql_cond_init(m_key_update_cond, &update_cond, 0);
  my_atomic_rwlock_init(&m_prep_xids_lock);
  mysql_cond_init(m_key_prep_xids_cond, &m_prep_xids_cond, NULL);
  stage_manager.init(
#ifdef HAVE_PSI_INTERFACE
                   m_key_LOCK_flush_queue,
                   m_key_LOCK_sync_queue,
                   m_key_LOCK_commit_queue,
                   m_key_LOCK_done, m_key_COND_done
#endif
                   );
}

bool MYSQL_BIN_LOG::open_index_file(const char *index_file_name_arg,
                                    const char *log_name, bool need_lock_index)
{
  File index_file_nr= -1;
  DBUG_ASSERT(!my_b_inited(&index_file));

  /*
    First open of this class instance
    Create an index file that will hold all file names uses for logging.
    Add new entries to the end of it.
  */
  myf opt= MY_UNPACK_FILENAME;
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
    return TRUE;
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
    return TRUE;
  }

  if ((index_file_nr= mysql_file_open(m_key_file_log_index,
                                      index_file_name,
                                      O_RDWR | O_CREAT | O_BINARY,
                                      MYF(MY_WME))) < 0 ||
       mysql_file_sync(index_file_nr, MYF(MY_WME)) ||
       init_io_cache(&index_file, index_file_nr,
                     IO_SIZE, READ_CACHE,
                     mysql_file_seek(index_file_nr, 0L, MY_SEEK_END, MYF(0)),
                                     0, MYF(MY_WME | MY_WAIT_IF_FULL)) ||
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
    return TRUE;
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
      purge_index_entry(NULL, NULL, need_lock_index) ||
      close_purge_index_file() ||
      DBUG_EVALUATE_IF("fault_injection_recovering_index", 1, 0))
  {
    sql_print_error("MYSQL_BIN_LOG::open_index_file failed to sync the index "
                    "file.");
    return TRUE;
  }
#endif

  return FALSE;
}


/**
  Reads GTIDs from the given binlog file.

  @param filename File to read from.
  @param all_gtids If not NULL, then the GTIDs from the
  Previous_gtids_log_event and from all Gtid_log_events are stored in
  this object.
  @param prev_gtids If not NULL, then the GTIDs from the
  Previous_gtids_log_events are stored in this object.
  @param verify_checksum Set to true to verify event checksums.

  @retval GOT_GTIDS The file was successfully read and it contains
  both Gtid_log_events and Previous_gtids_log_events.
  @retval GOT_PREVIOUS_GTIDS The file was successfully read and it
  contains Previous_gtids_log_events but no Gtid_log_events.
  @retval NO_GTIDS The file was successfully read and it does not
  contain GTID events.
  @retval ERROR Out of memory, or the file contains GTID events
  when GTID_MODE = OFF, or the file is malformed (e.g., contains
  Gtid_log_events but no Previous_gtids_log_event).
  @retval TRUNCATED The file was truncated before the end of the
  first Previous_gtids_log_event.
*/
enum enum_read_gtids_from_binlog_status
{ GOT_GTIDS, GOT_PREVIOUS_GTIDS, NO_GTIDS, ERROR, TRUNCATED };
static enum_read_gtids_from_binlog_status
read_gtids_from_binlog(const char *filename, Gtid_set *all_gtids,
                       Gtid_set *prev_gtids, bool verify_checksum)
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
  while (!done &&
         (ev= Log_event::read_log_event(&log, 0, fd_ev_p, verify_checksum)) !=
         NULL)
  {
    DBUG_PRINT("info", ("Read event of type %s", ev->get_type_str()));
    switch (ev->get_type_code())
    {
    case FORMAT_DESCRIPTION_EVENT:
      if (fd_ev_p != &fd_ev)
        delete fd_ev_p;
      fd_ev_p= (Format_description_log_event *)ev;
      break;
    case ROTATE_EVENT:
      // do nothing; just accept this event and go to next
      break;
    case PREVIOUS_GTIDS_LOG_EVENT:
    {
      if (gtid_mode == 0)
      {
        my_error(ER_FOUND_GTID_EVENT_WHEN_GTID_MODE_IS_OFF, MYF(0));
        ret= ERROR;
      }
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
      break;
    }
    case GTID_LOG_EVENT:
    {
      if (ret != GOT_GTIDS)
      {
        if (ret != GOT_PREVIOUS_GTIDS)
          // should not happen
          my_error(ER_MASTER_FATAL_ERROR_READING_BINLOG, MYF(0));
        else
          ret= GOT_GTIDS;
      }
      Gtid_log_event *gtid_ev= (Gtid_log_event *)ev;
      rpl_sidno sidno= gtid_ev->get_sidno(false/*false=don't need lock*/);
      if (sidno < 0)
        ret= ERROR, done= true;
      /*
        We need to read GTID_LOG_EVENT when all_gtids == NULL to allow
        read both PREVIOUS_GTIDS_LOG_EVENT and GTID_LOG_EVENT events.
      */
      else if (NULL != all_gtids)
      {
        if (all_gtids->ensure_sidno(sidno) != RETURN_STATUS_OK)
          ret= ERROR, done= true;
        else if (all_gtids->_add_gtid(sidno, gtid_ev->get_gno()) !=
                 RETURN_STATUS_OK)
          ret= ERROR, done= true;
      }
      DBUG_PRINT("info", ("Got Gtid from file '%s': Gtid(%d, %lld).",
                          filename, sidno, gtid_ev->get_gno()));
      break;
    }
    case ANONYMOUS_GTID_LOG_EVENT:
    default:
      // if we found any other event type without finding a
      // previous_gtids_log_event, then the rest of this binlog
      // cannot contain gtids
      if (ret != GOT_GTIDS && ret != GOT_PREVIOUS_GTIDS)
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

  DBUG_PRINT("info", ("returning %d", ret));
  DBUG_RETURN(ret);
}


bool MYSQL_BIN_LOG::init_gtid_sets(Gtid_set *all_gtids, Gtid_set *lost_gtids,
                                   bool verify_checksum, bool need_lock)
{
  DBUG_ENTER("MYSQL_BIN_LOG::init_gtid_sets");
  DBUG_PRINT("info", ("lost_gtids=%p; so we are recovering a %s log",
                      lost_gtids, lost_gtids == NULL ? "relay" : "binary"));

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

  for (error= find_log_pos(&linfo, NULL, false/*need_lock_index=false*/); !error;
       error= find_next_log(&linfo, false/*need_lock_index=false*/))
  {
    DBUG_PRINT("info", ("read log filename '%s'", linfo.log_file_name));
    filename_list.push_back(string(linfo.log_file_name));
  }
  if (error != LOG_INFO_EOF)
  {
    DBUG_PRINT("error", ("Error reading binlog index"));
    goto end;
  }
  error= 0;

  if (all_gtids != NULL)
  {
    DBUG_PRINT("info", ("Iterating backwards through binary logs, looking for the last binary log that contains a Previous_gtids_log_event."));
    // Iterate over all files in reverse order until we find one that
    // contains a Previous_gtids_log_event.
    rit= filename_list.rbegin();
    bool got_gtids= false;
    reached_first_file= (rit == filename_list.rend());
    DBUG_PRINT("info", ("filename='%s' reached_first_file=%d",
                        rit->c_str(), reached_first_file));
    while (!got_gtids && !reached_first_file)
    {
      const char *filename= rit->c_str();
      rit++;
      reached_first_file= (rit == filename_list.rend());
      DBUG_PRINT("info", ("filename='%s' got_gtids=%d reached_first_file=%d",
                          filename, got_gtids, reached_first_file));
      switch (read_gtids_from_binlog(filename, all_gtids,
                                     reached_first_file ? lost_gtids : NULL,
                                     verify_checksum))
      {
      case ERROR:
        error= 1;
        goto end;
      case GOT_GTIDS:
      case GOT_PREVIOUS_GTIDS:
        got_gtids= true;
        /*FALLTHROUGH*/
      case NO_GTIDS:
      case TRUNCATED:
        break;
      }
    }
  }
  if (lost_gtids != NULL && !reached_first_file)
  {
    DBUG_PRINT("info", ("Iterating forwards through binary logs, looking for the first binary log that contains a Previous_gtids_log_event."));
    for (it= filename_list.begin(); it != filename_list.end(); it++)
    {
      const char *filename= it->c_str();
      DBUG_PRINT("info", ("filename='%s'", filename));
      switch (read_gtids_from_binlog(filename, NULL, lost_gtids,
                                     verify_checksum))
      {
      case ERROR:
        error= 1;
        /*FALLTHROUGH*/
      case GOT_GTIDS:
        goto end;
      case GOT_PREVIOUS_GTIDS:
      case NO_GTIDS:
      case TRUNCATED:
        break;
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
                                enum cache_type io_cache_type_arg,
                                ulong max_size_arg,
                                bool null_created_arg,
                                bool need_lock_index,
                                bool need_sid_lock,
                                Format_description_log_event *extra_description_event)
{
  File file= -1;

  // lock_index must be acquired *before* sid_lock.
  DBUG_ASSERT(need_sid_lock || !need_lock_index);
  DBUG_ENTER("MYSQL_BIN_LOG::open_binlog(const char *, ...)");
  DBUG_PRINT("enter",("name: %s", log_name));

  if (init_and_set_log_file_name(log_name, new_name, LOG_BIN,
                                 io_cache_type_arg))
  {
    sql_print_error("MYSQL_BIN_LOG::open failed to generate new file name.");
    DBUG_RETURN(1);
  }

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

      Perhaps we might need the code below in MYSQL_LOG_BIN::cleanup
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
  if (MYSQL_LOG::open(
#ifdef HAVE_PSI_INTERFACE
                      m_key_file_log,
#endif
                      log_name, LOG_BIN, new_name, io_cache_type_arg))
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
    s.flags |= LOG_EVENT_BINLOG_IN_USE_F;
  s.checksum_alg= is_relay_log ?
    /* relay-log */
    /* inherit master's A descriptor if one has been received */
    (relay_log_checksum_alg=
     (relay_log_checksum_alg != BINLOG_CHECKSUM_ALG_UNDEF) ?
     relay_log_checksum_alg :
     /* otherwise use slave's local preference of RL events verification */
     (opt_slave_sql_verify_checksum == 0) ?
     (uint8) BINLOG_CHECKSUM_ALG_OFF : binlog_checksum_options):
    /* binlog */
    binlog_checksum_options;
  DBUG_ASSERT(s.checksum_alg != BINLOG_CHECKSUM_ALG_UNDEF);
  if (!s.is_valid())
    goto err;
  s.dont_set_created= null_created_arg;
  /* Set LOG_EVENT_RELAY_LOG_F flag for relay log's FD */
  if (is_relay_log)
    s.set_relay_log_event();
  if (s.write(&log_file))
    goto err;
  bytes_written+= s.data_written;
  /*
    We need to revisit this code and improve it.
    See further comments in the mysqld.
    /Alfranio
  */
  if (current_thd && gtid_mode > 0)
  {
    if (need_sid_lock)
      global_sid_lock->wrlock();
    else
      global_sid_lock->assert_some_wrlock();
    Previous_gtids_log_event prev_gtids_ev(previous_gtid_set);
    if (need_sid_lock)
      global_sid_lock->unlock();
    prev_gtids_ev.checksum_alg= s.checksum_alg;
    if (prev_gtids_ev.write(&log_file))
      goto err;
    bytes_written+= prev_gtids_ev.data_written;
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
    bytes_written+= extra_description_event->data_written;
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
    if (DBUG_EVALUATE_IF("fault_injection_updating_index", 1, 0) ||
        add_log_to_index((uchar*) log_file_name, strlen(log_file_name),
                         need_lock_index))
      goto err;

#ifdef HAVE_REPLICATION
    DBUG_EXECUTE_IF("crash_create_after_update_index", DBUG_SUICIDE(););
#endif
  }

  log_state= LOG_OPENED;

#ifdef HAVE_REPLICATION
  close_purge_index_file();
#endif

  DBUG_RETURN(0);

err:
#ifdef HAVE_REPLICATION
  if (is_inited_purge_index_file())
    purge_index_entry(NULL, NULL, need_lock_index);
  close_purge_index_file();
#endif
  sql_print_error("Could not use %s for logging (error %d). \
Turning logging off for the whole duration of the MySQL server process. \
To turn it on again: fix the cause, \
shutdown the MySQL server and restart it.", name, errno);
  if (file >= 0)
    mysql_file_close(file, MYF(0));
  end_io_cache(&log_file);
  end_io_cache(&index_file);
  my_free(name);
  name= NULL;
  log_state= LOG_CLOSED;
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
      sql_print_error("MYSQL_BIN_LOG::move_crash_safe_index_file_to_index_file "
                      "failed to close the index file.");
      goto err;
    }
    mysql_file_delete(key_file_binlog_index, index_file_name, MYF(MY_WME));
  }

  DBUG_EXECUTE_IF("crash_create_before_rename_index_file", DBUG_SUICIDE(););
  if (my_rename(crash_safe_index_file_name, index_file_name, MYF(MY_WME)))
  {
    error= -1;
    sql_print_error("MYSQL_BIN_LOG::move_crash_safe_index_file_to_index_file "
                    "failed to move crash_safe_index_file to index file.");
    goto err;
  }
  DBUG_EXECUTE_IF("crash_create_after_rename_index_file", DBUG_SUICIDE(););

  if ((fd= mysql_file_open(key_file_binlog_index,
                           index_file_name,
                           O_RDWR | O_CREAT | O_BINARY,
                           MYF(MY_WME))) < 0 ||
           mysql_file_sync(fd, MYF(MY_WME)) ||
           init_io_cache(&index_file, fd, IO_SIZE, READ_CACHE,
                         mysql_file_seek(fd, 0L, MY_SEEK_END, MYF(0)),
                                         0, MYF(MY_WME | MY_WAIT_IF_FULL)))
  {
    error= -1;
    sql_print_error("MYSQL_BIN_LOG::move_crash_safe_index_file_to_index_file "
                    "failed to open the index file.");
    goto err;
  }

err:
  if (need_lock_index)
    mysql_mutex_unlock(&LOCK_index);
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
                                    int log_name_len, bool need_lock_index)
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

int MYSQL_BIN_LOG::get_current_log(LOG_INFO* linfo)
{
  mysql_mutex_lock(&LOCK_log);
  int ret = raw_get_current_log(linfo);
  mysql_mutex_unlock(&LOCK_log);
  return ret;
}

int MYSQL_BIN_LOG::raw_get_current_log(LOG_INFO* linfo)
{
  strmake(linfo->log_file_name, log_file_name, sizeof(linfo->log_file_name)-1);
  linfo->pos = my_b_tell(&log_file);
  return 0;
}

bool MYSQL_BIN_LOG::check_write_error(THD *thd)
{
  DBUG_ENTER("MYSQL_BIN_LOG::check_write_error");

  bool checked= FALSE;

  if (!thd->is_error())
    DBUG_RETURN(checked);

  switch (thd->get_stmt_da()->sql_errno())
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

  if (my_errno == EFBIG)
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
  uint log_name_len= 0, fname_len= 0;
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

  // extend relative paths for log_name to be searched
  if (log_name)
  {
    if(normalize_binlog_name(full_log_name, log_name, is_relay_log))
    {
      error= LOG_INFO_EOF;
      goto end;
    }
  }

  log_name_len= log_name ? (uint) strlen(full_log_name) : 0;
  DBUG_PRINT("enter", ("log_name: %s, full_log_name: %s", 
                       log_name ? log_name : "NULL", full_log_name));

  /* As the file is flushed, we can't get an error here */
  my_b_seek(&index_file, (my_off_t) 0);

  for (;;)
  {
    uint length;
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
    fname_len= (uint) strlen(full_fname);

    // if the log entry matches, null string matching anything
    if (!log_name ||
       (log_name_len == fname_len-1 && full_fname[log_name_len] == '\n' &&
        !memcmp(full_fname, full_log_name, log_name_len)))
    {
      DBUG_PRINT("info", ("Found log file entry"));
      full_fname[fname_len-1]= 0;                      // remove last \n
      linfo->index_file_start_offset= offset;
      linfo->index_file_offset = my_b_tell(&index_file);
      break;
    }
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
  uint length;
  char fname[FN_REFLEN];
  char *full_fname= linfo->log_file_name;

  if (need_lock_index)
    mysql_mutex_lock(&LOCK_index);
  else
    mysql_mutex_assert_owner(&LOCK_index);

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

  full_fname[length-1]= 0;                     // kill \n
  linfo->index_file_offset= my_b_tell(&index_file);

err:
  if (need_lock_index)
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
bool MYSQL_BIN_LOG::reset_logs(THD* thd)
{
  LOG_INFO linfo;
  bool error=0;
  int err;
  const char* save_name;
  DBUG_ENTER("reset_logs");

  ha_reset_logs(thd);

  /*
    The following mutex is needed to ensure that no threads call
    'delete thd' as we would then risk missing a 'rollback' from this
    thread. If the transaction involved MyISAM tables, it should go
    into binlog even on rollback.
  */
  mysql_mutex_lock(&LOCK_thread_count);

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
  close(LOG_CLOSE_TO_BE_OPENED);

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
      if (my_errno == ENOENT) 
      {
        push_warning_printf(current_thd, Sql_condition::WARN_LEVEL_WARN,
                            ER_LOG_PURGE_NO_FILE, ER(ER_LOG_PURGE_NO_FILE),
                            linfo.log_file_name);
        sql_print_information("Failed to delete file '%s'",
                              linfo.log_file_name);
        my_errno= 0;
        error= 0;
      }
      else
      {
        push_warning_printf(current_thd, Sql_condition::WARN_LEVEL_WARN,
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
  close(LOG_CLOSE_INDEX | LOG_CLOSE_TO_BE_OPENED);
  if ((error= my_delete_allow_opened(index_file_name, MYF(0))))	// Reset (open will update)
  {
    if (my_errno == ENOENT) 
    {
      push_warning_printf(current_thd, Sql_condition::WARN_LEVEL_WARN,
                          ER_LOG_PURGE_NO_FILE, ER(ER_LOG_PURGE_NO_FILE),
                          index_file_name);
      sql_print_information("Failed to delete file '%s'",
                            index_file_name);
      my_errno= 0;
      error= 0;
    }
    else
    {
      push_warning_printf(current_thd, Sql_condition::WARN_LEVEL_WARN,
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
  if (is_relay_log)
  {
    DBUG_ASSERT(active_mi != NULL);
    DBUG_ASSERT(active_mi->rli != NULL);
    (const_cast<Gtid_set *>(active_mi->rli->get_gtid_set()))->clear();
  }
  else
  {
    gtid_state->clear();
    // don't clear global_sid_map because it's used by the relay log too
    if (gtid_state->init() != 0)
      goto err;
  }
#endif

  if (!open_index_file(index_file_name, 0, false/*need_lock_index=false*/))
    if ((error= open_binlog(save_name, 0, io_cache_type,
                            max_size, false,
                            false/*need_lock_index=false*/,
                            false/*need_sid_lock=false*/,
                            NULL)))
      goto err;
  my_free((void *) save_name);

err:
  if (error == 1)
    name= const_cast<char*>(save_name);
  global_sid_lock->unlock();
  mysql_mutex_unlock(&LOCK_thread_count);
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
                       MYF(MY_WME | ME_WAITTANG))) < 0  ||
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
  to_purge_if_included= my_strdup(rli->get_group_relay_log_name(), MYF(0));

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

  /* Store where we are in the new file for the execution thread */
  rli->flush_info(TRUE);

  DBUG_EXECUTE_IF("crash_before_purge_logs", DBUG_SUICIDE(););

  mysql_mutex_lock(&rli->log_space_lock);
  rli->relay_log.purge_logs(to_purge_if_included, included,
                            false/*need_lock_index=false*/,
                            false/*need_update_threads=false*/,
                            &rli->log_space_total);
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
                              ulonglong *decrease_log_space)
{
  int error= 0;
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
  while ((strcmp(to_log,log_info.log_file_name) || (exit_loop=included)) &&
         !is_active(log_info.log_file_name) &&
         !log_in_use(log_info.log_file_name))
  {
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
  if (gtid_mode > 0 && !is_relay_log)
  {
    global_sid_lock->wrlock();
    if (init_gtid_sets(NULL,
                       const_cast<Gtid_set *>(gtid_state->get_lost_gtids()),
                       opt_master_verify_checksum,
                       false/*false=don't need lock*/))
      goto err;
    global_sid_lock->unlock();
  }

  DBUG_EXECUTE_IF("crash_purge_critical_after_update_index", DBUG_SUICIDE(););

err:
  /* Read each entry from purge_index_file and delete the file. */
  if (is_inited_purge_index_file() &&
      (error= purge_index_entry(thd, decrease_log_space, false/*need_lock_index=false*/)))
    sql_print_error("MYSQL_BIN_LOG::purge_logs failed to process registered files"
                    " that would be purged.");
  close_purge_index_file();

  DBUG_EXECUTE_IF("crash_purge_non_critical_after_update_index", DBUG_SUICIDE(););

  if (need_lock_index)
    mysql_mutex_unlock(&LOCK_index);
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
                       MYF(MY_WME | ME_WAITTANG))) < 0  ||
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
    uint length;

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
      if (my_errno == ENOENT) 
      {
        /*
          It's not fatal if we can't stat a log file that does not exist;
          If we could not stat, we won't delete.
        */
        if (thd)
        {
          push_warning_printf(thd, Sql_condition::WARN_LEVEL_WARN,
                              ER_LOG_PURGE_NO_FILE, ER(ER_LOG_PURGE_NO_FILE),
                              log_info.log_file_name);
        }
        sql_print_information("Failed to execute mysql_file_stat on file '%s'",
			      log_info.log_file_name);
        my_errno= 0;
      }
      else
      {
        /*
          Other than ENOENT are fatal
        */
        if (thd)
        {
          push_warning_printf(thd, Sql_condition::WARN_LEVEL_WARN,
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
            push_warning_printf(thd, Sql_condition::WARN_LEVEL_WARN,
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
        if (!my_delete(log_info.log_file_name, MYF(0)))
        {
          if (decrease_log_space)
            *decrease_log_space-= s.st_size;
        }
        else
        {
          if (my_errno == ENOENT)
          {
            if (thd)
            {
              push_warning_printf(thd, Sql_condition::WARN_LEVEL_WARN,
                                  ER_LOG_PURGE_NO_FILE, ER(ER_LOG_PURGE_NO_FILE),
                                  log_info.log_file_name);
            }
            sql_print_information("Failed to delete file '%s'",
                                  log_info.log_file_name);
            my_errno= 0;
          }
          else
          {
            if (thd)
            {
              push_warning_printf(thd, Sql_condition::WARN_LEVEL_WARN,
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
            if (my_errno == EMFILE)
            {
              DBUG_PRINT("info",
                         ("my_errno: %d, set ret = LOG_INFO_EMFILE", my_errno));
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

int MYSQL_BIN_LOG::purge_logs_before_date(time_t purge_time)
{
  int error;
  char to_log[FN_REFLEN];
  LOG_INFO log_info;
  MY_STAT stat_area;
  THD *thd= current_thd;
  
  DBUG_ENTER("purge_logs_before_date");

  mysql_mutex_lock(&LOCK_index);
  to_log[0]= 0;

  if ((error=find_log_pos(&log_info, NullS, false/*need_lock_index=false*/)))
    goto err;

  while (strcmp(log_file_name, log_info.log_file_name) &&
	 !is_active(log_info.log_file_name) &&
         !log_in_use(log_info.log_file_name))
  {
    if (!mysql_file_stat(m_key_file_log,
                         log_info.log_file_name, &stat_area, MYF(0)))
    {
      if (my_errno == ENOENT) 
      {
        /*
          It's not fatal if we can't stat a log file that does not exist.
        */
        my_errno= 0;
      }
      else
      {
        /*
          Other than ENOENT are fatal
        */
        if (thd)
        {
          push_warning_printf(thd, Sql_condition::WARN_LEVEL_WARN,
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
    else
    {
      if (stat_area.st_mtime < purge_time) 
        strmake(to_log, 
                log_info.log_file_name, 
                sizeof(log_info.log_file_name) - 1);
      else
        break;
    }
    if (find_next_log(&log_info, false/*need_lock_index=false*/))
      break;
  }

  error= (to_log[0] ? purge_logs(to_log, true,
                                 false/*need_lock_index=false*/,
                                 true/*need_update_threads=true*/,
                                 (ulonglong *) 0) : 0);

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
  uint dir_len = dirname_length(log_file_name); 
  if (dir_len >= FN_REFLEN)
    dir_len=FN_REFLEN-1;
  strnmov(buf, log_file_name, dir_len);
  strmake(buf+dir_len, log_ident, FN_REFLEN - dir_len -1);
}


/**
  Check if we are writing/reading to the given log file.
*/

bool MYSQL_BIN_LOG::is_active(const char *log_file_name_arg)
{
  return !strcmp(log_file_name, log_file_name_arg);
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
  int error= 0, close_on_error= FALSE;
  char new_name[FN_REFLEN], *new_name_ptr, *old_name, *file_to_open;

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
  mysql_mutex_lock(&LOCK_commit);
  /*
    We need to ensure that the number of prepared XIDs are 0.

    If m_prep_xids is not zero:
    - We release the LOCK_commit lock to allow sessions to commit,
      hence decrease m_prep_xids
    - We keep the LOCK_log to block new transactions from being
      written to the binary log.
   */
  while (get_prep_xids() > 0)
    mysql_cond_wait(&m_prep_xids_cond, &LOCK_commit);
  mysql_mutex_lock(&LOCK_index);

  if ((error= ha_flush_logs(0)))
    goto end;

  mysql_mutex_assert_owner(&LOCK_log);
  mysql_mutex_assert_owner(&LOCK_commit);
  mysql_mutex_assert_owner(&LOCK_index);

  /* Reuse old name if not binlog and not update log */
  new_name_ptr= name;

  /*
    If user hasn't specified an extension, generate a new log name
    We have to do this here and not in open as we want to store the
    new file name in the current binary log file.
  */
  if ((error= generate_new_name(new_name, name)))
    goto end;
  else
  {
    new_name_ptr=new_name;
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
      r.checksum_alg= relay_log_checksum_alg;
    DBUG_ASSERT(!is_relay_log || relay_log_checksum_alg != BINLOG_CHECKSUM_ALG_UNDEF);
    if(DBUG_EVALUATE_IF("fault_injection_new_file_rotate_event", (error=close_on_error=TRUE), FALSE) ||
       (error= r.write(&log_file)))
    {
      char errbuf[MYSYS_STRERROR_SIZE];
      DBUG_EXECUTE_IF("fault_injection_new_file_rotate_event", errno=2;);
      close_on_error= TRUE;
      my_printf_error(ER_ERROR_ON_WRITE, ER(ER_CANT_OPEN_FILE),
                      MYF(ME_FATALERROR), name,
                      errno, my_strerror(errbuf, sizeof(errbuf), errno));
      goto end;
    }
    bytes_written += r.data_written;
  }
  /*
    Update needs to be signalled even if there is no rotate event
    log rotation should give the waiting thread a signal to
    discover EOF and move on to the next log.
  */
  signal_update();

  old_name=name;
  name=0;				// Don't free name
  close(LOG_CLOSE_TO_BE_OPENED | LOG_CLOSE_INDEX);

  if (checksum_alg_reset != BINLOG_CHECKSUM_ALG_UNDEF)
  {
    DBUG_ASSERT(!is_relay_log);
    DBUG_ASSERT(binlog_checksum_options != checksum_alg_reset);
    binlog_checksum_options= checksum_alg_reset;
  }
  /*
     Note that at this point, log_state != LOG_CLOSED (important for is_open()).
  */

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
    error= open_binlog(old_name, new_name_ptr, io_cache_type,
                       max_size, true,
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
    close_on_error= TRUE;
  }
  my_free(old_name);

end:

  if (error && close_on_error /* rotate or reopen failed */)
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
    close(LOG_CLOSE_INDEX);
    sql_print_error("Could not open %s for logging (error %d). "
                    "Turning logging off for the whole duration "
                    "of the MySQL server process. To turn it on "
                    "again: fix the cause, shutdown the MySQL "
                    "server and restart it.", 
                    new_name_ptr, errno);
  }

  mysql_mutex_unlock(&LOCK_index);
  mysql_mutex_unlock(&LOCK_commit);
  if (need_lock_log)
    mysql_mutex_unlock(&LOCK_log);

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

  // Flush and sync
  bool error= false;
  if (flush_and_sync(0) == 0)
  {
    // If relay log is too big, rotate
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
    bytes_written+= ev->data_written;
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
    this will close all tables on the slave.
  */
  bool const end_stmt=
    thd->locked_tables_mode && thd->lex->requires_prelocking();
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
          Intvar_log_event e(thd,(uchar) LAST_INSERT_ID_EVENT,
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
          Intvar_log_event e(thd, (uchar) INSERT_ID_EVENT,
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
        if (thd->user_var_events.elements)
        {
          for (uint i= 0; i < thd->user_var_events.elements; i++)
          {
            BINLOG_USER_VAR_EVENT *user_var_event;
            get_dynamic(&thd->user_var_events,(uchar*) &user_var_event, i);

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
    if (cache_data->write_event(thd, event_info) ||
        DBUG_EVALUATE_IF("injecting_fault_writing", 1, 0))
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

  if (force_rotate || (my_b_tell(&log_file) >= (my_off_t) max_size))
  {
    if ((error= new_file_without_locking(NULL)))
      /** 
        Be conservative... There are possible lost events (eg, 
        failing to log the Execute_load_query_log_event
        on a LOAD DATA while using a non-transactional
        table)!

        We give it a shot and try to write an incident event anyway
        to the current log. 
      */
      if (!write_incident(current_thd, false/*need_lock_log=false*/,
                          false/*do_flush_and_sync==false*/))
        flush_and_sync(0);

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
    if (purge_time >= 0)
    {
      purge_logs_before_date(purge_time);
    }
  }
#endif
}

/**
  The method is a shortcut of @c rotate() and @c purge().
  LOCK_log is acquired prior to rotate and is released after it.

  @param force_rotate  caller can request the log rotation

  @retval
    nonzero - error in rotating routine.
*/
int MYSQL_BIN_LOG::rotate_and_purge(bool force_rotate)
{
  int error= 0;
  DBUG_ENTER("MYSQL_BIN_LOG::rotate_and_purge");
  bool check_purge= false;

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


/**
  Calculate checksum of possibly a part of an event containing at least
  the whole common header.

  @param    buf       the pointer to trans cache's buffer
  @param    off       the offset of the beginning of the event in the buffer
  @param    event_len no-checksum length of the event
  @param    length    the current size of the buffer

  @param    crc       [in-out] the checksum

  Event size in incremented by @c BINLOG_CHECKSUM_LEN.

  @return 0 or number of unprocessed yet bytes of the event excluding 
            the checksum part.
*/
  static ulong fix_log_event_crc(uchar *buf, uint off, uint event_len,
                                 uint length, ha_checksum *crc)
{
  ulong ret;
  uchar *event_begin= buf + off;
  uint16 flags= uint2korr(event_begin + FLAGS_OFFSET);

  DBUG_ASSERT(length >= off + LOG_EVENT_HEADER_LEN); //at least common header in
  int2store(event_begin + FLAGS_OFFSET, flags);
  ret= length >= off + event_len ? 0 : off + event_len - length;
  *crc= my_checksum(*crc, event_begin, event_len - ret); 
  return ret;
}

/*
  Write the contents of a cache to the binary log.

  SYNOPSIS
    do_write_cache()
    cache    Cache to write to the binary log
    lock_log True if the LOCK_log mutex should be aquired, false otherwise

  DESCRIPTION
    Write the contents of the cache to the binary log. The cache will
    be reset as a READ_CACHE to be able to read the contents from it.

    Reading from the trans cache with possible (per @c binlog_checksum_options) 
    adding checksum value  and then fixing the length and the end_log_pos of 
    events prior to fill in the binlog cache.
*/

int MYSQL_BIN_LOG::do_write_cache(IO_CACHE *cache)
{
  DBUG_ENTER("MYSQL_BIN_LOG::do_write_cache(IO_CACHE *)");

  DBUG_EXECUTE_IF("simulate_do_write_cache_failure",
                  {
                    /* 
                       see binlog_cache_data::write_event() that reacts on
                       @c simulate_disk_full_at_flush_pending.
                    */
                    DBUG_SET("-d,simulate_do_write_cache_failure");
                    DBUG_RETURN(ER_ERROR_ON_WRITE);
                  });

  if (reinit_io_cache(cache, READ_CACHE, 0, 0, 0))
    DBUG_RETURN(ER_ERROR_ON_WRITE);
  uint length= my_b_bytes_in_cache(cache), group, carry, hdr_offs;
  ulong remains= 0; // part of unprocessed yet netto length of the event
  long val;
  ulong end_log_pos_inc= 0; // each event processed adds BINLOG_CHECKSUM_LEN 2 t
  uchar header[LOG_EVENT_HEADER_LEN];
  ha_checksum crc= 0, crc_0= 0; // assignments to keep compiler happy
  my_bool do_checksum= (binlog_checksum_options != BINLOG_CHECKSUM_ALG_OFF);
  uchar buf[BINLOG_CHECKSUM_LEN];

  // while there is just one alg the following must hold:
  DBUG_ASSERT(!do_checksum ||
              binlog_checksum_options == BINLOG_CHECKSUM_ALG_CRC32);

  /*
    The events in the buffer have incorrect end_log_pos data
    (relative to beginning of group rather than absolute),
    so we'll recalculate them in situ so the binlog is always
    correct, even in the middle of a group. This is possible
    because we now know the start position of the group (the
    offset of this cache in the log, if you will); all we need
    to do is to find all event-headers, and add the position of
    the group to the end_log_pos of each event.  This is pretty
    straight forward, except that we read the cache in segments,
    so an event-header might end up on the cache-border and get
    split.
  */

  group= (uint)my_b_tell(&log_file);
  DBUG_PRINT("debug", ("length: %llu, group: %llu",
                       (ulonglong) length, (ulonglong) group));
  hdr_offs= carry= 0;
  if (do_checksum)
    crc= crc_0= my_checksum(0L, NULL, 0);

  if (DBUG_EVALUATE_IF("fault_injection_crc_value", 1, 0))
    crc= crc - 1;

  do
  {
    /*
      if we only got a partial header in the last iteration,
      get the other half now and process a full header.
    */
    if (unlikely(carry > 0))
    {
      DBUG_ASSERT(carry < LOG_EVENT_HEADER_LEN);

      /* assemble both halves */
      memcpy(&header[carry], (char *)cache->read_pos,
             LOG_EVENT_HEADER_LEN - carry);

      /* fix end_log_pos */
      val=uint4korr(header + LOG_POS_OFFSET);
      val+= group +
        (end_log_pos_inc+= (do_checksum ? BINLOG_CHECKSUM_LEN : 0));
      int4store(&header[LOG_POS_OFFSET], val);

      if (do_checksum)
      {
        ulong len= uint4korr(header + EVENT_LEN_OFFSET);
        /* fix len */
        int4store(&header[EVENT_LEN_OFFSET], len + BINLOG_CHECKSUM_LEN);
      }

      /* write the first half of the split header */
      if (my_b_write(&log_file, header, carry))
        DBUG_RETURN(ER_ERROR_ON_WRITE);

      /*
        copy fixed second half of header to cache so the correct
        version will be written later.
      */
      memcpy((char *)cache->read_pos, &header[carry],
             LOG_EVENT_HEADER_LEN - carry);

      /* next event header at ... */
      hdr_offs= uint4korr(header + EVENT_LEN_OFFSET) - carry -
        (do_checksum ? BINLOG_CHECKSUM_LEN : 0);

      if (do_checksum)
      {
        DBUG_ASSERT(crc == crc_0 && remains == 0);
        crc= my_checksum(crc, header, carry);
        remains= uint4korr(header + EVENT_LEN_OFFSET) - carry -
          BINLOG_CHECKSUM_LEN;
      }
      carry= 0;
    }

    /* if there is anything to write, process it. */

    if (likely(length > 0))
    {
      /*
        process all event-headers in this (partial) cache.
        if next header is beyond current read-buffer,
        we'll get it later (though not necessarily in the
        very next iteration, just "eventually").
      */

      /* crc-calc the whole buffer */
      if (do_checksum && hdr_offs >= length)
      {

        DBUG_ASSERT(remains != 0 && crc != crc_0);

        crc= my_checksum(crc, cache->read_pos, length); 
        remains -= length;
        if (my_b_write(&log_file, cache->read_pos, length))
          DBUG_RETURN(ER_ERROR_ON_WRITE);
        if (remains == 0)
        {
          int4store(buf, crc);
          if (my_b_write(&log_file, buf, BINLOG_CHECKSUM_LEN))
            DBUG_RETURN(ER_ERROR_ON_WRITE);
          crc= crc_0;
        }
      }

      while (hdr_offs < length)
      {
        /*
          partial header only? save what we can get, process once
          we get the rest.
        */

        if (do_checksum)
        {
          if (remains != 0)
          {
            /*
              finish off with remains of the last event that crawls
              from previous into the current buffer
            */
            DBUG_ASSERT(crc != crc_0);
            crc= my_checksum(crc, cache->read_pos, hdr_offs);
            int4store(buf, crc);
            remains -= hdr_offs;
            DBUG_ASSERT(remains == 0);
            if (my_b_write(&log_file, cache->read_pos, hdr_offs) ||
                my_b_write(&log_file, buf, BINLOG_CHECKSUM_LEN))
              DBUG_RETURN(ER_ERROR_ON_WRITE);
            crc= crc_0;
          }
        }

        if (hdr_offs + LOG_EVENT_HEADER_LEN > length)
        {
          carry= length - hdr_offs;
          memcpy(header, (char *)cache->read_pos + hdr_offs, carry);
          length= hdr_offs;
        }
        else
        {
          /* we've got a full event-header, and it came in one piece */
          uchar *ev= (uchar *)cache->read_pos + hdr_offs;
          uint event_len= uint4korr(ev + EVENT_LEN_OFFSET); // netto len
          uchar *log_pos= ev + LOG_POS_OFFSET;

          /* fix end_log_pos */
          val= uint4korr(log_pos) + group +
            (end_log_pos_inc += (do_checksum ? BINLOG_CHECKSUM_LEN : 0));
          int4store(log_pos, val);

	  /* fix CRC */
	  if (do_checksum)
          {
            /* fix length */
            int4store(ev + EVENT_LEN_OFFSET, event_len + BINLOG_CHECKSUM_LEN);
            remains= fix_log_event_crc(cache->read_pos, hdr_offs, event_len,
                                       length, &crc);
            if (my_b_write(&log_file, ev, 
                           remains == 0 ? event_len : length - hdr_offs))
              DBUG_RETURN(ER_ERROR_ON_WRITE);
            if (remains == 0)
            {
              int4store(buf, crc);
              if (my_b_write(&log_file, buf, BINLOG_CHECKSUM_LEN))
                DBUG_RETURN(ER_ERROR_ON_WRITE);
              crc= crc_0; // crc is complete
            }
          }

          /* next event header at ... */
          hdr_offs += event_len; // incr by the netto len

          DBUG_ASSERT(!do_checksum || remains == 0 || hdr_offs >= length);
        }
      }

      /*
        Adjust hdr_offs. Note that it may still point beyond the segment
        read in the next iteration; if the current event is very long,
        it may take a couple of read-iterations (and subsequent adjustments
        of hdr_offs) for it to point into the then-current segment.
        If we have a split header (!carry), hdr_offs will be set at the
        beginning of the next iteration, overwriting the value we set here:
      */
      hdr_offs -= length;
    }

    /* Write the entire buf to the binary log file */
    if (!do_checksum)
      if (my_b_write(&log_file, cache->read_pos, length))
        DBUG_RETURN(ER_ERROR_ON_WRITE);
    cache->read_pos=cache->read_end;		// Mark buffer used up
  } while ((length= my_b_fill(cache)));

  DBUG_ASSERT(carry == 0);
  DBUG_ASSERT(!do_checksum || remains == 0);
  DBUG_ASSERT(!do_checksum || crc == crc_0);

  DBUG_RETURN(0); // All OK
}

/**
  Writes an incident event to the binary log.

  @param ev Incident event to be written
  @param need_lock_log If true, will acquire LOCK_log; otherwise the
  caller should already have acquired LOCK_log.
  @do_flush_and_sync If true, will call flush_and_sync(), rotate() and
  purge().

  @retval false error
  @retval true success
*/
bool MYSQL_BIN_LOG::write_incident(Incident_log_event *ev, bool need_lock_log,
                                   bool do_flush_and_sync)
{
  uint error= 0;
  DBUG_ENTER("MYSQL_BIN_LOG::write_incident");

  if (!is_open())
    DBUG_RETURN(error);

  if (need_lock_log)
    mysql_mutex_lock(&LOCK_log);
  else
    mysql_mutex_assert_owner(&LOCK_log);

  // @todo make this work with the group log. /sven

  error= ev->write(&log_file);

  if (do_flush_and_sync)
  {
    if (!error && !(error= flush_and_sync()))
    {
      bool check_purge= false;
      signal_update();
      error= rotate(true, &check_purge);
      if (!error && check_purge)
        purge();
    }
  }

  if (need_lock_log)
    mysql_mutex_unlock(&LOCK_log);

  DBUG_RETURN(error);
}
/**
  Creates an incident event and writes it to the binary log.

  @param thd  Thread variable
  @param ev   Incident event to be written
  @param lock If the binary lock should be locked or not

  @retval
    0    error
  @retval
    1    success
*/
bool MYSQL_BIN_LOG::write_incident(THD *thd, bool need_lock_log,
                                   bool do_flush_and_sync)
{
  DBUG_ENTER("MYSQL_BIN_LOG::write_incident");

  if (!is_open())
    DBUG_RETURN(0);

  LEX_STRING const write_error_msg=
    { C_STRING_WITH_LEN("error writing to the binary log") };
  Incident incident= INCIDENT_LOST_EVENTS;
  Incident_log_event ev(thd, incident, write_error_msg);

  DBUG_RETURN(write_incident(&ev, need_lock_log, do_flush_and_sync));
}

/**
  Write a cached log entry to the binary log.

  @param thd            Thread variable
  @param cache		The cache to copy to the binlog
  @param incident       Defines if an incident event should be created to
                        notify that some non-transactional changes did
                        not get into the binlog.
  @param prepared       Defines if a transaction is part of a 2-PC.

  @note
    We only come here if there is something in the cache.
  @note
    The thing in the cache is always a complete transaction.
  @note
    'cache' needs to be reinitialized after this functions returns.
*/

bool MYSQL_BIN_LOG::write_cache(THD *thd, binlog_cache_data *cache_data)
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
     */
    if (my_b_tell(cache) > 0)
    {
      DBUG_EXECUTE_IF("crash_before_writing_xid",
                      {
                        if ((write_error= do_write_cache(cache)))
                          DBUG_PRINT("info", ("error writing binlog cache: %d",
                                               write_error));
                        flush_and_sync(true);
                        DBUG_PRINT("info", ("crashing before writing xid"));
                        DBUG_SUICIDE();
                      });

      if ((write_error= do_write_cache(cache)))
        goto err;

      if (incident && write_incident(thd, false/*need_lock_log=false*/,
                                     false/*do_flush_and_sync==false*/))
        goto err;

      DBUG_EXECUTE_IF("half_binlogged_transaction", DBUG_SUICIDE(););
      if (cache->error)				// Error on read
      {
        char errbuf[MYSYS_STRERROR_SIZE];
        sql_print_error(ER(ER_ERROR_ON_READ), cache->file_name,
                        errno, my_strerror(errbuf, sizeof(errbuf), errno));
        write_error=1;				// Don't give more errors
        goto err;
      }

      thd->variables.gtid_next.set_undefined();
      global_sid_lock->rdlock();
      if (gtid_state->update_on_flush(thd) != RETURN_STATUS_OK)
      {
        global_sid_lock->unlock();
        goto err;
      }
      global_sid_lock->unlock();
    }
    update_thd_next_event_pos(thd);
  }

  DBUG_RETURN(0);

err:
  if (!write_error)
  {
    char errbuf[MYSYS_STRERROR_SIZE];
    write_error= 1;
    sql_print_error(ER(ER_ERROR_ON_WRITE), name,
                    errno, my_strerror(errbuf, sizeof(errbuf), errno));
  }
  DBUG_RETURN(1);
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
    mysql_cond_wait(&update_cond, &LOCK_log);
  else
    ret= mysql_cond_timedwait(&update_cond, &LOCK_log,
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

  @note
    One can do an open on the object at once after doing a close.
    The internal structures are not freed until cleanup() is called
*/

void MYSQL_BIN_LOG::close(uint exiting)
{					// One can't set log_type here!
  DBUG_ENTER("MYSQL_BIN_LOG::close");
  DBUG_PRINT("enter",("exiting: %d", (int) exiting));
  if (log_state == LOG_OPENED)
  {
#ifdef HAVE_REPLICATION
    if ((exiting & LOG_CLOSE_STOP_EVENT) != 0)
    {
      Stop_log_event s;
      // the checksumming rule for relay-log case is similar to Rotate
        s.checksum_alg= is_relay_log ?
          relay_log_checksum_alg : binlog_checksum_options;
      DBUG_ASSERT(!is_relay_log ||
                  relay_log_checksum_alg != BINLOG_CHECKSUM_ALG_UNDEF);
      s.write(&log_file);
      bytes_written+= s.data_written;
      signal_update();
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
    MYSQL_LOG::close(exiting);
  }

  /*
    The following test is needed even if is_open() is not set, as we may have
    called a not complete close earlier and the index file is still open.
  */

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
  log_state= (exiting & LOG_CLOSE_TO_BE_OPENED) ? LOG_TO_BE_OPENED : LOG_CLOSED;
  my_free(name);
  name= NULL;
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


void MYSQL_BIN_LOG::signal_update()
{
  DBUG_ENTER("MYSQL_BIN_LOG::signal_update");
  signal_cnt++;
  mysql_cond_broadcast(&update_cond);
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
  DBUG_ASSERT(total_ha_2pc > 1);
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
    open_binlog(opt_name, 0, WRITE_CACHE, max_binlog_size, false,
                true/*need_lock_index=true*/,
                true/*need_sid_lock=true*/,
                NULL);
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

    if ((ev= Log_event::read_log_event(&log, 0, &fdle,
                                       opt_master_verify_checksum)) &&
        ev->get_type_code() == FORMAT_DESCRIPTION_EVENT &&
        ev->flags & LOG_EVENT_BINLOG_IN_USE_F)
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

  @retval 0    success
  @retval 1    error, transaction was neither logged nor committed
  @retval 2    error, transaction was logged but not committed
*/
TC_LOG::enum_result MYSQL_BIN_LOG::commit(THD *thd, bool all)
{
  DBUG_ENTER("MYSQL_BIN_LOG::commit");

  binlog_cache_mngr *cache_mngr= thd_get_cache_mngr(thd);
  my_xid xid= thd->transaction.xid_state.xid.get_my_xid();
  int error= RESULT_SUCCESS;
  bool stuff_logged= false;

  DBUG_PRINT("enter", ("thd: 0x%llx, all: %s, xid: %llu, cache_mngr: 0x%llx",
                       (ulonglong) thd, YESNO(all), (ulonglong) xid,
                       (ulonglong) cache_mngr));

  /*
    No cache manager means nothing to log, but we still have to commit
    the transaction.
   */
  if (cache_mngr == NULL)
  {
    if (ha_commit_low(thd, all))
      DBUG_RETURN(RESULT_ABORTED);
    DBUG_RETURN(RESULT_SUCCESS);
  }

  THD_TRANS *trans= all ? &thd->transaction.all : &thd->transaction.stmt;

  DBUG_PRINT("debug", ("in_transaction: %s, no_2pc: %s, rw_ha_count: %d",
                       YESNO(thd->in_multi_stmt_transaction_mode()),
                       YESNO(trans->no_2pc),
                       trans->rw_ha_count));
  DBUG_PRINT("debug",
             ("all.cannot_safely_rollback(): %s, trx_cache_empty: %s",
              YESNO(thd->transaction.all.cannot_safely_rollback()),
              YESNO(cache_mngr->trx_cache.is_binlog_empty())));
  DBUG_PRINT("debug",
             ("stmt.cannot_safely_rollback(): %s, stmt_cache_empty: %s",
              YESNO(thd->transaction.stmt.cannot_safely_rollback()),
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
  if (!all && trans->ha_list == 0 &&
      cache_mngr->stmt_cache.is_binlog_empty())
    DBUG_RETURN(RESULT_SUCCESS);

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
    error= write_empty_groups_to_cache(thd, &cache_mngr->stmt_cache);
    if (error == 0)
    {
      if (cache_mngr->stmt_cache.finalize(thd))
        DBUG_RETURN(RESULT_ABORTED);
      stuff_logged= true;
    }
  }

  /*
    We commit the transaction if:
     - We are not in a transaction and committing a statement, or
     - We are in a transaction and a full transaction is committed.
    Otherwise, we accumulate the changes.
  */
  if (!error && !cache_mngr->trx_cache.is_binlog_empty() &&
      ending_trans(thd, all))
  {
    const bool real_trans= (all || thd->transaction.all.ha_list == 0);
    /*
      We are committing an XA transaction if it is a "real" transaction
      and have an XID assigned (because some handlerton registered). A
      transaction is "real" if either 'all' is true or the 'all.ha_list'
      is empty.

      Note: This is kind of strange since registering the binlog
      handlerton will then make the transaction XA, which is not really
      true. This occurs for example if a MyISAM statement is executed
      with row-based replication on.
   */
    if (real_trans && xid && trans->rw_ha_count > 1 && !trans->no_2pc)
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
    stuff_logged= true;
  }

  /*
    This is part of the stmt rollback.
  */
  if (!all)
    cache_mngr->trx_cache.set_prev_position(MY_OFF_T_UNDEF);

  DBUG_PRINT("debug", ("error: %d", error));

  if (error)
    DBUG_RETURN(RESULT_ABORTED);

  /*
    Now all the events are written to the caches, so we will commit
    the transaction in the engines. This is done using the group
    commit logic in ordered_commit, which will return when the
    transaction is committed.

    If the commit in the engines fail, we still have something logged
    to the binary log so we have to report this as a "bad" failure
    (failed to commit, but logged something).
  */
  if (stuff_logged)
  {
    if (ordered_commit(thd, all))
      DBUG_RETURN(RESULT_INCONSISTENT);
  }
  else
  {
    if (ha_commit_low(thd, all))
      DBUG_RETURN(RESULT_INCONSISTENT);
  }

  DBUG_RETURN(error ? RESULT_INCONSISTENT : RESULT_SUCCESS);
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
    {
      inc_prep_xids();
      thd->transaction.flags.xid_written= true;
    }
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
  DBUG_ASSERT(total_bytes_var && rotate_var && out_queue_var);
  my_off_t total_bytes= 0;
  int flush_error= 0;
  mysql_mutex_assert_owner(&LOCK_log);

  my_atomic_rwlock_rdlock(&opt_binlog_max_flush_queue_time_lock);
  const ulonglong max_udelay= my_atomic_load32(&opt_binlog_max_flush_queue_time);
  my_atomic_rwlock_rdunlock(&opt_binlog_max_flush_queue_time_lock);
  const ulonglong start_utime= max_udelay > 0 ? my_micro_time() : 0;

  /*
    First we read the queue until it either is empty or the difference
    between the time we started and the current time is too large.

    We remember the first thread we unqueued, because this will be the
    beginning of the out queue.
   */
  bool has_more= true;
  THD *first_seen= NULL;
  while ((max_udelay == 0 || my_micro_time() < start_utime + max_udelay) && has_more)
  {
    std::pair<bool,THD*> current= stage_manager.pop_front(Stage_manager::FLUSH_STAGE);
    std::pair<int,my_off_t> result= flush_thread_caches(current.second);
    has_more= current.first;
    total_bytes+= result.second;
    if (flush_error == 0)
      flush_error= result.first;
    if (first_seen == NULL)
      first_seen= current.second;
  }

  /*
    Either the queue is empty, or we ran out of time. If we ran out of
    time, we have to fetch the entire queue (and flush it) since
    otherwise the next batch will not have a leader.
   */
  if (has_more)
  {
    THD *queue= stage_manager.fetch_queue_for(Stage_manager::FLUSH_STAGE);
    for (THD *head= queue ; head ; head = head->next_to_commit)
    {
      std::pair<int,my_off_t> result= flush_thread_caches(head);
      total_bytes+= result.second;
      if (flush_error == 0)
        flush_error= result.first;
    }
    if (first_seen == NULL)
      first_seen= queue;
  }

  *out_queue_var= first_seen;
  *total_bytes_var= total_bytes;
  if (total_bytes > 0 && my_b_tell(&log_file) >= (my_off_t) max_size)
    *rotate_var= true;
  return flush_error;
}


/**
  Commit a sequence of sessions.

  This function commit an entire queue of sessions starting with the
  session in @c first. If there were an error in the flushing part of
  the ordered commit, the error code is passed in and all the threads
  are marked accordingly (but not committed).

  @see MYSQL_BIN_LOG::ordered_commit

  @param thd The "master" thread
  @param first First thread in the queue of threads to commit
  @param flush_error Error code from flush operation.
 */

void
MYSQL_BIN_LOG::process_commit_stage_queue(THD *thd, THD *first,
                                          int flush_error)
{
  mysql_mutex_assert_owner(&LOCK_commit);
  Thread_excursion excursion(thd);
#ifndef DBUG_OFF
  thd->transaction.flags.ready_preempt= 1; // formality by the leader
#endif
  for (THD *head= first ; head ; head = head->next_to_commit)
  {
    DBUG_PRINT("debug", ("Thread ID: %lu, commit_error: %d, flags.pending: %s",
                         head->thread_id, head->commit_error,
                         YESNO(head->transaction.flags.pending)));
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
    if (flush_error != 0)
      head->commit_error= flush_error;
    else if (int error= excursion.attach_to(head))
      head->commit_error= error;
    else
    {
      bool all= head->transaction.flags.real_commit;
      if (head->transaction.flags.commit_low)
      {
        /* head is parked to have exited append() */
        DBUG_ASSERT(head->transaction.flags.ready_preempt);

        if (int error= ha_commit_low(head, all))
          head->commit_error= error;
        else if (head->transaction.flags.xid_written)
          dec_prep_xids();
      }
      DBUG_PRINT("debug", ("commit_error: %d, flags.pending: %s",
                           head->commit_error,
                           YESNO(head->transaction.flags.pending)));
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
  mysql_mutex_lock(enter_mutex);
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
    return ER_ERROR_ON_WRITE;
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
    if (mysql_file_sync(log_file.file, MYF(MY_WME)))
      return std::make_pair(true, synced);
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
  if (thd->commit_error == 0 && thd->transaction.flags.commit_low)
  {
    const bool all= thd->transaction.flags.real_commit;
    thd->commit_error= ha_commit_low(thd, all);
    if (thd->transaction.flags.xid_written)
      dec_prep_xids();
  }

  thd->variables.gtid_next.set_undefined();
  /*
    Remove committed GTID from owned_gtids, it was already logged on
    MYSQL_BIN_LOG::write_cache().
  */
  global_sid_lock->rdlock();
  gtid_state->update_on_commit(thd);
  global_sid_lock->unlock();

  DBUG_ASSERT(thd->commit_error || !thd->transaction.flags.commit_low);
  DBUG_ASSERT(!thd_get_cache_mngr(thd)->dbug_any_finalized());
  DBUG_PRINT("return", ("Thread ID: %lu, commit_error: %d",
                        thd->thread_id, thd->commit_error));
  return thd->commit_error;
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
  int flush_error= 0;
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
      ha_commit_low since that calls st_transaction::cleanup.
  */
  thd->transaction.flags.pending= true;
  thd->commit_error= 0;
  thd->next_to_commit= NULL;
  thd->durability_property= HA_IGNORE_DURABILITY;
  thd->transaction.flags.real_commit= all;
  thd->transaction.flags.xid_written= false;
  thd->transaction.flags.commit_low= !skip_commit;
#ifndef DBUG_OFF
  /*
     The group commit Leader may have to wait for follower whose transaction
     is not ready to be preempted. Initially the status is pessimistic.
     Preemption guarding logics is necessary only when DBUG_ON is set.
     It won't be required for the dbug-off case as long as the follower won't
     execute any thread-specific write access code in this method, which is
     the case as of current.
  */
  thd->transaction.flags.ready_preempt= 0;
#endif

  DBUG_PRINT("enter", ("flags.pending: %s, commit_error: %d, thread_id: %lu",
                       YESNO(thd->transaction.flags.pending),
                       thd->commit_error, thd->thread_id));

  /*
    Stage #1: flushing transactions to binary log

    While flushing, we allow new threads to enter and will process
    them in due time. Once the queue was empty, we cannot reap
    anything more since it is possible that a thread entered and
    appointed itself leader for the flush phase.
  */
  if (change_stage(thd, Stage_manager::FLUSH_STAGE, thd, NULL, &LOCK_log))
  {
    DBUG_PRINT("return", ("Thread ID: %lu, commit_error: %d",
                          thd->thread_id, thd->commit_error));
    DBUG_RETURN(finish_commit(thd));
  }

  THD *wait_queue= NULL;
  flush_error= process_flush_stage_queue(&total_bytes, &do_rotate, &wait_queue);

  my_off_t flush_end_pos= 0;
  if (flush_error == 0 && total_bytes > 0)
    flush_error= flush_cache_to_file(&flush_end_pos);

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

    signal_update();
    DBUG_EXECUTE_IF("crash_commit_after_log", DBUG_SUICIDE(););
  }

  /*
    Stage #2: Syncing binary log file to disk
  */
  if (change_stage(thd, Stage_manager::SYNC_STAGE, wait_queue, &LOCK_log, &LOCK_sync))
  {
    DBUG_PRINT("return", ("Thread ID: %lu, commit_error: %d",
                          thd->thread_id, thd->commit_error));
    DBUG_RETURN(finish_commit(thd));
  }
  THD *final_queue= stage_manager.fetch_queue_for(Stage_manager::SYNC_STAGE);
  if (flush_error == 0 && total_bytes > 0)
  {
    std::pair<bool, bool> result= sync_binlog_file(false);
    flush_error= result.first;
  }

  /*
    Stage #3: Commit all transactions in order.

    This stage is skipped if we do not need to order the commits and
    each thread have to execute the handlerton commit instead.

    Howver, since we are keeping the lock from the previous stage, we
    need to unlock it if we skip the stage.
   */
  if (opt_binlog_order_commits)
  {
    if (change_stage(thd, Stage_manager::COMMIT_STAGE,
                     final_queue, &LOCK_sync, &LOCK_commit))
    {
      DBUG_PRINT("return", ("Thread ID: %lu, commit_error: %d",
                            thd->thread_id, thd->commit_error));
      DBUG_RETURN(finish_commit(thd));
    }
    THD *commit_queue= stage_manager.fetch_queue_for(Stage_manager::COMMIT_STAGE);
    process_commit_stage_queue(thd, commit_queue, flush_error);
    mysql_mutex_unlock(&LOCK_commit);
    final_queue= commit_queue;
  }
  else
    mysql_mutex_unlock(&LOCK_sync);

  /* Commit done so signal all waiting threads */
  stage_manager.signal_done(final_queue);

  /*
    Finish the commit before executing a rotate, or run the risk of a
    deadlock. We don't need the return value here since it is in
    thd->commit_error, which is returned below.
  */
  (void) finish_commit(thd);

  /*
    If we need to rotate, we do it now.
   */
  if (do_rotate)
  {
    /*
      We can force the rotate since we did the check in
      flush_session_queue(). Giving "false" would have the same
      result, but will do the check again.

      NOTE: Run purge_logs wo/ holding LOCK_log because it does not
      need the mutex. Otherwise causes various deadlocks.

      NOTE: The LOCK_commit is necessary when doing a rotate, but that
      is grabbed inside new_file_impl().
    */

    DBUG_EXECUTE_IF("crash_commit_before_unlog", DBUG_SUICIDE(););
    bool check_purge= false;
    mysql_mutex_lock(&LOCK_log);
    int error= rotate(true, &check_purge);
    mysql_mutex_unlock(&LOCK_log);

    if (!error && check_purge)
      purge();
    else
      thd->commit_error= error;
  }
  DBUG_RETURN(thd->commit_error);
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

  if (! fdle->is_valid() ||
      my_hash_init(&xids, &my_charset_bin, TC_LOG_PAGE_SIZE/3, 0,
                   sizeof(my_xid), 0, 0, MYF(0)))
    goto err1;

  init_alloc_root(&mem_root, TC_LOG_PAGE_SIZE, TC_LOG_PAGE_SIZE);

  while ((ev= Log_event::read_log_event(log, 0, fdle, TRUE))
         && ev->is_valid())
  {
    if (ev->get_type_code() == QUERY_EVENT &&
        !strcmp(((Query_log_event*)ev)->query, "BEGIN"))
      in_transaction= TRUE;

    if (ev->get_type_code() == QUERY_EVENT &&
        !strcmp(((Query_log_event*)ev)->query, "COMMIT"))
    {
      DBUG_ASSERT(in_transaction == TRUE);
      in_transaction= FALSE;
    }
    else if (ev->get_type_code() == XID_EVENT)
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

  if (ha_recover(&xids))
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

Group_cache *THD::get_group_cache(bool is_transactional)
{
  DBUG_ENTER("THD::get_group_cache(bool)");

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

  DBUG_RETURN(&cache_data->group_cache);
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

  cache_mngr= (binlog_cache_mngr*) my_malloc(sizeof(binlog_cache_mngr), MYF(MY_ZEROFILL));
  if (!cache_mngr ||
      open_cached_file(&cache_mngr->stmt_cache.cache_log, mysql_tmpdir,
                       LOG_PREFIX, binlog_stmt_cache_size, MYF(MY_WME)) ||
      open_cached_file(&cache_mngr->trx_cache.cache_log, mysql_tmpdir,
                       LOG_PREFIX, binlog_cache_size, MYF(MY_WME)))
  {
    my_free(cache_mngr);
    DBUG_RETURN(1);                      // Didn't manage to set it up
  }
  DBUG_PRINT("debug", ("Set ha_data slot %d to 0x%llx", binlog_hton->slot, (ulonglong) cache_mngr));
  thd_set_ha_data(this, binlog_hton, cache_mngr);

  cache_mngr= new (thd_get_cache_mngr(this))
              binlog_cache_mngr(max_binlog_stmt_cache_size,
                                &binlog_stmt_cache_use,
                                &binlog_stmt_cache_disk_use,
                                max_binlog_cache_size,
                                &binlog_cache_use,
                                &binlog_cache_disk_use);
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
      trans_register_ha(thd, TRUE, binlog_hton);
    trans_register_ha(thd, FALSE, binlog_hton);

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
      select_create::prepare() and THD::binlog_write_table_map()) , but
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
    Query_log_event qinfo(thd, STRING_WITH_LEN("BEGIN"),
                          is_transactional, FALSE, TRUE, 0, TRUE);
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
  DBUG_PRINT("enter", ("table: 0x%lx  (%s: #%lu)",
                       (long) table, table->s->table_name.str,
                       table->s->table_map_id));

  /* Pre-conditions */
  DBUG_ASSERT(is_current_stmt_binlog_format_row() && mysql_bin_log.is_open());
  DBUG_ASSERT(table->s->table_map_id != ULONG_MAX);

  Table_map_log_event
    the_event(this, table, table->s->table_map_id, is_transactional);

  binlog_start_trans_and_stmt(this, &the_event);

  binlog_cache_mngr *const cache_mngr= thd_get_cache_mngr(this);

  binlog_cache_data *cache_data=
    cache_mngr->get_binlog_cache_data(is_transactional);

  if (binlog_rows_query && this->query())
  {
    /* Write the Rows_query_log_event into binlog before the table map */
    Rows_query_log_event
      rows_query_ev(this, this->query(), this->query_length());
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
  MEM_ROOT *db_mem_root= &main_mem_root;

  if (!binlog_accessed_db_names)
    binlog_accessed_db_names= new (db_mem_root) List<char>;

  if (binlog_accessed_db_names->elements >  MAX_DBS_IN_EVENT_MTS)
  {
    push_warning_printf(this, Sql_condition::WARN_LEVEL_WARN,
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
    binlog_accessed_db_names->push_back(after_db, &main_mem_root);
}


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
    INSERT DELAYED).  A row injection is either a BINLOG statement, or
    a row event executed by the slave's SQL thread.

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

  For each error case above, the statement is prevented from being
  logged, we report an error, and roll back the statement.  For
  warnings, we set the thd->binlog_flags variable: the warning will be
  printed only if the statement is successfully logged.

  @see THD::binlog_query

  @param[in] thd    Client thread
  @param[in] tables Tables involved in the query

  @retval 0 No error; statement can be logged.
  @retval -1 One of the error conditions above applies (1, 2, 4, 5, or 6).
*/

int THD::decide_logging_format(TABLE_LIST *tables)
{
  DBUG_ENTER("THD::decide_logging_format");
  DBUG_PRINT("info", ("query: %s", query()));
  DBUG_PRINT("info", ("variables.binlog_format: %lu",
                      variables.binlog_format));
  DBUG_PRINT("info", ("lex->get_stmt_unsafe_flags(): 0x%x",
                      lex->get_stmt_unsafe_flags()));

  reset_binlog_local_stmt_filter();

  /*
    We should not decide logging format if the binlog is closed or
    binlogging is off, or if the statement is filtered out from the
    binlog by filtering rules.
  */
  if (mysql_bin_log.is_open() && (variables.option_bits & OPTION_BIN_LOG) &&
      !(variables.binlog_format == BINLOG_FORMAT_STMT &&
        !binlog_filter->db_ok(db)))
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
#ifndef DBUG_OFF
    {
      static const char *prelocked_mode_name[] = {
        "NON_PRELOCKED",
        "PRELOCKED",
        "PRELOCKED_UNDER_LOCK_TABLES",
      };
      DBUG_PRINT("debug", ("prelocked_mode: %s",
                           prelocked_mode_name[locked_tables_mode]));
    }
#endif

    /*
      Get the capabilities vector for all involved storage engines and
      mask out the flags for the binary log.
    */
    for (TABLE_LIST *table= tables; table; table= table->next_global)
    {
      if (table->placeholder())
        continue;

      handler::Table_flags const flags= table->table->file->ha_table_flags();

      DBUG_PRINT("info", ("table: %s; ha_table_flags: 0x%llx",
                          table->table_name, flags));

      if (table->table->no_replicate)
      {
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
               sqlcom_can_generate_row_events(this))
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
                 sqlcom_can_generate_row_events(this))
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

    if (!error && enforce_gtid_consistency &&
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
        if (table->placeholder())
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
  }
#ifndef DBUG_OFF
  else
    DBUG_PRINT("info", ("decision: no logging since "
                        "mysql_bin_log.is_open() = %d "
                        "and (options & OPTION_BIN_LOG) = 0x%llx "
                        "and binlog_format = %lu "
                        "and binlog_filter->db_ok(db) = %d",
                        mysql_bin_log.is_open(),
                        (variables.option_bits & OPTION_BIN_LOG),
                        variables.binlog_format,
                        binlog_filter->db_ok(db)));
#endif

  DBUG_RETURN(0);
}


bool THD::is_ddl_gtid_compatible() const
{
  DBUG_ENTER("THD::is_ddl_gtid_compatible");

  // If @@session.sql_log_bin has been manually turned off (only
  // doable by SUPER), then no problem, we can execute any statement.
  if ((variables.option_bits & OPTION_BIN_LOG) == 0)
    DBUG_RETURN(true);

  if (lex->sql_command == SQLCOM_CREATE_TABLE &&
      !(lex->create_info.options & HA_LEX_CREATE_TMP_TABLE) &&
      lex->select_lex.item_list.elements)
  {
    /*
      CREATE ... SELECT (without TEMPORARY) is unsafe because if
      binlog_format=row it will be logged as a CREATE TABLE followed
      by row events, re-executed non-atomically as two transactions,
      and then written to the slave's binary log as two separate
      transactions with the same GTID.
    */
    my_error(ER_GTID_UNSAFE_CREATE_SELECT, MYF(0));
    DBUG_RETURN(false);
  }
  if ((lex->sql_command == SQLCOM_CREATE_TABLE &&
       (lex->create_info.options & HA_LEX_CREATE_TMP_TABLE) != 0) ||
      (lex->sql_command == SQLCOM_DROP_TABLE && lex->drop_temporary))
  {
    /*
      [CREATE|DROP] TEMPORARY TABLE is unsafe to execute
      inside a transaction because the table will stay and the
      transaction will be written to the slave's binary log with the
      GTID even if the transaction is rolled back.
    */
    if (in_multi_stmt_transaction_mode())
    {
      my_error(ER_GTID_UNSAFE_CREATE_DROP_TEMPORARY_TABLE_IN_TRANSACTION,
               MYF(0));
      DBUG_RETURN(false);
    }
  }
  DBUG_RETURN(true);
}


bool
THD::is_dml_gtid_compatible(bool transactional_table,
                            bool non_transactional_table,
                            bool non_transactional_tmp_tables) const
{
  DBUG_ENTER("THD::is_dml_gtid_compatible(bool, bool, bool)");

  // If @@session.sql_log_bin has been manually turned off (only
  // doable by SUPER), then no problem, we can execute any statement.
  if ((variables.option_bits & OPTION_BIN_LOG) == 0)
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
  if (non_transactional_table &&
      (transactional_table || trans_has_updated_trans_table(this)) &&
      !(non_transactional_tmp_tables && is_current_stmt_binlog_format_row()) &&
      !DBUG_EVALUATE_IF("allow_gtid_unsafe_non_transactional_updates", 1, 0))
  {
    my_error(ER_GTID_UNSAFE_NON_TRANSACTIONAL_TABLE, MYF(0));
    DBUG_RETURN(false);
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
				       RowsEventT *hint __attribute__((unused)),
                                       const uchar* extra_row_info)
{
  DBUG_ENTER("binlog_prepare_pending_rows_event");
  /* Pre-conditions */
  DBUG_ASSERT(table->s->table_map_id != ~0UL);

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
CPP_UNNAMED_NS_START

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
        m_memory= (uchar *) my_malloc(total_length, MYF(MY_WME));
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

CPP_UNNAMED_NS_END

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

  /*
    Don't print debug messages when running valgrind since they can
    trigger false warnings.
   */
#ifndef HAVE_purify
  DBUG_DUMP("before_record", before_record, table->s->reclength);
  DBUG_DUMP("after_record",  after_record, table->s->reclength);
  DBUG_DUMP("before_row",    before_row, before_size);
  DBUG_DUMP("after_row",     after_row, after_size);
#endif

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
    non-null unique index) and we dont want to ship the entire image.
   */
  if (table->s->primary_key < MAX_KEY &&
      (thd->variables.binlog_row_image < BINLOG_ROW_IMAGE_FULL))
  {
    /**
      Just to be sure that tmp_set is currently not in use as
      the read_set already.
    */
    DBUG_ASSERT(table->read_set != &table->tmp_set);

    bitmap_clear_all(&table->tmp_set);

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

#if !defined(DBUG_OFF) && !defined(_lint)
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
                                 char* query)
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
  SYNOPSIS:
  @params
   buf         - buffer to hold the warning message text
   unsafe_type - The type of unsafety.
   query       - The actual query statement.

  TODO: Remove this function and implement a general service for all warnings
  that would prevent flooding the error log.
*/
static void do_unsafe_limit_checkout(char* buf, int unsafe_type, char* query)
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
      push_warning_printf(this, Sql_condition::WARN_LEVEL_NOTE,
                          ER_BINLOG_UNSAFE_STATEMENT,
                          ER(ER_BINLOG_UNSAFE_STATEMENT),
                          ER(LEX::binlog_stmt_unsafe_errcode[unsafe_type]));
      if (log_warnings)
      {
        if (unsafe_type == LEX::BINLOG_STMT_UNSAFE_LIMIT)
          do_unsafe_limit_checkout( buf, unsafe_type, query());
        else //cases other than LIMIT unsafety
          print_unsafe_warning_to_log(unsafe_type, buf, query());
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
int THD::binlog_query(THD::enum_binlog_query_type qtype, char const *query_arg,
                      ulong query_len, bool is_trans, bool direct, 
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
      The MYSQL_LOG::write() function will set the STMT_END_F flag and
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
  NULL, /* Plugin Deinit */
  0x0100 /* 1.0 */,
  NULL,                       /* status variables                */
  NULL,                       /* system variables                */
  NULL,                       /* config options                  */
  0,  
}
mysql_declare_plugin_end;

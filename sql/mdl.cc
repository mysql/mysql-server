/* Copyright (C) 2007-2008 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */


#include "mdl.h"
#include "debug_sync.h"
#include <hash.h>
#include <mysqld_error.h>


void notify_shared_lock(THD *thd, MDL_ticket *conflicting_ticket);


static bool mdl_initialized= 0;


/**
  A collection of all MDL locks. A singleton,
  there is only one instance of the map in the server.
  Maps MDL_key to MDL_lock instances.
*/

class MDL_map
{
public:
  void init();
  void destroy();
  MDL_lock *find(const MDL_key *key);
  MDL_lock *find_or_insert(const MDL_key *key);
  void remove(MDL_lock *lock);
private:
  bool move_from_hash_to_lock_mutex(MDL_lock *lock);
private:
  /** All acquired locks in the server. */
  HASH m_locks;
  /* Protects access to m_locks hash. */
  mysql_mutex_t m_mutex;
};


enum enum_deadlock_weight
{
  MDL_DEADLOCK_WEIGHT_DML= 0,
  MDL_DEADLOCK_WEIGHT_DDL= 100
};



/**
  A context of the recursive traversal through all contexts
  in all sessions in search for deadlock.
*/

class Deadlock_detection_context
{
public:
  Deadlock_detection_context(MDL_context *start_arg)
    : start(start_arg),
      victim(NULL),
      current_search_depth(0)
  { }
  MDL_context *start;
  MDL_context *victim;
  uint current_search_depth;
  /**
    Maximum depth for deadlock searches. After this depth is
    achieved we will unconditionally declare that there is a
    deadlock.

    @note This depth should be small enough to avoid stack
          being exhausted by recursive search algorithm.

    TODO: Find out what is the optimal value for this parameter.
          Current value is safe, but probably sub-optimal,
          as there is an anecdotal evidence that real-life
          deadlocks are even shorter typically.
  */
  static const uint MAX_SEARCH_DEPTH= 32;
};


/**
  Get a bit corresponding to enum_mdl_type value in a granted/waiting bitmaps
  and compatibility matrices.
*/

#define MDL_BIT(A) static_cast<MDL_lock::bitmap_t>(1U << A)

/**
  The lock context. Created internally for an acquired lock.
  For a given name, there exists only one MDL_lock instance,
  and it exists only when the lock has been granted.
  Can be seen as an MDL subsystem's version of TABLE_SHARE.

  This is an abstract class which lacks information about
  compatibility rules for lock types. They should be specified
  in its descendants.
*/

class MDL_lock
{
public:
  typedef uchar bitmap_t;

  class Ticket_list
  {
  public:
    typedef I_P_List<MDL_ticket,
                     I_P_List_adapter<MDL_ticket,
                                      &MDL_ticket::next_in_lock,
                                      &MDL_ticket::prev_in_lock> >
            List;
    operator const List &() const { return m_list; }
    Ticket_list() :m_bitmap(0) {}

    void add_ticket(MDL_ticket *ticket);
    void remove_ticket(MDL_ticket *ticket);
    bool is_empty() const { return m_list.is_empty(); }
    bitmap_t bitmap() const { return m_bitmap; }
  private:
    void clear_bit_if_not_in_list(enum_mdl_type type);
  private:
    /** List of tickets. */
    List m_list;
    /** Bitmap of types of tickets in this list. */
    bitmap_t m_bitmap;
  };

  typedef Ticket_list::List::Iterator Ticket_iterator;

public:
  /** The key of the object (data) being protected. */
  MDL_key key;
  void   *cached_object;
  mdl_cached_object_release_hook cached_object_release_hook;
  /**
    Read-write lock protecting this lock context.

    TODO/FIXME: Replace with RW-lock which will prefer readers
                on all platforms and not only on Linux.
  */
  rw_lock_t m_rwlock;

  bool is_empty() const
  {
    return (m_granted.is_empty() && m_waiting.is_empty());
  }

  virtual const bitmap_t *incompatible_granted_types_bitmap() const = 0;
  virtual const bitmap_t *incompatible_waiting_types_bitmap() const = 0;

  bool has_pending_conflicting_lock(enum_mdl_type type);

  bool can_grant_lock(enum_mdl_type type, MDL_context *requstor_ctx) const;

  inline static MDL_lock *create(const MDL_key *key);

  void notify_shared_locks(MDL_context *ctx)
  {
    Ticket_iterator it(m_granted);
    MDL_ticket *conflicting_ticket;

    while ((conflicting_ticket= it++))
    {
      if (conflicting_ticket->get_ctx() != ctx)
        notify_shared_lock(ctx->get_thd(), conflicting_ticket);
    }
  }

  /**
    Wake up contexts which are waiting to acquire lock on the object and
    which may succeed now, when we released some lock on it or removed
    some pending request from its waiters list (the latter can happen,
    for example, when context trying to acquire exclusive on the object
    lock is killed).
  */
  void wake_up_waiters()
  {
    MDL_lock::Ticket_iterator it(m_waiting);
    MDL_ticket *awake_ticket;

    while ((awake_ticket= it++))
      awake_ticket->get_ctx()->awake(MDL_context::NORMAL_WAKE_UP);
  }
  void remove_ticket(Ticket_list MDL_lock::*queue, MDL_ticket *ticket);

  bool find_deadlock(MDL_ticket *waiting_ticket,
                     Deadlock_detection_context *deadlock_ctx);

  /** List of granted tickets for this lock. */
  Ticket_list m_granted;
  /** Tickets for contexts waiting to acquire a lock. */
  Ticket_list m_waiting;
public:

  MDL_lock(const MDL_key *key_arg)
  : key(key_arg),
    cached_object(NULL),
    cached_object_release_hook(NULL),
    m_ref_usage(0),
    m_ref_release(0),
    m_is_destroyed(FALSE)
  {
    my_rwlock_init(&m_rwlock, NULL);
  }

  virtual ~MDL_lock()
  {
    rwlock_destroy(&m_rwlock);
  }
  inline static void destroy(MDL_lock *lock);
public:
  /**
    These three members are used to make it possible to separate
    the mdl_locks.m_mutex mutex and MDL_lock::m_rwlock in
    MDL_map::find_or_insert() for increased scalability.
    The 'm_is_destroyed' member is only set by destroyers that
    have both the mdl_locks.m_mutex and MDL_lock::m_rwlock, thus
    holding any of the mutexes is sufficient to read it.
    The 'm_ref_usage; is incremented under protection by
    mdl_locks.m_mutex, but when 'm_is_destroyed' is set to TRUE, this
    member is moved to be protected by the MDL_lock::m_rwlock.
    This means that the MDL_map::find_or_insert() which only
    holds the MDL_lock::m_rwlock can compare it to 'm_ref_release'
    without acquiring mdl_locks.m_mutex again and if equal it can also
    destroy the lock object safely.
    The 'm_ref_release' is incremented under protection by
    MDL_lock::m_rwlock.
    Note since we are only interested in equality of these two
    counters we don't have to worry about overflows as long as
    their size is big enough to hold maximum number of concurrent
    threads on the system.
  */
  uint m_ref_usage;
  uint m_ref_release;
  bool m_is_destroyed;
};


/**
  An implementation of the global metadata lock. The only locking modes
  which are supported at the moment are SHARED and INTENTION EXCLUSIVE.
*/

class MDL_global_lock : public MDL_lock
{
public:
  MDL_global_lock(const MDL_key *key_arg)
    : MDL_lock(key_arg)
  { }

  virtual const bitmap_t *incompatible_granted_types_bitmap() const
  {
    return m_granted_incompatible;
  }
  virtual const bitmap_t *incompatible_waiting_types_bitmap() const
  {
    return m_waiting_incompatible;
  }

private:
  static const bitmap_t m_granted_incompatible[MDL_TYPE_END];
  static const bitmap_t m_waiting_incompatible[MDL_TYPE_END];
};


/**
  An implementation of a per-object lock. Supports SHARED, SHARED_UPGRADABLE,
  SHARED HIGH PRIORITY and EXCLUSIVE locks.
*/

class MDL_object_lock : public MDL_lock
{
public:
  MDL_object_lock(const MDL_key *key_arg)
    : MDL_lock(key_arg)
  { }

  virtual const bitmap_t *incompatible_granted_types_bitmap() const
  {
    return m_granted_incompatible;
  }
  virtual const bitmap_t *incompatible_waiting_types_bitmap() const
  {
    return m_waiting_incompatible;
  }

private:
  static const bitmap_t m_granted_incompatible[MDL_TYPE_END];
  static const bitmap_t m_waiting_incompatible[MDL_TYPE_END];
};


static MDL_map mdl_locks;

extern "C"
{
static uchar *
mdl_locks_key(const uchar *record, size_t *length,
              my_bool not_used __attribute__((unused)))
{
  MDL_lock *lock=(MDL_lock*) record;
  *length= lock->key.length();
  return (uchar*) lock->key.ptr();
}
} /* extern "C" */


/**
  Initialize the metadata locking subsystem.

  This function is called at server startup.

  In particular, initializes the new global mutex and
  the associated condition variable: LOCK_mdl and COND_mdl.
  These locking primitives are implementation details of the MDL
  subsystem and are private to it.

  Note, that even though the new implementation adds acquisition
  of a new global mutex to the execution flow of almost every SQL
  statement, the design capitalizes on that to later save on
  look ups in the table definition cache. This leads to reduced
  contention overall and on LOCK_open in particular.
  Please see the description of MDL_context::acquire_shared_lock()
  for details.
*/

void mdl_init()
{
  DBUG_ASSERT(! mdl_initialized);
  mdl_initialized= TRUE;
  mdl_locks.init();
}


/**
  Release resources of metadata locking subsystem.

  Destroys the global mutex and the condition variable.
  Called at server shutdown.
*/

void mdl_destroy()
{
  if (mdl_initialized)
  {
    mdl_initialized= FALSE;
    mdl_locks.destroy();
  }
}


/** Initialize the global hash containing all MDL locks. */

void MDL_map::init()
{
  mysql_mutex_init(NULL /* pfs key */,&m_mutex, NULL);
  my_hash_init(&m_locks, &my_charset_bin, 16 /* FIXME */, 0, 0,
               mdl_locks_key, 0, 0);
}


/**
  Destroy the global hash containing all MDL locks.
  @pre It must be empty.
*/

void MDL_map::destroy()
{
  DBUG_ASSERT(!m_locks.records);
  mysql_mutex_destroy(&m_mutex);
  my_hash_free(&m_locks);
}


/**
  Find MDL_lock object corresponding to the key, create it
  if it does not exist.

  @retval non-NULL - Success. MDL_lock instance for the key with
                     locked MDL_lock::m_rwlock.
  @retval NULL     - Failure (OOM).
*/

MDL_lock* MDL_map::find_or_insert(const MDL_key *mdl_key)
{
  MDL_lock *lock;
  my_hash_value_type hash_value;

  hash_value= my_calc_hash(&m_locks, mdl_key->ptr(), mdl_key->length());

retry:
  mysql_mutex_lock(&m_mutex);
  if (!(lock= (MDL_lock*) my_hash_search_using_hash_value(&m_locks,
                                                          hash_value,
                                                          mdl_key->ptr(),
                                                          mdl_key->length())))
  {
    lock= MDL_lock::create(mdl_key);
    if (!lock || my_hash_insert(&m_locks, (uchar*)lock))
    {
      mysql_mutex_unlock(&m_mutex);
      MDL_lock::destroy(lock);
      return NULL;
    }
  }

  if (move_from_hash_to_lock_mutex(lock))
    goto retry;

  return lock;
}


/**
  Find MDL_lock object corresponding to the key.

  @retval non-NULL - MDL_lock instance for the key with locked
                     MDL_lock::m_rwlock.
  @retval NULL     - There was no MDL_lock for the key.
*/

MDL_lock* MDL_map::find(const MDL_key *mdl_key)
{
  MDL_lock *lock;
  my_hash_value_type hash_value;

  hash_value= my_calc_hash(&m_locks, mdl_key->ptr(), mdl_key->length());

retry:
  mysql_mutex_lock(&m_mutex);
  if (!(lock= (MDL_lock*) my_hash_search_using_hash_value(&m_locks,
                                                          hash_value,
                                                          mdl_key->ptr(),
                                                          mdl_key->length())))
  {
    mysql_mutex_unlock(&m_mutex);
    return NULL;
  }

  if (move_from_hash_to_lock_mutex(lock))
    goto retry;

  return lock;
}


/**
  Release mdl_locks.m_mutex mutex and lock MDL_lock::m_rwlock for lock
  object from the hash. Handle situation when object was released
  while the held no mutex.

  @retval FALSE - Success.
  @retval TRUE  - Object was released while we held no mutex, caller
                  should re-try looking up MDL_lock object in the hash.
*/

bool MDL_map::move_from_hash_to_lock_mutex(MDL_lock *lock)
{
  DBUG_ASSERT(! lock->m_is_destroyed);
  mysql_mutex_assert_owner(&m_mutex);

  /*
    We increment m_ref_usage which is a reference counter protected by
    mdl_locks.m_mutex under the condition it is present in the hash and
    m_is_destroyed is FALSE.
  */
  lock->m_ref_usage++;
  mysql_mutex_unlock(&m_mutex);

  rw_wrlock(&lock->m_rwlock);
  lock->m_ref_release++;
  if (unlikely(lock->m_is_destroyed))
  {
    /*
      Object was released while we held no mutex, we need to
      release it if no others hold references to it, while our own
      reference count ensured that the object as such haven't got
      its memory released yet. We can also safely compare
      m_ref_usage and m_ref_release since the object is no longer
      present in the hash so no one will be able to find it and
      increment m_ref_usage anymore.
    */
    uint ref_usage= lock->m_ref_usage;
    uint ref_release= lock->m_ref_release;
    rw_unlock(&lock->m_rwlock);
    if (ref_usage == ref_release)
      MDL_lock::destroy(lock);
    return TRUE;
  }
  return FALSE;
}


/**
  Destroy MDL_lock object or delegate this responsibility to
  whatever thread that holds the last outstanding reference to
  it.
*/

void MDL_map::remove(MDL_lock *lock)
{
  uint ref_usage, ref_release;

  if (lock->cached_object)
    (*lock->cached_object_release_hook)(lock->cached_object);

  /*
    Destroy the MDL_lock object, but ensure that anyone that is
    holding a reference to the object is not remaining, if so he
    has the responsibility to release it.

    Setting of m_is_destroyed to TRUE while holding _both_
    mdl_locks.m_mutex and MDL_lock::m_rwlock mutexes transfers the
    protection of m_ref_usage from mdl_locks.m_mutex to
    MDL_lock::m_rwlock while removal of object from the hash makes
    it read-only.  Therefore whoever acquires MDL_lock::m_rwlock next
    will see most up to date version of m_ref_usage.

    This means that when m_is_destroyed is TRUE and we hold the
    MDL_lock::m_rwlock we can safely read the m_ref_usage
    member.
  */
  mysql_mutex_lock(&m_mutex);
  my_hash_delete(&m_locks, (uchar*) lock);
  lock->m_is_destroyed= TRUE;
  ref_usage= lock->m_ref_usage;
  ref_release= lock->m_ref_release;
  rw_unlock(&lock->m_rwlock);
  mysql_mutex_unlock(&m_mutex);
  if (ref_usage == ref_release)
    MDL_lock::destroy(lock);
}


/**
  Initialize a metadata locking context.

  This is to be called when a new server connection is created.
*/

MDL_context::MDL_context()
  :m_trans_sentinel(NULL),
  m_thd(NULL),
  m_needs_thr_lock_abort(FALSE),
  m_waiting_for(NULL),
  m_deadlock_weight(0),
  m_signal(NO_WAKE_UP)
{
  my_rwlock_init(&m_waiting_for_lock, NULL);
  mysql_mutex_init(NULL /* pfs key */, &m_signal_lock, NULL);
  mysql_cond_init(NULL /* pfs key */, &m_signal_cond, NULL);
}


/**
  Destroy metadata locking context.

  Assumes and asserts that there are no active or pending locks
  associated with this context at the time of the destruction.

  Currently does nothing. Asserts that there are no pending
  or satisfied lock requests. The pending locks must be released
  prior to destruction. This is a new way to express the assertion
  that all tables are closed before a connection is destroyed.
*/

void MDL_context::destroy()
{
  DBUG_ASSERT(m_tickets.is_empty());

  rwlock_destroy(&m_waiting_for_lock);
  mysql_mutex_destroy(&m_signal_lock);
  mysql_cond_destroy(&m_signal_cond);
}


/**
  Initialize a lock request.

  This is to be used for every lock request.

  Note that initialization and allocation are split into two
  calls. This is to allow flexible memory management of lock
  requests. Normally a lock request is stored in statement memory
  (e.g. is a member of struct TABLE_LIST), but we would also like
  to allow allocation of lock requests in other memory roots,
  for example in the grant subsystem, to lock privilege tables.

  The MDL subsystem does not own or manage memory of lock requests.

  @param  mdl_namespace  Id of namespace of object to be locked
  @param  db             Name of database to which the object belongs
  @param  name           Name of of the object
  @param  mdl_type       The MDL lock type for the request.
*/

void MDL_request::init(MDL_key::enum_mdl_namespace mdl_namespace,
                       const char *db_arg,
                       const char *name_arg,
                       enum enum_mdl_type mdl_type_arg)
{
  key.mdl_key_init(mdl_namespace, db_arg, name_arg);
  type= mdl_type_arg;
  ticket= NULL;
}


/**
  Initialize a lock request using pre-built MDL_key.

  @sa MDL_request::init(namespace, db, name, type).

  @param key_arg       The pre-built MDL key for the request.
  @param mdl_type_arg  The MDL lock type for the request.
*/

void MDL_request::init(const MDL_key *key_arg,
                       enum enum_mdl_type mdl_type_arg)
{
  key.mdl_key_init(key_arg);
  type= mdl_type_arg;
  ticket= NULL;
}


/**
  Allocate and initialize one lock request.

  Same as mdl_init_lock(), but allocates the lock and the key buffer
  on a memory root. Necessary to lock ad-hoc tables, e.g.
  mysql.* tables of grant and data dictionary subsystems.

  @param  mdl_namespace  Id of namespace of object to be locked
  @param  db             Name of database to which object belongs
  @param  name           Name of of object
  @param  root           MEM_ROOT on which object should be allocated

  @note The allocated lock request will have MDL_SHARED type.

  @retval 0      Error if out of memory
  @retval non-0  Pointer to an object representing a lock request
*/

MDL_request *
MDL_request::create(MDL_key::enum_mdl_namespace mdl_namespace, const char *db,
                    const char *name, enum_mdl_type mdl_type,
                    MEM_ROOT *root)
{
  MDL_request *mdl_request;

  if (!(mdl_request= (MDL_request*) alloc_root(root, sizeof(MDL_request))))
    return NULL;

  mdl_request->init(mdl_namespace, db, name, mdl_type);

  return mdl_request;
}


uint MDL_request::get_deadlock_weight() const
{
  return key.mdl_namespace() == MDL_key::GLOBAL ||
         type > MDL_SHARED_NO_WRITE ?
    MDL_DEADLOCK_WEIGHT_DDL : MDL_DEADLOCK_WEIGHT_DML;
}

/**
  Auxiliary functions needed for creation/destruction of MDL_lock objects.

  @note Also chooses an MDL_lock descendant appropriate for object namespace.

  @todo This naive implementation should be replaced with one that saves
        on memory allocation by reusing released objects.
*/

inline MDL_lock *MDL_lock::create(const MDL_key *mdl_key)
{
  switch (mdl_key->mdl_namespace())
  {
    case MDL_key::GLOBAL:
      return new MDL_global_lock(mdl_key);
    default:
      return new MDL_object_lock(mdl_key);
  }
}


void MDL_lock::destroy(MDL_lock *lock)
{
  delete lock;
}


/**
  Auxiliary functions needed for creation/destruction of MDL_ticket
  objects.

  @todo This naive implementation should be replaced with one that saves
        on memory allocation by reusing released objects.
*/

MDL_ticket *MDL_ticket::create(MDL_context *ctx_arg, enum_mdl_type type_arg)
{
  return new MDL_ticket(ctx_arg, type_arg);
}


void MDL_ticket::destroy(MDL_ticket *ticket)
{
  delete ticket;
}


/**
  Helper functions and macros to be used for killable waiting in metadata
  locking subsystem.

  @sa THD::enter_cond()/exit_cond()/killed.

  @note We can't use THD::enter_cond()/exit_cond()/killed directly here
        since this will make metadata subsystem dependent on THD class
        and thus prevent us from writing unit tests for it. And usage of
        wrapper functions to access THD::killed/enter_cond()/exit_cond()
        will probably introduce too much overhead.
*/

#define MDL_ENTER_COND(A, B, C, D) \
        mdl_enter_cond(A, B, C, D, __func__, __FILE__, __LINE__)

static inline const char *mdl_enter_cond(THD *thd,
                                         st_my_thread_var *mysys_var,
                                         mysql_cond_t *cond,
                                         mysql_mutex_t *mutex,
                                         const char *calling_func,
                                         const char *calling_file,
                                         const unsigned int calling_line)
{
  mysql_mutex_assert_owner(mutex);

  mysys_var->current_mutex= mutex;
  mysys_var->current_cond= cond;

  DEBUG_SYNC(thd, "mdl_enter_cond");

  return set_thd_proc_info(thd, "Waiting for table",
                           calling_func, calling_file, calling_line);
}

#define MDL_EXIT_COND(A, B, C, D) \
        mdl_exit_cond(A, B, C, D, __func__, __FILE__, __LINE__)

static inline void mdl_exit_cond(THD *thd,
                                 st_my_thread_var *mysys_var,
                                 mysql_mutex_t *mutex,
                                 const char* old_msg,
                                 const char *calling_func,
                                 const char *calling_file,
                                 const unsigned int calling_line)
{
  DBUG_ASSERT(mutex == mysys_var->current_mutex);

  mysql_mutex_unlock(mutex);
  mysql_mutex_lock(&mysys_var->mutex);
  mysys_var->current_mutex= NULL;
  mysys_var->current_cond= NULL;
  mysql_mutex_unlock(&mysys_var->mutex);

  DEBUG_SYNC(thd, "mdl_exit_cond");

  (void) set_thd_proc_info(thd, old_msg, calling_func,
                           calling_file, calling_line);
}


MDL_context::mdl_signal_type MDL_context::timed_wait(struct timespec
                                                     *abs_timeout)
{
  const char *old_msg;
  mdl_signal_type result;
  st_my_thread_var *mysys_var= my_thread_var;
  int wait_result= 0;

  mysql_mutex_lock(&m_signal_lock);

  old_msg= MDL_ENTER_COND(m_thd, mysys_var, &m_signal_cond, &m_signal_lock);

  while (!m_signal && !mysys_var->abort &&
         wait_result != ETIMEDOUT && wait_result != ETIME)
    wait_result= mysql_cond_timedwait(&m_signal_cond, &m_signal_lock,
                                      abs_timeout);

  result= (m_signal != NO_WAKE_UP || mysys_var->abort) ?
    m_signal : TIMEOUT_WAKE_UP;

  MDL_EXIT_COND(m_thd, mysys_var, &m_signal_lock, old_msg);

  return result;
}


/**
  Clear bit corresponding to the type of metadata lock in bitmap representing
  set of such types if list of tickets does not contain ticket with such type.

  @param[in,out]  bitmap  Bitmap representing set of types of locks.
  @param[in]      list    List to inspect.
  @param[in]      type    Type of metadata lock to look up in the list.
*/

void MDL_lock::Ticket_list::clear_bit_if_not_in_list(enum_mdl_type type)
{
  MDL_lock::Ticket_iterator it(m_list);
  const MDL_ticket *ticket;

  while ((ticket= it++))
    if (ticket->get_type() == type)
      return;
  m_bitmap&= ~ MDL_BIT(type);
}


/**
  Add ticket to MDL_lock's list of waiting requests and
  update corresponding bitmap of lock types.
*/

void MDL_lock::Ticket_list::add_ticket(MDL_ticket *ticket)
{
  /*
    Ticket being added to the list must have MDL_ticket::m_lock set,
    since for such tickets methods accessing this member might be
    called by other threads.
  */
  DBUG_ASSERT(ticket->get_lock());
  m_list.push_front(ticket);
  m_bitmap|= MDL_BIT(ticket->get_type());
}


/**
  Remove ticket from MDL_lock's list of requests and
  update corresponding bitmap of lock types.
*/

void MDL_lock::Ticket_list::remove_ticket(MDL_ticket *ticket)
{
  m_list.remove(ticket);
  /*
    Check if waiting queue has another ticket with the same type as
    one which was removed. If there is no such ticket, i.e. we have
    removed last ticket of particular type, then we need to update
    bitmap of waiting ticket's types.
    Note that in most common case, i.e. when shared lock is removed
    from waiting queue, we are likely to find ticket of the same
    type early without performing full iteration through the list.
    So this method should not be too expensive.
  */
  clear_bit_if_not_in_list(ticket->get_type());
}


/**
  Compatibility (or rather "incompatibility") matrices for global metadata
  lock. Arrays of bitmaps which elements specify which granted/waiting locks
  are incompatible with type of lock being requested.

  Here is how types of individual locks are translated to type of global lock:

    ----------------+-------------+
    Type of request | Correspond. |
    for indiv. lock | global lock |
    ----------------+-------------+
    S, SH, SR, SW   |   IS        |
    SNW, SNRW, X    |   IX        |
    SNW, SNRW -> X  |   IX (*)    |

  The first array specifies if particular type of request can be satisfied
  if there is granted global lock of certain type.

             | Type of active |
     Request |   global lock  |
      type   | IS(**) IX   S  |
    ---------+----------------+
    IS       |  +      +   +  |
    IX       |  +      +   -  |
    S        |  +      -   +  |

  The second array specifies if particular type of request can be satisfied
  if there is already waiting request for the global lock of certain type.
  I.e. it specifies what is the priority of different lock types.

             |    Pending   |
     Request |  global lock |
      type   | IS(**) IX  S |
    ---------+--------------+
    IS       |  +      +  + |
    IX       |  +      +  - |
    S        |  +      +  + |

  Here: "+" -- means that request can be satisfied
        "-" -- means that request can't be satisfied and should wait

  (*)   Since for upgradable locks we always take intention exclusive global
        lock at the same time when obtaining the shared lock, there is no
        need to obtain such lock during the upgrade itself.
  (**)  Since intention shared global locks are compatible with all other
        type of locks we don't even have any accounting for them.
*/

const MDL_lock::bitmap_t MDL_global_lock::m_granted_incompatible[MDL_TYPE_END] =
{
  MDL_BIT(MDL_SHARED), MDL_BIT(MDL_INTENTION_EXCLUSIVE), 0, 0, 0, 0, 0, 0
};

const MDL_lock::bitmap_t MDL_global_lock::m_waiting_incompatible[MDL_TYPE_END] =
{
  MDL_BIT(MDL_SHARED), 0, 0, 0, 0, 0, 0, 0
};


/**
  Compatibility (or rather "incompatibility") matrices for per-object
  metadata lock. Arrays of bitmaps which elements specify which granted/
  waiting locks are incompatible with type of lock being requested.

  The first array specifies if particular type of request can be satisfied
  if there is granted lock of certain type.

     Request  |  Granted requests for lock   |
      type    | S  SH  SR  SW  SNW  SNRW  X  |
    ----------+------------------------------+
    S         | +   +   +   +   +    +    -  |
    SH        | +   +   +   +   +    +    -  |
    SR        | +   +   +   +   +    -    -  |
    SW        | +   +   +   +   -    -    -  |
    SNW       | +   +   +   -   -    -    -  |
    SNRW      | +   +   -   -   -    -    -  |
    X         | -   -   -   -   -    -    -  |
    SNW -> X  | -   -   -   0   0    0    0  |
    SNRW -> X | -   -   0   0   0    0    0  |

  The second array specifies if particular type of request can be satisfied
  if there is waiting request for the same lock of certain type. In other
  words it specifies what is the priority of different lock types.

     Request  |  Pending requests for lock  |
      type    | S  SH  SR  SW  SNW  SNRW  X |
    ----------+-----------------------------+
    S         | +   +   +   +   +     +   - |
    SH        | +   +   +   +   +     +   + |
    SR        | +   +   +   +   +     -   - |
    SW        | +   +   +   +   -     -   - |
    SNW       | +   +   +   +   +     +   - |
    SNRW      | +   +   +   +   +     +   - |
    X         | +   +   +   +   +     +   + |
    SNW -> X  | +   +   +   +   +     +   + |
    SNRW -> X | +   +   +   +   +     +   + |

  Here: "+" -- means that request can be satisfied
        "-" -- means that request can't be satisfied and should wait
        "0" -- means impossible situation which will trigger assert

  @note In cases then current context already has "stronger" type
        of lock on the object it will be automatically granted
        thanks to usage of the MDL_context::find_ticket() method.
*/

const MDL_lock::bitmap_t
MDL_object_lock::m_granted_incompatible[MDL_TYPE_END] =
{
  0,
  MDL_BIT(MDL_EXCLUSIVE),
  MDL_BIT(MDL_EXCLUSIVE),
  MDL_BIT(MDL_EXCLUSIVE) | MDL_BIT(MDL_SHARED_NO_READ_WRITE),
  MDL_BIT(MDL_EXCLUSIVE) | MDL_BIT(MDL_SHARED_NO_READ_WRITE) |
    MDL_BIT(MDL_SHARED_NO_WRITE),
  MDL_BIT(MDL_EXCLUSIVE) | MDL_BIT(MDL_SHARED_NO_READ_WRITE) |
    MDL_BIT(MDL_SHARED_NO_WRITE) | MDL_BIT(MDL_SHARED_WRITE),
  MDL_BIT(MDL_EXCLUSIVE) | MDL_BIT(MDL_SHARED_NO_READ_WRITE) |
    MDL_BIT(MDL_SHARED_NO_WRITE) | MDL_BIT(MDL_SHARED_WRITE) |
    MDL_BIT(MDL_SHARED_READ),
  MDL_BIT(MDL_EXCLUSIVE) | MDL_BIT(MDL_SHARED_NO_READ_WRITE) |
    MDL_BIT(MDL_SHARED_NO_WRITE) | MDL_BIT(MDL_SHARED_WRITE) |
    MDL_BIT(MDL_SHARED_READ) | MDL_BIT(MDL_SHARED_HIGH_PRIO) |
    MDL_BIT(MDL_SHARED)
};


const MDL_lock::bitmap_t
MDL_object_lock::m_waiting_incompatible[MDL_TYPE_END] =
{
  0,
  MDL_BIT(MDL_EXCLUSIVE),
  0,
  MDL_BIT(MDL_EXCLUSIVE) | MDL_BIT(MDL_SHARED_NO_READ_WRITE),
  MDL_BIT(MDL_EXCLUSIVE) | MDL_BIT(MDL_SHARED_NO_READ_WRITE) |
    MDL_BIT(MDL_SHARED_NO_WRITE),
  MDL_BIT(MDL_EXCLUSIVE),
  MDL_BIT(MDL_EXCLUSIVE),
  0
};


/**
  Check if request for the metadata lock can be satisfied given its
  current state.

  @param  type_arg       The requested lock type.
  @param  requestor_ctx  The MDL context of the requestor.

  @retval TRUE   Lock request can be satisfied
  @retval FALSE  There is some conflicting lock.

  @note In cases then current context already has "stronger" type
        of lock on the object it will be automatically granted
        thanks to usage of the MDL_context::find_ticket() method.
*/

bool
MDL_lock::can_grant_lock(enum_mdl_type type_arg,
                         MDL_context *requestor_ctx) const
{
  bool can_grant= FALSE;
  bitmap_t waiting_incompat_map= incompatible_waiting_types_bitmap()[type_arg];
  bitmap_t granted_incompat_map= incompatible_granted_types_bitmap()[type_arg];
  /*
    New lock request can be satisfied iff:
    - There are no incompatible types of satisfied requests
    in other contexts
    - There are no waiting requests which have higher priority
    than this request.
  */
  if (! (m_waiting.bitmap() & waiting_incompat_map))
  {
    if (! (m_granted.bitmap() & granted_incompat_map))
      can_grant= TRUE;
    else
    {
      Ticket_iterator it(m_granted);
      MDL_ticket *ticket;

      /* Check that the incompatible lock belongs to some other context. */
      while ((ticket= it++))
      {
        if (ticket->get_ctx() != requestor_ctx &&
            ticket->is_incompatible_when_granted(type_arg))
          break;
      }
      if (ticket == NULL)             /* Incompatible locks are our own. */
        can_grant= TRUE;
    }
  }
  return can_grant;
}


/** Remove a ticket from waiting or pending queue and wakeup up waiters. */

void MDL_lock::remove_ticket(Ticket_list MDL_lock::*list, MDL_ticket *ticket)
{
  rw_wrlock(&m_rwlock);
  (this->*list).remove_ticket(ticket);
  if (is_empty())
    mdl_locks.remove(this);
  else
  {
    /*
      There can be some contexts waiting to acquire a lock
      which now might be able to do it. Wake them up!
    */
    wake_up_waiters();
    rw_unlock(&m_rwlock);
  }
}


/**
  Check if we have any pending locks which conflict with existing
  shared lock.

  @pre The ticket must match an acquired lock.

  @return TRUE if there is a conflicting lock request, FALSE otherwise.
*/

bool MDL_lock::has_pending_conflicting_lock(enum_mdl_type type)
{
  bool result;

  mysql_mutex_assert_not_owner(&LOCK_open);

  rw_rdlock(&m_rwlock);
  result= (m_waiting.bitmap() & incompatible_granted_types_bitmap()[type]);
  rw_unlock(&m_rwlock);
  return result;
}


/**
  Check if ticket represents metadata lock of "stronger" or equal type
  than specified one. I.e. if metadata lock represented by ticket won't
  allow any of locks which are not allowed by specified type of lock.

  @return TRUE  if ticket has stronger or equal type
          FALSE otherwise.
*/

bool MDL_ticket::has_stronger_or_equal_type(enum_mdl_type type) const
{
  const MDL_lock::bitmap_t *
    granted_incompat_map= m_lock->incompatible_granted_types_bitmap();

  return ! (granted_incompat_map[type] & ~(granted_incompat_map[m_type]));
}


bool MDL_ticket::is_incompatible_when_granted(enum_mdl_type type) const
{
  return (MDL_BIT(m_type) &
          m_lock->incompatible_granted_types_bitmap()[type]);
}


bool MDL_ticket::is_incompatible_when_waiting(enum_mdl_type type) const
{
  return (MDL_BIT(m_type) &
          m_lock->incompatible_waiting_types_bitmap()[type]);
}


/**
  Check whether the context already holds a compatible lock ticket
  on an object.
  Start searching the transactional locks. If not
  found in the list of transactional locks, look at LOCK TABLES
  and HANDLER locks.

  @param mdl_request  Lock request object for lock to be acquired
  @param[out] is_transactional FALSE if we pass beyond m_trans_sentinel
                      while searching for ticket, otherwise TRUE.

  @note Tickets which correspond to lock types "stronger" than one
        being requested are also considered compatible.

  @return A pointer to the lock ticket for the object or NULL otherwise.
*/

MDL_ticket *
MDL_context::find_ticket(MDL_request *mdl_request,
                         bool *is_transactional)
{
  MDL_ticket *ticket;
  Ticket_iterator it(m_tickets);

  *is_transactional= TRUE;

  while ((ticket= it++))
  {
    if (ticket == m_trans_sentinel)
      *is_transactional= FALSE;

    if (mdl_request->key.is_equal(&ticket->m_lock->key) &&
        ticket->has_stronger_or_equal_type(mdl_request->type))
      break;
  }

  return ticket;
}


/**
  Acquire one lock with waiting for conflicting locks to go away if needed.

  @note This is an internal method which should not be used outside of MDL
        subsystem as in most cases simply waiting for conflicting locks to
        go away will lead to deadlock.

  @param mdl_request [in/out] Lock request object for lock to be acquired

  @param lock_wait_timeout [in] Seconds to wait before timeout.

  @retval  FALSE   Success. MDL_request::ticket points to the ticket
                   for the lock.
  @retval  TRUE    Failure (Out of resources or waiting is aborted),
*/

bool
MDL_context::acquire_lock(MDL_request *mdl_request, ulong lock_wait_timeout)
{
  return acquire_lock_impl(mdl_request, lock_wait_timeout);
}


/**
  Try to acquire one lock.

  Unlike exclusive locks, shared locks are acquired one by
  one. This is interface is chosen to simplify introduction of
  the new locking API to the system. MDL_context::try_acquire_lock()
  is currently used from open_table(), and there we have only one
  table to work with.

  This function may also be used to try to acquire an exclusive
  lock on a destination table, by ALTER TABLE ... RENAME.

  Returns immediately without any side effect if encounters a lock
  conflict. Otherwise takes the lock.

  FIXME: Compared to lock_table_name_if_not_cached() (from 5.1)
         it gives slightly more false negatives.

  @param mdl_request [in/out] Lock request object for lock to be acquired

  @retval  FALSE   Success. The lock may have not been acquired.
                   Check the ticket, if it's NULL, a conflicting lock
                   exists and another attempt should be made after releasing
                   all current locks and waiting for conflicting lock go
                   away (using MDL_context::wait_for_lock()).
  @retval  TRUE    Out of resources, an error has been reported.
*/

bool
MDL_context::try_acquire_lock(MDL_request *mdl_request)
{
  MDL_lock *lock;
  MDL_key *key= &mdl_request->key;
  MDL_ticket *ticket;
  bool is_transactional;

  DBUG_ASSERT(mdl_request->type < MDL_SHARED_NO_WRITE ||
              (is_lock_owner(MDL_key::GLOBAL, "", "",
                             MDL_INTENTION_EXCLUSIVE)));
  DBUG_ASSERT(mdl_request->ticket == NULL);

  /* Don't take chances in production. */
  mdl_request->ticket= NULL;
  mysql_mutex_assert_not_owner(&LOCK_open);

  /*
    Check whether the context already holds a shared lock on the object,
    and if so, grant the request.
  */
  if ((ticket= find_ticket(mdl_request, &is_transactional)))
  {
    DBUG_ASSERT(ticket->m_lock);
    DBUG_ASSERT(ticket->has_stronger_or_equal_type(mdl_request->type));
    /*
      If the request is for a transactional lock, and we found
      a transactional lock, just reuse the found ticket.

      It's possible that we found a transactional lock,
      but the request is for a HANDLER lock. In that case HANDLER
      code will clone the ticket (see below why it's needed).

      If the request is for a transactional lock, and we found
      a HANDLER lock, create a copy, to make sure that when user
      does HANDLER CLOSE, the transactional lock is not released.

      If the request is for a handler lock, and we found a
      HANDLER lock, also do the clone. HANDLER CLOSE for one alias
      should not release the lock on the table HANDLER opened through
      a different alias.
    */
    mdl_request->ticket= ticket;
    if (!is_transactional && clone_ticket(mdl_request))
    {
      /* Clone failed. */
      mdl_request->ticket= NULL;
      return TRUE;
    }
    return FALSE;
  }

  if (!(ticket= MDL_ticket::create(this, mdl_request->type)))
    return TRUE;

  /* The below call implicitly locks MDL_lock::m_rwlock on success. */
  if (!(lock= mdl_locks.find_or_insert(key)))
  {
    MDL_ticket::destroy(ticket);
    return TRUE;
  }

  if (lock->can_grant_lock(mdl_request->type, this))
  {
    ticket->m_lock= lock;
    lock->m_granted.add_ticket(ticket);
    rw_unlock(&lock->m_rwlock);

    m_tickets.push_front(ticket);

    mdl_request->ticket= ticket;
  }
  else
  {
    /* We can't get here if we allocated a new lock. */
    DBUG_ASSERT(! lock->is_empty());
    rw_unlock(&lock->m_rwlock);
    MDL_ticket::destroy(ticket);
  }

  return FALSE;
}


/**
  Create a copy of a granted ticket.
  This is used to make sure that HANDLER ticket
  is never shared with a ticket that belongs to
  a transaction, so that when we HANDLER CLOSE,
  we don't release a transactional ticket, and
  vice versa -- when we COMMIT, we don't mistakenly
  release a ticket for an open HANDLER.

  @retval TRUE   Out of memory.
  @retval FALSE  Success.
*/

bool
MDL_context::clone_ticket(MDL_request *mdl_request)
{
  MDL_ticket *ticket;

  mysql_mutex_assert_not_owner(&LOCK_open);
  /*
    By submitting mdl_request->type to MDL_ticket::create()
    we effectively downgrade the cloned lock to the level of
    the request.
  */
  if (!(ticket= MDL_ticket::create(this, mdl_request->type)))
    return TRUE;

  /* clone() is not supposed to be used to get a stronger lock. */
  DBUG_ASSERT(mdl_request->ticket->has_stronger_or_equal_type(ticket->m_type));

  ticket->m_lock= mdl_request->ticket->m_lock;
  mdl_request->ticket= ticket;

  rw_wrlock(&ticket->m_lock->m_rwlock);
  ticket->m_lock->m_granted.add_ticket(ticket);
  rw_unlock(&ticket->m_lock->m_rwlock);

  m_tickets.push_front(ticket);

  return FALSE;
}


/**
  Notify a thread holding a shared metadata lock which
  conflicts with a pending exclusive lock.

  @param thd               Current thread context
  @param conflicting_ticket  Conflicting metadata lock
*/

void notify_shared_lock(THD *thd, MDL_ticket *conflicting_ticket)
{
  /* Only try to abort locks on which we back off. */
  if (conflicting_ticket->get_type() < MDL_SHARED_NO_WRITE)
  {
    MDL_context *conflicting_ctx= conflicting_ticket->get_ctx();
    THD *conflicting_thd= conflicting_ctx->get_thd();
    DBUG_ASSERT(thd != conflicting_thd); /* Self-deadlock */

    /*
      If thread which holds conflicting lock is waiting on table-level
      lock or some other non-MDL resource we might need to wake it up
      by calling code outside of MDL.
    */
    mysql_notify_thread_having_shared_lock(thd, conflicting_thd,
                               conflicting_ctx->get_needs_thr_lock_abort());
  }
}


/**
  Auxiliary method for acquiring an exclusive lock.

  @param mdl_request  Request for the lock to be acqured.

  @param lock_wait_timeout  Seconds to wait before timeout.

  @note Should not be used outside of MDL subsystem. Instead one
        should call acquire_lock() or acquire_locks()
        methods which ensure that conditions for deadlock-free
        lock acquisition are fulfilled.

  @retval FALSE  Success
  @retval TRUE   Failure
*/

bool MDL_context::acquire_lock_impl(MDL_request *mdl_request,
                                    ulong lock_wait_timeout)
{
  MDL_lock *lock;
  MDL_ticket *ticket;
  bool not_used;
  st_my_thread_var *mysys_var= my_thread_var;
  MDL_key *key= &mdl_request->key;
  struct timespec abs_timeout;
  struct timespec abs_shortwait;
  set_timespec(abs_timeout, lock_wait_timeout);

  mysql_mutex_assert_not_owner(&LOCK_open);

  DBUG_ASSERT(mdl_request->ticket == NULL);
  /* Don't take chances in production. */
  mdl_request->ticket= NULL;

  /*
    Check whether the context already holds an exclusive lock on the object,
    and if so, grant the request.
  */
  if ((ticket= find_ticket(mdl_request, &not_used)))
  {
    DBUG_ASSERT(ticket->m_lock);
    mdl_request->ticket= ticket;
    return FALSE;
  }

  DBUG_ASSERT(mdl_request->type < MDL_SHARED_NO_WRITE ||
              is_lock_owner(MDL_key::GLOBAL, "", "", MDL_INTENTION_EXCLUSIVE));

  /* Early allocation: ticket will be needed in any case. */
  if (!(ticket= MDL_ticket::create(this, mdl_request->type)))
    return TRUE;

  /* The below call implicitly locks MDL_lock::m_rwlock on success. */
  if (!(lock= mdl_locks.find_or_insert(key)))
  {
    MDL_ticket::destroy(ticket);
    return TRUE;
  }

  ticket->m_lock= lock;

  lock->m_waiting.add_ticket(ticket);

  while (!lock->can_grant_lock(mdl_request->type, this))
  {
    wait_reset();

    if (ticket->is_upgradable_or_exclusive())
      lock->notify_shared_locks(this);

    rw_unlock(&lock->m_rwlock);

    set_deadlock_weight(mdl_request->get_deadlock_weight());
    will_wait_for(ticket);

    /* There is a shared or exclusive lock on the object. */
    DEBUG_SYNC(m_thd, "mdl_acquire_lock_wait");

    bool is_deadlock= find_deadlock();
    bool is_timeout= FALSE;
    if (!is_deadlock)
    {
      set_timespec(abs_shortwait, 1);
      bool timeout_is_near= cmp_timespec(abs_shortwait, abs_timeout) > 0;
      mdl_signal_type wait_result=
        timed_wait(timeout_is_near ? &abs_timeout : &abs_shortwait);

      if (timeout_is_near && wait_result == TIMEOUT_WAKE_UP)
        is_timeout= TRUE;
      else if (wait_result == VICTIM_WAKE_UP)
        is_deadlock= TRUE;
    }

    stop_waiting();

    if (mysys_var->abort || is_deadlock || is_timeout)
    {
      lock->remove_ticket(&MDL_lock::m_waiting, ticket);
      MDL_ticket::destroy(ticket);
      if (is_deadlock)
        my_error(ER_LOCK_DEADLOCK, MYF(0));
      else if (is_timeout)
        my_error(ER_LOCK_WAIT_TIMEOUT, MYF(0));
      return TRUE;
    }
    rw_wrlock(&lock->m_rwlock);
  }

  lock->m_waiting.remove_ticket(ticket);
  lock->m_granted.add_ticket(ticket);

  if (ticket->get_type() == MDL_EXCLUSIVE && lock->cached_object)
    (*lock->cached_object_release_hook)(lock->cached_object);
  lock->cached_object= NULL;

  rw_unlock(&lock->m_rwlock);

  m_tickets.push_front(ticket);

  mdl_request->ticket= ticket;

  return FALSE;
}


extern "C" int mdl_request_ptr_cmp(const void* ptr1, const void* ptr2)
{
  MDL_request *req1= *(MDL_request**)ptr1;
  MDL_request *req2= *(MDL_request**)ptr2;
  return req1->key.cmp(&req2->key);
}


/**
  Acquire exclusive locks. There must be no granted locks in the
  context.

  This is a replacement of lock_table_names(). It is used in
  RENAME, DROP and other DDL SQL statements.

  @param  mdl_requests  List of requests for locks to be acquired.

  @param lock_wait_timeout  Seconds to wait before timeout.

  @note The list of requests should not contain non-exclusive lock requests.
        There should not be any acquired locks in the context.

  @note Assumes that one already owns global intention exclusive lock.

  @retval FALSE  Success
  @retval TRUE   Failure
*/

bool MDL_context::acquire_locks(MDL_request_list *mdl_requests,
                                ulong lock_wait_timeout)
{
  MDL_request_list::Iterator it(*mdl_requests);
  MDL_request **sort_buf, **p_req;
  MDL_ticket *mdl_svp= mdl_savepoint();
  ssize_t req_count= static_cast<ssize_t>(mdl_requests->elements());

  if (req_count == 0)
    return FALSE;

  /*
    To reduce deadlocks, the server acquires all exclusive
    locks at once. For shared locks, try_acquire_lock() is
    used instead.
  */
  DBUG_ASSERT(m_tickets.is_empty() || m_tickets.front() == m_trans_sentinel);

  /* Sort requests according to MDL_key. */
  if (! (sort_buf= (MDL_request **)my_malloc(req_count *
                                             sizeof(MDL_request*),
                                             MYF(MY_WME))))
    return TRUE;

  for (p_req= sort_buf; p_req < sort_buf + req_count; p_req++)
    *p_req= it++;

  my_qsort(sort_buf, req_count, sizeof(MDL_request*),
           mdl_request_ptr_cmp);

  for (p_req= sort_buf; p_req < sort_buf + req_count; p_req++)
  {
    if (acquire_lock_impl(*p_req, lock_wait_timeout))
      goto err;
  }
  my_free(sort_buf, MYF(0));
  return FALSE;

err:
  /*
    Release locks we have managed to acquire so far.
    Use rollback_to_savepoint() since there may be duplicate
    requests that got assigned the same ticket.
  */
  rollback_to_savepoint(mdl_svp);
  /* Reset lock requests back to its initial state. */
  for (req_count= p_req - sort_buf, p_req= sort_buf;
       p_req < sort_buf + req_count; p_req++)
  {
    (*p_req)->ticket= NULL;
  }
  my_free(sort_buf, MYF(0));
  return TRUE;
}


/**
  Upgrade a shared metadata lock to exclusive.

  Used in ALTER TABLE, when a copy of the table with the
  new definition has been constructed.

  @param lock_wait_timeout  Seconds to wait before timeout.

  @note In case of failure to upgrade lock (e.g. because upgrader
        was killed) leaves lock in its original state (locked in
        shared mode).

  @note There can be only one upgrader for a lock or we will have deadlock.
        This invariant is ensured by code outside of metadata subsystem usually
        by obtaining some sort of exclusive table-level lock (e.g. TL_WRITE,
        TL_WRITE_ALLOW_READ) before performing upgrade of metadata lock.

  @retval FALSE  Success
  @retval TRUE   Failure (thread was killed)
*/

bool
MDL_context::upgrade_shared_lock_to_exclusive(MDL_ticket *mdl_ticket,
                                              ulong lock_wait_timeout)
{
  MDL_request mdl_xlock_request;
  MDL_ticket *mdl_svp= mdl_savepoint();
  bool is_new_ticket;

  DBUG_ENTER("MDL_ticket::upgrade_shared_lock_to_exclusive");
  DEBUG_SYNC(get_thd(), "mdl_upgrade_shared_lock_to_exclusive");

  /*
    Do nothing if already upgraded. Used when we FLUSH TABLE under
    LOCK TABLES and a table is listed twice in LOCK TABLES list.
  */
  if (mdl_ticket->m_type == MDL_EXCLUSIVE)
    DBUG_RETURN(FALSE);

  /* Only allow upgrades from MDL_SHARED_NO_WRITE/NO_READ_WRITE */
  DBUG_ASSERT(mdl_ticket->m_type == MDL_SHARED_NO_WRITE ||
              mdl_ticket->m_type == MDL_SHARED_NO_READ_WRITE);

  mdl_xlock_request.init(&mdl_ticket->m_lock->key, MDL_EXCLUSIVE);

  if (acquire_lock_impl(&mdl_xlock_request, lock_wait_timeout))
    DBUG_RETURN(TRUE);

  is_new_ticket= ! has_lock(mdl_svp, mdl_xlock_request.ticket);

  /* Merge the acquired and the original lock. @todo: move to a method. */
  rw_wrlock(&mdl_ticket->m_lock->m_rwlock);
  if (is_new_ticket)
    mdl_ticket->m_lock->m_granted.remove_ticket(mdl_xlock_request.ticket);
  /*
    Set the new type of lock in the ticket. To update state of
    MDL_lock object correctly we need to temporarily exclude
    ticket from the granted queue and then include it back.
  */
  mdl_ticket->m_lock->m_granted.remove_ticket(mdl_ticket);
  mdl_ticket->m_type= MDL_EXCLUSIVE;
  mdl_ticket->m_lock->m_granted.add_ticket(mdl_ticket);

  rw_unlock(&mdl_ticket->m_lock->m_rwlock);

  if (is_new_ticket)
  {
    m_tickets.remove(mdl_xlock_request.ticket);
    MDL_ticket::destroy(mdl_xlock_request.ticket);
  }

  DBUG_RETURN(FALSE);
}


bool MDL_lock::find_deadlock(MDL_ticket *waiting_ticket,
                             Deadlock_detection_context *deadlock_ctx)
{
  MDL_ticket *ticket;
  bool result= FALSE;

  rw_rdlock(&m_rwlock);

  Ticket_iterator granted_it(m_granted);
  Ticket_iterator waiting_it(m_waiting);

  while ((ticket= granted_it++))
  {
    if (ticket->is_incompatible_when_granted(waiting_ticket->get_type()) &&
        ticket->get_ctx() != waiting_ticket->get_ctx() &&
        ticket->get_ctx() == deadlock_ctx->start)
    {
      result= TRUE;
      goto end;
    }
  }

  while ((ticket= waiting_it++))
  {
    if (ticket->is_incompatible_when_waiting(waiting_ticket->get_type()) &&
        ticket->get_ctx() != waiting_ticket->get_ctx() &&
        ticket->get_ctx() == deadlock_ctx->start)
    {
      result= TRUE;
      goto end;
    }
  }

  granted_it.rewind();
  while ((ticket= granted_it++))
  {
    if (ticket->is_incompatible_when_granted(waiting_ticket->get_type()) &&
        ticket->get_ctx() != waiting_ticket->get_ctx() &&
        ticket->get_ctx()->find_deadlock(deadlock_ctx))
    {
      result= TRUE;
      goto end;
    }
  }

  waiting_it.rewind();
  while ((ticket= waiting_it++))
  {
    if (ticket->is_incompatible_when_waiting(waiting_ticket->get_type()) &&
        ticket->get_ctx() != waiting_ticket->get_ctx() &&
        ticket->get_ctx()->find_deadlock(deadlock_ctx))
    {
      result= TRUE;
      goto end;
    }
  }

end:
  rw_unlock(&m_rwlock);
  return result;
}


bool MDL_context::find_deadlock(Deadlock_detection_context *deadlock_ctx)
{
  bool result= FALSE;

  rw_rdlock(&m_waiting_for_lock);

  if (m_waiting_for)
  {
    /*
      QQ: should we rather be checking for NO_WAKE_UP ?

      We want to do check signal only when m_waiting_for is set
      to avoid reading left-overs from previous kills.
    */
    if (peek_signal() != VICTIM_WAKE_UP)
    {

      if (++deadlock_ctx->current_search_depth >
          deadlock_ctx->MAX_SEARCH_DEPTH)
        result= TRUE;
      else
        result= m_waiting_for->m_lock->find_deadlock(m_waiting_for,
                                                     deadlock_ctx);
      --deadlock_ctx->current_search_depth;
    }
  }

  if (result)
  {
    if (! deadlock_ctx->victim)
      deadlock_ctx->victim= this;
    else if (deadlock_ctx->victim->m_deadlock_weight >= m_deadlock_weight)
    {
      rw_unlock(&deadlock_ctx->victim->m_waiting_for_lock);
      deadlock_ctx->victim= this;
    }
    else
      rw_unlock(&m_waiting_for_lock);
  }
  else
    rw_unlock(&m_waiting_for_lock);

  return result;
}


bool MDL_context::find_deadlock()
{
  while (1)
  {
    /*
      The fact that we use fresh instance of deadlock_ctx for each
      search performed by find_deadlock() below is important, code
      responsible for victim selection relies on this.
    */
    Deadlock_detection_context deadlock_ctx(this);

    if (! find_deadlock(&deadlock_ctx))
    {
      /* No deadlocks are found! */
      break;
    }

    if (deadlock_ctx.victim != this)
    {
      deadlock_ctx.victim->awake(VICTIM_WAKE_UP);
      rw_unlock(&deadlock_ctx.victim->m_waiting_for_lock);
      /*
        After adding new arc to waiting graph we found that it participates
        in some loop (i.e. there is a deadlock). We decided to destroy this
        loop by removing some arc other than newly added. Since this doesn't
        guarantee that all loops created by addition of this arc are
        destroyed we have to repeat search.
      */
      continue;
    }
    else
    {
      DBUG_ASSERT(&deadlock_ctx.victim->m_waiting_for_lock == &m_waiting_for_lock);
      rw_unlock(&deadlock_ctx.victim->m_waiting_for_lock);
      return TRUE;
    }
  }
  return FALSE;
}


/**
  Wait until there will be no locks that conflict with lock requests
  in the given list.

  This is a part of the locking protocol and must be used by the
  acquirer of shared locks after a back-off.

  Does not acquire the locks!

  @param lock_wait_timeout  Seconds to wait before timeout.

  @retval FALSE  Success. One can try to obtain metadata locks.
  @retval TRUE   Failure (thread was killed or deadlock is possible).
*/

bool
MDL_context::wait_for_lock(MDL_request *mdl_request, ulong lock_wait_timeout)
{
  MDL_lock *lock;
  st_my_thread_var *mysys_var= my_thread_var;
  struct timespec abs_timeout;
  set_timespec(abs_timeout, lock_wait_timeout);

  mysql_mutex_assert_not_owner(&LOCK_open);

  DBUG_ASSERT(mdl_request->ticket == NULL);

  while (TRUE)
  {
    /*
      We have to check if there are some HANDLERs open by this thread
      which conflict with some pending exclusive locks. Otherwise we
      might have a deadlock in situations when we are waiting for
      pending writer to go away, which in its turn waits for HANDLER
      open by our thread.

      TODO: investigate situations in which we need to broadcast on
            COND_mdl because of above scenario.
    */
    mysql_ha_flush(m_thd);

    MDL_key *key= &mdl_request->key;

    /* The below call implicitly locks MDL_lock::m_rwlock on success. */
    if (! (lock= mdl_locks.find(key)))
      return FALSE;

    if (lock->can_grant_lock(mdl_request->type, this))
    {
      rw_unlock(&lock->m_rwlock);
      return FALSE;
    }

    MDL_ticket *pending_ticket;
    if (! (pending_ticket= MDL_ticket::create(this, mdl_request->type)))
    {
      rw_unlock(&lock->m_rwlock);
      return TRUE;
    }

    pending_ticket->m_lock= lock;

    lock->m_waiting.add_ticket(pending_ticket);

    wait_reset();
    rw_unlock(&lock->m_rwlock);

    set_deadlock_weight(MDL_DEADLOCK_WEIGHT_DML);
    will_wait_for(pending_ticket);

    bool is_deadlock= find_deadlock();
    bool is_timeout= FALSE;
    if (!is_deadlock)
    {
      mdl_signal_type wait_result= timed_wait(&abs_timeout);
      if (wait_result == TIMEOUT_WAKE_UP)
        is_timeout= TRUE;
      else if (wait_result == VICTIM_WAKE_UP)
        is_deadlock= TRUE;
    }

    stop_waiting();

    lock->remove_ticket(&MDL_lock::m_waiting, pending_ticket);
    MDL_ticket::destroy(pending_ticket);

    if (mysys_var->abort || is_deadlock || is_timeout)
    {
      if (is_deadlock)
        my_error(ER_LOCK_DEADLOCK, MYF(0));
      else if (is_timeout)
        my_error(ER_LOCK_WAIT_TIMEOUT, MYF(0));
      return TRUE;
    }
  }
  return TRUE;
}


/**
  Release lock.

  @param ticket Ticket for lock to be released.
*/

void MDL_context::release_lock(MDL_ticket *ticket)
{
  MDL_lock *lock= ticket->m_lock;
  DBUG_ENTER("MDL_context::release_lock");
  DBUG_PRINT("enter", ("db=%s name=%s", lock->key.db_name(),
                                        lock->key.name()));

  DBUG_ASSERT(this == ticket->get_ctx());
  mysql_mutex_assert_not_owner(&LOCK_open);

  if (ticket == m_trans_sentinel)
    m_trans_sentinel= ++Ticket_list::Iterator(m_tickets, ticket);

  lock->remove_ticket(&MDL_lock::m_granted, ticket);

  m_tickets.remove(ticket);
  MDL_ticket::destroy(ticket);

  DBUG_VOID_RETURN;
}


/**
  Release all locks associated with the context. If the sentinel
  is not NULL, do not release locks stored in the list after and
  including the sentinel.

  Transactional locks are added to the beginning of the list, i.e.
  stored in reverse temporal order. This allows to employ this
  function to:
  - back off in case of a lock conflict.
  - release all locks in the end of a transaction
  - rollback to a savepoint.

  The sentinel semantics is used to support LOCK TABLES
  mode and HANDLER statements: locks taken by these statements
  survive COMMIT, ROLLBACK, ROLLBACK TO SAVEPOINT.
*/

void MDL_context::release_locks_stored_before(MDL_ticket *sentinel)
{
  MDL_ticket *ticket;
  Ticket_iterator it(m_tickets);
  DBUG_ENTER("MDL_context::release_locks_stored_before");

  if (m_tickets.is_empty())
    DBUG_VOID_RETURN;

  while ((ticket= it++) && ticket != sentinel)
  {
    DBUG_PRINT("info", ("found lock to release ticket=%p", ticket));
    release_lock(ticket);
  }
  /*
    If all locks were released, then the sentinel was not present
    in the list. It must never happen because the sentinel was
    bogus, i.e. pointed to a ticket that no longer exists.
  */
  DBUG_ASSERT(! m_tickets.is_empty() || sentinel == NULL);

  DBUG_VOID_RETURN;
}


/**
  Release all locks in the context which correspond to the same name/
  object as this lock request.

  @param ticket    One of the locks for the name/object for which all
                   locks should be released.
*/

void MDL_context::release_all_locks_for_name(MDL_ticket *name)
{
  /* Use MDL_ticket::m_lock to identify other locks for the same object. */
  MDL_lock *lock= name->m_lock;

  /* Remove matching lock tickets from the context. */
  MDL_ticket *ticket;
  Ticket_iterator it_ticket(m_tickets);

  while ((ticket= it_ticket++))
  {
    DBUG_ASSERT(ticket->m_lock);
    /*
      We rarely have more than one ticket in this loop,
      let's not bother saving on pthread_cond_broadcast().
    */
    if (ticket->m_lock == lock)
      release_lock(ticket);
  }
}


/**
  Downgrade an exclusive lock to shared metadata lock.

  @param type  Type of lock to which exclusive lock should be downgraded.
*/

void MDL_ticket::downgrade_exclusive_lock(enum_mdl_type type)
{
  mysql_mutex_assert_not_owner(&LOCK_open);

  /*
    Do nothing if already downgraded. Used when we FLUSH TABLE under
    LOCK TABLES and a table is listed twice in LOCK TABLES list.
  */
  if (m_type != MDL_EXCLUSIVE)
    return;

  rw_wrlock(&m_lock->m_rwlock);
  /*
    To update state of MDL_lock object correctly we need to temporarily
    exclude ticket from the granted queue and then include it back.
  */
  m_lock->m_granted.remove_ticket(this);
  m_type= type;
  m_lock->m_granted.add_ticket(this);
  m_lock->wake_up_waiters();
  rw_unlock(&m_lock->m_rwlock);
}


/**
  Auxiliary function which allows to check if we have some kind of lock on
  a object. Returns TRUE if we have a lock of a given or stronger type.

  @param mdl_namespace Id of object namespace
  @param db            Name of the database
  @param name          Name of the object
  @param mdl_type      Lock type. Pass in the weakest type to find
                       out if there is at least some lock.

  @return TRUE if current context contains satisfied lock for the object,
          FALSE otherwise.
*/

bool
MDL_context::is_lock_owner(MDL_key::enum_mdl_namespace mdl_namespace,
                           const char *db, const char *name,
                           enum_mdl_type mdl_type)
{
  MDL_request mdl_request;
  bool is_transactional_unused;
  mdl_request.init(mdl_namespace, db, name, mdl_type);
  MDL_ticket *ticket= find_ticket(&mdl_request, &is_transactional_unused);

  DBUG_ASSERT(ticket == NULL || ticket->m_lock);

  return ticket;
}


/**
  Check if we have any pending locks which conflict with existing shared lock.

  @pre The ticket must match an acquired lock.

  @return TRUE if there is a conflicting lock request, FALSE otherwise.
*/

bool MDL_ticket::has_pending_conflicting_lock() const
{
  return m_lock->has_pending_conflicting_lock(m_type);
}


/**
  Associate pointer to an opaque object with a lock.

  @param cached_object Pointer to the object
  @param release_hook  Cleanup function to be called when MDL subsystem
                       decides to remove lock or associate another object.

  This is used to cache a pointer to TABLE_SHARE in the lock
  structure. Such caching can save one acquisition of LOCK_open
  and one table definition cache lookup for every table.

  Since the pointer may be stored only inside an acquired lock,
  the caching is only effective when there is more than one lock
  granted on a given table.

  This function has the following usage pattern:
    - try to acquire an MDL lock
    - when done, call for mdl_get_cached_object(). If it returns NULL, our
      thread has the only lock on this table.
    - look up TABLE_SHARE in the table definition cache
    - call mdl_set_cache_object() to assign the share to the opaque pointer.

 The release hook is invoked when the last shared metadata
 lock on this name is released.
*/

void
MDL_ticket::set_cached_object(void *cached_object,
                              mdl_cached_object_release_hook release_hook)
{
  DBUG_ENTER("mdl_set_cached_object");
  DBUG_PRINT("enter", ("db=%s name=%s cached_object=%p",
                        m_lock->key.db_name(), m_lock->key.name(),
                        cached_object));
  /*
    TODO: This assumption works now since we do get_cached_object()
          and set_cached_object() in the same critical section. Once
          this becomes false we will have to call release_hook here and
          use additional mutex protecting 'cached_object' member.
  */
  DBUG_ASSERT(!m_lock->cached_object);

  m_lock->cached_object= cached_object;
  m_lock->cached_object_release_hook= release_hook;

  DBUG_VOID_RETURN;
}


/**
  Get a pointer to an opaque object that associated with the lock.

  @param ticket  Lock ticket for the lock which the object is associated to.

  @return Pointer to an opaque object associated with the lock.
*/

void *MDL_ticket::get_cached_object()
{
  return m_lock->cached_object;
}


/**
  Releases metadata locks that were acquired after a specific savepoint.

  @note Used to release tickets acquired during a savepoint unit.
  @note It's safe to iterate and unlock any locks after taken after this
        savepoint because other statements that take other special locks
        cause a implicit commit (ie LOCK TABLES).

  @param mdl_savepont  The last acquired MDL lock when the
                       savepoint was set.
*/

void MDL_context::rollback_to_savepoint(MDL_ticket *mdl_savepoint)
{
  DBUG_ENTER("MDL_context::rollback_to_savepoint");

  /* If savepoint is NULL, it is from the start of the transaction. */
  release_locks_stored_before(mdl_savepoint ?
                              mdl_savepoint : m_trans_sentinel);

  DBUG_VOID_RETURN;
}


/**
  Release locks acquired by normal statements (SELECT, UPDATE,
  DELETE, etc) in the course of a transaction. Do not release
  HANDLER locks, if there are any.

  This method is used at the end of a transaction, in
  implementation of COMMIT (implicit or explicit) and ROLLBACK.
*/

void MDL_context::release_transactional_locks()
{
  DBUG_ENTER("MDL_context::release_transactional_locks");
  release_locks_stored_before(m_trans_sentinel);
  DBUG_VOID_RETURN;
}


/**
  Does this savepoint have this lock?
  
  @retval TRUE  The ticket is older than the savepoint and
                is not LT, HA or GLR ticket. Thus it belongs
                to the savepoint.
  @retval FALSE The ticket is newer than the savepoint
                or is an LT, HA or GLR ticket.
*/

bool MDL_context::has_lock(MDL_ticket *mdl_savepoint,
                           MDL_ticket *mdl_ticket)
{
  MDL_ticket *ticket;
  /* Start from the beginning, most likely mdl_ticket's been just acquired. */
  MDL_context::Ticket_iterator it(m_tickets);
  bool found_savepoint= FALSE;

  while ((ticket= it++) && ticket != m_trans_sentinel)
  {
    /*
      First met the savepoint. The ticket must be
      somewhere after it.
    */
    if (ticket == mdl_savepoint)
      found_savepoint= TRUE;
    /*
      Met the ticket. If we haven't yet met the savepoint,
      the ticket is newer than the savepoint.
    */
    if (ticket == mdl_ticket)
      return found_savepoint;
  }
  /* Reached m_trans_sentinel. The ticket must be LT, HA or GRL ticket. */
  return FALSE;
}


/**
  Rearrange the ticket to reside in the part of the list that's
  beyond m_trans_sentinel. This effectively changes the ticket
  life cycle, from automatic to manual: i.e. the ticket is no
  longer released by MDL_context::release_transactional_locks() or
  MDL_context::rollback_to_savepoint(), it must be released manually.
*/

void MDL_context::move_ticket_after_trans_sentinel(MDL_ticket *mdl_ticket)
{
  m_tickets.remove(mdl_ticket);
  if (m_trans_sentinel == NULL)
  {
    m_trans_sentinel= mdl_ticket;
    /* sic: linear from the number of transactional tickets acquired so-far! */
    m_tickets.push_back(mdl_ticket);
  }
  else
    m_tickets.insert_after(m_trans_sentinel, mdl_ticket);
}

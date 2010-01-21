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
  pthread_mutex_t m_mutex;
};


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
  typedef I_P_List<MDL_ticket,
                   I_P_List_adapter<MDL_ticket,
                                    &MDL_ticket::next_in_lock,
                                    &MDL_ticket::prev_in_lock> >
          Ticket_list;

  typedef Ticket_list::Iterator Ticket_iterator;

public:
  /** The key of the object (data) being protected. */
  MDL_key key;
  /** List of granted tickets for this lock. */
  Ticket_list granted;
  /** Tickets for contexts waiting to acquire a shared lock. */
  Ticket_list waiting_shared;
  /**
    Tickets for contexts waiting to acquire an exclusive lock.
    There can be several upgraders and active exclusive
    locks belonging to the same context. E.g.
    in case of RENAME t1 to t2, t2 to t3, we attempt to
    exclusively lock t2 twice.
  */
  Ticket_list waiting_exclusive;
  void   *cached_object;
  mdl_cached_object_release_hook cached_object_release_hook;
  /** Mutex protecting this lock context. */
  pthread_mutex_t m_mutex;

  bool is_empty() const
  {
    return (granted.is_empty() && waiting_shared.is_empty() &&
            waiting_exclusive.is_empty());
  }

  bool has_pending_exclusive_lock()
  {
    bool has_locks;
    pthread_mutex_lock(&m_mutex);
    has_locks= ! waiting_exclusive.is_empty();
    pthread_mutex_unlock(&m_mutex);
    return has_locks;
  }
  virtual bool can_grant_lock(const MDL_context *requestor_ctx,
                              enum_mdl_type type, bool is_upgrade)= 0;
  virtual void wake_up_waiters()= 0;

  inline static MDL_lock *create(const MDL_key *key);

  MDL_lock(const MDL_key *key_arg)
  : key(key_arg),
    cached_object(NULL),
    cached_object_release_hook(NULL),
    m_ref_usage(0),
    m_ref_release(0),
    m_is_destroyed(FALSE)
  {
    pthread_mutex_init(&m_mutex, NULL);
  }

  virtual ~MDL_lock()
  {
    pthread_mutex_destroy(&m_mutex);
  }
  inline static void destroy(MDL_lock *lock);
public:
  /**
    These three members are used to make it possible to separate
    the mdl_locks.m_mutex mutex and MDL_lock::m_mutex in
    MDL_map::find_or_insert() for increased scalability.
    The 'm_is_destroyed' member is only set by destroyers that
    have both the mdl_locks.m_mutex and MDL_lock::m_mutex, thus
    holding any of the mutexes is sufficient to read it.
    The 'm_ref_usage; is incremented under protection by
    mdl_locks.m_mutex, but when 'm_is_destroyed' is set to TRUE, this
    member is moved to be protected by the MDL_lock::m_mutex.
    This means that the MDL_map::find_or_insert() which only
    holds the MDL_lock::m_mutex can compare it to 'm_ref_release'
    without acquiring mdl_locks.m_mutex again and if equal it can also
    destroy the lock object safely.
    The 'm_ref_release' is incremented under protection by
    MDL_lock::m_mutex.
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

  virtual bool can_grant_lock(const MDL_context *requestor_ctx,
                              enum_mdl_type type, bool is_upgrade);
  virtual void wake_up_waiters();
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

  virtual bool can_grant_lock(const MDL_context *requestor_ctx,
                              enum_mdl_type type, bool is_upgrade);
  virtual void wake_up_waiters();
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
  pthread_mutex_init(&m_mutex, NULL);
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
  pthread_mutex_destroy(&m_mutex);
  my_hash_free(&m_locks);
}


/**
  Find MDL_lock object corresponding to the key, create it
  if it does not exist.

  @retval non-NULL - Success. MDL_lock instance for the key with
                     locked MDL_lock::m_mutex.
  @retval NULL     - Failure (OOM).
*/

MDL_lock* MDL_map::find_or_insert(const MDL_key *mdl_key)
{
  MDL_lock *lock;

retry:
  pthread_mutex_lock(&m_mutex);
  if (!(lock= (MDL_lock*) my_hash_search(&m_locks,
                                         mdl_key->ptr(),
                                         mdl_key->length())))
  {
    lock= MDL_lock::create(mdl_key);
    if (!lock || my_hash_insert(&m_locks, (uchar*)lock))
    {
      pthread_mutex_unlock(&m_mutex);
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
                     MDL_lock::m_mutex.
  @retval NULL     - There was no MDL_lock for the key.
*/

MDL_lock* MDL_map::find(const MDL_key *mdl_key)
{
  MDL_lock *lock;

retry:
  pthread_mutex_lock(&m_mutex);
  if (!(lock= (MDL_lock*) my_hash_search(&m_locks,
                                         mdl_key->ptr(),
                                         mdl_key->length())))
  {
    pthread_mutex_unlock(&m_mutex);
    return NULL;
  }

  if (move_from_hash_to_lock_mutex(lock))
    goto retry;

  return lock;
}


/**
  Release mdl_locks.m_mutex mutex and lock MDL_lock::m_mutex for lock
  object from the hash. Handle situation when object was released
  while the held no mutex.

  @retval FALSE - Success.
  @retval TRUE  - Object was released while we held no mutex, caller
                  should re-try looking up MDL_lock object in the hash.
*/

bool MDL_map::move_from_hash_to_lock_mutex(MDL_lock *lock)
{
  DBUG_ASSERT(! lock->m_is_destroyed);
  safe_mutex_assert_owner(&m_mutex);

  /*
    We increment m_ref_usage which is a reference counter protected by
    mdl_locks.m_mutex under the condition it is present in the hash and
    m_is_destroyed is FALSE.
  */
  lock->m_ref_usage++;
  pthread_mutex_unlock(&m_mutex);

  pthread_mutex_lock(&lock->m_mutex);
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
    pthread_mutex_unlock(&lock->m_mutex);
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

  safe_mutex_assert_owner(&lock->m_mutex);

  if (lock->cached_object)
    (*lock->cached_object_release_hook)(lock->cached_object);

  /*
    Destroy the MDL_lock object, but ensure that anyone that is
    holding a reference to the object is not remaining, if so he
    has the responsibility to release it.

    Setting of m_is_destroyed to TRUE while holding _both_
    mdl_locks.m_mutex and MDL_lock::m_mutex mutexes transfers the
    protection of m_ref_usage from mdl_locks.m_mutex to
    MDL_lock::m_mutex while removal of object from the hash makes
    it read-only.  Therefore whoever acquires MDL_lock::m_mutex next
    will see most up to date version of m_ref_usage.

    This means that when m_is_destroyed is TRUE and we hold the
    MDL_lock::m_mutex we can safely read the m_ref_usage
    member.
  */
  pthread_mutex_lock(&m_mutex);
  my_hash_delete(&m_locks, (uchar*) lock);
  lock->m_is_destroyed= TRUE;
  ref_usage= lock->m_ref_usage;
  ref_release= lock->m_ref_release;
  pthread_mutex_unlock(&lock->m_mutex);
  pthread_mutex_unlock(&m_mutex);
  if (ref_usage == ref_release)
    MDL_lock::destroy(lock);
}


/**
  Initialize a metadata locking context.

  This is to be called when a new server connection is created.
*/

MDL_context::MDL_context()
  :m_lt_or_ha_sentinel(NULL),
  m_thd(NULL)
{
  pthread_cond_init(&m_ctx_wakeup_cond, NULL);
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
  pthread_cond_destroy(&m_ctx_wakeup_cond);
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
                                         pthread_cond_t *cond,
                                         pthread_mutex_t *mutex,
                                         const char *calling_func,
                                         const char *calling_file,
                                         const unsigned int calling_line)
{
  safe_mutex_assert_owner(mutex);

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
                                 pthread_mutex_t *mutex,
                                 const char* old_msg,
                                 const char *calling_func,
                                 const char *calling_file,
                                 const unsigned int calling_line)
{
  DBUG_ASSERT(mutex == mysys_var->current_mutex);

  pthread_mutex_unlock(mutex);
  pthread_mutex_lock(&mysys_var->mutex);
  mysys_var->current_mutex= 0;
  mysys_var->current_cond= 0;
  pthread_mutex_unlock(&mysys_var->mutex);

  DEBUG_SYNC(thd, "mdl_exit_cond");

  (void) set_thd_proc_info(thd, old_msg, calling_func,
                           calling_file, calling_line);
}


/**
  Check if request for the global metadata lock can be satisfied given
  its current state,

  @param  requestor_ctx  The context that identifies the owner of the request.
  @param  type_arg       The requested type of global lock. Usually derived
                         from the type of lock on individual object to be
                         requested. See table below.
  @param  is_upgrade     TRUE if we are performing lock upgrade (not unused).

  @retval TRUE  - Lock request can be satisfied
  @retval FALSE - There is some conflicting lock

  Here is a compatibility matrix defined by this function:

                  |             | Satisfied or pending requests
                  |             | for global metadata lock
  ----------------+-------------+--------------------------------------------
  Type of request | Correspond. |
  for indiv. lock | global lock | Active-S  Pending-S  Active-IS(**) Active-IX
  ----------------+-------------+--------------------------------------------
  S, high-prio S  |   IS        |    +         +          +             +
  upgradable S    |   IX        |    -         -          +             +
  X               |   IX        |    -         -          +             +
  S upgraded to X |   IX (*)    |    0         +          +             +

  Here: "+" -- means that request can be satisfied
        "-" -- means that request can't be satisfied and should wait
        "0" -- means impossible situation.

  (*)  Since for upgradable shared locks we always take intention exclusive
       global lock at the same time when obtaining the shared lock, there
       is no need to obtain such lock during the upgrade itself.
  (**) Since intention shared global locks are compatible with all other
       type of locks we don't even have any accounting for them.
*/

bool
MDL_global_lock::can_grant_lock(const MDL_context *requestor_ctx,
                                enum_mdl_type type_arg,
                                bool is_upgrade)
{
  switch (type_arg)
  {
  case MDL_SHARED:
    if (! granted.is_empty() && granted.front()->m_type == MDL_INTENTION_EXCLUSIVE)
    {
      /*
        We are going to obtain global shared lock and there is active
        intention exclusive lock. Have to wait.
      */
      return FALSE;
    }
    return TRUE;
    break;
  case MDL_INTENTION_EXCLUSIVE:
    if ((! granted.is_empty() && granted.front()->m_type == MDL_SHARED) ||
        ! waiting_shared.is_empty())
    {
      /*
        We are going to obtain intention exclusive global lock and
        there is active or pending shared global lock. Have to wait.
      */
      return FALSE;
    }
    else
      return TRUE;
    break;
  default:
    DBUG_ASSERT(0);
    break;
  }
  return FALSE;
}


/**
  Wake up contexts which are waiting to acquire the global
  metadata lock and which may succeed now, when we released it, or
  removed a blocking request for it from the waiters list.
  The latter can happen when the context trying to acquire the
  global shared lock is killed.
*/

void MDL_global_lock::wake_up_waiters()
{
  /*
    If there are no active locks or they are of INTENTION
    EXCLUSIVE type and there are no pending requests for global
    SHARED lock, wake up contexts waiting for an INTENTION
    EXCLUSIVE lock.
    This happens when we release the global SHARED lock or abort
    or remove a pending request for it, i.e. abort the
    context waiting for it.
  */
  if ((granted.is_empty() ||
       granted.front()->m_type == MDL_INTENTION_EXCLUSIVE) &&
      waiting_shared.is_empty() && ! waiting_exclusive.is_empty())
  {
    MDL_lock::Ticket_iterator it(waiting_exclusive);
    MDL_ticket *awake_ticket;
    while ((awake_ticket= it++))
      awake_ticket->get_ctx()->awake();
  }

  /*
    If there are no active locks, wake up contexts waiting for
    the global shared lock (happens when an INTENTION EXCLUSIVE
    lock is released).

    We don't wake up contexts waiting for the global shared lock
    if there is an active global shared lock since such situation
    is transient and in it contexts marked as waiting for global
    shared lock must be already woken up and simply have not
    managed to update lock object yet.
  */
  if (granted.is_empty() &&
      ! waiting_shared.is_empty())
  {
    MDL_lock::Ticket_iterator it(waiting_shared);
    MDL_ticket *awake_ticket;
    while ((awake_ticket= it++))
      awake_ticket->get_ctx()->awake();
  }
}


/**
  Check if request for the per-object lock can be satisfied given current
  state of the lock.

  @param  requestor_ctx  The context that identifies the owner of the request.
  @param  type_arg       The requested lock type.
  @param  is_upgrade     Must be set to TRUE when we are upgrading
                         a shared upgradable lock to exclusive.

  @retval TRUE   Lock request can be satisfied
  @retval FALSE  There is some conflicting lock.

  This function defines the following compatibility matrix for metadata locks:

                  | Satisfied or pending requests which we have in MDL_lock
  ----------------+---------------------------------------------------------
  Current request | Active-S  Pending-X Active-X Act-S-pend-upgrade-to-X
  ----------------+---------------------------------------------------------
  S, upgradable S |    +         -         - (*)           -
  High-prio S     |    +         +         -               +
  X               |    -         +         -               -
  S upgraded to X |    - (**)    +         0               0

  Here: "+" -- means that request can be satisfied
        "-" -- means that request can't be satisfied and should wait
        "0" -- means impossible situation which will trigger assert

  (*)  Unless active exclusive lock belongs to the same context as shared
       lock being requested.
  (**) Unless all active shared locks belong to the same context as one
       being upgraded.
*/

bool
MDL_object_lock::can_grant_lock(const MDL_context *requestor_ctx,
                                enum_mdl_type type_arg,
                                bool is_upgrade)
{
  bool can_grant= FALSE;

  switch (type_arg) {
  case MDL_SHARED:
  case MDL_SHARED_UPGRADABLE:
  case MDL_SHARED_HIGH_PRIO:
    if (granted.is_empty() || granted.front()->is_shared())
    {
      /* Pending exclusive locks have higher priority over shared locks. */
      if (waiting_exclusive.is_empty() || type_arg == MDL_SHARED_HIGH_PRIO)
        can_grant= TRUE;
    }
    else if (granted.front()->get_ctx() == requestor_ctx)
    {
      /*
        When exclusive lock comes from the same context we can satisfy our
        shared lock. This is required for CREATE TABLE ... SELECT ... and
        ALTER VIEW ... AS ....
      */
      can_grant= TRUE;
    }
    break;
  case MDL_EXCLUSIVE:
    if (is_upgrade)
    {
      /* We are upgrading MDL_SHARED to MDL_EXCLUSIVE. */
      MDL_ticket *conflicting_ticket;
      MDL_lock::Ticket_iterator it(granted);

      /*
        There should be no active exclusive locks since we own shared lock
        on the object.
      */
      DBUG_ASSERT(granted.front()->is_shared());

      while ((conflicting_ticket= it++))
      {
        /*
          When upgrading shared lock to exclusive one we can have other shared
          locks for the same object in the same context, e.g. in case when several
          instances of TABLE are open.
        */
        if (conflicting_ticket->get_ctx() != requestor_ctx)
          break;
      }
      /* Grant lock if there are no conflicting shared locks. */
      if (conflicting_ticket == NULL)
        can_grant= TRUE;
      break;
    }
    else if (granted.is_empty())
    {
      /*
        We are trying to acquire fresh MDL_EXCLUSIVE and there are no active
        shared or exclusive locks.
      */
      can_grant= TRUE;
    }
    break;
  default:
    DBUG_ASSERT(0);
  }
  return can_grant;
}


/**
  Wake up contexts which are waiting to acquire lock on individual object
  and which may succeed now, when we released some lock on it or removed
  some pending request from its waiters list (the latter can happen, for
  example, when context trying to acquire exclusive lock is killed).
*/

void MDL_object_lock::wake_up_waiters()
{
  /*
    There are no active locks or they are of shared type.
    We have to wake up contexts waiting for shared lock even if there is
    a pending exclusive lock as some them might be trying to acquire high
    priority shared lock.
  */
  if ((granted.is_empty() || granted.front()->is_shared()) &&
      ! waiting_shared.is_empty())
  {
    MDL_lock::Ticket_iterator it(waiting_shared);
    MDL_ticket *waiting_ticket;
    while ((waiting_ticket= it++))
      waiting_ticket->get_ctx()->awake();
  }

  /*
    There are no active locks (shared or exclusive).
    Wake up contexts waiting to acquire exclusive locks.
  */
  if (granted.is_empty() && ! waiting_exclusive.is_empty())
  {
    MDL_lock::Ticket_iterator it(waiting_exclusive);
    MDL_ticket *waiting_ticket;
    while ((waiting_ticket= it++))
      waiting_ticket->get_ctx()->awake();
  }
}


/**
  Check whether the context already holds a compatible lock ticket
  on an object.
  Start searching the transactional locks. If not
  found in the list of transactional locks, look at LOCK TABLES
  and HANDLER locks.

  @param mdl_request  Lock request object for lock to be acquired
  @param[out] is_lt_or_ha  Did we pass beyond m_lt_or_ha_sentinel while
                            searching for ticket?

  @return A pointer to the lock ticket for the object or NULL otherwise.
*/

MDL_ticket *
MDL_context::find_ticket(MDL_request *mdl_request,
                         bool *is_lt_or_ha)
{
  MDL_ticket *ticket;
  Ticket_iterator it(m_tickets);

  *is_lt_or_ha= FALSE;

  while ((ticket= it++))
  {
    if (ticket == m_lt_or_ha_sentinel)
      *is_lt_or_ha= TRUE;

    if (mdl_request->type == ticket->m_type &&
        mdl_request->key.is_equal(&ticket->m_lock->key))
      break;
  }

  return ticket;
}


/**
  Try to acquire global intention exclusive lock.

  @param[in/out]  mdl_request  Lock request object for lock to be acquired

  @retval  FALSE   Success. The lock may have not been acquired.
                   One needs to check value of 'MDL_request::ticket'
                   to find out what has happened.
  @retval  TRUE    Error.
*/

bool
MDL_context::
try_acquire_global_intention_exclusive_lock(MDL_request *mdl_request)
{
  DBUG_ASSERT(mdl_request->key.mdl_namespace() == MDL_key::GLOBAL &&
              mdl_request->type == MDL_INTENTION_EXCLUSIVE);

  if (is_global_lock_owner(MDL_SHARED))
  {
    my_error(ER_CANT_UPDATE_WITH_READLOCK, MYF(0));
    return TRUE;
  }

  return try_acquire_lock_impl(mdl_request);
}


/**
  Acquire one lock with waiting for conflicting locks to go away if needed.

  @note This is an internal method which should not be used outside of MDL
        subsystem as in most cases simply waiting for conflicting locks to
        go away will lead to deadlock.

  @param mdl_request [in/out] Lock request object for lock to be acquired

  @retval  FALSE   Success. MDL_request::ticket points to the ticket
                   for the lock.
  @retval  TRUE    Failure (Out of resources or waiting is aborted),
*/

bool
MDL_context::acquire_lock_impl(MDL_request *mdl_request)
{
  bool not_used;
  MDL_ticket *ticket;
  MDL_key *key= &mdl_request->key;
  MDL_lock *lock;
  const char *old_msg;
  st_my_thread_var *mysys_var= my_thread_var;

  DBUG_ASSERT(mdl_request->ticket == NULL);
  safe_mutex_assert_not_owner(&LOCK_open);

  /*
    Grant lock without waiting if this context already owns this type of lock
    on this object.

    The fact that we don't wait in such situation allows to avoid deadlocks
    in cases when pending request for global shared lock pops up after the
    moment when thread has acquired its first intention exclusive lock but
    before it has requested the second instance of such lock.
  */
  if ((mdl_request->ticket= find_ticket(mdl_request, &not_used)))
    return FALSE;

  if (! (ticket= MDL_ticket::create(this, mdl_request->type)))
    return TRUE;

  /* The below call also implicitly locks MDL_lock::m_mutex. */
  if (! (lock= mdl_locks.find_or_insert(key)))
  {
    MDL_ticket::destroy(ticket);
    return TRUE;
  }

  old_msg= MDL_ENTER_COND(m_thd, mysys_var, &m_ctx_wakeup_cond,
                          &lock->m_mutex);

  if (! lock->can_grant_lock(this, mdl_request->type, FALSE))
  {
    if (mdl_request->is_shared())
      lock->waiting_shared.push_front(ticket);
    else
      lock->waiting_exclusive.push_front(ticket);

    do
    {
      pthread_cond_wait(&m_ctx_wakeup_cond, &lock->m_mutex);
    }
    while (! lock->can_grant_lock(this, mdl_request->type, FALSE) &&
           ! mysys_var->abort);

    if (mysys_var->abort)
    {
      /*
        We have to do MDL_EXIT_COND here and then re-acquire the lock
        as there is a chance that we will destroy MDL_lock object and
        won't be able to call MDL_EXIT_COND after it.
      */
      MDL_EXIT_COND(m_thd, mysys_var, &lock->m_mutex, old_msg);

      pthread_mutex_lock(&lock->m_mutex);
      /* Get rid of pending ticket. */
      if (mdl_request->is_shared())
        lock->waiting_shared.remove(ticket);
      else
        lock->waiting_exclusive.remove(ticket);
      if (lock->is_empty())
        mdl_locks.remove(lock);
      else
      {
        lock->wake_up_waiters();
        pthread_mutex_unlock(&lock->m_mutex);
      }
      MDL_ticket::destroy(ticket);
      return TRUE;
    }

    if (mdl_request->is_shared())
      lock->waiting_shared.remove(ticket);
    else
      lock->waiting_exclusive.remove(ticket);
  }

  lock->granted.push_front(ticket);
  MDL_EXIT_COND(m_thd, mysys_var, &lock->m_mutex, old_msg);

  ticket->m_state= MDL_ACQUIRED;
  ticket->m_lock= lock;

  m_tickets.push_front(ticket);

  mdl_request->ticket= ticket;

  return FALSE;
}


/**
  Acquire global intention exclusive lock.

  @param[in]  mdl_request  Lock request object for lock to be acquired

  @retval  FALSE   Success. The lock has been acquired.
  @retval  TRUE    Error.
*/

bool
MDL_context::acquire_global_intention_exclusive_lock(MDL_request *mdl_request)
{
  DBUG_ASSERT(mdl_request->key.mdl_namespace() == MDL_key::GLOBAL &&
              mdl_request->type == MDL_INTENTION_EXCLUSIVE);

  if (is_global_lock_owner(MDL_SHARED))
  {
    my_error(ER_CANT_UPDATE_WITH_READLOCK, MYF(0));
    return TRUE;
  }

  /*
    If this is a non-recursive attempt to acquire global intention
    exclusive lock we might have to wait until active global shared
    lock or pending requests will go away. Since we won't hold any
    resources (except associated with open HANDLERs) while doing it
    deadlocks are not possible,
  */
  DBUG_ASSERT(is_global_lock_owner(MDL_INTENTION_EXCLUSIVE) ||
              ! has_locks() ||
              (m_lt_or_ha_sentinel &&
               m_tickets.front() == m_lt_or_ha_sentinel));

  return acquire_lock_impl(mdl_request);
}


/**
  Try to acquire one lock.

  @param mdl_request [in/out] Lock request object for lock to be acquired

  @retval  FALSE   Success. The lock may have not been acquired.
                   Check the ticket, if it's NULL, a conflicting lock
                   exists.
  @retval  TRUE    Out of resources, an error has been reported.
*/

bool
MDL_context::try_acquire_lock_impl(MDL_request *mdl_request)
{
  MDL_lock *lock;
  MDL_key *key= &mdl_request->key;
  MDL_ticket *ticket;
  bool is_lt_or_ha;

  DBUG_ASSERT(mdl_request->ticket == NULL);

  /* Don't take chances in production. */
  mdl_request->ticket= NULL;
  safe_mutex_assert_not_owner(&LOCK_open);

  /*
    Check whether the context already holds a shared lock on the object,
    and if so, grant the request.
  */
  if ((ticket= find_ticket(mdl_request, &is_lt_or_ha)))
  {
    DBUG_ASSERT(ticket->m_state == MDL_ACQUIRED);
    DBUG_ASSERT(ticket->m_type == mdl_request->type);
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
    if (is_lt_or_ha && clone_ticket(mdl_request))
    {
      /* Clone failed. */
      mdl_request->ticket= NULL;
      return TRUE;
    }
    return FALSE;
  }

  if (!(ticket= MDL_ticket::create(this, mdl_request->type)))
    return TRUE;

  /* The below call also implicitly locks MDL_lock::m_mutex. */
  if (!(lock= mdl_locks.find_or_insert(key)))
  {
    MDL_ticket::destroy(ticket);
    return TRUE;
  }

  if (lock->can_grant_lock(this, mdl_request->type, FALSE))
  {
    lock->granted.push_front(ticket);
    pthread_mutex_unlock(&lock->m_mutex);

    ticket->m_state= MDL_ACQUIRED;
    ticket->m_lock= lock;

    m_tickets.push_front(ticket);

    mdl_request->ticket= ticket;
  }
  else
  {
    /* We can't get here if we allocated a new lock. */
    DBUG_ASSERT(! lock->is_empty());
    pthread_mutex_unlock(&lock->m_mutex);
    MDL_ticket::destroy(ticket);
  }

  return FALSE;
}


/**
  Try to acquire one shared lock.

  Unlike exclusive locks, shared locks are acquired one by
  one. This is interface is chosen to simplify introduction of
  the new locking API to the system. MDL_context::try_acquire_shared_lock()
  is currently used from open_table(), and there we have only one
  table to work with.

  In future we may consider allocating multiple shared locks at once.

  @param mdl_request [in/out] Lock request object for lock to be acquired

  @retval  FALSE   Success. The lock may have not been acquired.
                   Check the ticket, if it's NULL, a conflicting lock
                   exists and another attempt should be made after releasing
                   all current locks and waiting for conflicting lock go
                   away (using MDL_context::wait_for_locks()).
  @retval  TRUE    Out of resources, an error has been reported.
*/

bool
MDL_context::try_acquire_shared_lock(MDL_request *mdl_request)
{
  DBUG_ASSERT(mdl_request->is_shared());
  DBUG_ASSERT(mdl_request->type != MDL_SHARED_UPGRADABLE ||
              is_global_lock_owner(MDL_INTENTION_EXCLUSIVE));

  return try_acquire_lock_impl(mdl_request);
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

  safe_mutex_assert_not_owner(&LOCK_open);
  /* Only used for HANDLER. */
  DBUG_ASSERT(mdl_request->ticket && mdl_request->ticket->is_shared());

  if (!(ticket= MDL_ticket::create(this, mdl_request->type)))
    return TRUE;

  ticket->m_state= MDL_ACQUIRED;
  ticket->m_lock= mdl_request->ticket->m_lock;
  mdl_request->ticket= ticket;

  pthread_mutex_lock(&ticket->m_lock->m_mutex);
  ticket->m_lock->granted.push_front(ticket);
  pthread_mutex_unlock(&ticket->m_lock->m_mutex);

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
  if (conflicting_ticket->is_shared())
  {
    THD *conflicting_thd= conflicting_ticket->get_ctx()->get_thd();
    DBUG_ASSERT(thd != conflicting_thd); /* Self-deadlock */

    /*
      If the thread that holds the conflicting lock is waiting in MDL
      subsystem it has to be woken up by calling MDL_context::awake().
    */
    conflicting_ticket->get_ctx()->awake();
    /*
      If it is waiting on table-level lock or some other non-MDL resource
      we delegate its waking up to code outside of MDL.
    */
    mysql_notify_thread_having_shared_lock(thd, conflicting_thd);
  }
}


/**
  Auxiliary method for acquiring an exclusive lock.

  @param mdl_request  Request for the lock to be acqured.

  @note Should not be used outside of MDL subsystem. Instead one should
        call acquire_exclusive_lock() or acquire_exclusive_locks() methods
        which ensure that conditions for deadlock-free lock acquisition are
        fulfilled.

  @retval FALSE  Success
  @retval TRUE   Failure
*/

bool MDL_context::acquire_exclusive_lock_impl(MDL_request *mdl_request)
{
  MDL_lock *lock;
  const char *old_msg;
  MDL_ticket *ticket;
  bool not_used;
  st_my_thread_var *mysys_var= my_thread_var;
  MDL_key *key= &mdl_request->key;

  DBUG_ASSERT(mdl_request->type == MDL_EXCLUSIVE &&
              mdl_request->ticket == NULL);

  safe_mutex_assert_not_owner(&LOCK_open);

  /* Don't take chances in production. */
  mdl_request->ticket= NULL;

  /*
    Check whether the context already holds an exclusive lock on the object,
    and if so, grant the request.
  */
  if ((ticket= find_ticket(mdl_request, &not_used)))
  {
    DBUG_ASSERT(ticket->m_state == MDL_ACQUIRED);
    DBUG_ASSERT(ticket->m_type == MDL_EXCLUSIVE);
    mdl_request->ticket= ticket;
    return FALSE;
  }

  DBUG_ASSERT(is_global_lock_owner(MDL_INTENTION_EXCLUSIVE));

  /* Early allocation: ticket will be needed in any case. */
  if (!(ticket= MDL_ticket::create(this, mdl_request->type)))
    return TRUE;

  /* The below call also implicitly locks MDL_lock::m_mutex. */
  if (!(lock= mdl_locks.find_or_insert(key)))
  {
    MDL_ticket::destroy(ticket);
    return TRUE;
  }

  lock->waiting_exclusive.push_front(ticket);

  old_msg= MDL_ENTER_COND(m_thd, mysys_var, &m_ctx_wakeup_cond,
                          &lock->m_mutex);

  while (!lock->can_grant_lock(this, mdl_request->type, FALSE))
  {
    if (m_lt_or_ha_sentinel)
    {
      /*
        We're about to start waiting. Don't do it if we have
        HANDLER locks (we can't have any other locks here).
        Waiting with locks may lead to a deadlock.

        We have to do MDL_EXIT_COND here and then re-acquire the
        lock as there is a chance that we will destroy MDL_lock
        object and won't be able to call MDL_EXIT_COND after it.
      */
      MDL_EXIT_COND(m_thd, mysys_var, &lock->m_mutex, old_msg);

      pthread_mutex_lock(&lock->m_mutex);
      /* Get rid of pending ticket. */
      lock->waiting_exclusive.remove(ticket);
      if (lock->is_empty())
        mdl_locks.remove(lock);
      else
      {
        /*
          There can be some contexts waiting to acquire shared
          lock which now might be able to do it. Wake them up!
        */
        lock->wake_up_waiters();
        pthread_mutex_unlock(&lock->m_mutex);
      }
      MDL_ticket::destroy(ticket);
      my_error(ER_LOCK_DEADLOCK, MYF(0));
      return TRUE;
    }

    MDL_ticket *conflicting_ticket;
    MDL_lock::Ticket_iterator it(lock->granted);

    while ((conflicting_ticket= it++))
      notify_shared_lock(m_thd, conflicting_ticket);

    /* There is a shared or exclusive lock on the object. */
    DEBUG_SYNC(m_thd, "mdl_acquire_exclusive_locks_wait");

    /*
      Another thread might have obtained a shared MDL lock on some table
      but has not yet opened it and/or tried to obtain data lock on it.
      Also invocation of acquire_exclusive_lock() method and consequently
      first call to notify_shared_lock() might have happened right after
      thread holding shared metadata lock in wait_for_locks() method
      checked that there are no pending conflicting locks but before
      it has started waiting.
      In both these cases we need to sleep until these threads will start
      waiting and try to abort them once again.

      QQ: What is the optimal value for this sleep?
    */
    struct timespec abstime;
    set_timespec(abstime, 1);
    pthread_cond_timedwait(&m_ctx_wakeup_cond, &lock->m_mutex, &abstime);

    if (mysys_var->abort)
    {
      /*
        We have to do MDL_EXIT_COND here and then re-acquire the lock
        as there is a chance that we will destroy MDL_lock object and
        won't be able to call MDL_EXIT_COND after it.
      */
      MDL_EXIT_COND(m_thd, mysys_var, &lock->m_mutex, old_msg);

      pthread_mutex_lock(&lock->m_mutex);
      /* Get rid of pending ticket. */
      lock->waiting_exclusive.remove(ticket);
      if (lock->is_empty())
        mdl_locks.remove(lock);
      else
      {
        /*
          There can be some contexts waiting to acquire shared
          lock which now might be able to do it. Wake them up!
        */
        lock->wake_up_waiters();
        pthread_mutex_unlock(&lock->m_mutex);
      }
      MDL_ticket::destroy(ticket);
      return TRUE;
    }
  }

  lock->waiting_exclusive.remove(ticket);
  lock->granted.push_front(ticket);

  if (lock->cached_object)
    (*lock->cached_object_release_hook)(lock->cached_object);
  lock->cached_object= NULL;

  MDL_EXIT_COND(m_thd, mysys_var, &lock->m_mutex, old_msg);

  ticket->m_state= MDL_ACQUIRED;
  ticket->m_lock= lock;

  m_tickets.push_front(ticket);

  mdl_request->ticket= ticket;

  return FALSE;
}


/**
  Acquire an exclusive lock.

  @param mdl_request  Request for the lock to be acqured.

  @note Assumes that one already owns global intention exclusive lock.

  @retval FALSE  Success
  @retval TRUE   Failure
*/

bool MDL_context::acquire_exclusive_lock(MDL_request *mdl_request)
{
  /* Exclusive locks must always be acquired first, all at once. */
  DBUG_ASSERT(! m_tickets.is_empty() &&
              m_tickets.front()->m_lock->key.mdl_namespace() == MDL_key::GLOBAL &&
              ++Ticket_list::Iterator(m_tickets) == m_lt_or_ha_sentinel);

  return acquire_exclusive_lock_impl(mdl_request);
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

  @note The list of requests should not contain non-exclusive lock requests.
        There should not be any acquired locks in the context.

  @note Assumes that one already owns global intention exclusive lock.

  @retval FALSE  Success
  @retval TRUE   Failure
*/

bool MDL_context::acquire_exclusive_locks(MDL_request_list *mdl_requests)
{
  MDL_request_list::Iterator it(*mdl_requests);
  MDL_request **sort_buf;
  uint i;

  /*
    Exclusive locks must always be acquired first, all at once.
  */
  DBUG_ASSERT(! m_tickets.is_empty() &&
              m_tickets.front()->m_lock->key.mdl_namespace() == MDL_key::GLOBAL &&
              ++Ticket_list::Iterator(m_tickets) == m_lt_or_ha_sentinel);

  if (mdl_requests->is_empty())
    return FALSE;

  /* Sort requests according to MDL_key. */
  if (! (sort_buf= (MDL_request **)my_malloc(mdl_requests->elements() *
                                             sizeof(MDL_request *),
                                             MYF(MY_WME))))
    return TRUE;

  for (i= 0; i < mdl_requests->elements(); i++)
    sort_buf[i]= it++;

  my_qsort(sort_buf, mdl_requests->elements(), sizeof(MDL_request*),
           mdl_request_ptr_cmp);

  for (i= 0; i < mdl_requests->elements(); i++)
  {
    if (acquire_exclusive_lock_impl(sort_buf[i]))
      goto err;
  }
  my_free(sort_buf, MYF(0));
  return FALSE;

err:
  /* Release locks we have managed to acquire so far. */
  for (i= 0; i < mdl_requests->elements() && sort_buf[i]->ticket; i++)
  {
    release_lock(sort_buf[i]->ticket);
    /* Reset lock request back to its initial state. */
    sort_buf[i]->ticket= NULL;
  }
  my_free(sort_buf, MYF(0));
  return TRUE;
}


/**
  Upgrade a shared metadata lock to exclusive.

  Used in ALTER TABLE, when a copy of the table with the
  new definition has been constructed.

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
MDL_ticket::upgrade_shared_lock_to_exclusive()
{
  const char *old_msg;
  st_my_thread_var *mysys_var= my_thread_var;
  THD *thd= m_ctx->get_thd();
  MDL_ticket *pending_ticket;

  DBUG_ENTER("MDL_ticket::upgrade_shared_lock_to_exclusive");
  DEBUG_SYNC(thd, "mdl_upgrade_shared_lock_to_exclusive");

  safe_mutex_assert_not_owner(&LOCK_open);

  /* Allow this function to be called twice for the same lock request. */
  if (m_type == MDL_EXCLUSIVE)
    DBUG_RETURN(FALSE);

  /* Only allow upgrades from MDL_SHARED_UPGRADABLE */
  DBUG_ASSERT(m_type == MDL_SHARED_UPGRADABLE);

  /*
    Since we should have already acquired an intention exclusive
    global lock this call is only enforcing asserts.
  */
  DBUG_ASSERT(m_ctx->is_global_lock_owner(MDL_INTENTION_EXCLUSIVE));

  /*
    Create an auxiliary ticket to represent a pending exclusive
    lock and add it to the 'waiting' queue for the duration
    of upgrade. During upgrade we abort waits of connections
    that own conflicting locks. A pending request is used
    to signal such connections that upon waking up they
    must back off, rather than fall into sleep again.
  */
  if (! (pending_ticket= MDL_ticket::create(m_ctx, MDL_EXCLUSIVE)))
    DBUG_RETURN(TRUE);

  pthread_mutex_lock(&m_lock->m_mutex);

  m_lock->waiting_exclusive.push_front(pending_ticket);

  old_msg= MDL_ENTER_COND(thd, mysys_var, &m_ctx->m_ctx_wakeup_cond,
                          &m_lock->m_mutex);

  while (1)
  {
    if (m_lock->can_grant_lock(m_ctx, MDL_EXCLUSIVE, TRUE))
      break;

    MDL_ticket *conflicting_ticket;
    MDL_lock::Ticket_iterator it(m_lock->granted);

    /*
      If m_ctx->lt_or_ha_sentinel(), and this sentinel is for HANDLER,
      we can deadlock. However, HANDLER is not allowed under
      LOCK TABLES, and apart from LOCK TABLES there are only
      two cases of lock upgrade: ALTER TABLE and CREATE/DROP
      TRIGGER (*). This leaves us with the following scenario
      for deadlock:

      connection 1                          connection 2
      handler t1 open;                      handler t2 open;
      alter table t2 ...                    alter table t1 ...

      This scenario is quite remote, since ALTER
      (and CREATE/DROP TRIGGER) performs mysql_ha_flush() in
      the beginning, and thus closes open HANDLERS against which
      there is a pending lock upgrade. Still, two ALTER statements
      can interleave and not notice each other's pending lock
      (e.g. if both upgrade their locks at the same time).
      This, however, is quite unlikely, so we do nothing to
      address it.

      (*) There is no requirement to upgrade lock in
      CREATE/DROP TRIGGER, it's used there just for convenience.

      A temporary work-around to avoid deadlocks/livelocks in
      a situation when in one connection ALTER TABLE tries to
      upgrade its metadata lock and in another connection
      the active transaction already got this lock in some
      of its earlier statements.
      In such case this transaction always succeeds with getting
      a metadata lock on the table -- it already has one.
      But later on it may block on the table level lock, since ALTER
      got TL_WRITE_ALLOW_READ, and subsequently get aborted
      by notify_shared_lock().
      An abort will lead to a back off, and a second attempt to
      get an MDL lock (successful), and a table lock (-> livelock).

      The call below breaks this loop by forcing transactions to call
      tdc_wait_for_old_versions() (even if the transaction doesn't need
      any new metadata locks), which in turn will check if someone
      is waiting on the owned MDL lock, and produce ER_LOCK_DEADLOCK.

      TODO: Long-term such deadlocks/livelock will be resolved within
            MDL subsystem and thus this call will become unnecessary.
    */
    mysql_abort_transactions_with_shared_lock(&m_lock->key);

    while ((conflicting_ticket= it++))
    {
      if (conflicting_ticket->m_ctx != m_ctx)
        notify_shared_lock(thd, conflicting_ticket);
    }

    /* There is a shared or exclusive lock on the object. */
    DEBUG_SYNC(thd, "mdl_upgrade_shared_lock_to_exclusive_wait");

    /*
      Another thread might have obtained a shared MDL lock on some table
      but has not yet opened it and/or tried to obtain data lock on it.
      Also invocation of acquire_exclusive_lock() method and consequently
      first call to notify_shared_lock() might have happened right after
      thread holding shared metadata lock in wait_for_locks() method
      checked that there are no pending conflicting locks but before
      it has started waiting.
      In both these cases we need to sleep until these threads will start
      waiting and try to abort them once again.
    */
    struct timespec abstime;
    set_timespec(abstime, 1);
    pthread_cond_timedwait(&m_ctx->m_ctx_wakeup_cond, &m_lock->m_mutex,
                           &abstime);

    if (mysys_var->abort)
    {
      m_lock->waiting_exclusive.remove(pending_ticket);
      /*
        If there are no other pending requests for exclusive locks
        we need to wake up threads waiting for a chance to acquire
        shared lock.
      */
      m_lock->wake_up_waiters();
      MDL_EXIT_COND(thd, mysys_var, &m_lock->m_mutex, old_msg);
      MDL_ticket::destroy(pending_ticket);
      DBUG_RETURN(TRUE);
    }
  }

  /* Set the new type of lock in the ticket. */
  m_type= MDL_EXCLUSIVE;

  /* Remove and destroy the auxiliary pending ticket. */
  m_lock->waiting_exclusive.remove(pending_ticket);

  if (m_lock->cached_object)
    (*m_lock->cached_object_release_hook)(m_lock->cached_object);
  m_lock->cached_object= 0;

  MDL_EXIT_COND(thd, mysys_var, &m_lock->m_mutex, old_msg);

  MDL_ticket::destroy(pending_ticket);

  DBUG_RETURN(FALSE);
}


/**
  Try to acquire an exclusive lock on the object if there are
  no conflicting locks.

  Similar to the previous function, but returns
  immediately without any side effect if encounters a lock
  conflict. Otherwise takes the lock.

  This function is used in CREATE TABLE ... LIKE to acquire a lock
  on the table to be created. In this statement we don't want to
  block and wait for the lock if the table already exists.

  @param mdl_request [in] The lock request
  @param conflict   [out] Indicates that conflicting lock exists

  @retval TRUE  Failure: some error occurred (probably OOM).
  @retval FALSE Success: the lock might have not been acquired,
                         check request.ticket to find out.

  FIXME: Compared to lock_table_name_if_not_cached()
         it gives slightly more false negatives.
*/

bool
MDL_context::try_acquire_exclusive_lock(MDL_request *mdl_request)
{
  DBUG_ASSERT(mdl_request->type == MDL_EXCLUSIVE);
  DBUG_ASSERT(is_global_lock_owner(MDL_INTENTION_EXCLUSIVE));

  return try_acquire_lock_impl(mdl_request);
}


/**
  Acquire the global shared metadata lock.

  Holding this lock will block all requests for exclusive locks
  and shared locks which can be potentially upgraded to exclusive.

  @retval FALSE Success -- the lock was granted.
  @retval TRUE  Failure -- our thread was killed.
*/

bool MDL_context::acquire_global_shared_lock()
{
  MDL_request mdl_request;

  DBUG_ASSERT(! is_global_lock_owner(MDL_SHARED));

  mdl_request.init(MDL_key::GLOBAL, "", "", MDL_SHARED);

  if (acquire_lock_impl(&mdl_request))
    return TRUE;

  move_ticket_after_lt_or_ha_sentinel(mdl_request.ticket);

  return FALSE;
}


/**
  Implement a simple deadlock detection heuristic: check if there
  are any pending exclusive locks which conflict with shared locks
  held by this thread. In that case waiting can be circular,
  i.e. lead to a deadlock.

  @return TRUE   If there are any pending conflicting locks.
          FALSE  Otherwise.
*/

bool MDL_context::can_wait_lead_to_deadlock() const
{
  Ticket_iterator ticket_it(m_tickets);
  MDL_ticket *ticket;

  while ((ticket= ticket_it++))
  {
    /*
      In MySQL we never call this method while holding exclusive or
      upgradeable shared metadata locks.
      Otherwise we would also have to check for the presence of pending
      requests for conflicting types of global lock.
      In addition MDL_ticket::has_pending_conflicting_lock()
      won't work properly for exclusive type of lock.
    */
    DBUG_ASSERT(! ticket->is_upgradable_or_exclusive());

    if (ticket->has_pending_conflicting_lock())
      return TRUE;
  }
  return FALSE;
}


/**
  Wait until there will be no locks that conflict with lock requests
  in the given list.

  This is a part of the locking protocol and must be used by the
  acquirer of shared locks after a back-off.

  Does not acquire the locks!

  @retval FALSE  Success. One can try to obtain metadata locks.
  @retval TRUE   Failure (thread was killed or deadlock is possible).
*/

bool
MDL_context::wait_for_locks(MDL_request_list *mdl_requests)
{
  MDL_lock *lock;
  MDL_request *mdl_request;
  MDL_request_list::Iterator it(*mdl_requests);
  const char *old_msg;
  st_my_thread_var *mysys_var= my_thread_var;

  safe_mutex_assert_not_owner(&LOCK_open);

  while (!mysys_var->abort)
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

    /*
      In cases when we wait while still holding some metadata
      locks deadlocks are possible.
      To avoid them we use the following simple empiric - don't
      wait for new lock request to be satisfied if for one of the
      locks which are already held by this connection there is
      a conflicting request (i.e. this connection should not wait
      if someone waits for it).
      This empiric should work well (e.g. give low number of false
      negatives) in situations when conflicts are rare (in our
      case this is true since DDL statements should be rare).
    */
    if (can_wait_lead_to_deadlock())
    {
      my_error(ER_LOCK_DEADLOCK, MYF(0));
      return TRUE;
    }

    it.rewind();
    while ((mdl_request= it++))
    {
      MDL_key *key= &mdl_request->key;
      DBUG_ASSERT(mdl_request->ticket == NULL);

      /*
        To avoid starvation we don't wait if we have a conflict against
        request for MDL_EXCLUSIVE lock.
      */
      if (mdl_request->is_shared() ||
          mdl_request->type == MDL_INTENTION_EXCLUSIVE)
      {
        /* The below call also implicitly locks MDL_lock::m_mutex. */
        if (! (lock= mdl_locks.find(key)))
          continue;

        if (lock->can_grant_lock(this, mdl_request->type, FALSE))
        {
          pthread_mutex_unlock(&lock->m_mutex);
          continue;
        }

        MDL_ticket *pending_ticket;
        if (! (pending_ticket= MDL_ticket::create(this, mdl_request->type)))
        {
          pthread_mutex_unlock(&lock->m_mutex);
          return TRUE;
        }
        if (mdl_request->is_shared())
          lock->waiting_shared.push_front(pending_ticket);
        else
          lock->waiting_exclusive.push_front(pending_ticket);

        old_msg= MDL_ENTER_COND(m_thd, mysys_var, &m_ctx_wakeup_cond,
                                &lock->m_mutex);

        pthread_cond_wait(&m_ctx_wakeup_cond, &lock->m_mutex);

        /*
          We have to do MDL_EXIT_COND here and then re-acquire the lock
          as there is a chance that we will destroy MDL_lock object and
          won't be able to call MDL_EXIT_COND after it.
        */
        MDL_EXIT_COND(m_thd, mysys_var, &lock->m_mutex, old_msg);

        pthread_mutex_lock(&lock->m_mutex);
        if (mdl_request->is_shared())
          lock->waiting_shared.remove(pending_ticket);
        else
          lock->waiting_exclusive.remove(pending_ticket);
        if (lock->is_empty())
          mdl_locks.remove(lock);
        else
          pthread_mutex_unlock(&lock->m_mutex);
        MDL_ticket::destroy(pending_ticket);
        break;
      }
    }
    if (!mdl_request)
    {
      /* There are no conflicts for any locks! */
      break;
    }
  }
  return mysys_var->abort;
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

  DBUG_ASSERT(this == ticket->m_ctx);
  safe_mutex_assert_not_owner(&LOCK_open);

  if (ticket == m_lt_or_ha_sentinel)
    m_lt_or_ha_sentinel= ++Ticket_list::Iterator(m_tickets, ticket);

  pthread_mutex_lock(&lock->m_mutex);

  lock->granted.remove(ticket);

  if (lock->is_empty())
    mdl_locks.remove(lock);
  else
  {
    lock->wake_up_waiters();
    pthread_mutex_unlock(&lock->m_mutex);
  }

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
    DBUG_ASSERT(ticket->m_state == MDL_ACQUIRED);
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
*/

void MDL_ticket::downgrade_exclusive_lock()
{
  safe_mutex_assert_not_owner(&LOCK_open);

  if (is_shared())
    return;

  pthread_mutex_lock(&m_lock->m_mutex);
  m_type= MDL_SHARED_UPGRADABLE;

  if (! m_lock->waiting_shared.is_empty())
  {
    MDL_lock::Ticket_iterator it(m_lock->waiting_shared);
    MDL_ticket *ticket;
    while ((ticket= it++))
      ticket->get_ctx()->awake();
  }

  pthread_mutex_unlock(&m_lock->m_mutex);
}


/**
  Release the global shared metadata lock.
*/

void MDL_context::release_global_shared_lock()
{
  MDL_request mdl_request;
  MDL_ticket *ticket;
  bool not_used;

  mdl_request.init(MDL_key::GLOBAL, "", "", MDL_SHARED);

  safe_mutex_assert_not_owner(&LOCK_open);

  /*
    TODO/QQ/FIXME: In theory we always should be able to find
                   ticket here. But in practice this is not
                   always TRUE.
  */

  if ((ticket= find_ticket(&mdl_request, &not_used)))
    release_lock(ticket);
}


/**
  Auxiliary function which allows to check if we have exclusive lock
  on the object.

  @param mdl_namespace Id of object namespace
  @param db            Name of the database
  @param name          Name of the object

  @return TRUE if current context contains exclusive lock for the object,
          FALSE otherwise.
*/

bool
MDL_context::is_exclusive_lock_owner(MDL_key::enum_mdl_namespace mdl_namespace,
                                     const char *db, const char *name)
{
  MDL_request mdl_request;
  bool is_lt_or_ha_unused;
  mdl_request.init(mdl_namespace, db, name, MDL_EXCLUSIVE);
  MDL_ticket *ticket= find_ticket(&mdl_request, &is_lt_or_ha_unused);

  DBUG_ASSERT(ticket == NULL || ticket->m_state == MDL_ACQUIRED);

  return ticket;
}


/**
  Auxiliary function which allows to check if we have some kind of lock on
  a object.

  @param mdl_namespace Id of object namespace
  @param db            Name of the database
  @param name          Name of the object

  @return TRUE if current context contains satisfied lock for the object,
          FALSE otherwise.
*/

bool
MDL_context::is_lock_owner(MDL_key::enum_mdl_namespace mdl_namespace,
                           const char *db, const char *name)
{
  MDL_key key(mdl_namespace, db, name);
  MDL_ticket *ticket;
  MDL_context::Ticket_iterator it(m_tickets);

  while ((ticket= it++))
  {
    if (ticket->m_lock->key.is_equal(&key))
      break;
  }

  return ticket;
}


/**
  Check if we have any pending exclusive locks which conflict with
  existing shared lock.

  @pre The ticket must match an acquired lock.

  @return TRUE if there is a conflicting lock request, FALSE otherwise.
*/

bool MDL_ticket::has_pending_conflicting_lock() const
{
  safe_mutex_assert_not_owner(&LOCK_open);
  DBUG_ASSERT(is_shared());

  return m_lock->has_pending_exclusive_lock();
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
                              mdl_savepoint : m_lt_or_ha_sentinel);

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
  release_locks_stored_before(m_lt_or_ha_sentinel);
  DBUG_VOID_RETURN;
}


/**
  Does this savepoint have this lock?
  
  @retval TRUE  The ticket is older than the savepoint and
                is not LT or HA ticket. Thus it belongs to
                the savepoint.
  @retval FALSE The ticket is newer than the savepoint
                or is an LT or HA ticket.
*/

bool MDL_context::has_lock(MDL_ticket *mdl_savepoint,
                           MDL_ticket *mdl_ticket)
{
  MDL_ticket *ticket;
  MDL_context::Ticket_iterator it(m_tickets);
  bool found_savepoint= FALSE;

  while ((ticket= it++) && ticket != m_lt_or_ha_sentinel)
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
  /* Reached m_lt_or_ha_sentinel. The ticket must be an LT or HA ticket. */
  return FALSE;
}


/**
  Rearrange the ticket to reside in the part of the list that's
  beyond m_lt_or_ha_sentinel. This effectively changes the ticket
  life cycle, from automatic to manual: i.e. the ticket is no
  longer released by MDL_context::release_transactional_locks() or
  MDL_context::rollback_to_savepoint(), it must be released manually.
*/

void MDL_context::move_ticket_after_lt_or_ha_sentinel(MDL_ticket *mdl_ticket)
{
  m_tickets.remove(mdl_ticket);
  if (m_lt_or_ha_sentinel == NULL)
  {
    m_lt_or_ha_sentinel= mdl_ticket;
    /* sic: linear from the number of transactional tickets acquired so-far! */
    m_tickets.push_back(mdl_ticket);
  }
  else
    m_tickets.insert_after(m_lt_or_ha_sentinel, mdl_ticket);
}

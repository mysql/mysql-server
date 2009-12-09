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
  The lock context. Created internally for an acquired lock.
  For a given name, there exists only one MDL_lock instance,
  and it exists only when the lock has been granted.
  Can be seen as an MDL subsystem's version of TABLE_SHARE.
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

  /** The type of lock (shared or exclusive). */
  enum
  {
    MDL_LOCK_SHARED,
    MDL_LOCK_EXCLUSIVE,
  } type;
  /** The key of the object (data) being protected. */
  MDL_key key;
  /** List of granted tickets for this lock. */
  Ticket_list granted;
  /**
    There can be several upgraders and active exclusive
    locks belonging to the same context. E.g.
    in case of RENAME t1 to t2, t2 to t3, we attempt to
    exclusively lock t2 twice.
  */
  Ticket_list waiting;
  void   *cached_object;
  mdl_cached_object_release_hook cached_object_release_hook;

  bool is_empty() const
  {
    return (granted.is_empty() && waiting.is_empty());
  }

  bool can_grant_lock(const MDL_context *requestor_ctx,
                      enum_mdl_type type, bool is_upgrade);

  inline static MDL_lock *create(const MDL_key *key);
  inline static void destroy(MDL_lock *lock);
private:
  MDL_lock(const MDL_key *key_arg)
  : type(MDL_LOCK_SHARED),
    key(key_arg),
    cached_object(NULL),
    cached_object_release_hook(NULL)
  {
  }
};


static pthread_mutex_t LOCK_mdl;
static pthread_cond_t  COND_mdl;
static HASH mdl_locks;

/**
  An implementation of the global metadata lock. The only
  locking modes which are supported at the moment are SHARED and
  INTENTION EXCLUSIVE. Note, that SHARED global metadata lock
  is acquired automatically when one tries to acquire an EXCLUSIVE
  or UPGRADABLE SHARED metadata lock on an individual object.
*/

class MDL_global_lock
{
public:
  uint waiting_shared;
  uint active_shared;
  uint active_intention_exclusive;

  bool is_empty() const
  {
    return (waiting_shared == 0 && active_shared == 0 &&
            active_intention_exclusive == 0);
  }
  bool is_lock_type_compatible(enum_mdl_type type, bool is_upgrade) const;
};


static MDL_global_lock global_lock;


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
  pthread_mutex_init(&LOCK_mdl, NULL);
  pthread_cond_init(&COND_mdl, NULL);
  my_hash_init(&mdl_locks, &my_charset_bin, 16 /* FIXME */, 0, 0,
               mdl_locks_key, 0, 0);
  /* The global lock is zero-initialized by the loader. */
  DBUG_ASSERT(global_lock.is_empty());
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
    DBUG_ASSERT(!mdl_locks.records);
    DBUG_ASSERT(global_lock.is_empty());
    pthread_mutex_destroy(&LOCK_mdl);
    pthread_cond_destroy(&COND_mdl);
    my_hash_free(&mdl_locks);
  }
}


/**
  Initialize a metadata locking context.

  This is to be called when a new server connection is created.
*/

void MDL_context::init(THD *thd_arg)
{
  m_has_global_shared_lock= FALSE;
  m_thd= thd_arg;
  /*
    FIXME: In reset_n_backup_open_tables_state,
    we abuse "init" as a reset, i.e. call it on an already
    constructed non-empty object. This is why we can't
    rely here on the default constructors of I_P_List
    to empty the list.
  */
  m_tickets.empty();
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
  DBUG_ASSERT(! m_has_global_shared_lock);
}


/**
  Backup and reset state of meta-data locking context.

  mdl_context_backup_and_reset(), mdl_context_restore() and
  mdl_context_merge() are used by HANDLER implementation which
  needs to open table for new HANDLER independently of already
  open HANDLERs and add this table/metadata lock to the set of
  tables open/metadata locks for HANDLERs afterwards.
*/

void MDL_context::backup_and_reset(MDL_context *backup)
{
  DBUG_ASSERT(backup->m_tickets.is_empty());

  m_tickets.swap(backup->m_tickets);

  backup->m_has_global_shared_lock= m_has_global_shared_lock;
  /*
    When the main context is swapped out, one can not take
    the global shared lock, and one can not rely on it:
    the functionality in this mode is reduced, since it exists as
    a temporary hack to support ad-hoc opening of system tables.
  */
  m_has_global_shared_lock= FALSE;
}


/**
  Restore state of meta-data locking context from backup.
*/

void MDL_context::restore_from_backup(MDL_context *backup)
{
  DBUG_ASSERT(m_tickets.is_empty());
  DBUG_ASSERT(m_has_global_shared_lock == FALSE);

  m_tickets.swap(backup->m_tickets);
  m_has_global_shared_lock= backup->m_has_global_shared_lock;
}


/**
  Merge meta-data locks from one context into another.
*/

void MDL_context::merge(MDL_context *src)
{
  MDL_ticket *ticket;

  DBUG_ASSERT(m_thd == src->m_thd);

  if (!src->m_tickets.is_empty())
  {
    Ticket_iterator it(src->m_tickets);
    while ((ticket= it++))
    {
      DBUG_ASSERT(ticket->m_ctx);
      ticket->m_ctx= this;
      m_tickets.push_front(ticket);
    }
    src->m_tickets.empty();
  }
  /*
    MDL_context::merge() is a hack used in one place only: to open
    an SQL handler. We never acquire the global shared lock there.
  */
  DBUG_ASSERT(! src->m_has_global_shared_lock);
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

void MDL_request::init(enum_mdl_namespace mdl_namespace,
                       const char *db_arg,
                       const char *name_arg,
                       enum enum_mdl_type mdl_type_arg)
{
  key.mdl_key_init(mdl_namespace, db_arg, name_arg);
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
MDL_request::create(enum_mdl_namespace mdl_namespace, const char *db,
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

  @todo This naive implementation should be replaced with one that saves
        on memory allocation by reusing released objects.
*/

inline MDL_lock *MDL_lock::create(const MDL_key *mdl_key)
{
  return new MDL_lock(mdl_key);
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

#define MDL_ENTER_COND(A, B) mdl_enter_cond(A, B, __func__, __FILE__, __LINE__)

static inline const char *mdl_enter_cond(THD *thd,
                                         st_my_thread_var *mysys_var,
                                         const char *calling_func,
                                         const char *calling_file,
                                         const unsigned int calling_line)
{
  safe_mutex_assert_owner(&LOCK_mdl);

  mysys_var->current_mutex= &LOCK_mdl;
  mysys_var->current_cond= &COND_mdl;

  DEBUG_SYNC(thd, "mdl_enter_cond");

  return set_thd_proc_info(thd, "Waiting for table",
                           calling_func, calling_file, calling_line);
}

#define MDL_EXIT_COND(A, B, C) mdl_exit_cond(A, B, C, __func__, __FILE__, __LINE__)

static inline void mdl_exit_cond(THD *thd,
                                 st_my_thread_var *mysys_var,
                                 const char* old_msg,
                                 const char *calling_func,
                                 const char *calling_file,
                                 const unsigned int calling_line)
{
  DBUG_ASSERT(&LOCK_mdl == mysys_var->current_mutex);

  pthread_mutex_unlock(&LOCK_mdl);
  pthread_mutex_lock(&mysys_var->mutex);
  mysys_var->current_mutex= 0;
  mysys_var->current_cond= 0;
  pthread_mutex_unlock(&mysys_var->mutex);

  DEBUG_SYNC(thd, "mdl_exit_cond");

  (void) set_thd_proc_info(thd, old_msg, calling_func,
                           calling_file, calling_line);
}


/**
  Check if request for the lock on particular object can be satisfied given
  current state of the global metadata lock.

  @note In other words, we're trying to check that the individual lock
        request, implying a form of lock on the global metadata, is
        compatible with the current state of the global metadata lock.

  @param mdl_request  Request for lock on an individual object, implying a
                      certain kind of global metadata lock.

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
        "0" -- means impossible situation which will trigger assert

  (*)  Since for upgradable shared locks we always take intention exclusive
       global lock at the same time when obtaining the shared lock, there
       is no need to obtain such lock during the upgrade itself.
  (**) Since intention shared global locks are compatible with all other
       type of locks we don't even have any accounting for them.
*/

bool
MDL_global_lock::is_lock_type_compatible(enum_mdl_type type,
                                         bool is_upgrade) const
{
  switch (type)
  {
  case MDL_SHARED:
  case MDL_SHARED_HIGH_PRIO:
    return TRUE;
    break;
  case MDL_SHARED_UPGRADABLE:
    if (active_shared || waiting_shared)
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
  case MDL_EXCLUSIVE:
    if (is_upgrade)
    {
      /*
        We are upgrading MDL_SHARED to MDL_EXCLUSIVE.

        There should be no conflicting global locks since for each upgradable
        shared lock we obtain intention exclusive global lock first.
      */
      DBUG_ASSERT(active_shared == 0 && active_intention_exclusive);
      return TRUE;
    }
    else
    {
      if (active_shared || waiting_shared)
      {
        /*
          We are going to obtain intention exclusive global lock and
          there is active or pending shared global lock.
        */
        return FALSE;
      }
      else
        return TRUE;
    }
    break;
  default:
    DBUG_ASSERT(0);
  }
  return FALSE;
}


/**
  Check if request for the lock can be satisfied given current state of lock.

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
MDL_lock::can_grant_lock(const MDL_context *requestor_ctx, enum_mdl_type type_arg,
                         bool is_upgrade)
{
  bool can_grant= FALSE;

  switch (type_arg) {
  case MDL_SHARED:
  case MDL_SHARED_UPGRADABLE:
  case MDL_SHARED_HIGH_PRIO:
    if (type == MDL_lock::MDL_LOCK_SHARED)
    {
      /* Pending exclusive locks have higher priority over shared locks. */
      if (waiting.is_empty() || type_arg == MDL_SHARED_HIGH_PRIO)
        can_grant= TRUE;
    }
    else if (granted.head()->get_ctx() == requestor_ctx)
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
      DBUG_ASSERT(type == MDL_lock::MDL_LOCK_SHARED);

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
    else if (type == MDL_lock::MDL_LOCK_SHARED)
    {
      can_grant= granted.is_empty();
    }
    break;
  default:
    DBUG_ASSERT(0);
  }
  return can_grant;
}


/**
  Check whether the context already holds a compatible lock ticket
  on an object.

  @param mdl_request  Lock request object for lock to be acquired

  @return A pointer to the lock ticket for the object or NULL otherwise.
*/

MDL_ticket *
MDL_context::find_ticket(MDL_request *mdl_request)
{
  MDL_ticket *ticket;
  Ticket_iterator it(m_tickets);

  while ((ticket= it++))
  {
    if (mdl_request->type == ticket->m_type &&
        mdl_request->key.is_equal(&ticket->m_lock->key))
      break;
  }

  return ticket;
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
  MDL_lock *lock;
  MDL_key *key= &mdl_request->key;
  MDL_ticket *ticket;

  DBUG_ASSERT(mdl_request->is_shared() && mdl_request->ticket == NULL);

  /* Don't take chances in production. */
  mdl_request->ticket= NULL;
  safe_mutex_assert_not_owner(&LOCK_open);

  if (m_has_global_shared_lock &&
      mdl_request->type == MDL_SHARED_UPGRADABLE)
  {
    my_error(ER_CANT_UPDATE_WITH_READLOCK, MYF(0));
    return TRUE;
  }

  /*
    Check whether the context already holds a shared lock on the object,
    and if so, grant the request.
  */
  if ((ticket= find_ticket(mdl_request)))
  {
    DBUG_ASSERT(ticket->m_state == MDL_ACQUIRED);
    /* Only shared locks can be recursive. */
    DBUG_ASSERT(ticket->is_shared());
    mdl_request->ticket= ticket;
    return FALSE;
  }

  pthread_mutex_lock(&LOCK_mdl);

  if (!global_lock.is_lock_type_compatible(mdl_request->type, FALSE))
  {
    pthread_mutex_unlock(&LOCK_mdl);
    return FALSE;
  }

  if (!(ticket= MDL_ticket::create(this, mdl_request->type)))
  {
    pthread_mutex_unlock(&LOCK_mdl);
    return TRUE;
  }

  if (!(lock= (MDL_lock*) my_hash_search(&mdl_locks,
                                         key->ptr(), key->length())))
  {
    /* Default lock type is MDL_lock::MDL_LOCK_SHARED */
    lock= MDL_lock::create(key);
    if (!lock || my_hash_insert(&mdl_locks, (uchar*)lock))
    {
      MDL_lock::destroy(lock);
      MDL_ticket::destroy(ticket);
      pthread_mutex_unlock(&LOCK_mdl);
      return TRUE;
    }
  }

  if (lock->can_grant_lock(this, mdl_request->type, FALSE))
  {
    mdl_request->ticket= ticket;
    lock->granted.push_front(ticket);
    m_tickets.push_front(ticket);
    ticket->m_state= MDL_ACQUIRED;
    ticket->m_lock= lock;
    if (mdl_request->type == MDL_SHARED_UPGRADABLE)
      global_lock.active_intention_exclusive++;
  }
  else
  {
    /* We can't get here if we allocated a new lock. */
    DBUG_ASSERT(! lock->is_empty());
    MDL_ticket::destroy(ticket);
  }
  pthread_mutex_unlock(&LOCK_mdl);

  return FALSE;
}


/**
  Notify a thread holding a shared metadata lock which
  conflicts with a pending exclusive lock.

  @param thd               Current thread context
  @param conflicting_ticket  Conflicting metadata lock

  @retval TRUE   A thread was woken up
  @retval FALSE  Lock is not a shared one or no thread was woken up
*/

static bool notify_shared_lock(THD *thd, MDL_ticket *conflicting_ticket)
{
  bool woke= FALSE;
  if (conflicting_ticket->is_shared())
  {
    THD *conflicting_thd= conflicting_ticket->get_ctx()->get_thd();
    DBUG_ASSERT(thd != conflicting_thd); /* Self-deadlock */
    woke= mysql_notify_thread_having_shared_lock(thd, conflicting_thd);
  }
  return woke;
}


/**
  Acquire a single exclusive lock. A convenience
  wrapper around the method acquiring a list of locks.
*/

bool MDL_context::acquire_exclusive_lock(MDL_request *mdl_request)
{
  MDL_request_list mdl_requests;
  mdl_requests.push_front(mdl_request);
  return acquire_exclusive_locks(&mdl_requests);
}


/**
  Acquire exclusive locks. The context must contain the list of
  locks to be acquired. There must be no granted locks in the
  context.

  This is a replacement of lock_table_names(). It is used in
  RENAME, DROP and other DDL SQL statements.

  @note The MDL context may not have non-exclusive lock requests
        or acquired locks.

  @retval FALSE  Success
  @retval TRUE   Failure
*/

bool MDL_context::acquire_exclusive_locks(MDL_request_list *mdl_requests)
{
  MDL_lock *lock;
  bool signalled= FALSE;
  const char *old_msg;
  MDL_request *mdl_request;
  MDL_ticket *ticket;
  st_my_thread_var *mysys_var= my_thread_var;
  MDL_request_list::Iterator it(*mdl_requests);

  safe_mutex_assert_not_owner(&LOCK_open);
  /* Exclusive locks must always be acquired first, all at once. */
  DBUG_ASSERT(! has_locks());

  if (m_has_global_shared_lock)
  {
    my_error(ER_CANT_UPDATE_WITH_READLOCK, MYF(0));
    return TRUE;
  }

  pthread_mutex_lock(&LOCK_mdl);

  old_msg= MDL_ENTER_COND(m_thd, mysys_var);

  while ((mdl_request= it++))
  {
    MDL_key *key= &mdl_request->key;
    DBUG_ASSERT(mdl_request->type == MDL_EXCLUSIVE &&
                mdl_request->ticket == NULL);

    /* Don't take chances in production. */
    mdl_request->ticket= NULL;

    /* Early allocation: ticket is used as a shortcut to the lock. */
    if (!(ticket= MDL_ticket::create(this, mdl_request->type)))
      goto err;

    if (!(lock= (MDL_lock*) my_hash_search(&mdl_locks,
                                           key->ptr(), key->length())))
    {
      lock= MDL_lock::create(key);
      if (!lock || my_hash_insert(&mdl_locks, (uchar*)lock))
      {
        MDL_ticket::destroy(ticket);
        MDL_lock::destroy(lock);
        goto err;
      }
    }

    mdl_request->ticket= ticket;
    lock->waiting.push_front(ticket);
    ticket->m_lock= lock;
  }

  while (1)
  {
    it.rewind();
    while ((mdl_request= it++))
    {
      lock= mdl_request->ticket->m_lock;

      if (!global_lock.is_lock_type_compatible(mdl_request->type, FALSE))
      {
        /*
          Someone owns or wants to acquire the global shared lock so
          we have to wait until he goes away.
        */
        signalled= TRUE;
        break;
      }
      else if (!lock->can_grant_lock(this, mdl_request->type, FALSE))
      {
        MDL_ticket *conflicting_ticket;
        MDL_lock::Ticket_iterator it(lock->granted);

        signalled= (lock->type == MDL_lock::MDL_LOCK_EXCLUSIVE);

        while ((conflicting_ticket= it++))
          signalled|= notify_shared_lock(m_thd, conflicting_ticket);

        break;
      }
    }
    if (!mdl_request)
      break;

    /* There is a shared or exclusive lock on the object. */
    DEBUG_SYNC(m_thd, "mdl_acquire_exclusive_locks_wait");

    if (signalled)
      pthread_cond_wait(&COND_mdl, &LOCK_mdl);
    else
    {
      /*
        Another thread obtained a shared MDL lock on some table but
        has not yet opened it and/or tried to obtain data lock on
        it. In this case we need to wait until this happens and try
        to abort this thread once again.
      */
      struct timespec abstime;
      set_timespec(abstime, 10);
      pthread_cond_timedwait(&COND_mdl, &LOCK_mdl, &abstime);
    }
    if (mysys_var->abort)
      goto err;
  }
  it.rewind();
  while ((mdl_request= it++))
  {
    global_lock.active_intention_exclusive++;
    ticket= mdl_request->ticket;
    lock= ticket->m_lock;
    lock->type= MDL_lock::MDL_LOCK_EXCLUSIVE;
    lock->waiting.remove(ticket);
    lock->granted.push_front(ticket);
    m_tickets.push_front(ticket);
    ticket->m_state= MDL_ACQUIRED;
    if (lock->cached_object)
      (*lock->cached_object_release_hook)(lock->cached_object);
    lock->cached_object= NULL;
  }
  /* As a side-effect MDL_EXIT_COND() unlocks LOCK_mdl. */
  MDL_EXIT_COND(m_thd, mysys_var, old_msg);
  return FALSE;

err:
  /* Remove our pending tickets from the locks. */
  it.rewind();
  while ((mdl_request= it++) && mdl_request->ticket)
  {
    ticket= mdl_request->ticket;
    DBUG_ASSERT(ticket->m_state == MDL_PENDING);
    lock= ticket->m_lock;
    lock->waiting.remove(ticket);
    MDL_ticket::destroy(ticket);
    /* Reset lock request back to its initial state. */
    mdl_request->ticket= NULL;
    if (lock->is_empty())
    {
      my_hash_delete(&mdl_locks, (uchar *)lock);
      MDL_lock::destroy(lock);
    }
  }
  /* May be some pending requests for shared locks can be satisfied now. */
  pthread_cond_broadcast(&COND_mdl);
  MDL_EXIT_COND(m_thd, mysys_var, old_msg);
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

  DBUG_ENTER("MDL_ticket::upgrade_shared_lock_to_exclusive");
  DEBUG_SYNC(thd, "mdl_upgrade_shared_lock_to_exclusive");

  safe_mutex_assert_not_owner(&LOCK_open);

  /* Allow this function to be called twice for the same lock request. */
  if (m_type == MDL_EXCLUSIVE)
    DBUG_RETURN(FALSE);

  pthread_mutex_lock(&LOCK_mdl);

  old_msg= MDL_ENTER_COND(thd, mysys_var);

  /*
    Since we should have already acquired an intention exclusive
    global lock this call is only enforcing asserts.
  */
  DBUG_ASSERT(global_lock.is_lock_type_compatible(MDL_EXCLUSIVE, TRUE));

  while (1)
  {
    if (m_lock->can_grant_lock(m_ctx, MDL_EXCLUSIVE, TRUE))
      break;

    bool signalled= FALSE;
    MDL_ticket *conflicting_ticket;
    MDL_lock::Ticket_iterator it(m_lock->granted);

    while ((conflicting_ticket= it++))
    {
      if (conflicting_ticket->m_ctx != m_ctx)
        signalled|= notify_shared_lock(thd, conflicting_ticket);
    }

    if (signalled)
      pthread_cond_wait(&COND_mdl, &LOCK_mdl);
    else
    {
      /*
        Another thread obtained a shared MDL lock on some table but
        has not yet opened it and/or tried to obtain data lock on
        it. In this case we need to wait until this happens and try
        to abort this thread once again.
      */
      struct timespec abstime;
      set_timespec(abstime, 10);
      DBUG_PRINT("info", ("Failed to wake-up from table-level lock ... sleeping"));
      pthread_cond_timedwait(&COND_mdl, &LOCK_mdl, &abstime);
    }
    if (mysys_var->abort)
    {
      /* Pending requests for shared locks can be satisfied now. */
      pthread_cond_broadcast(&COND_mdl);
      MDL_EXIT_COND(thd, mysys_var, old_msg);
      DBUG_RETURN(TRUE);
    }
  }

  m_lock->type= MDL_lock::MDL_LOCK_EXCLUSIVE;
  /* Set the new type of lock in the ticket. */
  m_type= MDL_EXCLUSIVE;
  if (m_lock->cached_object)
    (*m_lock->cached_object_release_hook)(m_lock->cached_object);
  m_lock->cached_object= 0;

  /* As a side-effect MDL_EXIT_COND() unlocks LOCK_mdl. */
  MDL_EXIT_COND(thd, mysys_var, old_msg);
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
  MDL_lock *lock;
  MDL_ticket *ticket;
  MDL_key *key= &mdl_request->key;

  DBUG_ASSERT(mdl_request->type == MDL_EXCLUSIVE &&
              mdl_request->ticket == NULL);

  safe_mutex_assert_not_owner(&LOCK_open);

  mdl_request->ticket= NULL;

  pthread_mutex_lock(&LOCK_mdl);

  if (!(lock= (MDL_lock*) my_hash_search(&mdl_locks,
                                         key->ptr(), key->length())))
  {
    ticket= MDL_ticket::create(this, mdl_request->type);
    lock= MDL_lock::create(key);
    if (!ticket || !lock || my_hash_insert(&mdl_locks, (uchar*)lock))
    {
      MDL_ticket::destroy(ticket);
      MDL_lock::destroy(lock);
      pthread_mutex_unlock(&LOCK_mdl);
      return TRUE;
    }
    mdl_request->ticket= ticket;
    lock->type= MDL_lock::MDL_LOCK_EXCLUSIVE;
    lock->granted.push_front(ticket);
    m_tickets.push_front(ticket);
    ticket->m_state= MDL_ACQUIRED;
    ticket->m_lock= lock;
    global_lock.active_intention_exclusive++;
  }
  pthread_mutex_unlock(&LOCK_mdl);
  return FALSE;
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
  st_my_thread_var *mysys_var= my_thread_var;
  const char *old_msg;

  safe_mutex_assert_not_owner(&LOCK_open);
  DBUG_ASSERT(!m_has_global_shared_lock);

  pthread_mutex_lock(&LOCK_mdl);

  global_lock.waiting_shared++;
  old_msg= MDL_ENTER_COND(m_thd, mysys_var);

  while (!mysys_var->abort && global_lock.active_intention_exclusive)
    pthread_cond_wait(&COND_mdl, &LOCK_mdl);

  global_lock.waiting_shared--;
  if (mysys_var->abort)
  {
    /* As a side-effect MDL_EXIT_COND() unlocks LOCK_mdl. */
    MDL_EXIT_COND(m_thd, mysys_var, old_msg);
    return TRUE;
  }
  global_lock.active_shared++;
  m_has_global_shared_lock= TRUE;
  /* As a side-effect MDL_EXIT_COND() unlocks LOCK_mdl. */
  MDL_EXIT_COND(m_thd, mysys_var, old_msg);
  return FALSE;
}


/**
  Wait until there will be no locks that conflict with lock requests
  in the given list.

  This is a part of the locking protocol and must be used by the
  acquirer of shared locks after a back-off.

  Does not acquire the locks!

  @retval FALSE  Success. One can try to obtain metadata locks.
  @retval TRUE   Failure (thread was killed)
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
    pthread_mutex_lock(&LOCK_mdl);
    old_msg= MDL_ENTER_COND(m_thd, mysys_var);
    it.rewind();
    while ((mdl_request= it++))
    {
      MDL_key *key= &mdl_request->key;
      DBUG_ASSERT(mdl_request->ticket == NULL);
      if (!global_lock.is_lock_type_compatible(mdl_request->type, FALSE))
        break;
      /*
        To avoid starvation we don't wait if we have a conflict against
        request for MDL_EXCLUSIVE lock.
      */
      if (mdl_request->is_shared() &&
          (lock= (MDL_lock*) my_hash_search(&mdl_locks, key->ptr(),
                                            key->length())) &&
          !lock->can_grant_lock(this, mdl_request->type, FALSE))
        break;
    }
    if (!mdl_request)
    {
      pthread_mutex_unlock(&LOCK_mdl);
      break;
    }
    pthread_cond_wait(&COND_mdl, &LOCK_mdl);
    /* As a side-effect MDL_EXIT_COND() unlocks LOCK_mdl. */
    MDL_EXIT_COND(m_thd, mysys_var, old_msg);
  }
  return mysys_var->abort;
}


/**
  Auxiliary function which allows to release particular lock
  ownership of which is represented by a lock ticket object.
*/

void MDL_context::release_ticket(MDL_ticket *ticket)
{
  MDL_lock *lock= ticket->m_lock;
  DBUG_ENTER("release_ticket");
  DBUG_PRINT("enter", ("db=%s name=%s", lock->key.db_name(),
                                        lock->key.table_name()));

  safe_mutex_assert_owner(&LOCK_mdl);

  m_tickets.remove(ticket);

  switch (ticket->m_type)
  {
    case MDL_SHARED_UPGRADABLE:
      global_lock.active_intention_exclusive--;
      /* Fallthrough. */
    case MDL_SHARED:
    case MDL_SHARED_HIGH_PRIO:
      lock->granted.remove(ticket);
      break;
    case MDL_EXCLUSIVE:
      lock->type= MDL_lock::MDL_LOCK_SHARED;
      lock->granted.remove(ticket);
      global_lock.active_intention_exclusive--;
      break;
    default:
      DBUG_ASSERT(0);
  }

  MDL_ticket::destroy(ticket);

  if (lock->is_empty())
  {
    my_hash_delete(&mdl_locks, (uchar *)lock);
    DBUG_PRINT("info", ("releasing cached_object cached_object=%p",
                        lock->cached_object));
    if (lock->cached_object)
      (*lock->cached_object_release_hook)(lock->cached_object);
    MDL_lock::destroy(lock);
  }

  DBUG_VOID_RETURN;
}


/**
  Release all locks associated with the context.

  This function is used to back off in case of a lock conflict.
  It is also used to release shared locks in the end of an SQL
  statement.
*/

void MDL_context::release_all_locks()
{
  MDL_ticket *ticket;
  Ticket_iterator it(m_tickets);
  DBUG_ENTER("MDL_context::release_all_locks");

  safe_mutex_assert_not_owner(&LOCK_open);

  if (m_tickets.is_empty())
    DBUG_VOID_RETURN;

  pthread_mutex_lock(&LOCK_mdl);
  while ((ticket= it++))
  {
    DBUG_PRINT("info", ("found lock to release ticket=%p", ticket));
    release_ticket(ticket);
  }
  /* Inefficient but will do for a while */
  pthread_cond_broadcast(&COND_mdl);
  pthread_mutex_unlock(&LOCK_mdl);

  m_tickets.empty();

  DBUG_VOID_RETURN;
}


/**
  Release a lock.

  @param ticket    Lock to be released
*/

void MDL_context::release_lock(MDL_ticket *ticket)
{
  DBUG_ASSERT(this == ticket->m_ctx);
  safe_mutex_assert_not_owner(&LOCK_open);

  pthread_mutex_lock(&LOCK_mdl);
  release_ticket(ticket);
  pthread_cond_broadcast(&COND_mdl);
  pthread_mutex_unlock(&LOCK_mdl);
}


/**
  Release all locks in the context which correspond to the same name/
  object as this lock request.

  @param ticket    One of the locks for the name/object for which all
                   locks should be released.
*/

void MDL_context::release_all_locks_for_name(MDL_ticket *name)
{
  /* Use MDL_ticket::lock to identify other locks for the same object. */
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

  pthread_mutex_lock(&LOCK_mdl);
  m_lock->type= MDL_lock::MDL_LOCK_SHARED;
  m_type= MDL_SHARED_UPGRADABLE;
  pthread_cond_broadcast(&COND_mdl);
  pthread_mutex_unlock(&LOCK_mdl);
}


/**
  Release the global shared metadata lock.
*/

void MDL_context::release_global_shared_lock()
{
  safe_mutex_assert_not_owner(&LOCK_open);
  DBUG_ASSERT(m_has_global_shared_lock);

  pthread_mutex_lock(&LOCK_mdl);
  global_lock.active_shared--;
  m_has_global_shared_lock= FALSE;
  pthread_cond_broadcast(&COND_mdl);
  pthread_mutex_unlock(&LOCK_mdl);
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
MDL_context::is_exclusive_lock_owner(enum_mdl_namespace mdl_namespace,
                                     const char *db, const char *name)
{
  MDL_request mdl_request;
  mdl_request.init(mdl_namespace, db, name, MDL_EXCLUSIVE);
  MDL_ticket *ticket= find_ticket(&mdl_request);

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
MDL_context::is_lock_owner(enum_mdl_namespace mdl_namespace,
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

  @param ticket Shared lock against which check should be performed.

  @return TRUE if there are any conflicting locks, FALSE otherwise.
*/

bool MDL_ticket::has_pending_conflicting_lock() const
{
  bool result;

  DBUG_ASSERT(is_shared());
  safe_mutex_assert_not_owner(&LOCK_open);

  pthread_mutex_lock(&LOCK_mdl);
  result= !m_lock->waiting.is_empty();
  pthread_mutex_unlock(&LOCK_mdl);
  return result;
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
                        m_lock->key.db_name(), m_lock->key.table_name(),
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
  MDL_ticket *ticket;
  Ticket_iterator it(m_tickets);
  DBUG_ENTER("MDL_context::rollback_to_savepoint");

  while ((ticket= it++))
  {
    /* Stop when lock was acquired before this savepoint. */
    if (ticket == mdl_savepoint)
      break;
    release_lock(ticket);
  }

  DBUG_VOID_RETURN;
}



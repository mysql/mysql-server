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


/**
  The lock context. Created internally for an acquired lock.
  For a given name, there exists only one MDL_LOCK instance,
  and it exists only when the lock has been granted.
  Can be seen as an MDL subsystem's version of TABLE_SHARE.
*/

struct MDL_LOCK
{
  typedef I_P_List<MDL_LOCK_TICKET,
                   I_P_List_adapter<MDL_LOCK_TICKET,
                                    &MDL_LOCK_TICKET::next_in_lock,
                                    &MDL_LOCK_TICKET::prev_in_lock> >
          Ticket_list;

  typedef Ticket_list::Iterator Ticket_iterator;

  /** The type of lock (shared or exclusive). */
  enum
  {
    SHARED,
    EXCLUSIVE,
  } type;
  /** The key of the object (data) being protected. */
  MDL_KEY key;
  /** List of granted tickets for this lock. */
  Ticket_list granted;
  /**
    There can be several upgraders and active exclusive
    belonging to the same context.
  */
  Ticket_list waiting;
  void   *cached_object;
  mdl_cached_object_release_hook cached_object_release_hook;

  MDL_LOCK(const MDL_KEY *mdl_key)
  : type(SHARED), cached_object(0), cached_object_release_hook(0)
  {
    key.mdl_key_init(mdl_key);
    granted.empty();
    waiting.empty();
  }

  bool is_empty()
  {
    return (granted.is_empty() && waiting.is_empty());
  }
};


pthread_mutex_t LOCK_mdl;
pthread_cond_t  COND_mdl;
HASH mdl_locks;

/**
  Structure implementing global metadata lock. The only types
  of locks which are supported at the moment are shared and
  intention exclusive locks. Note that the latter type of global
  lock acquired automatically when one tries to acquire exclusive
  or shared upgradable lock on particular object.
*/

struct MDL_GLOBAL_LOCK
{
  uint waiting_shared;
  uint active_shared;
  uint active_intention_exclusive;
} global_lock;


extern "C" uchar *
mdl_locks_key(const uchar *record, size_t *length,
              my_bool not_used __attribute__((unused)))
{
  MDL_LOCK *entry=(MDL_LOCK*) record;
  *length= entry->key.length();
  return (uchar*) entry->key.ptr();
}


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
  Please see the description of mdl_acquire_shared_lock() for details.
*/

void mdl_init()
{
  pthread_mutex_init(&LOCK_mdl, NULL);
  pthread_cond_init(&COND_mdl, NULL);
  my_hash_init(&mdl_locks, &my_charset_bin, 16 /* FIXME */, 0, 0,
               mdl_locks_key, 0, 0);
  global_lock.waiting_shared= global_lock.active_shared= 0;
  global_lock.active_intention_exclusive= 0;
}


/**
  Release resources of metadata locking subsystem.

  Destroys the global mutex and the condition variable.
  Called at server shutdown.
*/

void mdl_destroy()
{
  DBUG_ASSERT(!mdl_locks.records);
  pthread_mutex_destroy(&LOCK_mdl);
  pthread_cond_destroy(&COND_mdl);
  my_hash_free(&mdl_locks);
}


/**
  Initialize a metadata locking context.

  This is to be called when a new server connection is created.
*/

void mdl_context_init(MDL_CONTEXT *context, THD *thd)
{
  context->requests.empty();
  context->tickets.empty();
  context->thd= thd;
  context->has_global_shared_lock= FALSE;
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

void mdl_context_destroy(MDL_CONTEXT *context)
{
  DBUG_ASSERT(context->requests.is_empty());
  DBUG_ASSERT(context->tickets.is_empty());
  DBUG_ASSERT(!context->has_global_shared_lock);
}


/**
  Backup and reset state of meta-data locking context.

  mdl_context_backup_and_reset(), mdl_context_restore() and
  mdl_context_merge() are used by HANDLER implementation which
  needs to open table for new HANDLER independently of already
  open HANDLERs and add this table/metadata lock to the set of
  tables open/metadata locks for HANDLERs afterwards.
*/

void mdl_context_backup_and_reset(MDL_CONTEXT *ctx, MDL_CONTEXT *backup)
{
  backup->requests.empty();
  backup->tickets.empty();
  ctx->requests.swap(backup->requests);
  ctx->tickets.swap(backup->tickets);
}


/**
  Restore state of meta-data locking context from backup.
*/

void mdl_context_restore(MDL_CONTEXT *ctx, MDL_CONTEXT *backup)
{
  DBUG_ASSERT(ctx->requests.is_empty());
  DBUG_ASSERT(ctx->tickets.is_empty());
  ctx->requests.swap(backup->requests);
  ctx->tickets.swap(backup->tickets);
}


/**
  Merge meta-data locks from one context into another.
*/

void mdl_context_merge(MDL_CONTEXT *dst, MDL_CONTEXT *src)
{
  MDL_LOCK_TICKET *ticket;
  MDL_LOCK_REQUEST *lock_req;

  DBUG_ASSERT(dst->thd == src->thd);

  if (!src->requests.is_empty())
  {
    MDL_CONTEXT::Request_iterator it(src->requests);
    while ((lock_req= it++))
      dst->requests.push_front(lock_req);
    src->requests.empty();
  }

  if (!src->tickets.is_empty())
  {
    MDL_CONTEXT::Ticket_iterator it(src->tickets);
    while ((ticket= it++))
    {
      DBUG_ASSERT(ticket->ctx);
      ticket->ctx= dst;
      dst->tickets.push_front(ticket);
    }
    src->tickets.empty();
  }
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
  Instead it assumes that the life time of every lock request (including
  encompassed members db/name) encloses calls to mdl_request_add()
  and mdl_request_remove() or mdl_request_remove_all().

  @param  lock_req   Pointer to an MDL_LOCK_REQUEST object to initialize
  @param  type       Id of type of object to be locked
  @param  db         Name of database to which the object belongs
  @param  name       Name of of the object

  The initialized lock request will have MDL_SHARED type.

  Suggested lock types: TABLE - 0 PROCEDURE - 1 FUNCTION - 2
  Note that tables and views must have the same lock type, since
  they share the same name space in the SQL standard.
*/

void mdl_request_init(MDL_LOCK_REQUEST *lock_req, unsigned char type,
                      const char *db, const char *name)
{
  lock_req->key.mdl_key_init(type, db, name);
  lock_req->type= MDL_SHARED;
  lock_req->ticket= NULL;
}


/**
  Allocate and initialize one lock request.

  Same as mdl_init_lock(), but allocates the lock and the key buffer
  on a memory root. Necessary to lock ad-hoc tables, e.g.
  mysql.* tables of grant and data dictionary subsystems.

  @param  type       Id of type of object to be locked
  @param  db         Name of database to which object belongs
  @param  name       Name of of object
  @param  root       MEM_ROOT on which object should be allocated

  @note The allocated lock request will have MDL_SHARED type.

  @retval 0      Error if out of memory
  @retval non-0  Pointer to an object representing a lock request
*/

MDL_LOCK_REQUEST *
mdl_request_alloc(unsigned char type, const char *db,
                  const char *name, MEM_ROOT *root)
{
  MDL_LOCK_REQUEST *lock_req;

  if (!(lock_req= (MDL_LOCK_REQUEST*) alloc_root(root,
                                                 sizeof(MDL_LOCK_REQUEST))))
    return NULL;

  mdl_request_init(lock_req, type, db, name);

  return lock_req;
}


/**
  Add a lock request to the list of lock requests of the context.

  The procedure to acquire metadata locks is:
    - allocate and initialize lock requests (mdl_request_alloc())
    - associate them with a context (mdl_request_add())
    - call mdl_acquire_shared_lock()/mdl_ticket_release() (maybe repeatedly).

  Associates a lock request with the given context.

  @param  context    The MDL context to associate the lock with.
                     There should be no more than one context per
                     connection, to avoid deadlocks.
  @param  lock_req   The lock request to be added.
*/

void mdl_request_add(MDL_CONTEXT *context, MDL_LOCK_REQUEST *lock_req)
{
  DBUG_ENTER("mdl_request_add");
  DBUG_ASSERT(lock_req->ticket == NULL);
  context->requests.push_front(lock_req);
  DBUG_VOID_RETURN;
}


/**
  Remove a lock request from the list of lock requests.

  Disassociates a lock request from the given context.

  @param  context    The MDL context to remove the lock from.
  @param  lock_req   The lock request to be removed.

  @pre The lock request being removed should correspond to a ticket that
       was released or was not acquired.

  @note Resets lock request back to its initial state
        (i.e. sets type to MDL_SHARED).
*/

void mdl_request_remove(MDL_CONTEXT *context, MDL_LOCK_REQUEST *lock_req)
{
  DBUG_ENTER("mdl_request_remove");
  /* Reset lock request back to its initial state. */
  lock_req->type= MDL_SHARED;
  lock_req->ticket= NULL;
  context->requests.remove(lock_req);
  DBUG_VOID_RETURN;
}


/**
  Clear all lock requests in the context.
  Disassociates lock requests from the context.

  Also resets lock requests back to their initial state (i.e. MDL_SHARED).

  @param context Context to be cleared.
*/

void mdl_request_remove_all(MDL_CONTEXT *context)
{
  MDL_LOCK_REQUEST *lock_req;
  MDL_CONTEXT::Request_iterator it(context->requests);
  while ((lock_req= it++))
  {
    /* Reset lock request back to its initial state. */
    lock_req->type= MDL_SHARED;
    lock_req->ticket= NULL;
  }
  context->requests.empty();
}


/**
  Auxiliary functions needed for creation/destruction of MDL_LOCK objects.

  @todo This naive implementation should be replaced with one that saves
        on memory allocation by reusing released objects.
*/

static MDL_LOCK* alloc_lock_object(const MDL_KEY *mdl_key)
{
  return new MDL_LOCK(mdl_key);
}


static void free_lock_object(MDL_LOCK *lock)
{
  delete lock;
}


/**
  Auxiliary functions needed for creation/destruction of MDL_LOCK_TICKET
  objects.

  @todo This naive implementation should be replaced with one that saves
        on memory allocation by reusing released objects.
*/

static MDL_LOCK_TICKET* alloc_ticket_object(MDL_CONTEXT *context)
{
  MDL_LOCK_TICKET *ticket= new MDL_LOCK_TICKET;
  return ticket;
}


static void free_ticket_object(MDL_LOCK_TICKET *ticket)
{
  delete ticket;
}


/**
  Helper functions which simplifies writing various checks and asserts.
*/

template <typename T>
static inline bool is_shared(T *lock_data)
{
  return (lock_data->type < MDL_EXCLUSIVE);
}


/**
  Helper functions and macros to be used for killable waiting in metadata
  locking subsystem.

  @sa THD::enter_cond()/exit_cond()/killed.

  @note We can't use THD::enter_cond()/exit_cond()/killed directly here
        since this will make metadata subsystem dependant on THD class
        and thus prevent us from writing unit tests for it. And usage of
        wrapper functions to access THD::killed/enter_cond()/exit_cond()
        will probably introduce too much overhead.
*/

#define MDL_ENTER_COND(A, B) mdl_enter_cond(A, B, __func__, __FILE__, __LINE__)

static inline const char* mdl_enter_cond(MDL_CONTEXT *context,
                                         st_my_thread_var *mysys_var,
                                         const char *calling_func,
                                         const char *calling_file,
                                         const unsigned int calling_line)
{
  safe_mutex_assert_owner(&LOCK_mdl);

  mysys_var->current_mutex= &LOCK_mdl;
  mysys_var->current_cond= &COND_mdl;

  return set_thd_proc_info(context->thd, "Waiting for table",
                           calling_func, calling_file, calling_line);
}

#define MDL_EXIT_COND(A, B, C) mdl_exit_cond(A, B, C, __func__, __FILE__, __LINE__)

static inline void mdl_exit_cond(MDL_CONTEXT *context,
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

  (void) set_thd_proc_info(context->thd, old_msg, calling_func,
                           calling_file, calling_line);
}


/**
  Check if request for the lock on particular object can be satisfied given
  current state of the global metadata lock.

  @note In other words, we're trying to check that the individual lock
        request, implying a form of lock on the global metadata, is
        compatible with the current state of the global metadata lock.

  @param lock_req  Request for lock on an individual object, implying a
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

static bool can_grant_global_lock(enum_mdl_type type, bool is_upgrade)
{
  switch (type)
  {
  case MDL_SHARED:
  case MDL_SHARED_HIGH_PRIO:
    return TRUE;
    break;
  case MDL_SHARED_UPGRADABLE:
    if (global_lock.active_shared || global_lock.waiting_shared)
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
      DBUG_ASSERT(global_lock.active_shared == 0 &&
                  global_lock.active_intention_exclusive);
      return TRUE;
    }
    else
    {
      if (global_lock.active_shared || global_lock.waiting_shared)
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

  @param  lock       Lock.
  @param  lock_req   Request for lock.

  @retval TRUE   Lock request can be satisfied
  @retval FALSE  There is some conflicting lock.

  This function defines the following compatibility matrix for metadata locks:

                  | Satisfied or pending requests which we have in MDL_LOCK
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

static bool can_grant_lock(MDL_CONTEXT *ctx, MDL_LOCK *lock,
                           enum_mdl_type type, bool is_upgrade)
{
  bool can_grant= FALSE;

  switch (type) {
  case MDL_SHARED:
  case MDL_SHARED_UPGRADABLE:
  case MDL_SHARED_HIGH_PRIO:
    if (lock->type == MDL_LOCK::SHARED)
    {
      /* Pending exclusive locks have higher priority over shared locks. */
      if (lock->waiting.is_empty() || type == MDL_SHARED_HIGH_PRIO)
        can_grant= TRUE;
    }
    else if (lock->granted.head()->ctx == ctx)
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
      MDL_LOCK_TICKET *conf_lock_ticket;
      MDL_LOCK::Ticket_iterator it(lock->granted);

      /*
        There should be no active exclusive locks since we own shared lock
        on the object.
      */
      DBUG_ASSERT(lock->type == MDL_LOCK::SHARED);

      while ((conf_lock_ticket= it++))
      {
        /*
          When upgrading shared lock to exclusive one we can have other shared
          locks for the same object in the same context, e.g. in case when several
          instances of TABLE are open.
        */
        if (conf_lock_ticket->ctx != ctx)
          break;
      }
      /* Grant lock if there are no conflicting shared locks. */
      if (conf_lock_ticket == NULL)
        can_grant= TRUE;
      break;
    }
    else if (lock->type == MDL_LOCK::SHARED)
    {
      can_grant= lock->granted.is_empty();
    }
    break;
  default:
    DBUG_ASSERT(0);
  }
  return can_grant;
}


/**
  Check whether the context already holds a compatible lock ticket
  on a object. Only shared locks can be recursive.

  @param lock_req  Lock request object for lock to be acquired

  @return A pointer to the lock ticket for the object or NULL otherwise.
*/

static MDL_LOCK_TICKET *
mdl_context_find_ticket(MDL_CONTEXT *ctx, MDL_LOCK_REQUEST *lock_req)
{
  MDL_LOCK_TICKET *ticket;
  MDL_CONTEXT::Ticket_iterator it(ctx->tickets);

  DBUG_ASSERT(is_shared(lock_req));

  while ((ticket= it++))
  {
    if (lock_req->type == ticket->type &&
        lock_req->key.is_equal(&ticket->lock->key))
      break;
  }

  return ticket;
}


/**
  Try to acquire one shared lock.

  Unlike exclusive locks, shared locks are acquired one by
  one. This is interface is chosen to simplify introduction of
  the new locking API to the system. mdl_acquire_shared_lock()
  is currently used from open_table(), and there we have only one
  table to work with.

  In future we may consider allocating multiple shared locks at once.

  This function must be called after the lock is added to a context.

  @param context    [in]  Context containing request for lock
  @param lock_req   [in]  Lock request object for lock to be acquired
  @param retry      [out] Indicates that conflicting lock exists and another
                          attempt should be made after releasing all current
                          locks and waiting for conflicting lock go away
                          (using mdl_wait_for_locks()).

  @retval  FALSE   Success.
  @retval  TRUE    Failure. Either error occured or conflicting lock exists.
                   In the latter case "retry" parameter is set to TRUE.
*/

bool mdl_acquire_shared_lock(MDL_CONTEXT *context, MDL_LOCK_REQUEST *lock_req,
                             bool *retry)
{
  MDL_LOCK *lock;
  MDL_KEY *key= &lock_req->key;
  MDL_LOCK_TICKET *ticket;
  *retry= FALSE;

  DBUG_ASSERT(is_shared(lock_req) && lock_req->ticket == NULL);

  safe_mutex_assert_not_owner(&LOCK_open);

  if (context->has_global_shared_lock &&
      lock_req->type == MDL_SHARED_UPGRADABLE)
  {
    my_error(ER_CANT_UPDATE_WITH_READLOCK, MYF(0));
    return TRUE;
  }

  /*
    Check whether the context already holds a shared lock on the object,
    and if so, grant the request.
  */
  if ((ticket= mdl_context_find_ticket(context, lock_req)))
  {
    DBUG_ASSERT(ticket->state == MDL_ACQUIRED);
    lock_req->ticket= ticket;
    return FALSE;
  }

  pthread_mutex_lock(&LOCK_mdl);

  if (!can_grant_global_lock(lock_req->type, FALSE))
  {
    pthread_mutex_unlock(&LOCK_mdl);
    *retry= TRUE;
    return TRUE;
  }

  if (!(ticket= alloc_ticket_object(context)))
  {
    pthread_mutex_unlock(&LOCK_mdl);
    return TRUE;
  }

  if (!(lock= (MDL_LOCK*) my_hash_search(&mdl_locks,
                                         key->ptr(), key->length())))
  {
    /* Default lock type is MDL_LOCK::SHARED */
    lock= alloc_lock_object(key);
    if (!lock || my_hash_insert(&mdl_locks, (uchar*)lock))
    {
      free_lock_object(lock);
      free_ticket_object(ticket);
      pthread_mutex_unlock(&LOCK_mdl);
      return TRUE;
    }
  }

  if (can_grant_lock(context, lock, lock_req->type, FALSE))
  {
    lock->granted.push_front(ticket);
    context->tickets.push_front(ticket);
    ticket->state= MDL_ACQUIRED;
    lock_req->ticket= ticket;
    ticket->lock= lock;
    ticket->type= lock_req->type;
    ticket->ctx= context;
    if (lock_req->type == MDL_SHARED_UPGRADABLE)
      global_lock.active_intention_exclusive++;
  }
  else
  {
    /* We can't get here if we allocated a new lock. */
    DBUG_ASSERT(! lock->is_empty());
    *retry= TRUE;
    free_ticket_object(ticket);
  }

  pthread_mutex_unlock(&LOCK_mdl);

  return *retry;
}


static void release_ticket(MDL_CONTEXT *context, MDL_LOCK_TICKET *ticket);


/**
  Notify a thread holding a shared metadata lock of a pending exclusive lock.

  @param thd               Current thread context
  @param conf_lock_ticket  Conflicting metadata lock

  @retval TRUE   A thread was woken up
  @retval FALSE  Lock is not a shared one or no thread was woken up
*/

static bool notify_shared_lock(THD *thd, MDL_LOCK_TICKET *conf_lock_ticket)
{
  bool woke= FALSE;
  if (conf_lock_ticket->type != MDL_EXCLUSIVE)
    woke= mysql_notify_thread_having_shared_lock(thd, conf_lock_ticket->ctx->thd);
  return woke;
}


/**
  Acquire exclusive locks. The context must contain the list of
  locks to be acquired. There must be no granted locks in the
  context.

  This is a replacement of lock_table_names(). It is used in
  RENAME, DROP and other DDL SQL statements.

  @param context  A context containing requests for exclusive locks
                  The context may not have other lock requests.

  @retval FALSE  Success
  @retval TRUE   Failure
*/

bool mdl_acquire_exclusive_locks(MDL_CONTEXT *context)
{
  MDL_LOCK *lock;
  bool signalled= FALSE;
  const char *old_msg;
  MDL_LOCK_REQUEST *lock_req;
  MDL_LOCK_TICKET *ticket;
  st_my_thread_var *mysys_var= my_thread_var;
  MDL_CONTEXT::Request_iterator it(context->requests);

  safe_mutex_assert_not_owner(&LOCK_open);

  if (context->has_global_shared_lock)
  {
    my_error(ER_CANT_UPDATE_WITH_READLOCK, MYF(0));
    return TRUE;
  }

  pthread_mutex_lock(&LOCK_mdl);

  old_msg= MDL_ENTER_COND(context, mysys_var);

  while ((lock_req= it++))
  {
    MDL_KEY *key= &lock_req->key;
    DBUG_ASSERT(lock_req->type == MDL_EXCLUSIVE &&
                lock_req->ticket == NULL);

    /* Early allocation: ticket is used as a shortcut to the lock. */
    if (!(ticket= alloc_ticket_object(context)))
      goto err;

    if (!(lock= (MDL_LOCK*) my_hash_search(&mdl_locks,
                                           key->ptr(), key->length())))
    {
      lock= alloc_lock_object(key);
      if (!lock || my_hash_insert(&mdl_locks, (uchar*)lock))
      {
        free_ticket_object(ticket);
        free_lock_object(lock);
        goto err;
      }
    }

    lock_req->ticket= ticket;
    ticket->state= MDL_PENDING;
    ticket->ctx= context;
    ticket->lock= lock;
    ticket->type= lock_req->type;
    lock->waiting.push_front(ticket);
  }

  while (1)
  {
    it.rewind();
    while ((lock_req= it++))
    {
      lock= lock_req->ticket->lock;

      if (!can_grant_global_lock(lock_req->type, FALSE))
      {
        /*
          There is an active or pending global shared lock so we have
          to wait until it goes away.
        */
        signalled= TRUE;
        break;
      }
      else if (!can_grant_lock(context, lock, lock_req->type, FALSE))
      {
        MDL_LOCK_TICKET *conf_lock_ticket;
        MDL_LOCK::Ticket_iterator it(lock->granted);

        signalled= (lock->type == MDL_LOCK::EXCLUSIVE);

        while ((conf_lock_ticket= it++))
          signalled|= notify_shared_lock(context->thd, conf_lock_ticket);

        break;
      }
    }
    if (!lock_req)
      break;

    /* There is a shared or exclusive lock on the object. */
    DEBUG_SYNC(context->thd, "mdl_acquire_exclusive_locks_wait");

    if (signalled)
      pthread_cond_wait(&COND_mdl, &LOCK_mdl);
    else
    {
      /*
        Another thread obtained shared MDL-lock on some table but
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
  while ((lock_req= it++))
  {
    global_lock.active_intention_exclusive++;
    ticket= lock_req->ticket;
    lock= ticket->lock;
    lock->type= MDL_LOCK::EXCLUSIVE;
    lock->waiting.remove(ticket);
    lock->granted.push_front(ticket);
    context->tickets.push_front(ticket);
    ticket->state= MDL_ACQUIRED;
    if (lock->cached_object)
      (*lock->cached_object_release_hook)(lock->cached_object);
    lock->cached_object= NULL;
  }
  /* As a side-effect MDL_EXIT_COND() unlocks LOCK_mdl. */
  MDL_EXIT_COND(context, mysys_var, old_msg);
  return FALSE;

err:
  /*
    Remove our pending lock requests from the locks.
    Ignore those lock requests which were not made MDL_PENDING.
  */
  it.rewind();
  while ((lock_req= it++) && lock_req->ticket)
  {
    ticket= lock_req->ticket;
    DBUG_ASSERT(ticket->state == MDL_PENDING);
    lock= ticket->lock;
    free_ticket_object(ticket);
    lock->waiting.remove(ticket);
    /* Reset lock request back to its initial state. */
    lock_req->ticket= NULL;
    if (lock->is_empty())
    {
      my_hash_delete(&mdl_locks, (uchar *)lock);
      free_lock_object(lock);
    }
  }
  /* May be some pending requests for shared locks can be satisfied now. */
  pthread_cond_broadcast(&COND_mdl);
  MDL_EXIT_COND(context, mysys_var, old_msg);
  return TRUE;
}


/**
  Upgrade a shared metadata lock to exclusive.

  Used in ALTER TABLE, when a copy of the table with the
  new definition has been constructed.

  @param context   Context to which shared lock belongs
  @param ticket    Ticket for shared lock to be upgraded

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

bool mdl_upgrade_shared_lock_to_exclusive(MDL_CONTEXT *context,
                                          MDL_LOCK_TICKET *ticket)
{
  MDL_LOCK *lock= ticket->lock;
  const char *old_msg;
  st_my_thread_var *mysys_var= my_thread_var;

  DBUG_ENTER("mdl_upgrade_shared_lock_to_exclusive");
  DEBUG_SYNC(context->thd, "mdl_upgrade_shared_lock_to_exclusive");

  safe_mutex_assert_not_owner(&LOCK_open);

  /* Allow this function to be called twice for the same lock request. */
  if (ticket->type == MDL_EXCLUSIVE)
    DBUG_RETURN(FALSE);

  pthread_mutex_lock(&LOCK_mdl);

  old_msg= MDL_ENTER_COND(context, mysys_var);


  /*
    Since we should have already acquired an intention exclusive
    global lock this call is only enforcing asserts.
  */
  DBUG_ASSERT(can_grant_global_lock(MDL_EXCLUSIVE, TRUE));

  while (1)
  {
    if (can_grant_lock(context, lock, MDL_EXCLUSIVE, TRUE))
      break;

    bool signalled= FALSE;
    MDL_LOCK_TICKET *conf_lock_ticket;
    MDL_LOCK::Ticket_iterator it(lock->granted);

    while ((conf_lock_ticket= it++))
    {
      if (conf_lock_ticket->ctx != context)
        signalled|= notify_shared_lock(context->thd, conf_lock_ticket);
    }

    if (signalled)
      pthread_cond_wait(&COND_mdl, &LOCK_mdl);
    else
    {
      /*
        Another thread obtained shared MDL-lock on some table but
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
      MDL_EXIT_COND(context, mysys_var, old_msg);
      DBUG_RETURN(TRUE);
    }
  }

  lock->type= MDL_LOCK::EXCLUSIVE;
  /* Set the new type of lock in the ticket. */
  ticket->type= MDL_EXCLUSIVE;
  if (lock->cached_object)
    (*lock->cached_object_release_hook)(lock->cached_object);
  lock->cached_object= 0;

  /* As a side-effect MDL_EXIT_COND() unlocks LOCK_mdl. */
  MDL_EXIT_COND(context, mysys_var, old_msg);
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

  @param context  [in]  The context containing the lock request
  @param lock_req [in]  The lock request
  @param conflict [out] Indicates that conflicting lock exists

  @retval TRUE  Failure either conflicting lock exists or some error
                occured (probably OOM).
  @retval FALSE Success, lock was acquired.

  FIXME: Compared to lock_table_name_if_not_cached()
         it gives sligthly more false negatives.
*/

bool mdl_try_acquire_exclusive_lock(MDL_CONTEXT *context,
                                    MDL_LOCK_REQUEST *lock_req,
                                    bool *conflict)
{
  MDL_LOCK *lock;
  MDL_LOCK_TICKET *ticket;
  MDL_KEY *key= &lock_req->key;

  DBUG_ASSERT(lock_req->type == MDL_EXCLUSIVE &&
              lock_req->ticket == NULL);

  safe_mutex_assert_not_owner(&LOCK_open);

  *conflict= FALSE;

  pthread_mutex_lock(&LOCK_mdl);

  if (!(lock= (MDL_LOCK*) my_hash_search(&mdl_locks,
                                         key->ptr(), key->length())))
  {
    ticket= alloc_ticket_object(context);
    lock= alloc_lock_object(key);
    if (!ticket || !lock || my_hash_insert(&mdl_locks, (uchar*)lock))
    {
      free_ticket_object(ticket);
      free_lock_object(lock);
      goto err;
    }
    lock->type= MDL_LOCK::EXCLUSIVE;
    lock->granted.push_front(ticket);
    context->tickets.push_front(ticket);
    ticket->state= MDL_ACQUIRED;
    lock_req->ticket= ticket;
    ticket->ctx= context;
    ticket->lock= lock;
    ticket->type= lock_req->type;
    global_lock.active_intention_exclusive++;
    pthread_mutex_unlock(&LOCK_mdl);
    return FALSE;
  }

  /* There is some lock for the object. */
  *conflict= TRUE;

err:
  pthread_mutex_unlock(&LOCK_mdl);
  return TRUE;
}


/**
  Acquire global shared metadata lock.

  Holding this lock will block all requests for exclusive locks
  and shared locks which can be potentially upgraded to exclusive.

  @param context Current metadata locking context.

  @retval FALSE Success -- the lock was granted.
  @retval TRUE  Failure -- our thread was killed.
*/

bool mdl_acquire_global_shared_lock(MDL_CONTEXT *context)
{
  st_my_thread_var *mysys_var= my_thread_var;
  const char *old_msg;

  safe_mutex_assert_not_owner(&LOCK_open);
  DBUG_ASSERT(!context->has_global_shared_lock);

  pthread_mutex_lock(&LOCK_mdl);

  global_lock.waiting_shared++;
  old_msg= MDL_ENTER_COND(context, mysys_var);

  while (!mysys_var->abort && global_lock.active_intention_exclusive)
    pthread_cond_wait(&COND_mdl, &LOCK_mdl);

  global_lock.waiting_shared--;
  if (mysys_var->abort)
  {
    /* As a side-effect MDL_EXIT_COND() unlocks LOCK_mdl. */
    MDL_EXIT_COND(context, mysys_var, old_msg);
    return TRUE;
  }
  global_lock.active_shared++;
  context->has_global_shared_lock= TRUE;
  /* As a side-effect MDL_EXIT_COND() unlocks LOCK_mdl. */
  MDL_EXIT_COND(context, mysys_var, old_msg);
  return FALSE;
}


/**
  Wait until there will be no locks that conflict with lock requests
  in the context.

  This is a part of the locking protocol and must be used by the
  acquirer of shared locks after a back-off.

  Does not acquire the locks!

  @param context Context with which lock requests are associated.

  @retval FALSE  Success. One can try to obtain metadata locks.
  @retval TRUE   Failure (thread was killed)
*/

bool mdl_wait_for_locks(MDL_CONTEXT *context)
{
  MDL_LOCK *lock;
  MDL_LOCK_REQUEST *lock_req;
  MDL_CONTEXT::Request_iterator it(context->requests);
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
    mysql_ha_flush(context->thd);
    pthread_mutex_lock(&LOCK_mdl);
    old_msg= MDL_ENTER_COND(context, mysys_var);
    it.rewind();
    while ((lock_req= it++))
    {
      MDL_KEY *key= &lock_req->key;
      DBUG_ASSERT(lock_req->ticket == NULL);
      if (!can_grant_global_lock(lock_req->type, FALSE))
        break;
      /*
        To avoid starvation we don't wait if we have a conflict against
        request for MDL_EXCLUSIVE lock.
      */
      if (is_shared(lock_req) &&
          (lock= (MDL_LOCK*) my_hash_search(&mdl_locks, key->ptr(),
                                            key->length())) &&
          !can_grant_lock(context, lock, lock_req->type, FALSE))
        break;
    }
    if (!lock_req)
    {
      pthread_mutex_unlock(&LOCK_mdl);
      break;
    }
    pthread_cond_wait(&COND_mdl, &LOCK_mdl);
    /* As a side-effect MDL_EXIT_COND() unlocks LOCK_mdl. */
    MDL_EXIT_COND(context, mysys_var, old_msg);
  }
  return mysys_var->abort;
}


/**
  Auxiliary function which allows to release particular lock
  ownership of which is represented by a lock ticket object.
*/

static void release_ticket(MDL_CONTEXT *context, MDL_LOCK_TICKET *ticket)
{
  MDL_LOCK *lock= ticket->lock;
  DBUG_ENTER("release_ticket");
  DBUG_PRINT("enter", ("db=%s name=%s", lock->key.db_name(),
                                        lock->key.table_name()));

  safe_mutex_assert_owner(&LOCK_mdl);

  context->tickets.remove(ticket);

  switch (ticket->type)
  {
    case MDL_SHARED_UPGRADABLE:
      global_lock.active_intention_exclusive--;
      /* Fallthrough. */
    case MDL_SHARED:
    case MDL_SHARED_HIGH_PRIO:
      lock->granted.remove(ticket);
      break;
    case MDL_EXCLUSIVE:
      lock->type= MDL_LOCK::SHARED;
      lock->granted.remove(ticket);
      global_lock.active_intention_exclusive--;
      break;
    default:
      DBUG_ASSERT(0);
  }

  free_ticket_object(ticket);

  if (lock->is_empty())
  {
    my_hash_delete(&mdl_locks, (uchar *)lock);
    DBUG_PRINT("info", ("releasing cached_object cached_object=%p",
                        lock->cached_object));
    if (lock->cached_object)
      (*lock->cached_object_release_hook)(lock->cached_object);
    free_lock_object(lock);
  }

  DBUG_VOID_RETURN;
}


/**
  Release all locks associated with the context, but leave them
  in the context as lock requests.

  This function is used to back off in case of a lock conflict.
  It is also used to release shared locks in the end of an SQL
  statement.

  @param context The context with which the locks to be released
                 are associated.
*/

void mdl_ticket_release_all(MDL_CONTEXT *context)
{
  MDL_LOCK_TICKET *ticket;
  MDL_CONTEXT::Ticket_iterator it(context->tickets);
  DBUG_ENTER("mdl_ticket_release_all");

  safe_mutex_assert_not_owner(&LOCK_open);

  /* Detach lock tickets from the requests for back off. */
  {
    MDL_LOCK_REQUEST *lock_req;
    MDL_CONTEXT::Request_iterator it(context->requests);

    while ((lock_req= it++))
      lock_req->ticket= NULL;
  }

  if (context->tickets.is_empty())
    DBUG_VOID_RETURN;

  pthread_mutex_lock(&LOCK_mdl);
  while ((ticket= it++))
  {
    DBUG_PRINT("info", ("found lock to release ticket=%p", ticket));
    release_ticket(context, ticket);
  }
  /* Inefficient but will do for a while */
  pthread_cond_broadcast(&COND_mdl);
  pthread_mutex_unlock(&LOCK_mdl);

  context->tickets.empty();

  DBUG_VOID_RETURN;
}


/**
  Release a lock.

  @param context   Context containing lock in question
  @param ticket    Lock to be released
*/

void mdl_ticket_release(MDL_CONTEXT *context, MDL_LOCK_TICKET *ticket)
{
  DBUG_ASSERT(context == ticket->ctx);
  safe_mutex_assert_not_owner(&LOCK_open);

  pthread_mutex_lock(&LOCK_mdl);
  release_ticket(context, ticket);
  pthread_cond_broadcast(&COND_mdl);
  pthread_mutex_unlock(&LOCK_mdl);
}


/**
  Release all locks in the context which correspond to the same name/
  object as this lock request, remove lock requests from the context.

  @param context   Context containing locks in question
  @param ticket    One of the locks for the name/object for which all
                   locks should be released.
*/

void mdl_ticket_release_all_for_name(MDL_CONTEXT *context,
                                     MDL_LOCK_TICKET *ticket)
{
  MDL_LOCK *lock;

  /*
    We can use MDL_LOCK_TICKET::lock here to identify other locks for the same
    object since even though MDL_LOCK object might be reused for different
    lock after the first lock for this object have been released we can't
    have references to this other MDL_LOCK object in this context.
  */
  lock= ticket->lock;

  /* Remove matching lock requests from the context. */
  MDL_LOCK_REQUEST *lock_req;
  MDL_CONTEXT::Request_iterator it_lock_req(context->requests);

  while ((lock_req= it_lock_req++))
  {
    DBUG_ASSERT(lock_req->ticket && lock_req->ticket->state == MDL_ACQUIRED);
    if (lock_req->ticket->lock == lock)
      mdl_request_remove(context, lock_req);
  }

  /* Remove matching lock tickets from the context. */
  MDL_LOCK_TICKET *lock_tkt;
  MDL_CONTEXT::Ticket_iterator it_lock_tkt(context->tickets);

  while ((lock_tkt= it_lock_tkt++))
  {
    DBUG_ASSERT(lock_tkt->state == MDL_ACQUIRED);
    if (lock_tkt->lock == lock)
      mdl_ticket_release(context, lock_tkt);
  }
}


/**
  Downgrade an exclusive lock to shared metadata lock.

  @param context   A context to which exclusive lock belongs
  @param ticket    Ticket for exclusive lock to be downgraded
*/

void mdl_downgrade_exclusive_lock(MDL_CONTEXT *context,
                                  MDL_LOCK_TICKET *ticket)
{
  MDL_LOCK *lock;

  DBUG_ASSERT(context == ticket->ctx);

  safe_mutex_assert_not_owner(&LOCK_open);

  if (is_shared(ticket))
    return;

  lock= ticket->lock;

  pthread_mutex_lock(&LOCK_mdl);
  lock->type= MDL_LOCK::SHARED;
  ticket->type= MDL_SHARED_UPGRADABLE;
  pthread_cond_broadcast(&COND_mdl);
  pthread_mutex_unlock(&LOCK_mdl);
}


/**
  Release global shared metadata lock.

  @param context Current context
*/

void mdl_release_global_shared_lock(MDL_CONTEXT *context)
{
  safe_mutex_assert_not_owner(&LOCK_open);
  DBUG_ASSERT(context->has_global_shared_lock);

  pthread_mutex_lock(&LOCK_mdl);
  global_lock.active_shared--;
  context->has_global_shared_lock= FALSE;
  pthread_cond_broadcast(&COND_mdl);
  pthread_mutex_unlock(&LOCK_mdl);
}


/**
  Auxiliary function which allows to check if we have exclusive lock
  on the object.

  @param context Current context
  @param type    Id of object type
  @param db      Name of the database
  @param name    Name of the object

  @return TRUE if current context contains exclusive lock for the object,
          FALSE otherwise.
*/

bool mdl_is_exclusive_lock_owner(MDL_CONTEXT *context, unsigned char type,
                                 const char *db, const char *name)
{
  MDL_KEY key;
  MDL_LOCK_TICKET *ticket;
  MDL_CONTEXT::Ticket_iterator it(context->tickets);

  key.mdl_key_init(type, db, name);

  while ((ticket= it++))
  {
    if (ticket->lock->type == MDL_LOCK::EXCLUSIVE &&
        ticket->lock->key.is_equal(&key))
      break;
  }

  return ticket;
}


/**
  Auxiliary function which allows to check if we have some kind of lock on
  a object.

  @param context Current context
  @param type    Id of object type
  @param db      Name of the database
  @param name    Name of the object

  @return TRUE if current context contains satisfied lock for the object,
          FALSE otherwise.
*/

bool mdl_is_lock_owner(MDL_CONTEXT *context, unsigned char type,
                       const char *db, const char *name)
{
  MDL_KEY key;
  MDL_LOCK_TICKET *ticket;
  MDL_CONTEXT::Ticket_iterator it(context->tickets);

  key.mdl_key_init(type, db, name);

  while ((ticket= it++))
  {
    if (ticket->lock->key.is_equal(&key))
      break;
  }

  return ticket;
}


/**
  Check if we have any pending exclusive locks which conflict with
  existing shared lock.

  @param ticket Shared lock against which check should be performed.

  @return TRUE if there are any conflicting locks, FALSE otherwise.
*/

bool mdl_has_pending_conflicting_lock(MDL_LOCK_TICKET *ticket)
{
  bool result;

  DBUG_ASSERT(is_shared(ticket));
  safe_mutex_assert_not_owner(&LOCK_open);

  pthread_mutex_lock(&LOCK_mdl);
  result= !ticket->lock->waiting.is_empty();
  pthread_mutex_unlock(&LOCK_mdl);
  return result;
}


/**
  Associate pointer to an opaque object with a lock.

  @param ticket        Lock ticket for the lock with which the object
                       should be associated.
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

void mdl_set_cached_object(MDL_LOCK_TICKET *ticket, void *cached_object,
                           mdl_cached_object_release_hook release_hook)
{
  MDL_LOCK *lock= ticket->lock;
  DBUG_ENTER("mdl_set_cached_object");
  DBUG_PRINT("enter", ("db=%s name=%s cached_object=%p",
                        lock->key.db_name(), lock->key.table_name(),
                        cached_object));
  /*
    TODO: This assumption works now since we do mdl_get_cached_object()
          and mdl_set_cached_object() in the same critical section. Once
          this becomes false we will have to call release_hook here and
          use additional mutex protecting 'cached_object' member.
  */
  DBUG_ASSERT(!lock->cached_object);

  lock->cached_object= cached_object;
  lock->cached_object_release_hook= release_hook;

  DBUG_VOID_RETURN;
}


/**
  Get a pointer to an opaque object that associated with the lock.

  @param ticket  Lock ticket for the lock which the object is associated to.

  @return Pointer to an opaque object associated with the lock.
*/

void* mdl_get_cached_object(MDL_LOCK_TICKET *ticket)
{
  return ticket->lock->cached_object;
}



/**
  Releases metadata locks that were acquired after a specific savepoint.

  @note Used to release tickets acquired during a savepoint unit.
  @note It's safe to iterate and unlock any locks after taken after this
        savepoint because other statements that take other special locks
        cause a implicit commit (ie LOCK TABLES).

  @param thd  Current thread
  @param sv   Savepoint
*/

void mdl_rollback_to_savepoint(MDL_CONTEXT *ctx,
                               MDL_LOCK_TICKET *mdl_savepoint)
{
  MDL_LOCK_TICKET *mdl_lock_ticket;
  MDL_CONTEXT::Ticket_iterator it(ctx->tickets);
  DBUG_ENTER("mdl_rollback_to_savepoint");

  while ((mdl_lock_ticket= it++))
  {
    /* Stop when lock was acquired before this savepoint. */
    if (mdl_lock_ticket == mdl_savepoint)
      break;
    mdl_ticket_release(ctx, mdl_lock_ticket);
  }

  DBUG_VOID_RETURN;
}



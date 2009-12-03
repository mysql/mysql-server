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
  I_P_List<MDL_LOCK_DATA, MDL_LOCK_DATA_lock> active_shared;
  /*
    There can be several upgraders and active exclusive
    belonging to the same context.
  */
  I_P_List<MDL_LOCK_DATA, MDL_LOCK_DATA_lock> active_shared_waiting_upgrade;
  I_P_List<MDL_LOCK_DATA, MDL_LOCK_DATA_lock> active_exclusive;
  I_P_List<MDL_LOCK_DATA, MDL_LOCK_DATA_lock> waiting_exclusive;
  /**
     Number of MDL_LOCK_DATA objects associated with this MDL_LOCK instance
     and therefore present in one of above lists. Note that this number
     doesn't account for pending requests for shared lock since we don't
     associate them with MDL_LOCK and don't keep them in any list.
  */
  uint   lock_data_count;
  void   *cached_object;
  mdl_cached_object_release_hook cached_object_release_hook;

  MDL_LOCK() : cached_object(0), cached_object_release_hook(0) {}

  MDL_LOCK_DATA *get_key_owner()
  {
     return !active_shared.is_empty() ?
            active_shared.head() :
            (!active_shared_waiting_upgrade.is_empty() ?
             active_shared_waiting_upgrade.head() :
             (!active_exclusive.is_empty() ?
              active_exclusive.head() : waiting_exclusive.head()));
  }

  bool has_one_lock_data()
  {
    return (lock_data_count == 1);
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


extern "C" uchar *mdl_locks_key(const uchar *record, size_t *length,
                                my_bool not_used __attribute__((unused)))
{
  MDL_LOCK *entry=(MDL_LOCK*) record;
  *length= entry->get_key_owner()->key_length;
  return (uchar*) entry->get_key_owner()->key;
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
  context->locks.empty();
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
  DBUG_ASSERT(context->locks.is_empty());
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
  backup->locks.empty();
  ctx->locks.swap(backup->locks);
}


/**
   Restore state of meta-data locking context from backup.
*/

void mdl_context_restore(MDL_CONTEXT *ctx, MDL_CONTEXT *backup)
{
  DBUG_ASSERT(ctx->locks.is_empty());
  ctx->locks.swap(backup->locks);
}


/**
   Merge meta-data locks from one context into another.
*/

void mdl_context_merge(MDL_CONTEXT *dst, MDL_CONTEXT *src)
{
  MDL_LOCK_DATA *lock_data;

  DBUG_ASSERT(dst->thd == src->thd);

  if (!src->locks.is_empty())
  {
    I_P_List_iterator<MDL_LOCK_DATA, MDL_LOCK_DATA_context> it(src->locks);
    while ((lock_data= it++))
    {
      DBUG_ASSERT(lock_data->ctx);
      lock_data->ctx= dst;
      dst->locks.push_front(lock_data);
    }
    src->locks.empty();
  }
}


/**
   Initialize a lock request.

   This is to be used for every lock request.

   Note that initialization and allocation are split
   into two calls. This is to allow flexible memory management
   of lock requests. Normally a lock request is stored
   in statement memory (e.g. is a member of struct TABLE_LIST),
   but we would also like to allow allocation of lock
   requests in other memory roots, for example in the grant
   subsystem, to lock privilege tables.

   The MDL subsystem does not own or manage memory of lock
   requests. Instead it assumes that the life time of every lock
   request encloses calls to mdl_acquire_shared_lock() and
   mdl_release_locks().

   @param  lock_data  Pointer to an MDL_LOCK_DATA object to initialize
   @param  key_buff   Pointer to the buffer for key for the lock request
                      (should be at least 4+ strlen(db) + 1 + strlen(name)
                      + 1 bytes, or, if the lengths are not known,
                      MAX_MDLKEY_LENGTH)
   @param  type       Id of type of object to be locked
   @param  db         Name of database to which the object belongs
   @param  name       Name of of the object

   Stores the database name, object name and the type in the key
   buffer. Initializes mdl_el to point to the key.
   We can't simply initialize MDL_LOCK_DATA with type, db and name
   by-pointer because of the underlying HASH implementation
   requires the key to be a contiguous buffer.

   The initialized lock request will have MDL_SHARED type.

   Suggested lock types: TABLE - 0 PROCEDURE - 1 FUNCTION - 2
   Note that tables and views have the same lock type, since
   they share the same name space in the SQL standard.
*/

void mdl_init_lock(MDL_LOCK_DATA *lock_data, char *key, int type,
                   const char *db, const char *name)
{
  int4store(key, type);
  lock_data->key_length= (uint) (strmov(strmov(key+4, db)+1, name)-key)+1;
  lock_data->key= key;
  lock_data->type= MDL_SHARED;
  lock_data->state= MDL_INITIALIZED;
#ifndef DBUG_OFF
  lock_data->ctx= 0;
  lock_data->lock= 0;
#endif
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

   @retval 0      Error
   @retval non-0  Pointer to an object representing a lock request
*/

MDL_LOCK_DATA *mdl_alloc_lock(int type, const char *db, const char *name,
                              MEM_ROOT *root)
{
  MDL_LOCK_DATA *lock_data;
  char *key;

  if (!multi_alloc_root(root, &lock_data, sizeof(MDL_LOCK_DATA), &key,
                        MAX_MDLKEY_LENGTH, NULL))
    return NULL;

  mdl_init_lock(lock_data, key, type, db, name);

  return lock_data;
}


/**
   Add a lock request to the list of lock requests of the context.

   The procedure to acquire metadata locks is:
     - allocate and initialize lock requests (mdl_alloc_lock())
     - associate them with a context (mdl_add_lock())
     - call mdl_acquire_shared_lock()/mdl_release_lock() (maybe repeatedly).

   Associates a lock request with the given context.

   @param  context    The MDL context to associate the lock with.
                      There should be no more than one context per
                      connection, to avoid deadlocks.
   @param  lock_data  The lock request to be added.
*/

void mdl_add_lock(MDL_CONTEXT *context, MDL_LOCK_DATA *lock_data)
{
  DBUG_ENTER("mdl_add_lock");
  DBUG_ASSERT(lock_data->state == MDL_INITIALIZED);
  DBUG_ASSERT(!lock_data->ctx);
  lock_data->ctx= context;
  context->locks.push_front(lock_data);
  DBUG_VOID_RETURN;
}


/**
   Remove a lock request from the list of lock requests of the context.

   Disassociates a lock request from the given context.

   @param  context    The MDL context to remove the lock from.
   @param  lock_data  The lock request to be removed.

   @pre The lock request being removed should correspond to lock which
        was released or was not acquired.

   @note Resets lock request for lock released back to its initial state
         (i.e. sets type to MDL_SHARED).
*/

void mdl_remove_lock(MDL_CONTEXT *context, MDL_LOCK_DATA *lock_data)
{
  DBUG_ENTER("mdl_remove_lock");
  DBUG_ASSERT(lock_data->state == MDL_INITIALIZED);
  DBUG_ASSERT(context == lock_data->ctx);
  /* Reset lock request back to its initial state. */
  lock_data->type= MDL_SHARED;
#ifndef DBUG_OFF
  lock_data->ctx= 0;
#endif
  context->locks.remove(lock_data);
  DBUG_VOID_RETURN;
}


/**
   Clear all lock requests in the context (clear the context).

   Disassociates lock requests from the context.
   All granted locks must be released prior to calling this
   function.

   In other words, the expected procedure to release locks is:
     - mdl_release_locks();
     - mdl_remove_all_locks();

   We could possibly merge mdl_remove_all_locks() and mdl_release_locks(),
   but this function comes in handy when we need to back off: in that case
   we release all the locks acquired so-far but do not free them, since
   we know that the respective lock requests will be used again.

   Also resets lock requests back to their initial state (i.e. MDL_SHARED).

   @param context Context to be cleared.
*/

void mdl_remove_all_locks(MDL_CONTEXT *context)
{
  MDL_LOCK_DATA *lock_data;
  I_P_List_iterator<MDL_LOCK_DATA, MDL_LOCK_DATA_context> it(context->locks);
  while ((lock_data= it++))
  {
    /* Reset lock request back to its initial state. */
    lock_data->type= MDL_SHARED;
#ifndef DBUG_OFF
    lock_data->ctx= 0;
#endif
  }
  context->locks.empty();
}


/**
   Auxiliary functions needed for creation/destruction of MDL_LOCK
   objects.

   @todo This naive implementation should be replaced with one that saves
         on memory allocation by reusing released objects.
*/

static MDL_LOCK* get_lock_object(void)
{
  return new MDL_LOCK();
}


static void release_lock_object(MDL_LOCK *lock)
{
  delete lock;
}


/**
   Helper functions which simplifies writing various checks and asserts.
*/

static bool is_shared(MDL_LOCK_DATA *lock_data)
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

   @param lock_data Request for lock on an individual object, implying a
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

static bool can_grant_global_lock(MDL_LOCK_DATA *lock_data)
{
  switch (lock_data->type)
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
    if (lock_data->state == MDL_PENDING_UPGRADE)
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
   @param  lock_data  Request for lock.

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

static bool can_grant_lock(MDL_LOCK *lock, MDL_LOCK_DATA *lock_data)
{
  switch (lock_data->type)
  {
  case MDL_SHARED:
  case MDL_SHARED_UPGRADABLE:
  case MDL_SHARED_HIGH_PRIO:
    if ((lock->active_exclusive.is_empty() &&
         (lock_data->type == MDL_SHARED_HIGH_PRIO ||
          lock->waiting_exclusive.is_empty() &&
          lock->active_shared_waiting_upgrade.is_empty())) ||
        (!lock->active_exclusive.is_empty() &&
         lock->active_exclusive.head()->ctx == lock_data->ctx))
    {
      /*
        When exclusive lock comes from the same context we can satisfy our
        shared lock. This is required for CREATE TABLE ... SELECT ... and
        ALTER VIEW ... AS ....
      */
      return TRUE;
    }
    else
      return FALSE;
    break;
  case MDL_EXCLUSIVE:
    if (lock_data->state == MDL_PENDING_UPGRADE)
    {
      /* We are upgrading MDL_SHARED to MDL_EXCLUSIVE. */
      MDL_LOCK_DATA *conf_lock_data;
      I_P_List_iterator<MDL_LOCK_DATA,
                        MDL_LOCK_DATA_lock> it(lock->active_shared);

      /*
        There should be no active exclusive locks since we own shared lock
        on the object.
      */
      DBUG_ASSERT(lock->active_exclusive.is_empty() &&
                  lock->active_shared_waiting_upgrade.head() == lock_data);

      while ((conf_lock_data= it++))
      {
        /*
          When upgrading shared lock to exclusive one we can have other shared
          locks for the same object in the same context, e.g. in case when several
          instances of TABLE are open.
        */
        if (conf_lock_data->ctx != lock_data->ctx)
          return FALSE;
      }
      return TRUE;
    }
    else
    {
      return (lock->active_exclusive.is_empty() &&
              lock->active_shared_waiting_upgrade.is_empty() &&
              lock->active_shared.is_empty());
    }
    break;
  default:
    DBUG_ASSERT(0);
  }
  return FALSE;
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
   @param lock_data  [in]  Lock request object for lock to be acquired
   @param retry      [out] Indicates that conflicting lock exists and another
                           attempt should be made after releasing all current
                           locks and waiting for conflicting lock go away
                           (using mdl_wait_for_locks()).

   @retval  FALSE   Success.
   @retval  TRUE    Failure. Either error occured or conflicting lock exists.
                    In the latter case "retry" parameter is set to TRUE.
*/

bool mdl_acquire_shared_lock(MDL_CONTEXT *context, MDL_LOCK_DATA *lock_data,
                             bool *retry)
{
  MDL_LOCK *lock;
  *retry= FALSE;

  DBUG_ASSERT(is_shared(lock_data) && lock_data->state == MDL_INITIALIZED);

  DBUG_ASSERT(lock_data->ctx == context);

  safe_mutex_assert_not_owner(&LOCK_open);

  if (context->has_global_shared_lock &&
      lock_data->type == MDL_SHARED_UPGRADABLE)
  {
    my_error(ER_CANT_UPDATE_WITH_READLOCK, MYF(0));
    return TRUE;
  }

  pthread_mutex_lock(&LOCK_mdl);

  if (!can_grant_global_lock(lock_data))
  {
    pthread_mutex_unlock(&LOCK_mdl);
    *retry= TRUE;
    return TRUE;
  }

  if (!(lock= (MDL_LOCK *)my_hash_search(&mdl_locks, (uchar*)lock_data->key,
                                         lock_data->key_length)))
  {
    if (!(lock= get_lock_object()))
    {
      pthread_mutex_unlock(&LOCK_mdl);
      return TRUE;
    }
    /*
      Before inserting MDL_LOCK object into hash we should add at least one
      MDL_LOCK_DATA to its lists in order to provide key for this element.
      Thus we can't merge two branches of the above if-statement.
    */
    lock->active_shared.push_front(lock_data);
    lock->lock_data_count= 1;
    if (my_hash_insert(&mdl_locks, (uchar*)lock))
    {
      release_lock_object(lock);
      pthread_mutex_unlock(&LOCK_mdl);
      return TRUE;
    }
    lock_data->state= MDL_ACQUIRED;
    lock_data->lock= lock;
    if (lock_data->type == MDL_SHARED_UPGRADABLE)
      global_lock.active_intention_exclusive++;
  }
  else
  {
    if (can_grant_lock(lock, lock_data))
    {
      lock->active_shared.push_front(lock_data);
      lock->lock_data_count++;
      lock_data->state= MDL_ACQUIRED;
      lock_data->lock= lock;
      if (lock_data->type == MDL_SHARED_UPGRADABLE)
        global_lock.active_intention_exclusive++;
    }
    else
      *retry= TRUE;
  }
  pthread_mutex_unlock(&LOCK_mdl);

  return *retry;
}


static void release_lock(MDL_LOCK_DATA *lock_data);


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
  MDL_LOCK_DATA *lock_data;
  MDL_LOCK *lock;
  bool signalled= FALSE;
  const char *old_msg;
  I_P_List_iterator<MDL_LOCK_DATA, MDL_LOCK_DATA_context> it(context->locks);
  st_my_thread_var *mysys_var= my_thread_var;

  safe_mutex_assert_not_owner(&LOCK_open);

  if (context->has_global_shared_lock)
  {
    my_error(ER_CANT_UPDATE_WITH_READLOCK, MYF(0));
    return TRUE;
  }

  pthread_mutex_lock(&LOCK_mdl);

  old_msg= MDL_ENTER_COND(context, mysys_var);

  while ((lock_data= it++))
  {
    DBUG_ASSERT(lock_data->type == MDL_EXCLUSIVE &&
                lock_data->state == MDL_INITIALIZED);
    if (!(lock= (MDL_LOCK *) my_hash_search(&mdl_locks, (uchar*)lock_data->key,
                                            lock_data->key_length)))
    {
      if (!(lock= get_lock_object()))
        goto err;
      /*
        Again before inserting MDL_LOCK into hash provide key for
        it by adding MDL_LOCK_DATA to one of its lists.
      */
      lock->waiting_exclusive.push_front(lock_data);
      lock->lock_data_count= 1;
      if (my_hash_insert(&mdl_locks, (uchar*)lock))
      {
        release_lock_object(lock);
        goto err;
      }
      lock_data->lock= lock;
      lock_data->state= MDL_PENDING;
    }
    else
    {
      lock->waiting_exclusive.push_front(lock_data);
      lock->lock_data_count++;
      lock_data->lock= lock;
      lock_data->state= MDL_PENDING;
    }
  }

  while (1)
  {
    it.rewind();
    while ((lock_data= it++))
    {
      lock= lock_data->lock;

      if (!can_grant_global_lock(lock_data))
      {
        /*
          There is an active or pending global shared lock so we have
          to wait until it goes away.
        */
        signalled= TRUE;
        break;
      }
      else if (!can_grant_lock(lock, lock_data))
      {
        MDL_LOCK_DATA *conf_lock_data;
        I_P_List_iterator<MDL_LOCK_DATA,
                          MDL_LOCK_DATA_lock> it(lock->active_shared);

        signalled= !lock->active_exclusive.is_empty() ||
                   !lock->active_shared_waiting_upgrade.is_empty();

        while ((conf_lock_data= it++))
        {
          signalled|=
            mysql_notify_thread_having_shared_lock(context->thd,
                                                   conf_lock_data->ctx->thd);
        }

        break;
      }
    }
    if (!lock_data)
      break;
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
  while ((lock_data= it++))
  {
    global_lock.active_intention_exclusive++;
    lock= lock_data->lock;
    lock->waiting_exclusive.remove(lock_data);
    lock->active_exclusive.push_front(lock_data);
    lock_data->state= MDL_ACQUIRED;
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
  while ((lock_data= it++) && lock_data->state == MDL_PENDING)
  {
    release_lock(lock_data);
    lock_data->state= MDL_INITIALIZED;
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
   @param lock_data Satisfied request for shared lock to be upgraded

   @note In case of failure to upgrade lock (e.g. because upgrader
         was killed) leaves lock in its original state (locked in
         shared mode).

   @retval FALSE  Success
   @retval TRUE   Failure (thread was killed)
*/

bool mdl_upgrade_shared_lock_to_exclusive(MDL_CONTEXT *context,
                                          MDL_LOCK_DATA *lock_data)
{
  MDL_LOCK *lock;
  const char *old_msg;
  st_my_thread_var *mysys_var= my_thread_var;

  DBUG_ENTER("mdl_upgrade_shared_lock_to_exclusive");

  safe_mutex_assert_not_owner(&LOCK_open);

  DBUG_ASSERT(lock_data->state == MDL_ACQUIRED);

  /* Allow this function to be called twice for the same lock request. */
  if (lock_data->type == MDL_EXCLUSIVE)
    DBUG_RETURN(FALSE);

  DBUG_ASSERT(lock_data->type == MDL_SHARED_UPGRADABLE);

  lock= lock_data->lock;

  pthread_mutex_lock(&LOCK_mdl);

  old_msg= MDL_ENTER_COND(context, mysys_var);

  lock_data->state= MDL_PENDING_UPGRADE;
  /* Set type of lock request to the type at which we are aiming. */
  lock_data->type= MDL_EXCLUSIVE;
  lock->active_shared.remove(lock_data);
  /*
    There can be only one upgrader for this lock or we will have deadlock.
    This invariant is ensured by code outside of metadata subsystem usually
    by obtaining some sort of exclusive table-level lock (e.g. TL_WRITE,
    TL_WRITE_ALLOW_READ) before performing upgrade of metadata lock.
  */
  DBUG_ASSERT(lock->active_shared_waiting_upgrade.is_empty());
  lock->active_shared_waiting_upgrade.push_front(lock_data);

  /*
    Since we should have been already acquired intention exclusive global lock
    this call is only enforcing asserts.
  */
  DBUG_ASSERT(can_grant_global_lock(lock_data));

  while (1)
  {
    if (can_grant_lock(lock, lock_data))
      break;

    bool signalled= FALSE;
    MDL_LOCK_DATA *conf_lock_data;
    I_P_List_iterator<MDL_LOCK_DATA, MDL_LOCK_DATA_lock> it(lock->active_shared);

    while ((conf_lock_data= it++))
    {
      if (conf_lock_data->ctx != context)
      {
        signalled|=
          mysql_notify_thread_having_shared_lock(context->thd,
                                                 conf_lock_data->ctx->thd);
      }
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
      lock_data->state= MDL_ACQUIRED;
      lock_data->type= MDL_SHARED_UPGRADABLE;
      lock->active_shared_waiting_upgrade.remove(lock_data);
      lock->active_shared.push_front(lock_data);
      /* Pending requests for shared locks can be satisfied now. */
      pthread_cond_broadcast(&COND_mdl);
      MDL_EXIT_COND(context, mysys_var, old_msg);
      DBUG_RETURN(TRUE);
    }
  }

  lock->active_shared_waiting_upgrade.remove(lock_data);
  lock->active_exclusive.push_front(lock_data);
  lock_data->state= MDL_ACQUIRED;
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
   @param lock     [in]  The lock request
   @param conflict [out] Indicates that conflicting lock exists

   @retval TRUE  Failure either conflicting lock exists or some error
                 occured (probably OOM).
   @retval FALSE Success, lock was acquired.

   FIXME: Compared to lock_table_name_if_not_cached()
          it gives sligthly more false negatives.
*/

bool mdl_try_acquire_exclusive_lock(MDL_CONTEXT *context,
                                    MDL_LOCK_DATA *lock_data,
                                    bool *conflict)
{
  MDL_LOCK *lock;

  DBUG_ASSERT(lock_data->type == MDL_EXCLUSIVE &&
              lock_data->state == MDL_INITIALIZED);

  safe_mutex_assert_not_owner(&LOCK_open);

  *conflict= FALSE;

  pthread_mutex_lock(&LOCK_mdl);

  if (!(lock= (MDL_LOCK *)my_hash_search(&mdl_locks, (uchar*)lock_data->key,
                                         lock_data->key_length)))
  {
    if (!(lock= get_lock_object()))
      goto err;
    lock->active_exclusive.push_front(lock_data);
    lock->lock_data_count= 1;
    if (my_hash_insert(&mdl_locks, (uchar*)lock))
    {
      release_lock_object(lock);
      goto err;
    }
    lock_data->state= MDL_ACQUIRED;
    lock_data->lock= lock;
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
  MDL_LOCK_DATA *lock_data;
  MDL_LOCK *lock;
  I_P_List_iterator<MDL_LOCK_DATA, MDL_LOCK_DATA_context> it(context->locks);
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
    while ((lock_data= it++))
    {
      DBUG_ASSERT(lock_data->state == MDL_INITIALIZED);
      if (!can_grant_global_lock(lock_data))
        break;
      /*
        To avoid starvation we don't wait if we have a conflict against
        request for MDL_EXCLUSIVE lock.
      */
      if (is_shared(lock_data) &&
          (lock= (MDL_LOCK *)my_hash_search(&mdl_locks, (uchar*)lock_data->key,
                                            lock_data->key_length)) &&
          !can_grant_lock(lock, lock_data))
        break;
    }
    if (!lock_data)
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
   ownership of which is represented by lock request object.
*/

static void release_lock(MDL_LOCK_DATA *lock_data)
{
  MDL_LOCK *lock;

  DBUG_ENTER("release_lock");
  DBUG_PRINT("enter", ("db=%s name=%s", lock_data->key + 4,
                        lock_data->key + 4 + strlen(lock_data->key + 4) + 1));

  DBUG_ASSERT(lock_data->state == MDL_PENDING ||
              lock_data->state == MDL_ACQUIRED);

  lock= lock_data->lock;
  if (lock->has_one_lock_data())
  {
    my_hash_delete(&mdl_locks, (uchar *)lock);
    DBUG_PRINT("info", ("releasing cached_object cached_object=%p",
                        lock->cached_object));
    if (lock->cached_object)
      (*lock->cached_object_release_hook)(lock->cached_object);
    release_lock_object(lock);
    if (lock_data->state == MDL_ACQUIRED &&
        (lock_data->type == MDL_EXCLUSIVE ||
         lock_data->type == MDL_SHARED_UPGRADABLE))
      global_lock.active_intention_exclusive--;
  }
  else
  {
    switch (lock_data->type)
    {
      case MDL_SHARED_UPGRADABLE:
        global_lock.active_intention_exclusive--;
        /* Fallthrough. */
      case MDL_SHARED:
      case MDL_SHARED_HIGH_PRIO:
        lock->active_shared.remove(lock_data);
        break;
      case MDL_EXCLUSIVE:
        if (lock_data->state == MDL_PENDING)
          lock->waiting_exclusive.remove(lock_data);
        else
        {
          lock->active_exclusive.remove(lock_data);
          global_lock.active_intention_exclusive--;
        }
        break;
      default:
        DBUG_ASSERT(0);
    }
    lock->lock_data_count--;
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

void mdl_release_locks(MDL_CONTEXT *context)
{
  MDL_LOCK_DATA *lock_data;
  I_P_List_iterator<MDL_LOCK_DATA, MDL_LOCK_DATA_context> it(context->locks);
  DBUG_ENTER("mdl_release_locks");

  safe_mutex_assert_not_owner(&LOCK_open);

  pthread_mutex_lock(&LOCK_mdl);
  while ((lock_data= it++))
  {
    DBUG_PRINT("info", ("found lock to release lock_data=%p", lock_data));
    /*
      We should not release locks which pending shared locks as these
      are not associated with lock object and don't present in its
      lists. Allows us to avoid problems in open_tables() in case of
      back-off
    */
    if (lock_data->state != MDL_INITIALIZED)
    {
      release_lock(lock_data);
      lock_data->state= MDL_INITIALIZED;
#ifndef DBUG_OFF
      lock_data->lock= 0;
#endif
    }
    /*
      We will return lock request to its initial state only in
      mdl_remove_all_locks() since we need to know type of lock
      request in mdl_wait_for_locks().
    */
  }
  /* Inefficient but will do for a while */
  pthread_cond_broadcast(&COND_mdl);
  pthread_mutex_unlock(&LOCK_mdl);
  DBUG_VOID_RETURN;
}


/**
   Release a lock.

   @param context   Context containing lock in question
   @param lock_data Lock to be released

*/

void mdl_release_lock(MDL_CONTEXT *context, MDL_LOCK_DATA *lock_data)
{
  safe_mutex_assert_not_owner(&LOCK_open);

  pthread_mutex_lock(&LOCK_mdl);
  release_lock(lock_data);
#ifndef DBUG_OFF
  lock_data->lock= 0;
#endif
  lock_data->state= MDL_INITIALIZED;
  pthread_cond_broadcast(&COND_mdl);
  pthread_mutex_unlock(&LOCK_mdl);
}


/**
   Release all locks in the context which correspond to the same name/
   object as this lock request, remove lock requests from the context.

   @param context   Context containing locks in question
   @param lock_data One of the locks for the name/object for which all
                    locks should be released.
*/

void mdl_release_and_remove_all_locks_for_name(MDL_CONTEXT *context,
                                               MDL_LOCK_DATA *lock_data)
{
  MDL_LOCK *lock;
  I_P_List_iterator<MDL_LOCK_DATA, MDL_LOCK_DATA_context> it(context->locks);

  DBUG_ASSERT(lock_data->state == MDL_ACQUIRED);

  /*
    We can use MDL_LOCK_DATA::lock here to identify other locks for the same
    object since even altough MDL_LOCK object might be reused for different
    lock after the first lock for this object have been released we can't
    have references to this other MDL_LOCK object in this context.
  */
  lock= lock_data->lock;

  while ((lock_data= it++))
  {
    DBUG_ASSERT(lock_data->state == MDL_ACQUIRED);
    if (lock_data->lock == lock)
    {
      mdl_release_lock(context, lock_data);
      mdl_remove_lock(context, lock_data);
    }
  }
}


/**
   Downgrade an exclusive lock to shared metadata lock.

   @param context   A context to which exclusive lock belongs
   @param lock_data Satisfied request for exclusive lock to be downgraded
*/

void mdl_downgrade_exclusive_lock(MDL_CONTEXT *context,
                                  MDL_LOCK_DATA *lock_data)
{
  MDL_LOCK *lock;

  safe_mutex_assert_not_owner(&LOCK_open);

  DBUG_ASSERT(lock_data->state == MDL_ACQUIRED);

  if (is_shared(lock_data))
    return;

  lock= lock_data->lock;

  pthread_mutex_lock(&LOCK_mdl);
  lock->active_exclusive.remove(lock_data);
  lock_data->type= MDL_SHARED_UPGRADABLE;
  lock->active_shared.push_front(lock_data);
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

bool mdl_is_exclusive_lock_owner(MDL_CONTEXT *context, int type,
                                 const char *db, const char *name)
{
  char key[MAX_MDLKEY_LENGTH];
  uint key_length;
  MDL_LOCK_DATA *lock_data;
  I_P_List_iterator<MDL_LOCK_DATA, MDL_LOCK_DATA_context> it(context->locks);

  int4store(key, type);
  key_length= (uint) (strmov(strmov(key+4, db)+1, name)-key)+1;

  while ((lock_data= it++) &&
         (lock_data->key_length != key_length ||
          memcmp(lock_data->key, key, key_length) ||
          !(lock_data->type == MDL_EXCLUSIVE &&
            lock_data->state == MDL_ACQUIRED)))
    continue;
  return lock_data;
}


/**
   Auxiliary function which allows to check if we some kind of lock on
   the object.

   @param context Current context
   @param type    Id of object type
   @param db      Name of the database
   @param name    Name of the object

   @return TRUE if current context contains satisfied lock for the object,
           FALSE otherwise.
*/

bool mdl_is_lock_owner(MDL_CONTEXT *context, int type, const char *db,
                        const char *name)
{
  char key[MAX_MDLKEY_LENGTH];
  uint key_length;
  MDL_LOCK_DATA *lock_data;
  I_P_List_iterator<MDL_LOCK_DATA, MDL_LOCK_DATA_context> it(context->locks);

  int4store(key, type);
  key_length= (uint) (strmov(strmov(key+4, db)+1, name)-key)+1;

  while ((lock_data= it++) &&
         (lock_data->key_length != key_length ||
          memcmp(lock_data->key, key, key_length) ||
          lock_data->state != MDL_ACQUIRED))
    continue;

  return lock_data;
}


/**
   Check if we have any pending exclusive locks which conflict with
   existing shared lock.

   @param lock_data Shared lock against which check should be performed.

   @return TRUE if there are any conflicting locks, FALSE otherwise.
*/

bool mdl_has_pending_conflicting_lock(MDL_LOCK_DATA *lock_data)
{
  bool result;

  DBUG_ASSERT(is_shared(lock_data) && lock_data->state == MDL_ACQUIRED);
  safe_mutex_assert_not_owner(&LOCK_open);

  pthread_mutex_lock(&LOCK_mdl);
  result= !(lock_data->lock->waiting_exclusive.is_empty() &&
            lock_data->lock->active_shared_waiting_upgrade.is_empty());
  pthread_mutex_unlock(&LOCK_mdl);
  return result;
}


/**
   Associate pointer to an opaque object with a lock.

   @param lock_data     Lock request for the lock with which the
                        object should be associated.
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

void mdl_set_cached_object(MDL_LOCK_DATA *lock_data, void *cached_object,
                           mdl_cached_object_release_hook release_hook)
{
  DBUG_ENTER("mdl_set_cached_object");
  DBUG_PRINT("enter", ("db=%s name=%s cached_object=%p", lock_data->key + 4,
                       lock_data->key + 4 + strlen(lock_data->key + 4) + 1,
                       cached_object));

  DBUG_ASSERT(lock_data->state == MDL_ACQUIRED ||
              lock_data->state == MDL_PENDING_UPGRADE);

  /*
    TODO: This assumption works now since we do mdl_get_cached_object()
          and mdl_set_cached_object() in the same critical section. Once
          this becomes false we will have to call release_hook here and
          use additional mutex protecting 'cached_object' member.
  */
  DBUG_ASSERT(!lock_data->lock->cached_object);

  lock_data->lock->cached_object= cached_object;
  lock_data->lock->cached_object_release_hook= release_hook;

  DBUG_VOID_RETURN;
}


/**
   Get a pointer to an opaque object that associated with the lock.

   @param  lock_data Lock request for the lock with which the object is
                     associated.

   @return Pointer to an opaque object associated with the lock.
*/

void* mdl_get_cached_object(MDL_LOCK_DATA *lock_data)
{
  DBUG_ASSERT(lock_data->state == MDL_ACQUIRED ||
              lock_data->state == MDL_PENDING_UPGRADE);
  return lock_data->lock->cached_object;
}

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



/*
  TODO: Remove this dependency on mysql_priv.h. It's not
  trivial step at the moment since currently we access to
  some of THD members and use some of its methods here.
*/
#include "mysql_priv.h"
#include "mdl.h"


/**
   The lock context. Created internally for an acquired lock.
   For a given name, there exists only one MDL_LOCK_DATA instance,
   and it exists only when the lock has been granted.
   Can be seen as an MDL subsystem's version of TABLE_SHARE.
*/

struct MDL_LOCK_DATA
{
  I_P_List<MDL_LOCK, MDL_LOCK_lock> active_shared;
  /*
    There can be several upgraders and active exclusive
    belonging to the same context.
  */
  I_P_List<MDL_LOCK, MDL_LOCK_lock> active_shared_waiting_upgrade;
  I_P_List<MDL_LOCK, MDL_LOCK_lock> active_exclusive;
  I_P_List<MDL_LOCK, MDL_LOCK_lock> waiting_exclusive;
  /**
     Number of MDL_LOCK objects associated with this MDL_LOCK_DATA instance
     and therefore present in one of above lists. Note that this number
     doesn't account for pending requests for shared lock since we don't
     associate them with MDL_LOCK_DATA and don't keep them in any list.
  */
  uint   lock_count;
  void   *cached_object;
  mdl_cached_object_release_hook cached_object_release_hook;

  MDL_LOCK_DATA() : cached_object(0), cached_object_release_hook(0) {}

  MDL_LOCK *get_key_owner()
  {
     return !active_shared.is_empty() ?
            active_shared.head() :
            (!active_shared_waiting_upgrade.is_empty() ?
             active_shared_waiting_upgrade.head() :
             (!active_exclusive.is_empty() ?
              active_exclusive.head() : waiting_exclusive.head()));
  }

  bool has_one_lock()
  {
    return (lock_count == 1);
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

struct MDL_GLOBAL_LOCK_DATA
{
  uint shared_pending;
  uint shared_acquired;
  uint intention_exclusive_acquired;
} global_lock;


extern "C" uchar *mdl_locks_key(const uchar *record, size_t *length,
                                my_bool not_used __attribute__((unused)))
{
  MDL_LOCK_DATA *entry=(MDL_LOCK_DATA*) record;
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
  global_lock.shared_pending= global_lock.shared_acquired= 0;
  global_lock.intention_exclusive_acquired= 0;
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
  MDL_LOCK *l;

  DBUG_ASSERT(dst->thd == src->thd);

  if (!src->locks.is_empty())
  {
    I_P_List_iterator<MDL_LOCK, MDL_LOCK_context> it(src->locks);
    while ((l= it++))
    {
      DBUG_ASSERT(l->ctx);
      l->ctx= dst;
      dst->locks.push_front(l);
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

   @param  mdl        Pointer to an MDL_LOCK object to initialize
   @param  key_buff   Pointer to the buffer for key for the lock request
                      (should be at least strlen(db) + strlen(name)
                      + 2 bytes, or, if the lengths are not known,                                                                                       MAX_DBNAME_LENGTH)
   @param  type       Id of type of object to be locked
   @param  db         Name of database to which the object belongs
   @param  name       Name of of the object

   Stores the database name, object name and the type in the key
   buffer. Initializes mdl_el to point to the key.
   We can't simply initialize mdl_el with type, db and name
   by-pointer because of the underlying HASH implementation
   requires the key to be a contiguous buffer.

   The initialized lock request will have MDL_SHARED type and
   normal priority.

   Suggested lock types: TABLE - 0 PROCEDURE - 1 FUNCTION - 2
   Note that tables and views have the same lock type, since
   they share the same name space in the SQL standard.
*/

void mdl_init_lock(MDL_LOCK *mdl, char *key, int type, const char *db,
                   const char *name)
{
  int4store(key, type);
  mdl->key_length= (uint) (strmov(strmov(key+4, db)+1, name)-key)+1;
  mdl->key= key;
  mdl->type= MDL_SHARED;
  mdl->state= MDL_PENDING;
  mdl->prio= MDL_NORMAL_PRIO;
  mdl->is_upgradable= FALSE;
#ifndef DBUG_OFF
  mdl->ctx= 0;
  mdl->lock_data= 0;
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

   @note The allocated lock request will have MDL_SHARED type and
         normal priority.

   @retval 0      Error
   @retval non-0  Pointer to an object representing a lock request
*/

MDL_LOCK *mdl_alloc_lock(int type, const char *db, const char *name,
                       MEM_ROOT *root)
{
  MDL_LOCK *lock;
  char *key;

  if (!multi_alloc_root(root, &lock, sizeof(MDL_LOCK), &key,
                        MAX_DBKEY_LENGTH, NULL))
    return NULL;

  mdl_init_lock(lock, key, type, db, name);

  return lock;
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
   @param  lock       The lock request to be added.
*/

void mdl_add_lock(MDL_CONTEXT *context, MDL_LOCK *lock)
{
  DBUG_ENTER("mdl_add_lock");
  DBUG_ASSERT(lock->state == MDL_PENDING);
  DBUG_ASSERT(!lock->ctx);
  lock->ctx= context;
  context->locks.push_front(lock);
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

   Also resets lock requests back to their initial state (i.e.
   sets type and priority to MDL_SHARED and MDL_NORMAL_PRIO).

   @param context Context to be cleared.
*/

void mdl_remove_all_locks(MDL_CONTEXT *context)
{
  MDL_LOCK *l;
  I_P_List_iterator<MDL_LOCK, MDL_LOCK_context> it(context->locks);
  while ((l= it++))
  {
    /* Reset lock request back to its initial state. */
    l->type= MDL_SHARED;
    l->prio= MDL_NORMAL_PRIO;
    l->is_upgradable= FALSE;
#ifndef DBUG_OFF
    l->ctx= 0;
#endif
  }
  context->locks.empty();
}


/**
   Auxiliary functions needed for creation/destruction of MDL_LOCK_DATA
   objects.

   @todo This naive implementation should be replaced with one that saves
         on memory allocation by reusing released objects.
*/

static MDL_LOCK_DATA* get_lock_data_object(void)
{
  return new MDL_LOCK_DATA();
}


static void release_lock_data_object(MDL_LOCK_DATA *lock)
{
  delete lock;
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

   @param lock  [in]  Lock request object for lock to be acquired
   @param retry [out] Indicates that conflicting lock exists and another
                      attempt should be made after releasing all current
                      locks and waiting for conflicting lock go away
                      (using mdl_wait_for_locks()).

   @retval  FALSE   Success.
   @retval  TRUE    Failure. Either error occured or conflicting lock exists.
                    In the latter case "retry" parameter is set to TRUE.
*/

bool mdl_acquire_shared_lock(MDL_LOCK *l, bool *retry)
{
  MDL_LOCK_DATA *lock_data;
  *retry= FALSE;

  DBUG_ASSERT(l->type == MDL_SHARED && l->state == MDL_PENDING);

  safe_mutex_assert_not_owner(&LOCK_open);

  if (l->ctx->has_global_shared_lock && l->is_upgradable)
  {
    my_error(ER_CANT_UPDATE_WITH_READLOCK, MYF(0));
    return TRUE;
  }

  pthread_mutex_lock(&LOCK_mdl);

  if (l->is_upgradable &&
      (global_lock.shared_acquired || global_lock.shared_pending))
  {
    pthread_mutex_unlock(&LOCK_mdl);
    *retry= TRUE;
    return TRUE;
  }

  if (!(lock_data= (MDL_LOCK_DATA *)my_hash_search(&mdl_locks, (uchar*)l->key,
                                                   l->key_length)))
  {
    lock_data= get_lock_data_object();
    lock_data->active_shared.push_front(l);
    lock_data->lock_count= 1;
    my_hash_insert(&mdl_locks, (uchar*)lock_data);
    l->state= MDL_ACQUIRED;
    l->lock_data= lock_data;
    if (l->is_upgradable)
      global_lock.intention_exclusive_acquired++;
  }
  else
  {
    if ((lock_data->active_exclusive.is_empty() &&
         (l->prio == MDL_HIGH_PRIO ||
          lock_data->waiting_exclusive.is_empty() &&
          lock_data->active_shared_waiting_upgrade.is_empty())) ||
        (!lock_data->active_exclusive.is_empty() &&
         lock_data->active_exclusive.head()->ctx == l->ctx))
    {
      /*
        When exclusive lock comes from the same context we can satisfy our
        shared lock. This is required for CREATE TABLE ... SELECT ... and
        ALTER VIEW ... AS ....
      */
      lock_data->active_shared.push_front(l);
      lock_data->lock_count++;
      l->state= MDL_ACQUIRED;
      l->lock_data= lock_data;
      if (l->is_upgradable)
        global_lock.intention_exclusive_acquired++;
    }
    else
      *retry= TRUE;
  }
  pthread_mutex_unlock(&LOCK_mdl);

  return *retry;
}


static void release_lock(MDL_LOCK *l);


/**
   Acquire exclusive locks. The context must contain the list of
   locks to be acquired. There must be no granted locks in the
   context.

   This is a replacement of lock_table_names(). It is used in
   RENAME, DROP and other DDL SQL statements.

   @param context  A context containing requests for exclusive locks
                   The context may not have other lock requests.

   @note In case of failure (for example, if our thread was killed)
         resets lock requests back to their initial state (MDL_SHARED
         and MDL_NORMAL_PRIO).

   @retval FALSE  Success
   @retval TRUE   Failure
*/

bool mdl_acquire_exclusive_locks(MDL_CONTEXT *context)
{
  MDL_LOCK *l, *lh;
  MDL_LOCK_DATA *lock_data;
  bool signalled= FALSE;
  const char *old_msg;
  I_P_List_iterator<MDL_LOCK, MDL_LOCK_context> it(context->locks);
  THD *thd= context->thd;

  DBUG_ASSERT(thd == current_thd);

  safe_mutex_assert_not_owner(&LOCK_open);

  if (context->has_global_shared_lock)
  {
    my_error(ER_CANT_UPDATE_WITH_READLOCK, MYF(0));
    return TRUE;
  }

  pthread_mutex_lock(&LOCK_mdl);

  old_msg= thd->enter_cond(&COND_mdl, &LOCK_mdl, "Waiting for table");

  while ((l= it++))
  {
    DBUG_ASSERT(l->type == MDL_EXCLUSIVE && l->state == MDL_PENDING);
    if (!(lock_data= (MDL_LOCK_DATA *)my_hash_search(&mdl_locks, (uchar*)l->key,
                                                     l->key_length)))
    {
      lock_data= get_lock_data_object();
      lock_data->waiting_exclusive.push_front(l);
      lock_data->lock_count= 1;
      my_hash_insert(&mdl_locks, (uchar*)lock_data);
      l->lock_data= lock_data;
    }
    else
    {
      lock_data->waiting_exclusive.push_front(l);
      lock_data->lock_count++;
      l->lock_data= lock_data;
    }
  }

  while (1)
  {
    it.rewind();
    while ((l= it++))
    {
      lock_data= l->lock_data;

      if (global_lock.shared_acquired || global_lock.shared_pending)
      {
        /*
          There is active or pending global shared lock we have
          to wait until it goes away.
        */
        signalled= TRUE;
        break;
      }
      else if (!lock_data->active_exclusive.is_empty() ||
               !lock_data->active_shared_waiting_upgrade.is_empty())
      {
        /*
          Exclusive MDL owner won't wait on table-level lock the same
          applies to shared lock waiting upgrade (in this cases we already
          have some table-level lock).
        */
        signalled= TRUE;
        break;
      }
      else if ((lh= lock_data->active_shared.head()))
      {
        signalled= notify_thread_having_shared_lock(thd, lh->ctx->thd);
        break;
      }
    }
    if (!l)
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
    if (thd->killed)
    {
      /* Remove our pending lock requests from the locks. */
      it.rewind();
      while ((l= it++))
      {
        DBUG_ASSERT(l->type == MDL_EXCLUSIVE && l->state == MDL_PENDING);
        release_lock(l);
        /* Return lock request to its initial state. */
        l->type= MDL_SHARED;
        l->prio= MDL_NORMAL_PRIO;
        l->is_upgradable= FALSE;
        context->locks.remove(l);
      }
      /* Pending requests for shared locks can be satisfied now. */
      pthread_cond_broadcast(&COND_mdl);
      thd->exit_cond(old_msg);
      return TRUE;
    }
  }
  it.rewind();
  while ((l= it++))
  {
    global_lock.intention_exclusive_acquired++;
    lock_data= l->lock_data;
    lock_data->waiting_exclusive.remove(l);
    lock_data->active_exclusive.push_front(l);
    l->state= MDL_ACQUIRED;
    if (lock_data->cached_object)
      (*lock_data->cached_object_release_hook)(lock_data->cached_object);
    lock_data->cached_object= NULL;
  }
  /* As a side-effect THD::exit_cond() unlocks LOCK_mdl. */
  thd->exit_cond(old_msg);
  return FALSE; 
}


/**
   Upgrade a shared metadata lock to exclusive.

   Used in ALTER TABLE, when a copy of the table with the
   new definition has been constructed.

   @param context Context to which shared long belongs
   @param type    Id of object type
   @param db      Name of the database
   @param name    Name of the object

   @note In case of failure to upgrade locks (e.g. because upgrader
         was killed) leaves locks in their original state (locked
         in shared mode).

   @retval FALSE  Success
   @retval TRUE   Failure (thread was killed)
*/

bool mdl_upgrade_shared_lock_to_exclusive(MDL_CONTEXT *context, int type,
                                          const char *db, const char *name)
{
  char key[MAX_DBKEY_LENGTH];
  uint key_length;
  bool signalled= FALSE;
  MDL_LOCK *l, *lh;
  MDL_LOCK_DATA *lock_data;
  I_P_List_iterator<MDL_LOCK, MDL_LOCK_context> it(context->locks);
  const char *old_msg;
  THD *thd= context->thd;

  DBUG_ENTER("mdl_upgrade_shared_lock_to_exclusive");
  DBUG_PRINT("enter", ("db=%s name=%s", db, name));

  DBUG_ASSERT(thd == current_thd);

  int4store(key, type);
  key_length= (uint) (strmov(strmov(key+4, db)+1, name)-key)+1;

  safe_mutex_assert_not_owner(&LOCK_open);

  pthread_mutex_lock(&LOCK_mdl);

  old_msg= thd->enter_cond(&COND_mdl, &LOCK_mdl, "Waiting for table");

  while ((l= it++))
    if (l->key_length == key_length && !memcmp(l->key, key, key_length) &&
        l->type == MDL_SHARED)
    {
      DBUG_PRINT("info", ("found shared lock for upgrade"));
      DBUG_ASSERT(l->state == MDL_ACQUIRED);
      DBUG_ASSERT(l->is_upgradable);
      l->state= MDL_PENDING_UPGRADE;
      lock_data= l->lock_data;
      lock_data->active_shared.remove(l);
      lock_data->active_shared_waiting_upgrade.push_front(l);
    }

  while (1)
  {
    DBUG_PRINT("info", ("looking at conflicting locks"));
    it.rewind();
    while ((l= it++))
    {
      if (l->state == MDL_PENDING_UPGRADE)
      {
        DBUG_ASSERT(l->type == MDL_SHARED);

        lock_data= l->lock_data;

        DBUG_ASSERT(global_lock.shared_acquired == 0 &&
                    global_lock.intention_exclusive_acquired);

        if ((lh= lock_data->active_shared.head()))
        {
          DBUG_PRINT("info", ("found active shared locks"));
          signalled= notify_thread_having_shared_lock(thd, lh->ctx->thd);
          break;
        }
        else if (!lock_data->active_exclusive.is_empty())
        {
          DBUG_PRINT("info", ("found active exclusive locks"));
          signalled= TRUE;
          break;
        }
      }
    }
    if (!l)
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
      DBUG_PRINT("info", ("Failed to wake-up from table-level lock ... sleeping"));
      pthread_cond_timedwait(&COND_mdl, &LOCK_mdl, &abstime);
    }
    if (thd->killed)
    {
      it.rewind();
      while ((l= it++))
        if (l->state == MDL_PENDING_UPGRADE)
        {
          DBUG_ASSERT(l->type == MDL_SHARED);
          l->state= MDL_ACQUIRED;
          lock_data= l->lock_data;
          lock_data->active_shared_waiting_upgrade.remove(l);
          lock_data->active_shared.push_front(l);
        }
      /* Pending requests for shared locks can be satisfied now. */
      pthread_cond_broadcast(&COND_mdl);
      thd->exit_cond(old_msg);
      DBUG_RETURN(TRUE);
    }
  }

  it.rewind();
  while ((l= it++))
    if (l->state == MDL_PENDING_UPGRADE)
    {
      DBUG_ASSERT(l->type == MDL_SHARED);
      lock_data= l->lock_data;
      lock_data->active_shared_waiting_upgrade.remove(l);
      lock_data->active_exclusive.push_front(l);
      l->type= MDL_EXCLUSIVE;
      l->state= MDL_ACQUIRED;
      if (lock_data->cached_object)
        (*lock_data->cached_object_release_hook)(lock_data->cached_object);
      lock_data->cached_object= 0;
    }

  /* As a side-effect THD::exit_cond() unlocks LOCK_mdl. */
  thd->exit_cond(old_msg);
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

   @retval FALSE the lock was granted
   @retval TRUE  there were conflicting locks.

   FIXME: Compared to lock_table_name_if_not_cached()
          it gives sligthly more false negatives.
*/

bool mdl_try_acquire_exclusive_lock(MDL_CONTEXT *context, MDL_LOCK *l)
{
  MDL_LOCK_DATA *lock_data;

  DBUG_ASSERT(l->type == MDL_EXCLUSIVE && l->state == MDL_PENDING);

  safe_mutex_assert_not_owner(&LOCK_open);

  pthread_mutex_lock(&LOCK_mdl);

  if (!(lock_data= (MDL_LOCK_DATA *)my_hash_search(&mdl_locks, (uchar*)l->key,
                                                   l->key_length)))
  {
    lock_data= get_lock_data_object();
    lock_data->active_exclusive.push_front(l);
    lock_data->lock_count= 1;
    my_hash_insert(&mdl_locks, (uchar*)lock_data);
    l->state= MDL_ACQUIRED;
    l->lock_data= lock_data;
    lock_data= 0;
    global_lock.intention_exclusive_acquired++;
  }
  pthread_mutex_unlock(&LOCK_mdl);

  /*
    FIXME: We can't leave pending MDL_EXCLUSIVE lock request in the list since
           for such locks we assume that they have MDL_LOCK::lock properly set.
           Long term we should clearly define relation between lock types,
           presence in the context lists and MDL_LOCK::lock values.
  */
  if (lock_data)
    context->locks.remove(l);

  return lock_data;
}


/**
   Acquire global shared metadata lock.

   Holding this lock will block all requests for exclusive locks
   and shared locks which can be potentially upgraded to exclusive
   (see MDL_LOCK::is_upgradable).

   @param context Current metadata locking context.

   @retval FALSE Success -- the lock was granted.
   @retval TRUE  Failure -- our thread was killed.
*/

bool mdl_acquire_global_shared_lock(MDL_CONTEXT *context)
{
  THD *thd= context->thd;
  const char *old_msg;

  safe_mutex_assert_not_owner(&LOCK_open);
  DBUG_ASSERT(thd == current_thd);
  DBUG_ASSERT(!context->has_global_shared_lock);

  pthread_mutex_lock(&LOCK_mdl);

  global_lock.shared_pending++;
  old_msg= thd->enter_cond(&COND_mdl, &LOCK_mdl, "Waiting for table");

  while (!thd->killed && global_lock.intention_exclusive_acquired)
    pthread_cond_wait(&COND_mdl, &LOCK_mdl);

  global_lock.shared_pending--;
  if (thd->killed)
  {
    /* As a side-effect THD::exit_cond() unlocks LOCK_mdl. */
    thd->exit_cond(old_msg);
    return TRUE;
  }
  global_lock.shared_acquired++;
  context->has_global_shared_lock= TRUE;
  /* As a side-effect THD::exit_cond() unlocks LOCK_mdl. */
  thd->exit_cond(old_msg);
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
  MDL_LOCK *l;
  MDL_LOCK_DATA *lock_data;
  I_P_List_iterator<MDL_LOCK, MDL_LOCK_context> it(context->locks);
  const char *old_msg;
  THD *thd= context->thd;

  safe_mutex_assert_not_owner(&LOCK_open);
  DBUG_ASSERT(thd == current_thd);

  while (!thd->killed)
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
    old_msg= thd->enter_cond(&COND_mdl, &LOCK_mdl, "Waiting for table");
    it.rewind();
    while ((l= it++))
    {
      DBUG_ASSERT(l->state == MDL_PENDING);
      if ((l->is_upgradable || l->type == MDL_EXCLUSIVE) &&
          (global_lock.shared_acquired || global_lock.shared_pending))
        break;
      /*
        To avoid starvation we don't wait if we have pending MDL_EXCLUSIVE lock.
      */
      if (l->type == MDL_SHARED &&
          (lock_data= (MDL_LOCK_DATA *)my_hash_search(&mdl_locks, (uchar*)l->key,
                                                      l->key_length)) &&
          !(lock_data->active_exclusive.is_empty() &&
            lock_data->active_shared_waiting_upgrade.is_empty() &&
            lock_data->waiting_exclusive.is_empty()))
        break;
    }
    if (!l)
    {
      pthread_mutex_unlock(&LOCK_mdl);
      break;
    }
    pthread_cond_wait(&COND_mdl, &LOCK_mdl);
    /* As a side-effect THD::exit_cond() unlocks LOCK_mdl. */
    thd->exit_cond(old_msg);
  }
  return thd->killed;
}


/**
   Auxiliary function which allows to release particular lock
   ownership of which is represented by lock request object.
*/

static void release_lock(MDL_LOCK *l)
{
  MDL_LOCK_DATA *lock_data;

  DBUG_ENTER("release_lock");
  DBUG_PRINT("enter", ("db=%s name=%s", l->key + 4,
                        l->key + 4 + strlen(l->key + 4) + 1));

  lock_data= l->lock_data;
  if (lock_data->has_one_lock())
  {
    my_hash_delete(&mdl_locks, (uchar *)lock_data);
    DBUG_PRINT("info", ("releasing cached_object cached_object=%p",
                        lock_data->cached_object));
    if (lock_data->cached_object)
      (*lock_data->cached_object_release_hook)(lock_data->cached_object);
    release_lock_data_object(lock_data);
    if (l->type == MDL_EXCLUSIVE && l->state == MDL_ACQUIRED ||
        l->type == MDL_SHARED && l->state == MDL_ACQUIRED && l->is_upgradable)
      global_lock.intention_exclusive_acquired--;
  }
  else
  {
    switch (l->type)
    {
      case MDL_SHARED:
        lock_data->active_shared.remove(l);
        if (l->is_upgradable)
          global_lock.intention_exclusive_acquired--;
        break;
      case MDL_EXCLUSIVE:
        if (l->state == MDL_PENDING)
          lock_data->waiting_exclusive.remove(l);
        else
        {
          lock_data->active_exclusive.remove(l);
          global_lock.intention_exclusive_acquired--;
        }
        break;
      default:
        /* TODO Really? How about problems during lock upgrade ? */
        DBUG_ASSERT(0);
    }
    lock_data->lock_count--;
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
  MDL_LOCK *l;
  I_P_List_iterator<MDL_LOCK, MDL_LOCK_context> it(context->locks);
  DBUG_ENTER("mdl_release_locks");

  safe_mutex_assert_not_owner(&LOCK_open);

  pthread_mutex_lock(&LOCK_mdl);
  while ((l= it++))
  {
    DBUG_PRINT("info", ("found lock to release l=%p", l));
    /*
      We should not release locks which pending shared locks as these
      are not associated with lock object and don't present in its
      lists. Allows us to avoid problems in open_tables() in case of
      back-off
    */
    if (!(l->type == MDL_SHARED && l->state == MDL_PENDING))
    {
      release_lock(l);
      l->state= MDL_PENDING;
#ifndef DBUG_OFF
      l->lock_data= 0;
#endif
    }
    /*
      We will return lock request to its initial state only in
      mdl_remove_all_locks() since we need to know type of lock
      request and if it is upgradable in mdl_wait_for_locks().
    */
  }
  /* Inefficient but will do for a while */
  pthread_cond_broadcast(&COND_mdl);
  pthread_mutex_unlock(&LOCK_mdl);
  DBUG_VOID_RETURN;
}


/**
   Release all exclusive locks associated with context.
   Removes the locks from the context.

   @param context Context with exclusive locks.

   @note Shared locks are left intact.
   @note Resets lock requests for locks released back to their
         initial state (i.e.sets type and priority to MDL_SHARED
         and MDL_NORMAL_PRIO).
*/

void mdl_release_exclusive_locks(MDL_CONTEXT *context)
{
  MDL_LOCK *l;
  I_P_List_iterator<MDL_LOCK, MDL_LOCK_context> it(context->locks);

  safe_mutex_assert_not_owner(&LOCK_open);

  pthread_mutex_lock(&LOCK_mdl);
  while ((l= it++))
  {
    if (l->type == MDL_EXCLUSIVE)
    {
      DBUG_ASSERT(l->state == MDL_ACQUIRED);
      release_lock(l);
#ifndef DBUG_OFF
      l->ctx= 0;
      l->lock_data= 0;
#endif
      l->state= MDL_PENDING;
      /* Return lock request to its initial state. */
      l->type= MDL_SHARED;
      l->prio= MDL_NORMAL_PRIO;
      l->is_upgradable= FALSE;
      context->locks.remove(l);
    }
  }
  pthread_cond_broadcast(&COND_mdl);
  pthread_mutex_unlock(&LOCK_mdl);
}


/**
   Release a lock.
   Removes the lock from the context.

   @param context Context containing lock in question
   @param lock    Lock to be released

   @note Resets lock request for lock released back to its initial state
         (i.e.sets type and priority to MDL_SHARED and MDL_NORMAL_PRIO).
*/

void mdl_release_lock(MDL_CONTEXT *context, MDL_LOCK *lr)
{
  safe_mutex_assert_not_owner(&LOCK_open);

  pthread_mutex_lock(&LOCK_mdl);
  release_lock(lr);
#ifndef DBUG_OFF
  lr->ctx= 0;
  lr->lock_data= 0;
#endif
  lr->state= MDL_PENDING;
  /* Return lock request to its initial state. */
  lr->type= MDL_SHARED;
  lr->prio= MDL_NORMAL_PRIO;
  lr->is_upgradable= FALSE;
  context->locks.remove(lr);
  pthread_cond_broadcast(&COND_mdl);
  pthread_mutex_unlock(&LOCK_mdl);
}


/**
   Downgrade all exclusive locks in the context to
   shared.

   @param context A context with exclusive locks.
*/

void mdl_downgrade_exclusive_locks(MDL_CONTEXT *context)
{
  MDL_LOCK *l;
  MDL_LOCK_DATA *lock_data;
  I_P_List_iterator<MDL_LOCK, MDL_LOCK_context> it(context->locks);

  safe_mutex_assert_not_owner(&LOCK_open);

  pthread_mutex_lock(&LOCK_mdl);
  while ((l= it++))
    if (l->type == MDL_EXCLUSIVE)
    {
      DBUG_ASSERT(l->state == MDL_ACQUIRED);
      if (!l->is_upgradable)
        global_lock.intention_exclusive_acquired--;
      lock_data= l->lock_data;
      lock_data->active_exclusive.remove(l);
      l->type= MDL_SHARED;
      lock_data->active_shared.push_front(l);
    }
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
  global_lock.shared_acquired--;
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
  char key[MAX_DBKEY_LENGTH];
  uint key_length;
  MDL_LOCK *l;
  I_P_List_iterator<MDL_LOCK, MDL_LOCK_context> it(context->locks);

  int4store(key, type);
  key_length= (uint) (strmov(strmov(key+4, db)+1, name)-key)+1;

  while ((l= it++) && (l->key_length != key_length || memcmp(l->key, key, key_length)))
    continue;
  return (l && l->type == MDL_EXCLUSIVE && l->state == MDL_ACQUIRED);
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
  char key[MAX_DBKEY_LENGTH];
  uint key_length;
  MDL_LOCK *l;
  I_P_List_iterator<MDL_LOCK, MDL_LOCK_context> it(context->locks);

  int4store(key, type);
  key_length= (uint) (strmov(strmov(key+4, db)+1, name)-key)+1;

  while ((l= it++) && (l->key_length != key_length ||
                       memcmp(l->key, key, key_length) ||
                       l->state == MDL_PENDING))
    continue;

  return l;
}


/**
   Check if we have any pending exclusive locks which conflict with
   existing shared lock.

   @param l Shared lock against which check should be performed.

   @return TRUE if there are any conflicting locks, FALSE otherwise.
*/

bool mdl_has_pending_conflicting_lock(MDL_LOCK *l)
{
  bool result;

  DBUG_ASSERT(l->type == MDL_SHARED && l->state == MDL_ACQUIRED);
  safe_mutex_assert_not_owner(&LOCK_open);

  pthread_mutex_lock(&LOCK_mdl);
  result= !(l->lock_data->waiting_exclusive.is_empty() &&
            l->lock_data->active_shared_waiting_upgrade.is_empty());
  pthread_mutex_unlock(&LOCK_mdl);
  return result;
}


/**
   Associate pointer to an opaque object with a lock.

   @param l             Lock request for the lock with which the
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

void mdl_set_cached_object(MDL_LOCK *l, void *cached_object,
                           mdl_cached_object_release_hook release_hook)
{
  DBUG_ENTER("mdl_set_cached_object");
  DBUG_PRINT("enter", ("db=%s name=%s cached_object=%p", l->key + 4,
                       l->key + 4 + strlen(l->key + 4) + 1,
                       cached_object));

  DBUG_ASSERT(l->state == MDL_ACQUIRED || l->state == MDL_PENDING_UPGRADE);

  /*
    TODO: This assumption works now since we do mdl_get_cached_object()
          and mdl_set_cached_object() in the same critical section. Once
          this becomes false we will have to call release_hook here and
          use additional mutex protecting 'cached_object' member.
  */
  DBUG_ASSERT(!l->lock_data->cached_object);

  l->lock_data->cached_object= cached_object;
  l->lock_data->cached_object_release_hook= release_hook;

  DBUG_VOID_RETURN;
}


/**
   Get a pointer to an opaque object that associated with the lock.

   @param  l Lock request for the lock with which the object is
             associated.

   @return Pointer to an opaque object associated with the lock.
*/

void* mdl_get_cached_object(MDL_LOCK *l)
{
  DBUG_ASSERT(l->state == MDL_ACQUIRED || l->state == MDL_PENDING_UPGRADE);
  return l->lock_data->cached_object;
}

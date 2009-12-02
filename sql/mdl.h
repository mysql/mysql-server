#ifndef MDL_H
#define MDL_H
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


#include "sql_plist.h"
#include <my_sys.h>
#include <m_string.h>
#include <mysql_com.h>

class THD;

struct MDL_LOCK_DATA;
struct MDL_LOCK;
struct MDL_CONTEXT;

/**
   Type of metadata lock request.

   - High-priority shared locks differ from ordinary shared locks by
     that they ignore pending requests for exclusive locks.
   - Upgradable shared locks can be later upgraded to exclusive
     (because of that their acquisition involves implicit
      acquisition of global intention-exclusive lock).

   @see Comments for can_grant_lock() and can_grant_global_lock() for details.
*/

enum enum_mdl_type {MDL_SHARED=0, MDL_SHARED_HIGH_PRIO,
                    MDL_SHARED_UPGRADABLE, MDL_EXCLUSIVE};


/** States which metadata lock request can have. */

enum enum_mdl_state {MDL_INITIALIZED=0, MDL_PENDING,
                     MDL_ACQUIRED, MDL_PENDING_UPGRADE};


/**
   A pending lock request or a granted metadata lock. A lock is requested
   or granted based on a fully qualified name and type. E.g. for a table
   the key consists of <0 (=table)>+<database name>+<table name>.
   Later in this document this triple will be referred to simply as
   "key" or "name".
*/

struct MDL_LOCK_DATA
{
  char          *key;
  uint          key_length;
  enum          enum_mdl_type type;
  enum          enum_mdl_state state;

private:
  /**
     Pointers for participating in the list of lock requests for this context.
  */
  MDL_LOCK_DATA *next_context;
  MDL_LOCK_DATA **prev_context;
  /**
     Pointers for participating in the list of satisfied/pending requests
     for the lock.
  */
  MDL_LOCK_DATA *next_lock;
  MDL_LOCK_DATA **prev_lock;

  friend struct MDL_LOCK_DATA_context;
  friend struct MDL_LOCK_DATA_lock;

public:
  /*
    Pointer to the lock object for this lock request. Valid only if this lock
    request is satisified or is present in the list of pending lock requests
    for particular lock.
  */
  MDL_LOCK      *lock;
  MDL_CONTEXT   *ctx;
};


/**
   Helper class which specifies which members of MDL_LOCK_DATA are used for
   participation in the list lock requests belonging to one context.
*/

struct MDL_LOCK_DATA_context
{
  static inline MDL_LOCK_DATA **next_ptr(MDL_LOCK_DATA *l)
  {
    return &l->next_context;
  }
  static inline MDL_LOCK_DATA ***prev_ptr(MDL_LOCK_DATA *l)
  {
    return &l->prev_context;
  }
};


/**
   Helper class which specifies which members of MDL_LOCK_DATA are used for
   participation in the list of satisfied/pending requests for the lock.
*/

struct MDL_LOCK_DATA_lock
{
  static inline MDL_LOCK_DATA **next_ptr(MDL_LOCK_DATA *l)
  {
    return &l->next_lock;
  }
  static inline MDL_LOCK_DATA ***prev_ptr(MDL_LOCK_DATA *l)
  {
    return &l->prev_lock;
  }
};


/**
   Context of the owner of metadata locks. I.e. each server
   connection has such a context.
*/

struct MDL_CONTEXT
{
  I_P_List <MDL_LOCK_DATA, MDL_LOCK_DATA_context> locks;
  bool has_global_shared_lock;
  THD      *thd;
};


void mdl_init();
void mdl_destroy();

void mdl_context_init(MDL_CONTEXT *context, THD *thd);
void mdl_context_destroy(MDL_CONTEXT *context);
void mdl_context_backup_and_reset(MDL_CONTEXT *ctx, MDL_CONTEXT *backup);
void mdl_context_restore(MDL_CONTEXT *ctx, MDL_CONTEXT *backup);
void mdl_context_merge(MDL_CONTEXT *target, MDL_CONTEXT *source);

/** Maximal length of key for metadata locking subsystem. */
#define MAX_MDLKEY_LENGTH (4 + NAME_LEN + 1 + NAME_LEN + 1)

void mdl_init_lock(MDL_LOCK_DATA *lock_data, char *key, int type,
                   const char *db, const char *name);
MDL_LOCK_DATA *mdl_alloc_lock(int type, const char *db, const char *name,
                              MEM_ROOT *root);
void mdl_add_lock(MDL_CONTEXT *context, MDL_LOCK_DATA *lock_data);
void mdl_remove_lock(MDL_CONTEXT *context, MDL_LOCK_DATA *lock_data);
void mdl_remove_all_locks(MDL_CONTEXT *context);

/**
   Set type of lock request. Can be only applied to pending locks.
*/

inline void mdl_set_lock_type(MDL_LOCK_DATA *lock_data, enum_mdl_type lock_type)
{
  DBUG_ASSERT(lock_data->state == MDL_INITIALIZED);
  lock_data->type= lock_type;
}

bool mdl_acquire_shared_lock(MDL_CONTEXT *context, MDL_LOCK_DATA *lock_data,
                             bool *retry);
bool mdl_acquire_exclusive_locks(MDL_CONTEXT *context);
bool mdl_upgrade_shared_lock_to_exclusive(MDL_CONTEXT *context,
                                          MDL_LOCK_DATA *lock_data);
bool mdl_try_acquire_exclusive_lock(MDL_CONTEXT *context,
                                    MDL_LOCK_DATA *lock_data,
                                    bool *conflict);
bool mdl_acquire_global_shared_lock(MDL_CONTEXT *context);

bool mdl_wait_for_locks(MDL_CONTEXT *context);

void mdl_release_locks(MDL_CONTEXT *context);
void mdl_release_and_remove_all_locks_for_name(MDL_CONTEXT *context,
                                               MDL_LOCK_DATA *lock_data);
void mdl_release_lock(MDL_CONTEXT *context, MDL_LOCK_DATA *lock_data);
void mdl_downgrade_exclusive_lock(MDL_CONTEXT *context,
                                  MDL_LOCK_DATA *lock_data);
void mdl_release_global_shared_lock(MDL_CONTEXT *context);

bool mdl_is_exclusive_lock_owner(MDL_CONTEXT *context, int type, const char *db,
                                 const char *name);
bool mdl_is_lock_owner(MDL_CONTEXT *context, int type, const char *db,
                       const char *name);

bool mdl_has_pending_conflicting_lock(MDL_LOCK_DATA *lock_data);

inline bool mdl_has_locks(MDL_CONTEXT *context)
{
  return !context->locks.is_empty();
}


/**
   Get iterator for walking through all lock requests in the context.
*/

inline I_P_List_iterator<MDL_LOCK_DATA, MDL_LOCK_DATA_context>
mdl_get_locks(MDL_CONTEXT *ctx)
{
  I_P_List_iterator<MDL_LOCK_DATA, MDL_LOCK_DATA_context> result(ctx->locks);
  return result;
}

/**
   Give metadata lock request object for the table get table definition
   cache key corresponding to it.

   @param lock_data  [in]  Lock request object for the table.
   @param key        [out] LEX_STRING object where table definition cache key
                           should be put.

   @note This key will have the same life-time as this lock request object.

   @note This is yet another place where border between MDL subsystem and the
         rest of the server is broken. OTOH it allows to save some CPU cycles
         and memory by avoiding generating these TDC keys from table list.
*/

inline void mdl_get_tdc_key(MDL_LOCK_DATA *lock_data, LEX_STRING *key)
{
  key->str= lock_data->key + 4;
  key->length= lock_data->key_length - 4;
}


typedef void (* mdl_cached_object_release_hook)(void *);
void* mdl_get_cached_object(MDL_LOCK_DATA *lock_data);
void mdl_set_cached_object(MDL_LOCK_DATA *lock_data, void *cached_object,
                           mdl_cached_object_release_hook release_hook);


/*
  Functions in the server's kernel used by metadata locking subsystem.
*/

extern bool mysql_notify_thread_having_shared_lock(THD *thd, THD *in_use);
extern void mysql_ha_flush(THD *thd);
extern "C" const char *set_thd_proc_info(THD *thd, const char *info,
                                         const char *calling_function,
                                         const char *calling_file,
                                         const unsigned int calling_line);
#ifndef DBUG_OFF
extern pthread_mutex_t LOCK_open;
#endif

#endif

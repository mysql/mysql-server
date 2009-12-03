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

struct MDL_LOCK_REQUEST;
struct MDL_LOCK_TICKET;
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


/** States which a metadata lock ticket can have. */

enum enum_mdl_state { MDL_PENDING, MDL_ACQUIRED };


/** Maximal length of key for metadata locking subsystem. */
#define MAX_MDLKEY_LENGTH (1 + NAME_LEN + 1 + NAME_LEN + 1)


/**
   Metadata lock object key.

   A lock is requested or granted based on a fully qualified name and type.
   E.g. They key for a table consists of <0 (=table)>+<database>+<table name>.
   Elsewhere in the comments this triple will be referred to simply as "key"
   or "name".
*/

class MDL_KEY
{
public:
  const uchar *ptr() const { return (uchar*) m_ptr; }
  uint length() const { return m_length; }

  const char *db_name() const { return m_ptr + 1; }
  uint db_name_length() const { return m_db_name_length; }

  const char *table_name() const { return m_ptr + m_db_name_length + 2; }
  uint table_name_length() const { return m_length - m_db_name_length - 3; }

  /**
    Construct a metadata lock key from a triplet (type, database and name).

    @remark The key for a table is <0 (=table)>+<database name>+<table name>

    @param  type   Id of type of object to be locked
    @param  db     Name of database to which the object belongs
    @param  name   Name of of the object
    @param  key    Where to store the the MDL key.
  */
  void mdl_key_init(char type, const char *db, const char *name)
  {
    m_ptr[0]= type;
    m_db_name_length= (uint) (strmov(m_ptr + 1, db) - m_ptr - 1);
    m_length= (uint) (strmov(m_ptr + m_db_name_length + 2, name) - m_ptr + 1);
  }
  void mdl_key_init(const MDL_KEY *rhs)
  {
    memcpy(m_ptr, rhs->m_ptr, rhs->m_length);
    m_length= rhs->m_length;
    m_db_name_length= rhs->m_db_name_length;
  }
  bool is_equal(const MDL_KEY *rhs) const
  {
    return (m_length == rhs->m_length &&
            memcmp(m_ptr, rhs->m_ptr, m_length) == 0);
  }
private:
  char m_ptr[MAX_MDLKEY_LENGTH];
  uint m_length;
  uint m_db_name_length;
};



/**
   Hook class which via its methods specifies which members
   of T should be used for participating in MDL lists.
*/

template <typename T, T* T::*next, T** T::*prev>
struct I_P_List_adapter
{
  static inline T **next_ptr(T *el) { return &(el->*next); }

  static inline T ***prev_ptr(T *el) { return &(el->*prev); }
};


/**
   A pending metadata lock request.
   A pending lock request or a granted metadata lock share the same abstract
   base but are presented individually because they have different allocation
   sites and hence different lifetimes. The allocation of lock requests is
   controlled from outside of the MDL subsystem, while allocation of granted
   locks (tickets) is controlled within the MDL subsystem.
*/

struct MDL_LOCK_REQUEST
{
  /** Type of metadata lock. */
  enum          enum_mdl_type type;

  /**
     Pointers for participating in the list of lock requests for this context.
  */
  MDL_LOCK_REQUEST *next_in_context;
  MDL_LOCK_REQUEST **prev_in_context;
  /** A lock is requested based on a fully qualified name and type. */
  MDL_KEY key;

  /**
    Pointer to the lock ticket object for this lock request.
    Valid only if this lock request is satisfied.
  */
  MDL_LOCK_TICKET *ticket;
};


/**
   A granted metadata lock.

   @warning MDL_LOCK_TICKET members are private to the MDL subsystem.

   @note Multiple shared locks on a same object are represented by a
         single ticket. The same does not apply for other lock types.
*/

struct MDL_LOCK_TICKET
{
  /** Type of metadata lock. */
  enum          enum_mdl_type type;
  /** State of the metadata lock ticket. */
  enum enum_mdl_state state;

  /**
     Pointers for participating in the list of lock requests for this context.
  */
  MDL_LOCK_TICKET *next_in_context;
  MDL_LOCK_TICKET **prev_in_context;
  /**
     Pointers for participating in the list of satisfied/pending requests
     for the lock.
  */
  MDL_LOCK_TICKET *next_in_lock;
  MDL_LOCK_TICKET **prev_in_lock;
  /** Context of the owner of the metadata lock ticket. */
  MDL_CONTEXT *ctx;

  /** Pointer to the lock object for this lock ticket. */
  MDL_LOCK *lock;
};


/**
   Context of the owner of metadata locks. I.e. each server
   connection has such a context.
*/

struct MDL_CONTEXT
{
  typedef I_P_List<MDL_LOCK_REQUEST,
                   I_P_List_adapter<MDL_LOCK_REQUEST,
                                    &MDL_LOCK_REQUEST::next_in_context,
                                    &MDL_LOCK_REQUEST::prev_in_context> >
          Request_list;

  typedef Request_list::Iterator Request_iterator;

  typedef I_P_List<MDL_LOCK_TICKET,
                   I_P_List_adapter<MDL_LOCK_TICKET,
                                    &MDL_LOCK_TICKET::next_in_context,
                                    &MDL_LOCK_TICKET::prev_in_context> >
          Ticket_list;

  typedef Ticket_list::Iterator Ticket_iterator;

  Request_list requests;
  Ticket_list tickets;
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

void mdl_request_init(MDL_LOCK_REQUEST *lock_req, unsigned char type,
                      const char *db, const char *name);
MDL_LOCK_REQUEST *mdl_request_alloc(unsigned char type, const char *db,
                                    const char *name, MEM_ROOT *root);
void mdl_request_add(MDL_CONTEXT *context, MDL_LOCK_REQUEST *lock_req);
void mdl_request_remove(MDL_CONTEXT *context, MDL_LOCK_REQUEST *lock_req);
void mdl_request_remove_all(MDL_CONTEXT *context);

/**
   Set type of lock request. Can be only applied to pending locks.
*/

inline void mdl_request_set_type(MDL_LOCK_REQUEST *lock_req, enum_mdl_type lock_type)
{
  DBUG_ASSERT(lock_req->ticket == NULL);
  lock_req->type= lock_type;
}

bool mdl_acquire_shared_lock(MDL_CONTEXT *context, MDL_LOCK_REQUEST *lock_req,
                             bool *retry);
bool mdl_acquire_exclusive_locks(MDL_CONTEXT *context);
bool mdl_upgrade_shared_lock_to_exclusive(MDL_CONTEXT *context,
                                          MDL_LOCK_TICKET *ticket);
bool mdl_try_acquire_exclusive_lock(MDL_CONTEXT *context,
                                    MDL_LOCK_REQUEST *lock_req,
                                    bool *conflict);
bool mdl_acquire_global_shared_lock(MDL_CONTEXT *context);

bool mdl_wait_for_locks(MDL_CONTEXT *context);

void mdl_ticket_release_all(MDL_CONTEXT *context);
void mdl_ticket_release_all_for_name(MDL_CONTEXT *context,
                                     MDL_LOCK_TICKET *ticket);
void mdl_ticket_release(MDL_CONTEXT *context, MDL_LOCK_TICKET *ticket);
void mdl_downgrade_exclusive_lock(MDL_CONTEXT *context,
                                  MDL_LOCK_TICKET *ticket);
void mdl_release_global_shared_lock(MDL_CONTEXT *context);

bool mdl_is_exclusive_lock_owner(MDL_CONTEXT *context, unsigned char type,
                                 const char *db, const char *name);
bool mdl_is_lock_owner(MDL_CONTEXT *context, unsigned char type,
                       const char *db, const char *name);

bool mdl_has_pending_conflicting_lock(MDL_LOCK_TICKET *ticket);

inline bool mdl_has_locks(MDL_CONTEXT *context)
{
  return !context->tickets.is_empty();
}

inline MDL_LOCK_TICKET *mdl_savepoint(MDL_CONTEXT *ctx)
{
  return ctx->tickets.head();
}

void mdl_rollback_to_savepoint(MDL_CONTEXT *ctx,
                               MDL_LOCK_TICKET *mdl_savepoint);

/**
   Get iterator for walking through all lock requests in the context.
*/

inline MDL_CONTEXT::Request_iterator
mdl_get_requests(MDL_CONTEXT *ctx)
{
  MDL_CONTEXT::Request_iterator result(ctx->requests);
  return result;
}


void mdl_get_tdc_key(MDL_LOCK_TICKET *ticket, LEX_STRING *key);
typedef void (* mdl_cached_object_release_hook)(void *);
void *mdl_get_cached_object(MDL_LOCK_TICKET *ticket);
void mdl_set_cached_object(MDL_LOCK_TICKET *ticket, void *cached_object,
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

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

class MDL_context;
class MDL_lock;
class MDL_ticket;

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

/**
  Object namespaces
 
  Different types of objects exist in different namespaces
   - MDL_TABLE is for tables and views.
   - MDL_PROCEDURE is for stored procedures, stored functions and UDFs.
*/
enum enum_mdl_namespace { MDL_TABLE=0, MDL_PROCEDURE };

/** Maximal length of key for metadata locking subsystem. */
#define MAX_MDLKEY_LENGTH (1 + NAME_LEN + 1 + NAME_LEN + 1)


/**
  Metadata lock object key.

  A lock is requested or granted based on a fully qualified name and type.
  E.g. They key for a table consists of <0 (=table)>+<database>+<table name>.
  Elsewhere in the comments this triple will be referred to simply as "key"
  or "name".
*/

class MDL_key
{
public:
  const uchar *ptr() const { return (uchar*) m_ptr; }
  uint length() const { return m_length; }

  const char *db_name() const { return m_ptr + 1; }
  uint db_name_length() const { return m_db_name_length; }

  const char *table_name() const { return m_ptr + m_db_name_length + 2; }
  uint table_name_length() const { return m_length - m_db_name_length - 3; }

  /**
    Construct a metadata lock key from a triplet (mdl_namespace, database and name).

    @remark The key for a table is <mdl_namespace>+<database name>+<table name>

    @param  mdl_namespace Id of namespace of object to be locked
    @param  db            Name of database to which the object belongs
    @param  name          Name of of the object
    @param  key           Where to store the the MDL key.
  */
  void mdl_key_init(enum_mdl_namespace mdl_namespace, const char *db, const char *name)
  {
    m_ptr[0]= (char) mdl_namespace;
    m_db_name_length= (uint) (strmov(m_ptr + 1, db) - m_ptr - 1);
    m_length= (uint) (strmov(m_ptr + m_db_name_length + 2, name) - m_ptr + 1);
  }
  void mdl_key_init(const MDL_key *rhs)
  {
    memcpy(m_ptr, rhs->m_ptr, rhs->m_length);
    m_length= rhs->m_length;
    m_db_name_length= rhs->m_db_name_length;
  }
  bool is_equal(const MDL_key *rhs) const
  {
    return (m_length == rhs->m_length &&
            memcmp(m_ptr, rhs->m_ptr, m_length) == 0);
  }
  MDL_key(const MDL_key *rhs)
  {
    mdl_key_init(rhs);
  }
  MDL_key(enum_mdl_namespace namespace_arg, const char *db_arg, const char *name_arg)
  {
    mdl_key_init(namespace_arg, db_arg, name_arg);
  }
  MDL_key() {} /* To use when part of MDL_request. */

private:
  char m_ptr[MAX_MDLKEY_LENGTH];
  uint m_length;
  uint m_db_name_length;
private:
  MDL_key(const MDL_key &);                     /* not implemented */
  MDL_key &operator=(const MDL_key &);          /* not implemented */
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

  A lock request and a granted metadata lock are represented by
  different classes because they have different allocation
  sites and hence different lifetimes. The allocation of lock requests is
  controlled from outside of the MDL subsystem, while allocation of granted
  locks (tickets) is controlled within the MDL subsystem.

  MDL_request is a C structure, you don't need to call a constructor
  or destructor for it.
*/

class MDL_request
{
public:
  /** Type of metadata lock. */
  enum          enum_mdl_type type;

  /**
    Pointers for participating in the list of lock requests for this context.
  */
  MDL_request *next_in_list;
  MDL_request **prev_in_list;
  /**
    Pointer to the lock ticket object for this lock request.
    Valid only if this lock request is satisfied.
  */
  MDL_ticket *ticket;

  /** A lock is requested based on a fully qualified name and type. */
  MDL_key key;

public:
  void init(enum_mdl_namespace namespace_arg, const char *db_arg, const char *name_arg,
            enum_mdl_type mdl_type_arg);
  /** Set type of lock request. Can be only applied to pending locks. */
  inline void set_type(enum_mdl_type type_arg)
  {
    DBUG_ASSERT(ticket == NULL);
    type= type_arg;
  }
  bool is_shared() const { return type < MDL_EXCLUSIVE; }

  static MDL_request *create(enum_mdl_namespace mdl_namespace, const char *db,
                             const char *name, enum_mdl_type mdl_type,
                             MEM_ROOT *root);

  /*
    This is to work around the ugliness of TABLE_LIST
    compiler-generated assignment operator. It is currently used
    in several places to quickly copy "most" of the members of the
    table list. These places currently never assume that the mdl
    request is carried over to the new TABLE_LIST, or shared
    between lists.

    This method does not initialize the instance being assigned!
    Use of init() for initialization after this assignment operator
    is mandatory. Can only be used before the request has been
    granted.
  */
  MDL_request& operator=(const MDL_request &rhs)
  {
    ticket= NULL;
    /* Do nothing, in particular, don't try to copy the key. */
    return *this;
  }
  /* Another piece of ugliness for TABLE_LIST constructor */
  MDL_request() {}

  MDL_request(const MDL_request *rhs)
    :type(rhs->type),
    ticket(NULL),
    key(&rhs->key)
  {}
};


typedef void (*mdl_cached_object_release_hook)(void *);

/**
  A granted metadata lock.

  @warning MDL_ticket members are private to the MDL subsystem.

  @note Multiple shared locks on a same object are represented by a
        single ticket. The same does not apply for other lock types.
*/

class MDL_ticket
{
public:
  /**
    Pointers for participating in the list of lock requests for this context.
  */
  MDL_ticket *next_in_context;
  MDL_ticket **prev_in_context;
  /**
    Pointers for participating in the list of satisfied/pending requests
    for the lock.
  */
  MDL_ticket *next_in_lock;
  MDL_ticket **prev_in_lock;
public:
  bool has_pending_conflicting_lock() const;

  void *get_cached_object();
  void set_cached_object(void *cached_object,
                         mdl_cached_object_release_hook release_hook);
  const MDL_context *get_ctx() const { return m_ctx; }
  bool is_shared() const { return m_type < MDL_EXCLUSIVE; }
  bool upgrade_shared_lock_to_exclusive();
  void downgrade_exclusive_lock();
private:
  friend class MDL_context;

  MDL_ticket(MDL_context *ctx_arg, enum_mdl_type type_arg)
   : m_type(type_arg),
     m_state(MDL_PENDING),
     m_ctx(ctx_arg),
     m_lock(NULL)
  {}


  static MDL_ticket *create(MDL_context *ctx_arg, enum_mdl_type type_arg);
  static void destroy(MDL_ticket *ticket);
private:
  /** Type of metadata lock. */
  enum enum_mdl_type m_type;
  /** State of the metadata lock ticket. */
  enum enum_mdl_state m_state;

  /** Context of the owner of the metadata lock ticket. */
  MDL_context *m_ctx;

  /** Pointer to the lock object for this lock ticket. */
  MDL_lock *m_lock;
private:
  MDL_ticket(const MDL_ticket &);               /* not implemented */
  MDL_ticket &operator=(const MDL_ticket &);    /* not implemented */
};


typedef I_P_List<MDL_request, I_P_List_adapter<MDL_request,
                 &MDL_request::next_in_list,
                 &MDL_request::prev_in_list> >
        MDL_request_list;

/**
  Context of the owner of metadata locks. I.e. each server
  connection has such a context.
*/

class MDL_context
{
public:
  typedef I_P_List<MDL_ticket,
                   I_P_List_adapter<MDL_ticket,
                                    &MDL_ticket::next_in_context,
                                    &MDL_ticket::prev_in_context> >
          Ticket_list;

  typedef Ticket_list::Iterator Ticket_iterator;

  void init(THD *thd);
  void destroy();

  void backup_and_reset(MDL_context *backup);
  void restore_from_backup(MDL_context *backup);
  void merge(MDL_context *source);

  bool try_acquire_shared_lock(MDL_request *mdl_request);
  bool acquire_exclusive_lock(MDL_request *mdl_request);
  bool acquire_exclusive_locks(MDL_request_list *requests);
  bool try_acquire_exclusive_lock(MDL_request *mdl_request);
  bool acquire_global_shared_lock();

  bool wait_for_locks(MDL_request_list *requests);

  void release_all_locks();
  void release_all_locks_for_name(MDL_ticket *ticket);
  void release_lock(MDL_ticket *ticket);
  void release_global_shared_lock();

  bool is_exclusive_lock_owner(enum_mdl_namespace mdl_namespace,
                               const char *db,
                               const char *name);
  bool is_lock_owner(enum_mdl_namespace mdl_namespace, const char *db, const char *name);

  inline bool has_locks() const
  {
    return !m_tickets.is_empty();
  }

  inline MDL_ticket *mdl_savepoint()
  {
    return m_tickets.head();
  }

  void rollback_to_savepoint(MDL_ticket *mdl_savepoint);

  inline THD *get_thd() const { return m_thd; }
private:
  Ticket_list m_tickets;
  bool m_has_global_shared_lock;
  THD *m_thd;
private:
  void release_ticket(MDL_ticket *ticket);
  MDL_ticket *find_ticket(MDL_request *mdl_req);
};


void mdl_init();
void mdl_destroy();


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

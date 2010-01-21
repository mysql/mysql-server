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

  @sa Comments for MDL_object_lock::can_grant_lock() and
  MDL_global_lock::can_grant_lock() for details.
*/

enum enum_mdl_type {MDL_SHARED=0, MDL_SHARED_HIGH_PRIO,
                    MDL_SHARED_UPGRADABLE, MDL_INTENTION_EXCLUSIVE,
                    MDL_EXCLUSIVE};


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

class MDL_key
{
public:
  /**
    Object namespaces

    Different types of objects exist in different namespaces
     - TABLE is for tables and views.
     - FUNCTION is for stored functions.
     - PROCEDURE is for stored procedures.
     - TRIGGER is for triggers.
    Note that although there isn't metadata locking on triggers,
    it's necessary to have a separate namespace for them since
    MDL_key is also used outside of the MDL subsystem.
  */
  enum enum_mdl_namespace { TABLE=0,
                            FUNCTION,
                            PROCEDURE,
                            TRIGGER,
                            GLOBAL };

  const uchar *ptr() const { return (uchar*) m_ptr; }
  uint length() const { return m_length; }

  const char *db_name() const { return m_ptr + 1; }
  uint db_name_length() const { return m_db_name_length; }

  const char *name() const { return m_ptr + m_db_name_length + 2; }
  uint name_length() const { return m_length - m_db_name_length - 3; }

  enum_mdl_namespace mdl_namespace() const
  { return (enum_mdl_namespace)(m_ptr[0]); }

  /**
    Construct a metadata lock key from a triplet (mdl_namespace,
    database and name).

    @remark The key for a table is <mdl_namespace>+<database name>+<table name>

    @param  mdl_namespace Id of namespace of object to be locked
    @param  db            Name of database to which the object belongs
    @param  name          Name of of the object
    @param  key           Where to store the the MDL key.
  */
  void mdl_key_init(enum_mdl_namespace mdl_namespace,
                    const char *db, const char *name)
  {
    m_ptr[0]= (char) mdl_namespace;
    m_db_name_length= (uint16) (strmov(m_ptr + 1, db) - m_ptr - 1);
    m_length= (uint16) (strmov(m_ptr + m_db_name_length + 2, name) - m_ptr + 1);
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
  /**
    Compare two MDL keys lexicographically.
  */
  int cmp(const MDL_key *rhs) const
  {
    /*
      The key buffer is always '\0'-terminated. Since key
      character set is utf-8, we can safely assume that no
      character starts with a zero byte.
    */
    return memcmp(m_ptr, rhs->m_ptr, min(m_length, rhs->m_length)+1);
  }

  MDL_key(const MDL_key *rhs)
  {
    mdl_key_init(rhs);
  }
  MDL_key(enum_mdl_namespace namespace_arg,
          const char *db_arg, const char *name_arg)
  {
    mdl_key_init(namespace_arg, db_arg, name_arg);
  }
  MDL_key() {} /* To use when part of MDL_request. */

private:
  uint16 m_length;
  uint16 m_db_name_length;
  char m_ptr[MAX_MDLKEY_LENGTH];
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
  void init(MDL_key::enum_mdl_namespace namespace_arg,
            const char *db_arg, const char *name_arg,
            enum_mdl_type mdl_type_arg);
  void init(const MDL_key *key_arg, enum_mdl_type mdl_type_arg);
  /** Set type of lock request. Can be only applied to pending locks. */
  inline void set_type(enum_mdl_type type_arg)
  {
    DBUG_ASSERT(ticket == NULL);
    type= type_arg;
  }
  bool is_shared() const { return type < MDL_INTENTION_EXCLUSIVE; }

  static MDL_request *create(MDL_key::enum_mdl_namespace mdl_namespace,
                             const char *db, const char *name,
                             enum_mdl_type mdl_type, MEM_ROOT *root);

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

  @note There are two groups of MDL_ticket members:
        - "Externally accessible". These members can be accessed from
          threads/contexts different than ticket owner in cases when
          ticket participates in some list of granted or waiting tickets
          for a lock. Therefore one should change these members before
          including then to waiting/granted lists or while holding lock
          protecting those lists.
        - "Context private". Such members are private to thread/context
          owning this ticket. I.e. they should not be accessed from other
          threads/contexts.
*/

class MDL_ticket
{
public:
  /**
    Pointers for participating in the list of lock requests for this context.
    Context private.
  */
  MDL_ticket *next_in_context;
  MDL_ticket **prev_in_context;
  /**
    Pointers for participating in the list of satisfied/pending requests
    for the lock. Externally accessible.
  */
  MDL_ticket *next_in_lock;
  MDL_ticket **prev_in_lock;
public:
  bool has_pending_conflicting_lock() const;

  void *get_cached_object();
  void set_cached_object(void *cached_object,
                         mdl_cached_object_release_hook release_hook);
  MDL_context *get_ctx() const { return m_ctx; }
  bool is_shared() const { return m_type < MDL_INTENTION_EXCLUSIVE; }
  bool is_upgradable_or_exclusive() const
  {
    return m_type == MDL_SHARED_UPGRADABLE || m_type == MDL_EXCLUSIVE;
  }
  bool upgrade_shared_lock_to_exclusive();
  void downgrade_exclusive_lock();
private:
  friend class MDL_context;
  friend class MDL_global_lock;
  friend class MDL_object_lock;

  MDL_ticket(MDL_context *ctx_arg, enum_mdl_type type_arg)
   : m_type(type_arg),
     m_state(MDL_PENDING),
     m_ctx(ctx_arg),
     m_lock(NULL)
  {}

  static MDL_ticket *create(MDL_context *ctx_arg, enum_mdl_type type_arg);
  static void destroy(MDL_ticket *ticket);
private:
  /** Type of metadata lock. Externally accessible. */
  enum enum_mdl_type m_type;
  /** State of the metadata lock ticket. Context private. */
  enum enum_mdl_state m_state;

  /**
    Context of the owner of the metadata lock ticket. Externally accessible.
  */
  MDL_context *m_ctx;

  /** Pointer to the lock object for this lock ticket. Context private. */
  MDL_lock *m_lock;
private:
  MDL_ticket(const MDL_ticket &);               /* not implemented */
  MDL_ticket &operator=(const MDL_ticket &);    /* not implemented */
};


typedef I_P_List<MDL_request, I_P_List_adapter<MDL_request,
                 &MDL_request::next_in_list,
                 &MDL_request::prev_in_list>,
                 I_P_List_counter>
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

  MDL_context();
  void destroy();

  bool try_acquire_shared_lock(MDL_request *mdl_request);
  bool acquire_exclusive_lock(MDL_request *mdl_request);
  bool acquire_exclusive_locks(MDL_request_list *requests);
  bool try_acquire_exclusive_lock(MDL_request *mdl_request);
  bool clone_ticket(MDL_request *mdl_request);

  bool wait_for_locks(MDL_request_list *requests);

  void release_all_locks_for_name(MDL_ticket *ticket);
  void release_lock(MDL_ticket *ticket);

  bool is_exclusive_lock_owner(MDL_key::enum_mdl_namespace mdl_namespace,
                               const char *db,
                               const char *name);
  bool is_lock_owner(MDL_key::enum_mdl_namespace mdl_namespace,
                     const char *db, const char *name);


  bool has_lock(MDL_ticket *mdl_savepoint, MDL_ticket *mdl_ticket);

  inline bool has_locks() const
  {
    return !m_tickets.is_empty();
  }

  MDL_ticket *mdl_savepoint()
  {
    /*
      NULL savepoint represents the start of the transaction.
      Checking for m_lt_or_ha_sentinel also makes sure we never
      return a pointer to HANDLER ticket as a savepoint.
    */
    return m_tickets.front() == m_lt_or_ha_sentinel ? NULL : m_tickets.front();
  }

  void set_lt_or_ha_sentinel()
  {
    m_lt_or_ha_sentinel= mdl_savepoint();
  }
  MDL_ticket *lt_or_ha_sentinel() const { return m_lt_or_ha_sentinel; }

  void clear_lt_or_ha_sentinel()
  {
    m_lt_or_ha_sentinel= NULL;
  }
  void move_ticket_after_lt_or_ha_sentinel(MDL_ticket *mdl_ticket);

  void release_transactional_locks();
  void rollback_to_savepoint(MDL_ticket *mdl_savepoint);

  bool can_wait_lead_to_deadlock() const;

  inline THD *get_thd() const { return m_thd; }

  /**
    Wake up context which is waiting for a change of MDL_lock state.
  */
  void awake()
  {
    pthread_cond_signal(&m_ctx_wakeup_cond);
  }

  bool try_acquire_global_intention_exclusive_lock(MDL_request *mdl_request);
  bool acquire_global_intention_exclusive_lock(MDL_request *mdl_request);

  bool acquire_global_shared_lock();
  void release_global_shared_lock();

  /**
    Check if this context owns global lock of particular type.
  */
  bool is_global_lock_owner(enum_mdl_type type_arg)
  {
    MDL_request mdl_request;
    bool not_used;
    mdl_request.init(MDL_key::GLOBAL, "", "", type_arg);
    return find_ticket(&mdl_request, &not_used);
  }

  void init(THD *thd_arg) { m_thd= thd_arg; }
private:
  Ticket_list m_tickets;
  /**
    This member has two uses:
    1) When entering LOCK TABLES mode, remember the last taken
    metadata lock. COMMIT/ROLLBACK must preserve these metadata
    locks.
    2) When we have an open HANDLER tables, store the position
    in the list beyond which we keep locks for HANDLER tables.
    COMMIT/ROLLBACK must, again, preserve HANDLER metadata locks.
  */
  MDL_ticket *m_lt_or_ha_sentinel;
  THD *m_thd;
  /**
    Condvar which is used for waiting until this context's pending
    request can be satisfied or this thread has to perform actions
    to resolve potential deadlock (we subscribe for such notification
    by adding ticket corresponding to the request to an appropriate
    queue of waiters).
  */
  pthread_cond_t m_ctx_wakeup_cond;
private:
  MDL_ticket *find_ticket(MDL_request *mdl_req,
                          bool *is_lt_or_ha);
  void release_locks_stored_before(MDL_ticket *sentinel);

  bool try_acquire_lock_impl(MDL_request *mdl_request);
  bool acquire_lock_impl(MDL_request *mdl_request);
  bool acquire_exclusive_lock_impl(MDL_request *mdl_request);

  friend bool MDL_ticket::upgrade_shared_lock_to_exclusive();
private:
  MDL_context(const MDL_context &rhs);          /* not implemented */
  MDL_context &operator=(MDL_context &rhs);     /* not implemented */
};


void mdl_init();
void mdl_destroy();


/*
  Functions in the server's kernel used by metadata locking subsystem.
*/

extern bool mysql_notify_thread_having_shared_lock(THD *thd, THD *in_use);
extern void mysql_ha_flush(THD *thd);
extern void mysql_abort_transactions_with_shared_lock(const MDL_key *mdl_key);
extern "C" const char *set_thd_proc_info(THD *thd, const char *info,
                                         const char *calling_function,
                                         const char *calling_file,
                                         const unsigned int calling_line);
#ifndef DBUG_OFF
extern pthread_mutex_t LOCK_open;
#endif

#endif

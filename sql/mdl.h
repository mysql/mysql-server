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
class Deadlock_detection_context;

/**
  Type of metadata lock request.

  @sa Comments for MDL_object_lock::can_grant_lock() and
      MDL_global_lock::can_grant_lock() for details.
*/

enum enum_mdl_type {
  /*
    An intention exclusive metadata lock. Used only for global locks.
    Owner of this type of lock can acquire upgradable exclusive locks on
    individual objects.
    Compatible with other IX locks, but is incompatible with global S lock.
  */
  MDL_INTENTION_EXCLUSIVE= 0,
  /*
    A shared metadata lock.
    To be used in cases when we are interested in object metadata only
    and there is no intention to access object data (e.g. for stored
    routines or during preparing prepared statements).
    We also mis-use this type of lock for open HANDLERs, since lock
    acquired by this statement has to be compatible with lock acquired
    by LOCK TABLES ... WRITE statement, i.e. SNRW (We can't get by by
    acquiring S lock at HANDLER ... OPEN time and upgrading it to SR
    lock for HANDLER ... READ as it doesn't solve problem with need
    to abort DML statements which wait on table level lock while having
    open HANDLER in the same connection).
    To avoid deadlock which may occur when SNRW lock is being upgraded to
    X lock for table on which there is an active S lock which is owned by
    thread which waits in its turn for table-level lock owned by thread
    performing upgrade we have to use thr_abort_locks_for_thread()
    facility in such situation.
    This problem does not arise for locks on stored routines as we don't
    use SNRW locks for them. It also does not arise when S locks are used
    during PREPARE calls as table-level locks are not acquired in this
    case.
  */
  MDL_SHARED,
  /*
    A high priority shared metadata lock.
    Used for cases when there is no intention to access object data (i.e.
    data in the table).
    "High priority" means that, unlike other shared locks, it is granted
    ignoring pending requests for exclusive locks. Intended for use in
    cases when we only need to access metadata and not data, e.g. when
    filling an INFORMATION_SCHEMA table.
    Since SH lock is compatible with SNRW lock, the connection that
    holds SH lock lock should not try to acquire any kind of table-level
    or row-level lock, as this can lead to a deadlock. Moreover, after
    acquiring SH lock, the connection should not wait for any other
    resource, as it might cause starvation for X locks and a potential
    deadlock during upgrade of SNW or SNRW to X lock (e.g. if the
    upgrading connection holds the resource that is being waited for).
  */
  MDL_SHARED_HIGH_PRIO,
  /*
    A shared metadata lock for cases when there is an intention to read data
    from table.
    A connection holding this kind of lock can read table metadata and read
    table data (after acquiring appropriate table and row-level locks).
    This means that one can only acquire TL_READ, TL_READ_NO_INSERT, and
    similar table-level locks on table if one holds SR MDL lock on it.
    To be used for tables in SELECTs, subqueries, and LOCK TABLE ...  READ
    statements.
  */
  MDL_SHARED_READ,
  /*
    A shared metadata lock for cases when there is an intention to modify
    (and not just read) data in the table.
    A connection holding SW lock can read table metadata and modify or read
    table data (after acquiring appropriate table and row-level locks).
    To be used for tables to be modified by INSERT, UPDATE, DELETE
    statements, but not LOCK TABLE ... WRITE or DDL). Also taken by
    SELECT ... FOR UPDATE.
  */
  MDL_SHARED_WRITE,
  /*
    An upgradable shared metadata lock which blocks all attempts to update
    table data, allowing reads.
    A connection holding this kind of lock can read table metadata and read
    table data.
    Can be upgraded to X metadata lock.
    Note, that since this type of lock is not compatible with SNRW or SW
    lock types, acquiring appropriate engine-level locks for reading
    (TL_READ* for MyISAM, shared row locks in InnoDB) should be
    contention-free.
    To be used for the first phase of ALTER TABLE, when copying data between
    tables, to allow concurrent SELECTs from the table, but not UPDATEs.
  */
  MDL_SHARED_NO_WRITE,
  /*
    An upgradable shared metadata lock which allows other connections
    to access table metadata, but not data.
    It blocks all attempts to read or update table data, while allowing
    INFORMATION_SCHEMA and SHOW queries.
    A connection holding this kind of lock can read table metadata modify and
    read table data.
    Can be upgraded to X metadata lock.
    To be used for LOCK TABLES WRITE statement.
    Not compatible with any other lock type except S and SH.
  */
  MDL_SHARED_NO_READ_WRITE,
  /*
    An exclusive metadata lock.
    A connection holding this lock can modify both table's metadata and data.
    No other type of metadata lock can be granted while this lock is held.
    To be used for CREATE/DROP/RENAME TABLE statements and for execution of
    certain phases of other DDL statements.
  */
  MDL_EXCLUSIVE,
  /* This should be the last !!! */
  MDL_TYPE_END};


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
  enum enum_mdl_namespace { GLOBAL=0,
                            TABLE,
                            FUNCTION,
                            PROCEDURE,
                            TRIGGER };

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
  uint get_deadlock_weight() const;

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
  bool is_upgradable_or_exclusive() const
  {
    return m_type == MDL_SHARED_NO_WRITE ||
           m_type == MDL_SHARED_NO_READ_WRITE ||
           m_type == MDL_EXCLUSIVE;
  }
  enum_mdl_type get_type() const { return m_type; }
  MDL_lock *get_lock() const { return m_lock; }
  void downgrade_exclusive_lock(enum_mdl_type type);

  bool has_stronger_or_equal_type(enum_mdl_type type) const;

  bool is_incompatible_when_granted(enum_mdl_type type) const;
  bool is_incompatible_when_waiting(enum_mdl_type type) const;

private:
  friend class MDL_context;

  MDL_ticket(MDL_context *ctx_arg, enum_mdl_type type_arg)
   : m_type(type_arg),
     m_ctx(ctx_arg),
     m_lock(NULL)
  {}

  static MDL_ticket *create(MDL_context *ctx_arg, enum_mdl_type type_arg);
  static void destroy(MDL_ticket *ticket);
private:
  /** Type of metadata lock. Externally accessible. */
  enum enum_mdl_type m_type;
  /**
    Context of the owner of the metadata lock ticket. Externally accessible.
  */
  MDL_context *m_ctx;

  /**
    Pointer to the lock object for this lock ticket. Externally accessible.
  */
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

  enum mdl_signal_type { NO_WAKE_UP = 0,
                         NORMAL_WAKE_UP,
                         VICTIM_WAKE_UP,
                         TIMEOUT_WAKE_UP };

  MDL_context();
  void destroy();

  bool try_acquire_lock(MDL_request *mdl_request);
  bool acquire_lock(MDL_request *mdl_request);
  bool acquire_locks(MDL_request_list *requests);
  bool upgrade_shared_lock_to_exclusive(MDL_ticket *mdl_ticket);

  bool clone_ticket(MDL_request *mdl_request);

  bool wait_for_lock(MDL_request *mdl_request);

  void release_all_locks_for_name(MDL_ticket *ticket);
  void release_lock(MDL_ticket *ticket);

  bool is_lock_owner(MDL_key::enum_mdl_namespace mdl_namespace,
                     const char *db, const char *name,
                     enum_mdl_type mdl_type);

  bool has_lock(MDL_ticket *mdl_savepoint, MDL_ticket *mdl_ticket);

  inline bool has_locks() const
  {
    return !m_tickets.is_empty();
  }

  MDL_ticket *mdl_savepoint()
  {
    /*
      NULL savepoint represents the start of the transaction.
      Checking for m_trans_sentinel also makes sure we never
      return a pointer to HANDLER ticket as a savepoint.
    */
    return m_tickets.front() == m_trans_sentinel ? NULL : m_tickets.front();
  }

  void set_trans_sentinel()
  {
    m_trans_sentinel= mdl_savepoint();
  }
  MDL_ticket *trans_sentinel() const { return m_trans_sentinel; }

  void reset_trans_sentinel(MDL_ticket *sentinel_arg)
  {
    m_trans_sentinel= sentinel_arg;
  }
  void move_ticket_after_trans_sentinel(MDL_ticket *mdl_ticket);

  void release_transactional_locks();
  void rollback_to_savepoint(MDL_ticket *mdl_savepoint);

  inline THD *get_thd() const { return m_thd; }

  bool acquire_global_intention_exclusive_lock(MDL_request *mdl_request);

  /**
    Wake up context which is waiting for a change of MDL_lock state.
  */
  void awake(mdl_signal_type signal)
  {
    pthread_mutex_lock(&m_signal_lock);
    m_signal= signal;
    pthread_cond_signal(&m_signal_cond);
    pthread_mutex_unlock(&m_signal_lock);
  }

  void init(THD *thd_arg) { m_thd= thd_arg; }

  void set_needs_thr_lock_abort(bool needs_thr_lock_abort)
  {
    /*
      @note In theory, this member should be modified under protection
            of some lock since it can be accessed from different threads.
            In practice, this is not necessary as code which reads this
            value and so might miss the fact that value was changed will
            always re-try reading it after small timeout and therefore
            will see the new value eventually.
    */
    m_needs_thr_lock_abort= TRUE;
  }
  bool get_needs_thr_lock_abort() const
  {
    return m_needs_thr_lock_abort;
  }

  bool find_deadlock(Deadlock_detection_context *deadlock_ctx);
private:
  /**
    All MDL tickets acquired by this connection.

    The order of tickets in m_tickets list.
    ---------------------------------------
    The entire set of locks acquired by a connection
    can be separated in two subsets: transactional and
    non-transactional locks.

    Transactional locks are locks with automatic scope. They
    are accumulated in the course of a transaction, and
    released only on COMMIT, ROLLBACK or ROLLBACK TO SAVEPOINT.
    They must not be (and never are) released manually,
    i.e. with release_lock() call.

    Non-transactional locks are taken for locks that span
    multiple transactions or savepoints.
    These are: HANDLER SQL locks (HANDLER SQL is
    transaction-agnostic), LOCK TABLES locks (you can COMMIT/etc
    under LOCK TABLES, and the locked tables stay locked), and
    SET GLOBAL READ_ONLY=1 global shared lock.

    Transactional locks are always prepended to the beginning
    of the list. In other words, they are stored in reverse
    temporal order. Thus, when we rollback to a savepoint,
    we start popping and releasing tickets from the front
    until we reach the last ticket acquired after the
    savepoint.

    Non-transactional locks are always stored after
    transactional ones, and among each other can be
    split into three sets:

    [LOCK TABLES locks] [HANDLER locks] [GLOBAL READ LOCK locks]

    The following is known about these sets:

    * we can never have both HANDLER and LOCK TABLES locks
      together -- HANDLER statements are prohibited under LOCK
      TABLES, entering LOCK TABLES implicitly closes all open
      HANDLERs.
    * GLOBAL READ LOCK locks are always stored after LOCK TABLES
      locks and after HANDLER locks. This is because one can't say
      SET GLOBAL read_only=1 or FLUSH TABLES WITH READ LOCK
      if one has locked tables. One can, however, LOCK TABLES
      after having entered the read only mode. Note, that
      subsequent LOCK TABLES statement will unlock the previous
      set of tables, but not the GRL!
      There are no HANDLER locks after GRL locks because
      SET GLOBAL read_only performs a FLUSH TABLES WITH
      READ LOCK internally, and FLUSH TABLES, in turn, implicitly
      closes all open HANDLERs.
      However, one can open a few HANDLERs after entering the
      read only mode.
  */
  Ticket_list m_tickets;
  /**
    Separates transactional and non-transactional locks
    in m_tickets list, @sa m_tickets.
  */
  MDL_ticket *m_trans_sentinel;
  THD *m_thd;
  /**
    TRUE -  if for this context we will break protocol and try to
            acquire table-level locks while having only S lock on
            some table.
            To avoid deadlocks which might occur during concurrent
            upgrade of SNRW lock on such object to X lock we have to
            abort waits for table-level locks for such connections.
    FALSE - Otherwise.
  */
  bool m_needs_thr_lock_abort;

  /**
    Read-write lock protecting m_waiting_for member.

    TODO/FIXME: Replace with RW-lock which will prefer readers
                on all platforms and not only on Linux.
  */
  rw_lock_t m_waiting_for_lock;
  MDL_ticket *m_waiting_for;
  uint m_deadlock_weight;
  /**
    Condvar which is used for waiting until this context's pending
    request can be satisfied or this thread has to perform actions
    to resolve a potential deadlock (we subscribe to such
    notification by adding a ticket corresponding to the request
    to an appropriate queue of waiters).
  */
  pthread_mutex_t m_signal_lock;
  pthread_cond_t m_signal_cond;
  mdl_signal_type m_signal;

private:
  MDL_ticket *find_ticket(MDL_request *mdl_req,
                          bool *is_transactional);
  void release_locks_stored_before(MDL_ticket *sentinel);
  bool acquire_lock_impl(MDL_request *mdl_request);

  bool find_deadlock();

  void will_wait_for(MDL_ticket *pending_ticket)
  {
    rw_wrlock(&m_waiting_for_lock);
    m_waiting_for= pending_ticket;
    rw_unlock(&m_waiting_for_lock);
  }

  void set_deadlock_weight(uint weight)
  {
    /*
      m_deadlock_weight should not be modified while m_waiting_for is
      non-NULL as in this case this context might participate in deadlock
      and so m_deadlock_weight can be accessed from other threads.
    */
    DBUG_ASSERT(m_waiting_for == NULL);
    m_deadlock_weight= weight;
  }

  void stop_waiting()
  {
    rw_wrlock(&m_waiting_for_lock);
    m_waiting_for= NULL;
    rw_unlock(&m_waiting_for_lock);
  }

  void wait_reset()
  {
    pthread_mutex_lock(&m_signal_lock);
    m_signal= NO_WAKE_UP;
    pthread_mutex_unlock(&m_signal_lock);
  }

  mdl_signal_type wait();
  mdl_signal_type timed_wait(ulong timeout);

  mdl_signal_type peek_signal()
  {
    mdl_signal_type result;
    pthread_mutex_lock(&m_signal_lock);
    result= m_signal;
    pthread_mutex_unlock(&m_signal_lock);
    return result;
  }

private:
  MDL_context(const MDL_context &rhs);          /* not implemented */
  MDL_context &operator=(MDL_context &rhs);     /* not implemented */
};


void mdl_init();
void mdl_destroy();


/*
  Functions in the server's kernel used by metadata locking subsystem.
*/

extern bool mysql_notify_thread_having_shared_lock(THD *thd, THD *in_use,
                                                   bool needs_thr_lock_abort);
extern void mysql_ha_flush(THD *thd);
extern "C" const char *set_thd_proc_info(THD *thd, const char *info,
                                         const char *calling_function,
                                         const char *calling_file,
                                         const unsigned int calling_line);
#ifndef DBUG_OFF
extern pthread_mutex_t LOCK_open;
#endif

#endif

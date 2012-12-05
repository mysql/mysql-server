#ifndef MDL_H
#define MDL_H
/* Copyright (c) 2009, 2012, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   51 Franklin Street, Suite 500, Boston, MA 02110-1335 USA */

#if defined(__IBMC__) || defined(__IBMCPP__)
/* Further down, "next_in_lock" and "next_in_context" have the same type,
   and in "sql_plist.h" this leads to an identical signature, which causes
   problems in function overloading.
*/
#pragma namemangling(v5)
#endif


#include "sql_plist.h"
#include <my_sys.h>
#include <m_string.h>
#include <mysql_com.h>

#include <algorithm>

class THD;

class MDL_context;
class MDL_lock;
class MDL_ticket;

/**
  @def ENTER_COND(C, M, S, O)
  Start a wait on a condition.
  @param C the condition to wait on
  @param M the associated mutex
  @param S the new stage to enter
  @param O the previous stage
  @sa EXIT_COND().
*/
#define ENTER_COND(C, M, S, O) enter_cond(C, M, S, O, __func__, __FILE__, __LINE__)

/**
  @def EXIT_COND(S)
  End a wait on a condition
  @param S the new stage to enter
*/
#define EXIT_COND(S) exit_cond(S, __func__, __FILE__, __LINE__)

/**
   An interface to separate the MDL module from the THD, and the rest of the
   server code.
 */

class MDL_context_owner
{
public:
  virtual ~MDL_context_owner() {}

  /**
    Enter a condition wait.
    For @c enter_cond() / @c exit_cond() to work the mutex must be held before
    @c enter_cond(); this mutex is then released by @c exit_cond().
    Usage must be: lock mutex; enter_cond(); your code; exit_cond().
    @param cond the condition to wait on
    @param mutex the associated mutex
    @param [in] stage the stage to enter, or NULL
    @param [out] old_stage the previous stage, or NULL
    @param src_function function name of the caller
    @param src_file file name of the caller
    @param src_line line number of the caller
    @sa ENTER_COND(), THD::enter_cond()
    @sa EXIT_COND(), THD::exit_cond()
  */
  virtual void enter_cond(mysql_cond_t *cond, mysql_mutex_t *mutex,
                          const PSI_stage_info *stage, PSI_stage_info *old_stage,
                          const char *src_function, const char *src_file,
                          int src_line) = 0;

  /**
    @def EXIT_COND(S)
    End a wait on a condition
    @param [in] stage the new stage to enter
    @param src_function function name of the caller
    @param src_file file name of the caller
    @param src_line line number of the caller
    @sa ENTER_COND(), THD::enter_cond()
    @sa EXIT_COND(), THD::exit_cond()
  */
  virtual void exit_cond(const PSI_stage_info *stage,
                         const char *src_function, const char *src_file,
                         int src_line) = 0;
  /**
     Has the owner thread been killed?
   */
  virtual int  is_killed() = 0;

  /**
     This one is only used for DEBUG_SYNC.
     (Do not use it to peek/poke into other parts of THD.)
   */
  virtual THD* get_thd() = 0;

  /**
     @see THD::notify_shared_lock()
   */
  virtual bool notify_shared_lock(MDL_context_owner *in_use,
                                  bool needs_thr_lock_abort) = 0;
};

/**
  Type of metadata lock request.

  @sa Comments for MDL_object_lock::can_grant_lock() and
      MDL_scoped_lock::can_grant_lock() for details.
*/

enum enum_mdl_type {
  /*
    An intention exclusive metadata lock. Used only for scoped locks.
    Owner of this type of lock can acquire upgradable exclusive locks on
    individual objects.
    Compatible with other IX locks, but is incompatible with scoped S and
    X locks.
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
    An upgradable shared metadata lock for cases when there is an intention
    to modify (and not just read) data in the table.
    Can be upgraded to MDL_SHARED_NO_WRITE and MDL_EXCLUSIVE.
    A connection holding SU lock can read table metadata and modify or read
    table data (after acquiring appropriate table and row-level locks).
    To be used for the first phase of ALTER TABLE.
  */
  MDL_SHARED_UPGRADABLE,
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


/** Duration of metadata lock. */

enum enum_mdl_duration {
  /**
    Locks with statement duration are automatically released at the end
    of statement or transaction.
  */
  MDL_STATEMENT= 0,
  /**
    Locks with transaction duration are automatically released at the end
    of transaction.
  */
  MDL_TRANSACTION,
  /**
    Locks with explicit duration survive the end of statement and transaction.
    They have to be released explicitly by calling MDL_context::release_lock().
  */
  MDL_EXPLICIT,
  /* This should be the last ! */
  MDL_DURATION_END };


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
#ifdef HAVE_PSI_INTERFACE
  static void init_psi_keys();
#endif

  /**
    Object namespaces.
    Sic: when adding a new member to this enum make sure to
    update m_namespace_to_wait_state_name array in mdl.cc!

    Different types of objects exist in different namespaces
     - TABLE is for tables and views.
     - FUNCTION is for stored functions.
     - PROCEDURE is for stored procedures.
     - TRIGGER is for triggers.
     - EVENT is for event scheduler events
    Note that although there isn't metadata locking on triggers,
    it's necessary to have a separate namespace for them since
    MDL_key is also used outside of the MDL subsystem.
  */
  enum enum_mdl_namespace { GLOBAL=0,
                            SCHEMA,
                            TABLE,
                            FUNCTION,
                            PROCEDURE,
                            TRIGGER,
                            EVENT,
                            COMMIT,
                            /* This should be the last ! */
                            NAMESPACE_END };

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
    DBUG_ASSERT(rhs->m_length <= NAME_LEN);
    DBUG_ASSERT(rhs->m_db_name_length <= NAME_LEN);
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
    using std::min;
    return memcmp(m_ptr, rhs->m_ptr, min(m_length, rhs->m_length));
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

  /**
    Get thread state name to be used in case when we have to
    wait on resource identified by key.
  */
  const PSI_stage_info * get_wait_state_name() const
  {
    return & m_namespace_to_wait_state_name[(int)mdl_namespace()];
  }

private:
  uint16 m_length;
  uint16 m_db_name_length;
  char m_ptr[MAX_MDLKEY_LENGTH];
  static PSI_stage_info m_namespace_to_wait_state_name[NAMESPACE_END];
private:
  MDL_key(const MDL_key &);                     /* not implemented */
  MDL_key &operator=(const MDL_key &);          /* not implemented */
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
  /** Duration for requested lock. */
  enum enum_mdl_duration duration;

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
  static void *operator new(size_t size, MEM_ROOT *mem_root) throw ()
  { return alloc_root(mem_root, size); }
  static void operator delete(void *ptr, MEM_ROOT *mem_root) {}

  void init(MDL_key::enum_mdl_namespace namespace_arg,
            const char *db_arg, const char *name_arg,
            enum_mdl_type mdl_type_arg,
            enum_mdl_duration mdl_duration_arg);
  void init(const MDL_key *key_arg, enum_mdl_type mdl_type_arg,
            enum_mdl_duration mdl_duration_arg);
  /** Set type of lock request. Can be only applied to pending locks. */
  inline void set_type(enum_mdl_type type_arg)
  {
    DBUG_ASSERT(ticket == NULL);
    type= type_arg;
  }

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
    duration(rhs->duration),
    ticket(NULL),
    key(&rhs->key)
  {}
};


typedef void (*mdl_cached_object_release_hook)(void *);


/**
  An abstract class for inspection of a connected
  subgraph of the wait-for graph.
*/

class MDL_wait_for_graph_visitor
{
public:
  virtual bool enter_node(MDL_context *node) = 0;
  virtual void leave_node(MDL_context *node) = 0;

  virtual bool inspect_edge(MDL_context *dest) = 0;
  virtual ~MDL_wait_for_graph_visitor();
  MDL_wait_for_graph_visitor() :m_lock_open_count(0) {}
public:
  /**
   XXX, hack: During deadlock search, we may need to
   inspect TABLE_SHAREs and acquire LOCK_open. Since
   LOCK_open is not a recursive mutex, count here how many
   times we "took" it (but only take and release once).
   Not using a native recursive mutex or rwlock in 5.5 for
   LOCK_open since it has significant performance impacts.
  */
  uint m_lock_open_count;
};

/**
  Abstract class representing an edge in the waiters graph
  to be traversed by deadlock detection algorithm.
*/

class MDL_wait_for_subgraph
{
public:
  virtual ~MDL_wait_for_subgraph();

  /**
    Accept a wait-for graph visitor to inspect the node
    this edge is leading to.
  */
  virtual bool accept_visitor(MDL_wait_for_graph_visitor *gvisitor) = 0;

  enum enum_deadlock_weight
  {
    DEADLOCK_WEIGHT_DML= 0,
    DEADLOCK_WEIGHT_DDL= 100
  };
  /* A helper used to determine which lock request should be aborted. */
  virtual uint get_deadlock_weight() const = 0;
};


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

class MDL_ticket : public MDL_wait_for_subgraph
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

  MDL_context *get_ctx() const { return m_ctx; }
  bool is_upgradable_or_exclusive() const
  {
    return m_type == MDL_SHARED_UPGRADABLE ||
           m_type == MDL_SHARED_NO_WRITE ||
           m_type == MDL_SHARED_NO_READ_WRITE ||
           m_type == MDL_EXCLUSIVE;
  }
  enum_mdl_type get_type() const { return m_type; }
  MDL_lock *get_lock() const { return m_lock; }
  void downgrade_lock(enum_mdl_type type);

  bool has_stronger_or_equal_type(enum_mdl_type type) const;

  bool is_incompatible_when_granted(enum_mdl_type type) const;
  bool is_incompatible_when_waiting(enum_mdl_type type) const;

  /** Implement MDL_wait_for_subgraph interface. */
  virtual bool accept_visitor(MDL_wait_for_graph_visitor *dvisitor);
  virtual uint get_deadlock_weight() const;
private:
  friend class MDL_context;

  MDL_ticket(MDL_context *ctx_arg, enum_mdl_type type_arg
#ifndef DBUG_OFF
             , enum_mdl_duration duration_arg
#endif
            )
   : m_type(type_arg),
#ifndef DBUG_OFF
     m_duration(duration_arg),
#endif
     m_ctx(ctx_arg),
     m_lock(NULL)
  {}

  static MDL_ticket *create(MDL_context *ctx_arg, enum_mdl_type type_arg
#ifndef DBUG_OFF
                            , enum_mdl_duration duration_arg
#endif
                            );
  static void destroy(MDL_ticket *ticket);
private:
  /** Type of metadata lock. Externally accessible. */
  enum enum_mdl_type m_type;
#ifndef DBUG_OFF
  /**
    Duration of lock represented by this ticket.
    Context private. Debug-only.
  */
  enum_mdl_duration m_duration;
#endif
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


/**
  Savepoint for MDL context.

  Doesn't include metadata locks with explicit duration as
  they are not released during rollback to savepoint.
*/

class MDL_savepoint
{
public:
  MDL_savepoint() {};

private:
  MDL_savepoint(MDL_ticket *stmt_ticket, MDL_ticket *trans_ticket)
    : m_stmt_ticket(stmt_ticket), m_trans_ticket(trans_ticket)
  {}

  friend class MDL_context;

private:
  /**
    Pointer to last lock with statement duration which was taken
    before creation of savepoint.
  */
  MDL_ticket *m_stmt_ticket;
  /**
    Pointer to last lock with transaction duration which was taken
    before creation of savepoint.
  */
  MDL_ticket *m_trans_ticket;
};


/**
  A reliable way to wait on an MDL lock.
*/

class MDL_wait
{
public:
  MDL_wait();
  ~MDL_wait();

  enum enum_wait_status { EMPTY = 0, GRANTED, VICTIM, TIMEOUT, KILLED };

  bool set_status(enum_wait_status result_arg);
  enum_wait_status get_status();
  void reset_status();
  enum_wait_status timed_wait(MDL_context_owner *owner,
                              struct timespec *abs_timeout,
                              bool signal_timeout,
                              const PSI_stage_info *wait_state_name);
private:
  /**
    Condvar which is used for waiting until this context's pending
    request can be satisfied or this thread has to perform actions
    to resolve a potential deadlock (we subscribe to such
    notification by adding a ticket corresponding to the request
    to an appropriate queue of waiters).
  */
  mysql_mutex_t m_LOCK_wait_status;
  mysql_cond_t m_COND_wait_status;
  enum_wait_status m_wait_status;
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

  bool try_acquire_lock(MDL_request *mdl_request);
  bool acquire_lock(MDL_request *mdl_request, ulong lock_wait_timeout);
  bool acquire_locks(MDL_request_list *requests, ulong lock_wait_timeout);
  bool upgrade_shared_lock(MDL_ticket *mdl_ticket,
                           enum_mdl_type new_type,
                           ulong lock_wait_timeout);

  bool clone_ticket(MDL_request *mdl_request);

  void release_all_locks_for_name(MDL_ticket *ticket);
  void release_lock(MDL_ticket *ticket);

  bool is_lock_owner(MDL_key::enum_mdl_namespace mdl_namespace,
                     const char *db, const char *name,
                     enum_mdl_type mdl_type);

  bool has_lock(const MDL_savepoint &mdl_savepoint, MDL_ticket *mdl_ticket);

  inline bool has_locks() const
  {
    return !(m_tickets[MDL_STATEMENT].is_empty() &&
             m_tickets[MDL_TRANSACTION].is_empty() &&
             m_tickets[MDL_EXPLICIT].is_empty());
  }

  MDL_savepoint mdl_savepoint()
  {
    return MDL_savepoint(m_tickets[MDL_STATEMENT].front(),
                         m_tickets[MDL_TRANSACTION].front());
  }

  void set_explicit_duration_for_all_locks();
  void set_transaction_duration_for_all_locks();
  void set_lock_duration(MDL_ticket *mdl_ticket, enum_mdl_duration duration);

  void release_statement_locks();
  void release_transactional_locks();
  void rollback_to_savepoint(const MDL_savepoint &mdl_savepoint);

  MDL_context_owner *get_owner() { return m_owner; }

  /** @pre Only valid if we started waiting for lock. */
  inline uint get_deadlock_weight() const
  { return m_waiting_for->get_deadlock_weight(); }
  /**
    Post signal to the context (and wake it up if necessary).

    @retval FALSE - Success, signal was posted.
    @retval TRUE  - Failure, signal was not posted since context
                    already has received some signal or closed
                    signal slot.
  */
  void init(MDL_context_owner *arg) { m_owner= arg; }

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
    m_needs_thr_lock_abort= needs_thr_lock_abort;
  }
  bool get_needs_thr_lock_abort() const
  {
    return m_needs_thr_lock_abort;
  }
public:
  /**
    If our request for a lock is scheduled, or aborted by the deadlock
    detector, the result is recorded in this class.
  */
  MDL_wait m_wait;
private:
  /**
    Lists of all MDL tickets acquired by this connection.

    Lists of MDL tickets:
    ---------------------
    The entire set of locks acquired by a connection can be separated
    in three subsets according to their: locks released at the end of
    statement, at the end of transaction and locks are released
    explicitly.

    Statement and transactional locks are locks with automatic scope.
    They are accumulated in the course of a transaction, and released
    either at the end of uppermost statement (for statement locks) or
    on COMMIT, ROLLBACK or ROLLBACK TO SAVEPOINT (for transactional
    locks). They must not be (and never are) released manually,
    i.e. with release_lock() call.

    Locks with explicit duration are taken for locks that span
    multiple transactions or savepoints.
    These are: HANDLER SQL locks (HANDLER SQL is
    transaction-agnostic), LOCK TABLES locks (you can COMMIT/etc
    under LOCK TABLES, and the locked tables stay locked), and
    locks implementing "global read lock".

    Statement/transactional locks are always prepended to the
    beginning of the appropriate list. In other words, they are
    stored in reverse temporal order. Thus, when we rollback to
    a savepoint, we start popping and releasing tickets from the
    front until we reach the last ticket acquired after the savepoint.

    Locks with explicit duration stored are not stored in any
    particular order, and among each other can be split into
    three sets:

    [LOCK TABLES locks] [HANDLER locks] [GLOBAL READ LOCK locks]

    The following is known about these sets:

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
    * LOCK TABLES locks include intention exclusive locks on
      involved schemas and global intention exclusive lock.
  */
  Ticket_list m_tickets[MDL_DURATION_END];
  MDL_context_owner *m_owner;
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

    @note The fact that this read-write lock prefers readers is
          important as deadlock detector won't work correctly
          otherwise. @sa Comment for MDL_lock::m_rwlock.
  */
  mysql_prlock_t m_LOCK_waiting_for;
  /**
    Tell the deadlock detector what metadata lock or table
    definition cache entry this session is waiting for.
    In principle, this is redundant, as information can be found
    by inspecting waiting queues, but we'd very much like it to be
    readily available to the wait-for graph iterator.
   */
  MDL_wait_for_subgraph *m_waiting_for;
private:
  THD *get_thd() const { return m_owner->get_thd(); }
  MDL_ticket *find_ticket(MDL_request *mdl_req,
                          enum_mdl_duration *duration);
  void release_locks_stored_before(enum_mdl_duration duration, MDL_ticket *sentinel);
  void release_lock(enum_mdl_duration duration, MDL_ticket *ticket);
  bool try_acquire_lock_impl(MDL_request *mdl_request,
                             MDL_ticket **out_ticket);

public:
  void find_deadlock();

  bool visit_subgraph(MDL_wait_for_graph_visitor *dvisitor);

  /** Inform the deadlock detector there is an edge in the wait-for graph. */
  void will_wait_for(MDL_wait_for_subgraph *waiting_for_arg)
  {
    mysql_prlock_wrlock(&m_LOCK_waiting_for);
    m_waiting_for=  waiting_for_arg;
    mysql_prlock_unlock(&m_LOCK_waiting_for);
  }

  /** Remove the wait-for edge from the graph after we're done waiting. */
  void done_waiting_for()
  {
    mysql_prlock_wrlock(&m_LOCK_waiting_for);
    m_waiting_for= NULL;
    mysql_prlock_unlock(&m_LOCK_waiting_for);
  }
  void lock_deadlock_victim()
  {
    mysql_prlock_rdlock(&m_LOCK_waiting_for);
  }
  void unlock_deadlock_victim()
  {
    mysql_prlock_unlock(&m_LOCK_waiting_for);
  }
private:
  MDL_context(const MDL_context &rhs);          /* not implemented */
  MDL_context &operator=(MDL_context &rhs);     /* not implemented */
};


void mdl_init();
void mdl_destroy();


#ifndef DBUG_OFF
extern mysql_mutex_t LOCK_open;
#endif


/*
  Start-up parameter for the maximum size of the unused MDL_lock objects cache
  and a constant for its default value.
*/
extern ulong mdl_locks_cache_size;
static const ulong MDL_LOCKS_CACHE_SIZE_DEFAULT = 1024;

/*
  Start-up parameter for the number of partitions of the hash
  containing all the MDL_lock objects and a constant for
  its default value.
*/
extern ulong mdl_locks_hash_partitions;
static const ulong MDL_LOCKS_HASH_PARTITIONS_DEFAULT = 8;

/*
  Metadata locking subsystem tries not to grant more than
  max_write_lock_count high-prio, strong locks successively,
  to avoid starving out weak, low-prio locks.
*/
extern "C" ulong max_write_lock_count;
#endif

/* Copyright (c) 2007, 2012, Oracle and/or its affiliates. All rights reserved.

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


#include "mdl.h"
#include "debug_sync.h"
#include "sql_array.h"
#include <hash.h>
#include <mysqld_error.h>
#include <mysql/plugin.h>
#include <mysql/service_thd_wait.h>
#include <mysql/psi/mysql_stage.h>

#ifdef HAVE_PSI_INTERFACE
static PSI_mutex_key key_MDL_map_mutex;
static PSI_mutex_key key_MDL_wait_LOCK_wait_status;

static PSI_mutex_info all_mdl_mutexes[]=
{
  { &key_MDL_map_mutex, "MDL_map::mutex", 0},
  { &key_MDL_wait_LOCK_wait_status, "MDL_wait::LOCK_wait_status", 0}
};

static PSI_rwlock_key key_MDL_lock_rwlock;
static PSI_rwlock_key key_MDL_context_LOCK_waiting_for;

static PSI_rwlock_info all_mdl_rwlocks[]=
{
  { &key_MDL_lock_rwlock, "MDL_lock::rwlock", 0},
  { &key_MDL_context_LOCK_waiting_for, "MDL_context::LOCK_waiting_for", 0}
};

static PSI_cond_key key_MDL_wait_COND_wait_status;

static PSI_cond_info all_mdl_conds[]=
{
  { &key_MDL_wait_COND_wait_status, "MDL_context::COND_wait_status", 0}
};

/**
  Initialise all the performance schema instrumentation points
  used by the MDL subsystem.
*/
static void init_mdl_psi_keys(void)
{
  int count;

  count= array_elements(all_mdl_mutexes);
  mysql_mutex_register("sql", all_mdl_mutexes, count);

  count= array_elements(all_mdl_rwlocks);
  mysql_rwlock_register("sql", all_mdl_rwlocks, count);

  count= array_elements(all_mdl_conds);
  mysql_cond_register("sql", all_mdl_conds, count);

  MDL_key::init_psi_keys();
}
#endif /* HAVE_PSI_INTERFACE */


/**
  Thread state names to be used in case when we have to wait on resource
  belonging to certain namespace.
*/

PSI_stage_info MDL_key::m_namespace_to_wait_state_name[NAMESPACE_END]=
{
  {0, "Waiting for global read lock", 0},
  {0, "Waiting for schema metadata lock", 0},
  {0, "Waiting for table metadata lock", 0},
  {0, "Waiting for stored function metadata lock", 0},
  {0, "Waiting for stored procedure metadata lock", 0},
  {0, "Waiting for trigger metadata lock", 0},
  {0, "Waiting for event metadata lock", 0},
  {0, "Waiting for commit lock", 0}
};

#ifdef HAVE_PSI_INTERFACE
void MDL_key::init_psi_keys()
{
  int i;
  int count;
  PSI_stage_info *info __attribute__((unused));

  count= array_elements(MDL_key::m_namespace_to_wait_state_name);
  for (i= 0; i<count; i++)
  {
    /* mysql_stage_register wants an array of pointers, registering 1 by 1. */
    info= & MDL_key::m_namespace_to_wait_state_name[i];
    mysql_stage_register("sql", &info, 1);
  }
}
#endif

static bool mdl_initialized= 0;


class MDL_object_lock;
class MDL_object_lock_cache_adapter;


/**
  A partition in a collection of all MDL locks.
  MDL_map is partitioned for scalability reasons.
  Maps MDL_key to MDL_lock instances.
*/

class MDL_map_partition
{
public:
  MDL_map_partition();
  ~MDL_map_partition();
  inline MDL_lock *find_or_insert(const MDL_key *mdl_key,
                                  my_hash_value_type hash_value);
  inline void remove(MDL_lock *lock);
  my_hash_value_type get_key_hash(const MDL_key *mdl_key) const
  {
    return my_calc_hash(&m_locks, mdl_key->ptr(), mdl_key->length());
  }
private:
  bool move_from_hash_to_lock_mutex(MDL_lock *lock);
  /** A partition of all acquired locks in the server. */
  HASH m_locks;
  /* Protects access to m_locks hash. */
  mysql_mutex_t m_mutex;
  /**
    Cache of (unused) MDL_lock objects available for re-use.

    On some systems (e.g. Windows XP) constructing/destructing
    MDL_lock objects can be fairly expensive. We use this cache
    to avoid these costs in scenarios in which they can have
    significant negative effect on performance. For example, when
    there is only one thread constantly executing statements in
    auto-commit mode and thus constantly causing creation/
    destruction of MDL_lock objects for the tables it uses.

    Note that this cache contains only MDL_object_lock objects.

    Protected by m_mutex mutex.
  */
  typedef I_P_List<MDL_object_lock, MDL_object_lock_cache_adapter,
                   I_P_List_counter>
          Lock_cache;
  Lock_cache m_unused_locks_cache;
};


/**
  Start-up parameter for the number of partitions of the MDL_lock hash.
*/
ulong mdl_locks_hash_partitions;

/**
  A collection of all MDL locks. A singleton,
  there is only one instance of the map in the server.
  Contains instances of MDL_map_partition
*/

class MDL_map
{
public:
  void init();
  void destroy();
  MDL_lock *find_or_insert(const MDL_key *key);
  void remove(MDL_lock *lock);
private:
  /** Array of partitions where the locks are actually stored. */
  Dynamic_array<MDL_map_partition *> m_partitions;
  /** Pre-allocated MDL_lock object for GLOBAL namespace. */
  MDL_lock *m_global_lock;
  /** Pre-allocated MDL_lock object for COMMIT namespace. */
  MDL_lock *m_commit_lock;
};


/**
  A context of the recursive traversal through all contexts
  in all sessions in search for deadlock.
*/

class Deadlock_detection_visitor: public MDL_wait_for_graph_visitor
{
public:
  Deadlock_detection_visitor(MDL_context *start_node_arg)
    : m_start_node(start_node_arg),
      m_victim(NULL),
      m_current_search_depth(0),
      m_found_deadlock(FALSE)
  {}
  virtual bool enter_node(MDL_context *node);
  virtual void leave_node(MDL_context *node);

  virtual bool inspect_edge(MDL_context *dest);

  MDL_context *get_victim() const { return m_victim; }
private:
  /**
    Change the deadlock victim to a new one if it has lower deadlock
    weight.
  */
  void opt_change_victim_to(MDL_context *new_victim);
private:
  /**
    The context which has initiated the search. There
    can be multiple searches happening in parallel at the same time.
  */
  MDL_context *m_start_node;
  /** If a deadlock is found, the context that identifies the victim. */
  MDL_context *m_victim;
  /** Set to the 0 at start. Increased whenever
    we descend into another MDL context (aka traverse to the next
    wait-for graph node). When MAX_SEARCH_DEPTH is reached, we
    assume that a deadlock is found, even if we have not found a
    loop.
  */
  uint m_current_search_depth;
  /** TRUE if we found a deadlock. */
  bool m_found_deadlock;
  /**
    Maximum depth for deadlock searches. After this depth is
    achieved we will unconditionally declare that there is a
    deadlock.

    @note This depth should be small enough to avoid stack
          being exhausted by recursive search algorithm.

    TODO: Find out what is the optimal value for this parameter.
          Current value is safe, but probably sub-optimal,
          as there is an anecdotal evidence that real-life
          deadlocks are even shorter typically.
  */
  static const uint MAX_SEARCH_DEPTH= 32;
};


/**
  Enter a node of a wait-for graph. After
  a node is entered, inspect_edge() will be called
  for all wait-for destinations of this node. Then
  leave_node() will be called.
  We call "enter_node()" for all nodes we inspect,
  including the starting node.

  @retval  TRUE  Maximum search depth exceeded.
  @retval  FALSE OK.
*/

bool Deadlock_detection_visitor::enter_node(MDL_context *node)
{
  m_found_deadlock= ++m_current_search_depth >= MAX_SEARCH_DEPTH;
  if (m_found_deadlock)
  {
    DBUG_ASSERT(! m_victim);
    opt_change_victim_to(node);
  }
  return m_found_deadlock;
}


/**
  Done inspecting this node. Decrease the search
  depth. If a deadlock is found, and we are
  backtracking to the start node, optionally
  change the deadlock victim to one with lower
  deadlock weight.
*/

void Deadlock_detection_visitor::leave_node(MDL_context *node)
{
  --m_current_search_depth;
  if (m_found_deadlock)
    opt_change_victim_to(node);
}


/**
  Inspect a wait-for graph edge from one MDL context to another.

  @retval TRUE   A loop is found.
  @retval FALSE  No loop is found.
*/

bool Deadlock_detection_visitor::inspect_edge(MDL_context *node)
{
  m_found_deadlock= node == m_start_node;
  return m_found_deadlock;
}


/**
  Change the deadlock victim to a new one if it has lower deadlock
  weight.

  @retval new_victim  Victim is not changed.
  @retval !new_victim New victim became the current.
*/

void
Deadlock_detection_visitor::opt_change_victim_to(MDL_context *new_victim)
{
  if (m_victim == NULL ||
      m_victim->get_deadlock_weight() >= new_victim->get_deadlock_weight())
  {
    /* Swap victims, unlock the old one. */
    MDL_context *tmp= m_victim;
    m_victim= new_victim;
    m_victim->lock_deadlock_victim();
    if (tmp)
      tmp->unlock_deadlock_victim();
  }
}


/**
  Get a bit corresponding to enum_mdl_type value in a granted/waiting bitmaps
  and compatibility matrices.
*/

#define MDL_BIT(A) static_cast<MDL_lock::bitmap_t>(1U << A)

/**
  The lock context. Created internally for an acquired lock.
  For a given name, there exists only one MDL_lock instance,
  and it exists only when the lock has been granted.
  Can be seen as an MDL subsystem's version of TABLE_SHARE.

  This is an abstract class which lacks information about
  compatibility rules for lock types. They should be specified
  in its descendants.
*/

class MDL_lock
{
public:
  typedef unsigned short bitmap_t;

  class Ticket_list
  {
  public:
    typedef I_P_List<MDL_ticket,
                     I_P_List_adapter<MDL_ticket,
                                      &MDL_ticket::next_in_lock,
                                      &MDL_ticket::prev_in_lock>,
                     I_P_List_null_counter,
                     I_P_List_fast_push_back<MDL_ticket> >
            List;
    operator const List &() const { return m_list; }
    Ticket_list() :m_bitmap(0) {}

    void add_ticket(MDL_ticket *ticket);
    void remove_ticket(MDL_ticket *ticket);
    bool is_empty() const { return m_list.is_empty(); }
    bitmap_t bitmap() const { return m_bitmap; }
  private:
    void clear_bit_if_not_in_list(enum_mdl_type type);
  private:
    /** List of tickets. */
    List m_list;
    /** Bitmap of types of tickets in this list. */
    bitmap_t m_bitmap;
  };

  typedef Ticket_list::List::Iterator Ticket_iterator;

public:
  /** The key of the object (data) being protected. */
  MDL_key key;
  /**
    Read-write lock protecting this lock context.

    @note The fact that we use read-write lock prefers readers here is
          important as deadlock detector won't work correctly otherwise.

          For example, imagine that we have following waiters graph:

                       ctxA -> obj1 -> ctxB -> obj1 -|
                        ^                            |
                        |----------------------------|

          and both ctxA and ctxB start deadlock detection process:

            ctxA read-locks obj1             ctxB read-locks obj2
            ctxA goes deeper                 ctxB goes deeper

          Now ctxC comes in who wants to start waiting on obj1, also
          ctxD comes in who wants to start waiting on obj2.

            ctxC tries to write-lock obj1   ctxD tries to write-lock obj2
            ctxC is blocked                 ctxD is blocked

          Now ctxA and ctxB resume their search:

            ctxA tries to read-lock obj2    ctxB tries to read-lock obj1

          If m_rwlock prefers writes (or fair) both ctxA and ctxB would be
          blocked because of pending write locks from ctxD and ctxC
          correspondingly. Thus we will get a deadlock in deadlock detector.
          If m_wrlock prefers readers (actually ignoring pending writers is
          enough) ctxA and ctxB will continue and no deadlock will occur.
  */
  mysql_prlock_t m_rwlock;

  bool is_empty() const
  {
    return (m_granted.is_empty() && m_waiting.is_empty());
  }

  virtual const bitmap_t *incompatible_granted_types_bitmap() const = 0;
  virtual const bitmap_t *incompatible_waiting_types_bitmap() const = 0;

  bool has_pending_conflicting_lock(enum_mdl_type type);

  bool can_grant_lock(enum_mdl_type type, MDL_context *requstor_ctx,
                      bool ignore_lock_priority) const;

  inline static MDL_lock *create(const MDL_key *key,
                                 MDL_map_partition *map_part);

  void reschedule_waiters();

  void remove_ticket(Ticket_list MDL_lock::*queue, MDL_ticket *ticket);

  bool visit_subgraph(MDL_ticket *waiting_ticket,
                      MDL_wait_for_graph_visitor *gvisitor);

  virtual bool needs_notification(const MDL_ticket *ticket) const = 0;
  virtual void notify_conflicting_locks(MDL_context *ctx) = 0;

  virtual bitmap_t hog_lock_types_bitmap() const = 0;

  /** List of granted tickets for this lock. */
  Ticket_list m_granted;
  /** Tickets for contexts waiting to acquire a lock. */
  Ticket_list m_waiting;

  /**
    Number of times high priority lock requests have been granted while
    low priority lock requests were waiting.
  */
  ulong m_hog_lock_count;

public:

  MDL_lock(const MDL_key *key_arg, MDL_map_partition *map_part)
  : key(key_arg),
    m_hog_lock_count(0),
    m_ref_usage(0),
    m_ref_release(0),
    m_is_destroyed(FALSE),
    m_version(0),
    m_map_part(map_part)
  {
    mysql_prlock_init(key_MDL_lock_rwlock, &m_rwlock);
  }

  virtual ~MDL_lock()
  {
    mysql_prlock_destroy(&m_rwlock);
  }
  inline static void destroy(MDL_lock *lock);
public:
  /**
    These three members are used to make it possible to separate
    the MDL_map_partition::m_mutex mutex and MDL_lock::m_rwlock in
    MDL_map::find_or_insert() for increased scalability.
    The 'm_is_destroyed' member is only set by destroyers that
    have both the MDL_map_partition::m_mutex and MDL_lock::m_rwlock, thus
    holding any of the mutexes is sufficient to read it.
    The 'm_ref_usage; is incremented under protection by
    MDL_map_partition::m_mutex, but when 'm_is_destroyed' is set to TRUE, this
    member is moved to be protected by the MDL_lock::m_rwlock.
    This means that the MDL_map::find_or_insert() which only
    holds the MDL_lock::m_rwlock can compare it to 'm_ref_release'
    without acquiring MDL_map_partition::m_mutex again and if equal
    it can also destroy the lock object safely.
    The 'm_ref_release' is incremented under protection by
    MDL_lock::m_rwlock.
    Note since we are only interested in equality of these two
    counters we don't have to worry about overflows as long as
    their size is big enough to hold maximum number of concurrent
    threads on the system.
  */
  uint m_ref_usage;
  uint m_ref_release;
  bool m_is_destroyed;
  /**
    We use the same idea and an additional version counter to support
    caching of unused MDL_lock object for further re-use.
    This counter is incremented while holding both MDL_map_partition::m_mutex
    and MDL_lock::m_rwlock locks each time when a MDL_lock is moved from
    the partitioned hash to the paritioned unused objects list (or destroyed).
    A thread, which has found a MDL_lock object for the key in the hash
    and then released the MDL_map_partition::m_mutex before acquiring the
    MDL_lock::m_rwlock, can determine that this object was moved to the
    unused objects list (or destroyed) while it held no locks by comparing
    the version value which it read while holding the MDL_map_partition::m_mutex
    with the value read after acquiring the MDL_lock::m_rwlock.
    Note that since it takes several years to overflow this counter such
    theoretically possible overflows should not have any practical effects.
  */
  ulonglong m_version;
  /**
    Partition of MDL_map where the lock is stored.
  */
  MDL_map_partition *m_map_part;
};


/**
  An implementation of the scoped metadata lock. The only locking modes
  which are supported at the moment are SHARED and INTENTION EXCLUSIVE
  and EXCLUSIVE
*/

class MDL_scoped_lock : public MDL_lock
{
public:
  MDL_scoped_lock(const MDL_key *key_arg, MDL_map_partition *map_part)
    : MDL_lock(key_arg, map_part)
  { }

  virtual const bitmap_t *incompatible_granted_types_bitmap() const
  {
    return m_granted_incompatible;
  }
  virtual const bitmap_t *incompatible_waiting_types_bitmap() const
  {
    return m_waiting_incompatible;
  }
  virtual bool needs_notification(const MDL_ticket *ticket) const
  {
    return (ticket->get_type() == MDL_SHARED);
  }
  virtual void notify_conflicting_locks(MDL_context *ctx);

  /*
    In scoped locks, only IX lock request would starve because of X/S. But that
    is practically very rare case. So just return 0 from this function.
  */
  virtual bitmap_t hog_lock_types_bitmap() const
  {
    return 0;
  }

private:
  static const bitmap_t m_granted_incompatible[MDL_TYPE_END];
  static const bitmap_t m_waiting_incompatible[MDL_TYPE_END];
};


/**
  An implementation of a per-object lock. Supports SHARED, SHARED_UPGRADABLE,
  SHARED HIGH PRIORITY and EXCLUSIVE locks.
*/

class MDL_object_lock : public MDL_lock
{
public:
  MDL_object_lock(const MDL_key *key_arg, MDL_map_partition *map_part)
    : MDL_lock(key_arg, map_part)
  { }

  /**
    Reset unused MDL_object_lock object to represent the lock context for a
    different object.
  */
  void reset(const MDL_key *new_key)
  {
    /* We need to change only object's key. */
    key.mdl_key_init(new_key);
    /* m_granted and m_waiting should be already in the empty/initial state. */
    DBUG_ASSERT(is_empty());
    /* Object should not be marked as destroyed. */
    DBUG_ASSERT(! m_is_destroyed);
    /*
      Values of the rest of the fields should be preserved between old and
      new versions of the object. E.g., m_version and m_ref_usage/release
      should be kept intact to properly handle possible remaining references
      to the old version of the object.
    */
  }

  virtual const bitmap_t *incompatible_granted_types_bitmap() const
  {
    return m_granted_incompatible;
  }
  virtual const bitmap_t *incompatible_waiting_types_bitmap() const
  {
    return m_waiting_incompatible;
  }
  virtual bool needs_notification(const MDL_ticket *ticket) const
  {
    return (ticket->get_type() >= MDL_SHARED_NO_WRITE);
  }
  virtual void notify_conflicting_locks(MDL_context *ctx);

  /*
    To prevent starvation, these lock types that are only granted
    max_write_lock_count times in a row while other lock types are
    waiting.
  */
  virtual bitmap_t hog_lock_types_bitmap() const
  {
    return (MDL_BIT(MDL_SHARED_NO_WRITE) |
            MDL_BIT(MDL_SHARED_NO_READ_WRITE) |
            MDL_BIT(MDL_EXCLUSIVE));
  }

private:
  static const bitmap_t m_granted_incompatible[MDL_TYPE_END];
  static const bitmap_t m_waiting_incompatible[MDL_TYPE_END];

public:
  /** Members for linking the object into the list of unused objects. */
  MDL_object_lock *next_in_cache, **prev_in_cache;
};


/**
  Helper class for linking MDL_object_lock objects into the unused objects list.
*/
class MDL_object_lock_cache_adapter :
      public I_P_List_adapter<MDL_object_lock, &MDL_object_lock::next_in_cache,
                              &MDL_object_lock::prev_in_cache>
{
};


static MDL_map mdl_locks;
/**
  Start-up parameter for the maximum size of the unused MDL_lock objects cache.
*/
ulong mdl_locks_cache_size;


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
*/

void mdl_init()
{
  DBUG_ASSERT(! mdl_initialized);
  mdl_initialized= TRUE;

#ifdef HAVE_PSI_INTERFACE
  init_mdl_psi_keys();
#endif

  mdl_locks.init();
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
    mdl_locks.destroy();
  }
}


/** Initialize the container for all MDL locks. */

void MDL_map::init()
{
  MDL_key global_lock_key(MDL_key::GLOBAL, "", "");
  MDL_key commit_lock_key(MDL_key::COMMIT, "", "");

  m_global_lock= MDL_lock::create(&global_lock_key, NULL);
  m_commit_lock= MDL_lock::create(&commit_lock_key, NULL);

  for (uint i= 0; i < mdl_locks_hash_partitions; i++)
  {
    MDL_map_partition *part= new (std::nothrow) MDL_map_partition();
    m_partitions.append(part);
  }
}


/** Initialize the partition in the container with all MDL locks. */

MDL_map_partition::MDL_map_partition()
{
  mysql_mutex_init(key_MDL_map_mutex, &m_mutex, NULL);
  my_hash_init(&m_locks, &my_charset_bin, 16 /* FIXME */, 0, 0,
               mdl_locks_key, 0, 0);
};


/**
  Destroy the container for all MDL locks.
  @pre It must be empty.
*/

void MDL_map::destroy()
{
  MDL_lock::destroy(m_global_lock);
  MDL_lock::destroy(m_commit_lock);

  while (m_partitions.elements() > 0)
  {
    MDL_map_partition *part= m_partitions.pop();
    delete part;
  }
}


/**
  Destroy the partition in container for all MDL locks.
  @pre It must be empty.
*/

MDL_map_partition::~MDL_map_partition()
{
  DBUG_ASSERT(!m_locks.records);
  mysql_mutex_destroy(&m_mutex);
  my_hash_free(&m_locks);

  MDL_object_lock *lock;
  while ((lock= m_unused_locks_cache.pop_front()))
    MDL_lock::destroy(lock);
}


/**
  Find MDL_lock object corresponding to the key, create it
  if it does not exist.

  @retval non-NULL - Success. MDL_lock instance for the key with
                     locked MDL_lock::m_rwlock.
  @retval NULL     - Failure (OOM).
*/

MDL_lock* MDL_map::find_or_insert(const MDL_key *mdl_key)
{
  MDL_lock *lock;

  if (mdl_key->mdl_namespace() == MDL_key::GLOBAL ||
      mdl_key->mdl_namespace() == MDL_key::COMMIT)
  {
    /*
      Avoid locking any m_mutex when lock for GLOBAL or COMMIT namespace is
      requested. Return pointer to pre-allocated MDL_lock instance instead.
      Such an optimization allows to save one mutex lock/unlock for any
      statement changing data.

      It works since these namespaces contain only one element so keys
      for them look like '<namespace-id>\0\0'.
    */
    DBUG_ASSERT(mdl_key->length() == 3);

    lock= (mdl_key->mdl_namespace() == MDL_key::GLOBAL) ? m_global_lock :
                                                          m_commit_lock;

    mysql_prlock_wrlock(&lock->m_rwlock);

    return lock;
  }

  my_hash_value_type hash_value= m_partitions.at(0)->get_key_hash(mdl_key);
  uint part_id= hash_value % mdl_locks_hash_partitions;
  MDL_map_partition *part= m_partitions.at(part_id);

  return part->find_or_insert(mdl_key, hash_value);
}


/**
  Find MDL_lock object corresponding to the key and hash value in
  MDL_map partition, create it if it does not exist.

  @retval non-NULL - Success. MDL_lock instance for the key with
                     locked MDL_lock::m_rwlock.
  @retval NULL     - Failure (OOM).
*/

MDL_lock* MDL_map_partition::find_or_insert(const MDL_key *mdl_key,
                                            my_hash_value_type hash_value)
{
  MDL_lock *lock;

retry:
  mysql_mutex_lock(&m_mutex);
  if (!(lock= (MDL_lock*) my_hash_search_using_hash_value(&m_locks,
                                                          hash_value,
                                                          mdl_key->ptr(),
                                                          mdl_key->length())))
  {
    MDL_object_lock *unused_lock= NULL;

    /*
      No lock object found so we need to create a new one
      or reuse an existing unused object.
    */
    if (mdl_key->mdl_namespace() != MDL_key::SCHEMA &&
        m_unused_locks_cache.elements())
    {
      /*
        We need a MDL_object_lock type of object and the unused objects
        cache has some. Get the first object from the cache and set a new
        key for it.
      */
      DBUG_ASSERT(mdl_key->mdl_namespace() != MDL_key::GLOBAL &&
                  mdl_key->mdl_namespace() != MDL_key::COMMIT);

      unused_lock= m_unused_locks_cache.pop_front();
      unused_lock->reset(mdl_key);

      lock= unused_lock;
    }
    else
    {
      lock= MDL_lock::create(mdl_key, this);
    }

    if (!lock || my_hash_insert(&m_locks, (uchar*)lock))
    {
      if (unused_lock)
      {
        /*
          Note that we can't easily destroy an object from cache here as it
          still might be referenced by other threads. So we simply put it
          back into the cache.
        */
        m_unused_locks_cache.push_front(unused_lock);
      }
      else
      {
        MDL_lock::destroy(lock);
      }
      mysql_mutex_unlock(&m_mutex);
      return NULL;
    }
  }

  if (move_from_hash_to_lock_mutex(lock))
    goto retry;

  return lock;
}


/**
  Release MDL_map_partition::m_mutex mutex and lock MDL_lock::m_rwlock for lock
  object from the hash. Handle situation when object was released
  while we held no locks.

  @retval FALSE - Success.
  @retval TRUE  - Object was released while we held no mutex, caller
                  should re-try looking up MDL_lock object in the hash.
*/

bool MDL_map_partition::move_from_hash_to_lock_mutex(MDL_lock *lock)
{
  ulonglong version;

  DBUG_ASSERT(! lock->m_is_destroyed);
  mysql_mutex_assert_owner(&m_mutex);

  /*
    We increment m_ref_usage which is a reference counter protected by
    MDL_map_partition::m_mutex under the condition it is present in the hash
    and m_is_destroyed is FALSE.
  */
  lock->m_ref_usage++;
  /* Read value of the version counter under protection of m_mutex lock. */
  version= lock->m_version;
  mysql_mutex_unlock(&m_mutex);

  mysql_prlock_wrlock(&lock->m_rwlock);
  lock->m_ref_release++;

  if (unlikely(lock->m_version != version))
  {
    /*
      If the current value of version differs from one that was read while
      we held m_mutex mutex, this MDL_lock object was moved to the unused
      objects list or destroyed while we held no locks.
      We should retry our search. But first we should destroy the MDL_lock
      object if necessary.
    */
    if (unlikely(lock->m_is_destroyed))
    {
      /*
        Object was released while we held no locks, we need to
        release it if no others hold references to it, while our own
        reference count ensured that the object as such haven't got
        its memory released yet. We can also safely compare
        m_ref_usage and m_ref_release since the object is no longer
        present in the hash (or unused objects list) so no one will
        be able to find it and increment m_ref_usage anymore.
      */
      uint ref_usage= lock->m_ref_usage;
      uint ref_release= lock->m_ref_release;
      mysql_prlock_unlock(&lock->m_rwlock);
      if (ref_usage == ref_release)
        MDL_lock::destroy(lock);
    }
    else
    {
      /*
        Object was not destroyed but its version has changed.
        This means that it was moved to the unused objects list
        (and even might be already re-used). So now it might
        correspond to a different key, therefore we should simply
        retry our search.
      */
      mysql_prlock_unlock(&lock->m_rwlock);
    }
    return TRUE;
  }
  return FALSE;
}


/**
  Destroy MDL_lock object or delegate this responsibility to
  whatever thread that holds the last outstanding reference to
  it.
*/

void MDL_map::remove(MDL_lock *lock)
{
  if (lock->key.mdl_namespace() == MDL_key::GLOBAL ||
      lock->key.mdl_namespace() == MDL_key::COMMIT)
  {
    /*
      Never destroy pre-allocated MDL_lock objects for GLOBAL and
      COMMIT namespaces.
    */
    mysql_prlock_unlock(&lock->m_rwlock);
    return;
  }

  lock->m_map_part->remove(lock);
}


/**
  Destroy MDL_lock object belonging to specific MDL_map
  partition or delegate this responsibility to whatever
  thread that holds the last outstanding reference to it.
*/

void MDL_map_partition::remove(MDL_lock *lock)
{
  mysql_mutex_lock(&m_mutex);
  my_hash_delete(&m_locks, (uchar*) lock);
  /*
    To let threads holding references to the MDL_lock object know that it was
    moved to the list of unused objects or destroyed, we increment the version
    counter under protection of both MDL_map_partition::m_mutex and
    MDL_lock::m_rwlock locks. This allows us to read the version value while
    having either one of those locks.
  */
  lock->m_version++;

  if ((lock->key.mdl_namespace() != MDL_key::SCHEMA) &&
      (m_unused_locks_cache.elements() <
       mdl_locks_cache_size/mdl_locks_hash_partitions))
  {
    /*
      This is an object of MDL_object_lock type and the cache of unused
      objects has not reached its maximum size yet. So instead of destroying
      object we move it to the list of unused objects to allow its later
      re-use with possibly different key. Any threads holding references to
      this object (owning MDL_map_partition::m_mutex or MDL_lock::m_rwlock)
      will notice this thanks to the fact that we have changed the
      MDL_lock::m_version counter.
    */
    DBUG_ASSERT(lock->key.mdl_namespace() != MDL_key::GLOBAL &&
                lock->key.mdl_namespace() != MDL_key::COMMIT);

    m_unused_locks_cache.push_front((MDL_object_lock*)lock);
    mysql_mutex_unlock(&m_mutex);
    mysql_prlock_unlock(&lock->m_rwlock);
  }
  else
  {
    /*
      Destroy the MDL_lock object, but ensure that anyone that is
      holding a reference to the object is not remaining, if so he
      has the responsibility to release it.

      Setting of m_is_destroyed to TRUE while holding _both_
      MDL_map_partition::m_mutex and MDL_lock::m_rwlock mutexes transfers
      the protection of m_ref_usage from MDL_map_partition::m_mutex to
      MDL_lock::m_rwlock while removal of the object from the hash
      (and cache of unused objects) makes it read-only. Therefore
      whoever acquires MDL_lock::m_rwlock next will see the most up
      to date version of m_ref_usage.

      This means that when m_is_destroyed is TRUE and we hold the
      MDL_lock::m_rwlock we can safely read the m_ref_usage
      member.
    */
    uint ref_usage, ref_release;

    lock->m_is_destroyed= TRUE;
    ref_usage= lock->m_ref_usage;
    ref_release= lock->m_ref_release;
    mysql_mutex_unlock(&m_mutex);
    mysql_prlock_unlock(&lock->m_rwlock);
    if (ref_usage == ref_release)
      MDL_lock::destroy(lock);
  }
}


/**
  Initialize a metadata locking context.

  This is to be called when a new server connection is created.
*/

MDL_context::MDL_context()
  :
  m_owner(NULL),
  m_needs_thr_lock_abort(FALSE),
  m_waiting_for(NULL)
{
  mysql_prlock_init(key_MDL_context_LOCK_waiting_for, &m_LOCK_waiting_for);
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
  DBUG_ASSERT(m_tickets[MDL_STATEMENT].is_empty());
  DBUG_ASSERT(m_tickets[MDL_TRANSACTION].is_empty());
  DBUG_ASSERT(m_tickets[MDL_EXPLICIT].is_empty());

  mysql_prlock_destroy(&m_LOCK_waiting_for);
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

void MDL_request::init(MDL_key::enum_mdl_namespace mdl_namespace,
                       const char *db_arg,
                       const char *name_arg,
                       enum_mdl_type mdl_type_arg,
                       enum_mdl_duration mdl_duration_arg)
{
  key.mdl_key_init(mdl_namespace, db_arg, name_arg);
  type= mdl_type_arg;
  duration= mdl_duration_arg;
  ticket= NULL;
}


/**
  Initialize a lock request using pre-built MDL_key.

  @sa MDL_request::init(namespace, db, name, type).

  @param key_arg       The pre-built MDL key for the request.
  @param mdl_type_arg  The MDL lock type for the request.
*/

void MDL_request::init(const MDL_key *key_arg,
                       enum_mdl_type mdl_type_arg,
                       enum_mdl_duration mdl_duration_arg)
{
  key.mdl_key_init(key_arg);
  type= mdl_type_arg;
  duration= mdl_duration_arg;
  ticket= NULL;
}


/**
  Auxiliary functions needed for creation/destruction of MDL_lock objects.

  @note Also chooses an MDL_lock descendant appropriate for object namespace.
*/

inline MDL_lock *MDL_lock::create(const MDL_key *mdl_key,
                                  MDL_map_partition *map_part)
{
  switch (mdl_key->mdl_namespace())
  {
    case MDL_key::GLOBAL:
    case MDL_key::SCHEMA:
    case MDL_key::COMMIT:
      return new (std::nothrow) MDL_scoped_lock(mdl_key, map_part);
    default:
      return new (std::nothrow) MDL_object_lock(mdl_key, map_part);
  }
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

MDL_ticket *MDL_ticket::create(MDL_context *ctx_arg, enum_mdl_type type_arg
#ifndef DBUG_OFF
                               , enum_mdl_duration duration_arg
#endif
                               )
{
  return new (std::nothrow)
             MDL_ticket(ctx_arg, type_arg
#ifndef DBUG_OFF
                        , duration_arg
#endif
                        );
}


void MDL_ticket::destroy(MDL_ticket *ticket)
{
  delete ticket;
}


/**
  Return the 'weight' of this ticket for the
  victim selection algorithm. Requests with 
  lower weight are preferred to requests
  with higher weight when choosing a victim.
*/

uint MDL_ticket::get_deadlock_weight() const
{
  return (m_lock->key.mdl_namespace() == MDL_key::GLOBAL ||
          m_type >= MDL_SHARED_UPGRADABLE ?
          DEADLOCK_WEIGHT_DDL : DEADLOCK_WEIGHT_DML);
}


/** Construct an empty wait slot. */

MDL_wait::MDL_wait()
  :m_wait_status(EMPTY)
{
  mysql_mutex_init(key_MDL_wait_LOCK_wait_status, &m_LOCK_wait_status, NULL);
  mysql_cond_init(key_MDL_wait_COND_wait_status, &m_COND_wait_status, NULL);
}


/** Destroy system resources. */

MDL_wait::~MDL_wait()
{
  mysql_mutex_destroy(&m_LOCK_wait_status);
  mysql_cond_destroy(&m_COND_wait_status);
}


/**
  Set the status unless it's already set. Return FALSE if set,
  TRUE otherwise.
*/

bool MDL_wait::set_status(enum_wait_status status_arg)
{
  bool was_occupied= TRUE;
  mysql_mutex_lock(&m_LOCK_wait_status);
  if (m_wait_status == EMPTY)
  {
    was_occupied= FALSE;
    m_wait_status= status_arg;
    mysql_cond_signal(&m_COND_wait_status);
  }
  mysql_mutex_unlock(&m_LOCK_wait_status);
  return was_occupied;
}


/** Query the current value of the wait slot. */

MDL_wait::enum_wait_status MDL_wait::get_status()
{
  enum_wait_status result;
  mysql_mutex_lock(&m_LOCK_wait_status);
  result= m_wait_status;
  mysql_mutex_unlock(&m_LOCK_wait_status);
  return result;
}


/** Clear the current value of the wait slot. */

void MDL_wait::reset_status()
{
  mysql_mutex_lock(&m_LOCK_wait_status);
  m_wait_status= EMPTY;
  mysql_mutex_unlock(&m_LOCK_wait_status);
}


/**
  Wait for the status to be assigned to this wait slot.

  @param owner           MDL context owner.
  @param abs_timeout     Absolute time after which waiting should stop.
  @param set_status_on_timeout TRUE  - If in case of timeout waiting
                                       context should close the wait slot by
                                       sending TIMEOUT to itself.
                               FALSE - Otherwise.
  @param wait_state_name  Thread state name to be set for duration of wait.

  @returns Signal posted.
*/

MDL_wait::enum_wait_status
MDL_wait::timed_wait(MDL_context_owner *owner, struct timespec *abs_timeout,
                     bool set_status_on_timeout,
                     const PSI_stage_info *wait_state_name)
{
  PSI_stage_info old_stage;
  enum_wait_status result;
  int wait_result= 0;

  mysql_mutex_lock(&m_LOCK_wait_status);

  owner->ENTER_COND(&m_COND_wait_status, &m_LOCK_wait_status,
                    wait_state_name, & old_stage);
  thd_wait_begin(NULL, THD_WAIT_META_DATA_LOCK);
  while (!m_wait_status && !owner->is_killed() &&
         wait_result != ETIMEDOUT && wait_result != ETIME)
  {
    wait_result= mysql_cond_timedwait(&m_COND_wait_status, &m_LOCK_wait_status,
                                      abs_timeout);
  }
  thd_wait_end(NULL);

  if (m_wait_status == EMPTY)
  {
    /*
      Wait has ended not due to a status being set from another
      thread but due to this connection/statement being killed or a
      time out.
      To avoid races, which may occur if another thread sets
      GRANTED status before the code which calls this method
      processes the abort/timeout, we assign the status under
      protection of the m_LOCK_wait_status, within the critical
      section. An exception is when set_status_on_timeout is
      false, which means that the caller intends to restart the
      wait.
    */
    if (owner->is_killed())
      m_wait_status= KILLED;
    else if (set_status_on_timeout)
      m_wait_status= TIMEOUT;
  }
  result= m_wait_status;

  owner->EXIT_COND(& old_stage);

  return result;
}


/**
  Clear bit corresponding to the type of metadata lock in bitmap representing
  set of such types if list of tickets does not contain ticket with such type.

  @param[in,out]  bitmap  Bitmap representing set of types of locks.
  @param[in]      list    List to inspect.
  @param[in]      type    Type of metadata lock to look up in the list.
*/

void MDL_lock::Ticket_list::clear_bit_if_not_in_list(enum_mdl_type type)
{
  MDL_lock::Ticket_iterator it(m_list);
  const MDL_ticket *ticket;

  while ((ticket= it++))
    if (ticket->get_type() == type)
      return;
  m_bitmap&= ~ MDL_BIT(type);
}


/**
  Add ticket to MDL_lock's list of waiting requests and
  update corresponding bitmap of lock types.
*/

void MDL_lock::Ticket_list::add_ticket(MDL_ticket *ticket)
{
  /*
    Ticket being added to the list must have MDL_ticket::m_lock set,
    since for such tickets methods accessing this member might be
    called by other threads.
  */
  DBUG_ASSERT(ticket->get_lock());
  /*
    Add ticket to the *back* of the queue to ensure fairness
    among requests with the same priority.
  */
  m_list.push_back(ticket);
  m_bitmap|= MDL_BIT(ticket->get_type());
}


/**
  Remove ticket from MDL_lock's list of requests and
  update corresponding bitmap of lock types.
*/

void MDL_lock::Ticket_list::remove_ticket(MDL_ticket *ticket)
{
  m_list.remove(ticket);
  /*
    Check if waiting queue has another ticket with the same type as
    one which was removed. If there is no such ticket, i.e. we have
    removed last ticket of particular type, then we need to update
    bitmap of waiting ticket's types.
    Note that in most common case, i.e. when shared lock is removed
    from waiting queue, we are likely to find ticket of the same
    type early without performing full iteration through the list.
    So this method should not be too expensive.
  */
  clear_bit_if_not_in_list(ticket->get_type());
}


/**
  Determine waiting contexts which requests for the lock can be
  satisfied, grant lock to them and wake them up.

  @note Together with MDL_lock::add_ticket() this method implements
        fair scheduling among requests with the same priority.
        It tries to grant lock from the head of waiters list, while
        add_ticket() adds new requests to the back of this list.

*/

void MDL_lock::reschedule_waiters()
{
  MDL_lock::Ticket_iterator it(m_waiting);
  MDL_ticket *ticket;
  bool skip_high_priority= false;
  bitmap_t hog_lock_types= hog_lock_types_bitmap();

  if (m_hog_lock_count >= max_write_lock_count)
  {
    /*
      If number of successively granted high-prio, strong locks has exceeded
      max_write_lock_count give a way to low-prio, weak locks to avoid their
      starvation.
    */

    if ((m_waiting.bitmap() & ~hog_lock_types) != 0)
    {
      /*
        Even though normally when m_hog_lock_count is non-0 there is
        some pending low-prio lock, we still can encounter situation
        when m_hog_lock_count is non-0 and there are no pending low-prio
        locks. This, for example, can happen when a ticket for pending
        low-prio lock was removed from waiters list due to timeout,
        and reschedule_waiters() is called after that to update the
        waiters queue. m_hog_lock_count will be reset to 0 at the
        end of this call in such case.

        Note that it is not an issue if we fail to wake up any pending
        waiters for weak locks in the loop below. This would mean that
        all of them are either killed, timed out or chosen as a victim
        by deadlock resolver, but have not managed to remove ticket
        from the waiters list yet. After tickets will be removed from
        the waiters queue there will be another call to
        reschedule_waiters() with pending bitmap updated to reflect new
        state of waiters queue.
      */
      skip_high_priority= true;
    }
  }

  /*
    Find the first (and hence the oldest) waiting request which
    can be satisfied (taking into account priority). Grant lock to it.
    Repeat the process for the remainder of waiters.
    Note we don't need to re-start iteration from the head of the
    list after satisfying the first suitable request as in our case
    all compatible types of requests have the same priority.

    TODO/FIXME: We should:
                - Either switch to scheduling without priorities
                  which will allow to stop iteration through the
                  list of waiters once we found the first ticket
                  which can't be  satisfied
                - Or implement some check using bitmaps which will
                  allow to stop iteration in cases when, e.g., we
                  grant SNRW lock and there are no pending S or
                  SH locks.
  */
  while ((ticket= it++))
  {
    /*
      Skip high-prio, strong locks if earlier we have decided to give way to
      low-prio, weaker locks.
    */
    if (skip_high_priority &&
        ((MDL_BIT(ticket->get_type()) & hog_lock_types) != 0))
      continue;

    if (can_grant_lock(ticket->get_type(), ticket->get_ctx(),
                       skip_high_priority))
    {
      if (! ticket->get_ctx()->m_wait.set_status(MDL_wait::GRANTED))
      {
        /*
          Satisfy the found request by updating lock structures.
          It is OK to do so even after waking up the waiter since any
          session which tries to get any information about the state of
          this lock has to acquire MDL_lock::m_rwlock first and thus,
          when manages to do so, already sees an updated state of the
          MDL_lock object.
        */
        m_waiting.remove_ticket(ticket);
        m_granted.add_ticket(ticket);

        /*
          Increase counter of successively granted high-priority strong locks,
          if we have granted one.
        */
        if ((MDL_BIT(ticket->get_type()) & hog_lock_types) != 0)
          m_hog_lock_count++;
      }
      /*
        If we could not update the wait slot of the waiter,
        it can be due to fact that its connection/statement was
        killed or it has timed out (i.e. the slot is not empty).
        Since in all such cases the waiter assumes that the lock was
        not been granted, we should keep the request in the waiting
        queue and look for another request to reschedule.
      */
    }
  }

  if ((m_waiting.bitmap() & ~hog_lock_types) == 0)
  {
    /*
      Reset number of successively granted high-prio, strong locks
      if there are no pending low-prio, weak locks.
      This ensures:
      - That m_hog_lock_count is correctly reset after strong lock
      is released and weak locks are granted (or there are no
      other lock requests).
      - That situation when SNW lock is granted along with some SR
      locks, but SW locks are still blocked are handled correctly.
      - That m_hog_lock_count is zero in most cases when there are no pending
      weak locks (see comment at the start of this method for example of
      exception). This allows to save on checks at the start of this method.
    */
    m_hog_lock_count= 0;
  }
}


/**
  Compatibility (or rather "incompatibility") matrices for scoped metadata
  lock. Arrays of bitmaps which elements specify which granted/waiting locks
  are incompatible with type of lock being requested.

  The first array specifies if particular type of request can be satisfied
  if there is granted scoped lock of certain type.

             | Type of active   |
     Request |   scoped lock    |
      type   | IS(*)  IX   S  X |
    ---------+------------------+
    IS       |  +      +   +  + |
    IX       |  +      +   -  - |
    S        |  +      -   +  - |
    X        |  +      -   -  - |

  The second array specifies if particular type of request can be satisfied
  if there is already waiting request for the scoped lock of certain type.
  I.e. it specifies what is the priority of different lock types.

             |    Pending      |
     Request |  scoped lock    |
      type   | IS(*)  IX  S  X |
    ---------+-----------------+
    IS       |  +      +  +  + |
    IX       |  +      +  -  - |
    S        |  +      +  +  - |
    X        |  +      +  +  + |

  Here: "+" -- means that request can be satisfied
        "-" -- means that request can't be satisfied and should wait

  (*)  Since intention shared scoped locks are compatible with all other
       type of locks we don't even have any accounting for them.

  Note that relation between scoped locks and objects locks requested
  by statement is not straightforward and is therefore fully defined
  by SQL-layer.
  For example, in order to support global read lock implementation
  SQL-layer acquires IX lock in GLOBAL namespace for each statement
  that can modify metadata or data (i.e. for each statement that
  needs SW, SU, SNW, SNRW or X object locks). OTOH, to ensure that
  DROP DATABASE works correctly with concurrent DDL, IX metadata locks
  in SCHEMA namespace are acquired for DDL statements which can update
  metadata in the schema (i.e. which acquire SU, SNW, SNRW and X locks
  on schema objects) and aren't acquired for DML.
*/

const MDL_lock::bitmap_t MDL_scoped_lock::m_granted_incompatible[MDL_TYPE_END] =
{
  MDL_BIT(MDL_EXCLUSIVE) | MDL_BIT(MDL_SHARED),
  MDL_BIT(MDL_EXCLUSIVE) | MDL_BIT(MDL_INTENTION_EXCLUSIVE), 0, 0, 0, 0, 0, 0,
  MDL_BIT(MDL_EXCLUSIVE) | MDL_BIT(MDL_SHARED) | MDL_BIT(MDL_INTENTION_EXCLUSIVE)
};

const MDL_lock::bitmap_t MDL_scoped_lock::m_waiting_incompatible[MDL_TYPE_END] =
{
  MDL_BIT(MDL_EXCLUSIVE) | MDL_BIT(MDL_SHARED),
  MDL_BIT(MDL_EXCLUSIVE), 0, 0, 0, 0, 0, 0, 0
};


/**
  Compatibility (or rather "incompatibility") matrices for per-object
  metadata lock. Arrays of bitmaps which elements specify which granted/
  waiting locks are incompatible with type of lock being requested.

  The first array specifies if particular type of request can be satisfied
  if there is granted lock of certain type.

     Request  |  Granted requests for lock       |
      type    | S  SH  SR  SW  SU  SNW  SNRW  X  |
    ----------+----------------------------------+
    S         | +   +   +   +   +   +    +    -  |
    SH        | +   +   +   +   +   +    +    -  |
    SR        | +   +   +   +   +   +    -    -  |
    SW        | +   +   +   +   +   -    -    -  |
    SU        | +   +   +   +   -   -    -    -  |
    SNW       | +   +   +   -   -   -    -    -  |
    SNRW      | +   +   -   -   -   -    -    -  |
    X         | -   -   -   -   -   -    -    -  |
    SU -> X   | -   -   -   -   0   0    0    0  |
    SNW -> X  | -   -   -   0   0   0    0    0  |
    SNRW -> X | -   -   0   0   0   0    0    0  |

  The second array specifies if particular type of request can be satisfied
  if there is waiting request for the same lock of certain type. In other
  words it specifies what is the priority of different lock types.

     Request  |  Pending requests for lock      |
      type    | S  SH  SR  SW  SU  SNW  SNRW  X |
    ----------+---------------------------------+
    S         | +   +   +   +   +   +     +   - |
    SH        | +   +   +   +   +   +     +   + |
    SR        | +   +   +   +   +   +     -   - |
    SW        | +   +   +   +   +   -     -   - |
    SU        | +   +   +   +   +   +     +   - |
    SNW       | +   +   +   +   +   +     +   - |
    SNRW      | +   +   +   +   +   +     +   - |
    X         | +   +   +   +   +   +     +   + |
    SU -> X   | +   +   +   +   +   +     +   + |
    SNW -> X  | +   +   +   +   +   +     +   + |
    SNRW -> X | +   +   +   +   +   +     +   + |

  Here: "+" -- means that request can be satisfied
        "-" -- means that request can't be satisfied and should wait
        "0" -- means impossible situation which will trigger assert

  @note In cases then current context already has "stronger" type
        of lock on the object it will be automatically granted
        thanks to usage of the MDL_context::find_ticket() method.

  @note IX locks are excluded since they are not used for per-object
        metadata locks.
*/

const MDL_lock::bitmap_t
MDL_object_lock::m_granted_incompatible[MDL_TYPE_END] =
{
  0,
  MDL_BIT(MDL_EXCLUSIVE),
  MDL_BIT(MDL_EXCLUSIVE),
  MDL_BIT(MDL_EXCLUSIVE) | MDL_BIT(MDL_SHARED_NO_READ_WRITE),
  MDL_BIT(MDL_EXCLUSIVE) | MDL_BIT(MDL_SHARED_NO_READ_WRITE) |
    MDL_BIT(MDL_SHARED_NO_WRITE),
  MDL_BIT(MDL_EXCLUSIVE) | MDL_BIT(MDL_SHARED_NO_READ_WRITE) |
    MDL_BIT(MDL_SHARED_NO_WRITE) | MDL_BIT(MDL_SHARED_UPGRADABLE),
  MDL_BIT(MDL_EXCLUSIVE) | MDL_BIT(MDL_SHARED_NO_READ_WRITE) |
    MDL_BIT(MDL_SHARED_NO_WRITE) | MDL_BIT(MDL_SHARED_UPGRADABLE) |
    MDL_BIT(MDL_SHARED_WRITE),
  MDL_BIT(MDL_EXCLUSIVE) | MDL_BIT(MDL_SHARED_NO_READ_WRITE) |
    MDL_BIT(MDL_SHARED_NO_WRITE) | MDL_BIT(MDL_SHARED_UPGRADABLE) |
    MDL_BIT(MDL_SHARED_WRITE) | MDL_BIT(MDL_SHARED_READ),
  MDL_BIT(MDL_EXCLUSIVE) | MDL_BIT(MDL_SHARED_NO_READ_WRITE) |
    MDL_BIT(MDL_SHARED_NO_WRITE) | MDL_BIT(MDL_SHARED_UPGRADABLE) |
    MDL_BIT(MDL_SHARED_WRITE) | MDL_BIT(MDL_SHARED_READ) |
    MDL_BIT(MDL_SHARED_HIGH_PRIO) | MDL_BIT(MDL_SHARED)
};


const MDL_lock::bitmap_t
MDL_object_lock::m_waiting_incompatible[MDL_TYPE_END] =
{
  0,
  MDL_BIT(MDL_EXCLUSIVE),
  0,
  MDL_BIT(MDL_EXCLUSIVE) | MDL_BIT(MDL_SHARED_NO_READ_WRITE),
  MDL_BIT(MDL_EXCLUSIVE) | MDL_BIT(MDL_SHARED_NO_READ_WRITE) |
    MDL_BIT(MDL_SHARED_NO_WRITE),
  MDL_BIT(MDL_EXCLUSIVE),
  MDL_BIT(MDL_EXCLUSIVE),
  MDL_BIT(MDL_EXCLUSIVE),
  0
};


/**
  Check if request for the metadata lock can be satisfied given its
  current state.

  @param  type_arg             The requested lock type.
  @param  requestor_ctx        The MDL context of the requestor.
  @param  ignore_lock_priority Ignore lock priority.

  @retval TRUE   Lock request can be satisfied
  @retval FALSE  There is some conflicting lock.

  @note In cases then current context already has "stronger" type
        of lock on the object it will be automatically granted
        thanks to usage of the MDL_context::find_ticket() method.
*/

bool
MDL_lock::can_grant_lock(enum_mdl_type type_arg,
                         MDL_context *requestor_ctx,
                         bool ignore_lock_priority) const
{
  bool can_grant= FALSE;
  bitmap_t waiting_incompat_map= incompatible_waiting_types_bitmap()[type_arg];
  bitmap_t granted_incompat_map= incompatible_granted_types_bitmap()[type_arg];

  /*
    New lock request can be satisfied iff:
    - There are no incompatible types of satisfied requests
    in other contexts
    - There are no waiting requests which have higher priority
    than this request when priority was not ignored.
  */
  if (ignore_lock_priority || !(m_waiting.bitmap() & waiting_incompat_map))
  {
    if (! (m_granted.bitmap() & granted_incompat_map))
      can_grant= TRUE;
    else
    {
      Ticket_iterator it(m_granted);
      MDL_ticket *ticket;

      /* Check that the incompatible lock belongs to some other context. */
      while ((ticket= it++))
      {
        if (ticket->get_ctx() != requestor_ctx &&
            ticket->is_incompatible_when_granted(type_arg))
          break;
      }
      if (ticket == NULL)             /* Incompatible locks are our own. */
        can_grant= TRUE;
    }
  }
  return can_grant;
}


/** Remove a ticket from waiting or pending queue and wakeup up waiters. */

void MDL_lock::remove_ticket(Ticket_list MDL_lock::*list, MDL_ticket *ticket)
{
  mysql_prlock_wrlock(&m_rwlock);
  (this->*list).remove_ticket(ticket);
  if (is_empty())
    mdl_locks.remove(this);
  else
  {
    /*
      There can be some contexts waiting to acquire a lock
      which now might be able to do it. Grant the lock to
      them and wake them up!

      We always try to reschedule locks, since there is no easy way
      (i.e. by looking at the bitmaps) to find out whether it is
      required or not.
      In a general case, even when the queue's bitmap is not changed
      after removal of the ticket, there is a chance that some request
      can be satisfied (due to the fact that a granted request
      reflected in the bitmap might belong to the same context as a
      pending request).
    */
    reschedule_waiters();
    mysql_prlock_unlock(&m_rwlock);
  }
}


/**
  Check if we have any pending locks which conflict with existing
  shared lock.

  @pre The ticket must match an acquired lock.

  @return TRUE if there is a conflicting lock request, FALSE otherwise.
*/

bool MDL_lock::has_pending_conflicting_lock(enum_mdl_type type)
{
  bool result;

  mysql_mutex_assert_not_owner(&LOCK_open);

  mysql_prlock_rdlock(&m_rwlock);
  result= (m_waiting.bitmap() & incompatible_granted_types_bitmap()[type]);
  mysql_prlock_unlock(&m_rwlock);
  return result;
}


MDL_wait_for_graph_visitor::~MDL_wait_for_graph_visitor()
{
}


MDL_wait_for_subgraph::~MDL_wait_for_subgraph()
{
}

/**
  Check if ticket represents metadata lock of "stronger" or equal type
  than specified one. I.e. if metadata lock represented by ticket won't
  allow any of locks which are not allowed by specified type of lock.

  @return TRUE  if ticket has stronger or equal type
          FALSE otherwise.
*/

bool MDL_ticket::has_stronger_or_equal_type(enum_mdl_type type) const
{
  const MDL_lock::bitmap_t *
    granted_incompat_map= m_lock->incompatible_granted_types_bitmap();

  return ! (granted_incompat_map[type] & ~(granted_incompat_map[m_type]));
}


bool MDL_ticket::is_incompatible_when_granted(enum_mdl_type type) const
{
  return (MDL_BIT(m_type) &
          m_lock->incompatible_granted_types_bitmap()[type]);
}


bool MDL_ticket::is_incompatible_when_waiting(enum_mdl_type type) const
{
  return (MDL_BIT(m_type) &
          m_lock->incompatible_waiting_types_bitmap()[type]);
}


/**
  Check whether the context already holds a compatible lock ticket
  on an object.
  Start searching from list of locks for the same duration as lock
  being requested. If not look at lists for other durations.

  @param mdl_request  Lock request object for lock to be acquired
  @param[out] result_duration  Duration of lock which was found.

  @note Tickets which correspond to lock types "stronger" than one
        being requested are also considered compatible.

  @return A pointer to the lock ticket for the object or NULL otherwise.
*/

MDL_ticket *
MDL_context::find_ticket(MDL_request *mdl_request,
                         enum_mdl_duration *result_duration)
{
  MDL_ticket *ticket;
  int i;

  for (i= 0; i < MDL_DURATION_END; i++)
  {
    enum_mdl_duration duration= (enum_mdl_duration)((mdl_request->duration+i) %
                                                    MDL_DURATION_END);
    Ticket_iterator it(m_tickets[duration]);

    while ((ticket= it++))
    {
      if (mdl_request->key.is_equal(&ticket->m_lock->key) &&
          ticket->has_stronger_or_equal_type(mdl_request->type))
      {
        *result_duration= duration;
        return ticket;
      }
    }
  }
  return NULL;
}


/**
  Try to acquire one lock.

  Unlike exclusive locks, shared locks are acquired one by
  one. This is interface is chosen to simplify introduction of
  the new locking API to the system. MDL_context::try_acquire_lock()
  is currently used from open_table(), and there we have only one
  table to work with.

  This function may also be used to try to acquire an exclusive
  lock on a destination table, by ALTER TABLE ... RENAME.

  Returns immediately without any side effect if encounters a lock
  conflict. Otherwise takes the lock.

  FIXME: Compared to lock_table_name_if_not_cached() (from 5.1)
         it gives slightly more false negatives.

  @param mdl_request [in/out] Lock request object for lock to be acquired

  @retval  FALSE   Success. The lock may have not been acquired.
                   Check the ticket, if it's NULL, a conflicting lock
                   exists.
  @retval  TRUE    Out of resources, an error has been reported.
*/

bool
MDL_context::try_acquire_lock(MDL_request *mdl_request)
{
  MDL_ticket *ticket;

  if (try_acquire_lock_impl(mdl_request, &ticket))
    return TRUE;

  if (! mdl_request->ticket)
  {
    /*
      Our attempt to acquire lock without waiting has failed.
      Let us release resources which were acquired in the process.
      We can't get here if we allocated a new lock object so there
      is no need to release it.
    */
    DBUG_ASSERT(! ticket->m_lock->is_empty());
    mysql_prlock_unlock(&ticket->m_lock->m_rwlock);
    MDL_ticket::destroy(ticket);
  }

  return FALSE;
}


/**
  Auxiliary method for acquiring lock without waiting.

  @param mdl_request [in/out] Lock request object for lock to be acquired
  @param out_ticket  [out]    Ticket for the request in case when lock
                              has not been acquired.

  @retval  FALSE   Success. The lock may have not been acquired.
                   Check MDL_request::ticket, if it's NULL, a conflicting
                   lock exists. In this case "out_ticket" out parameter
                   points to ticket which was constructed for the request.
                   MDL_ticket::m_lock points to the corresponding MDL_lock
                   object and MDL_lock::m_rwlock write-locked.
  @retval  TRUE    Out of resources, an error has been reported.
*/

bool
MDL_context::try_acquire_lock_impl(MDL_request *mdl_request,
                                   MDL_ticket **out_ticket)
{
  MDL_lock *lock;
  MDL_key *key= &mdl_request->key;
  MDL_ticket *ticket;
  enum_mdl_duration found_duration;

  DBUG_ASSERT(mdl_request->type != MDL_EXCLUSIVE ||
              is_lock_owner(MDL_key::GLOBAL, "", "", MDL_INTENTION_EXCLUSIVE));
  DBUG_ASSERT(mdl_request->ticket == NULL);

  /* Don't take chances in production. */
  mdl_request->ticket= NULL;
  mysql_mutex_assert_not_owner(&LOCK_open);

  /*
    Check whether the context already holds a shared lock on the object,
    and if so, grant the request.
  */
  if ((ticket= find_ticket(mdl_request, &found_duration)))
  {
    DBUG_ASSERT(ticket->m_lock);
    DBUG_ASSERT(ticket->has_stronger_or_equal_type(mdl_request->type));
    /*
      If the request is for a transactional lock, and we found
      a transactional lock, just reuse the found ticket.

      It's possible that we found a transactional lock,
      but the request is for a HANDLER lock. In that case HANDLER
      code will clone the ticket (see below why it's needed).

      If the request is for a transactional lock, and we found
      a HANDLER lock, create a copy, to make sure that when user
      does HANDLER CLOSE, the transactional lock is not released.

      If the request is for a handler lock, and we found a
      HANDLER lock, also do the clone. HANDLER CLOSE for one alias
      should not release the lock on the table HANDLER opened through
      a different alias.
    */
    mdl_request->ticket= ticket;
    if ((found_duration != mdl_request->duration ||
         mdl_request->duration == MDL_EXPLICIT) &&
        clone_ticket(mdl_request))
    {
      /* Clone failed. */
      mdl_request->ticket= NULL;
      return TRUE;
    }
    return FALSE;
  }

  if (!(ticket= MDL_ticket::create(this, mdl_request->type
#ifndef DBUG_OFF
                                   , mdl_request->duration
#endif
                                   )))
    return TRUE;

  /* The below call implicitly locks MDL_lock::m_rwlock on success. */
  if (!(lock= mdl_locks.find_or_insert(key)))
  {
    MDL_ticket::destroy(ticket);
    return TRUE;
  }

  ticket->m_lock= lock;

  if (lock->can_grant_lock(mdl_request->type, this, false))
  {
    lock->m_granted.add_ticket(ticket);

    mysql_prlock_unlock(&lock->m_rwlock);

    m_tickets[mdl_request->duration].push_front(ticket);

    mdl_request->ticket= ticket;
  }
  else
    *out_ticket= ticket;

  return FALSE;
}


/**
  Create a copy of a granted ticket.
  This is used to make sure that HANDLER ticket
  is never shared with a ticket that belongs to
  a transaction, so that when we HANDLER CLOSE,
  we don't release a transactional ticket, and
  vice versa -- when we COMMIT, we don't mistakenly
  release a ticket for an open HANDLER.

  @retval TRUE   Out of memory.
  @retval FALSE  Success.
*/

bool
MDL_context::clone_ticket(MDL_request *mdl_request)
{
  MDL_ticket *ticket;

  mysql_mutex_assert_not_owner(&LOCK_open);
  /*
    By submitting mdl_request->type to MDL_ticket::create()
    we effectively downgrade the cloned lock to the level of
    the request.
  */
  if (!(ticket= MDL_ticket::create(this, mdl_request->type
#ifndef DBUG_OFF
                                   , mdl_request->duration
#endif
                                   )))
    return TRUE;

  /* clone() is not supposed to be used to get a stronger lock. */
  DBUG_ASSERT(mdl_request->ticket->has_stronger_or_equal_type(ticket->m_type));

  ticket->m_lock= mdl_request->ticket->m_lock;
  mdl_request->ticket= ticket;

  mysql_prlock_wrlock(&ticket->m_lock->m_rwlock);
  ticket->m_lock->m_granted.add_ticket(ticket);
  mysql_prlock_unlock(&ticket->m_lock->m_rwlock);

  m_tickets[mdl_request->duration].push_front(ticket);

  return FALSE;
}


/**
  Notify threads holding a shared metadata locks on object which
  conflict with a pending X, SNW or SNRW lock.

  @param  ctx  MDL_context for current thread.
*/

void MDL_object_lock::notify_conflicting_locks(MDL_context *ctx)
{
  Ticket_iterator it(m_granted);
  MDL_ticket *conflicting_ticket;

  while ((conflicting_ticket= it++))
  {
    /* Only try to abort locks on which we back off. */
    if (conflicting_ticket->get_ctx() != ctx &&
        conflicting_ticket->get_type() < MDL_SHARED_UPGRADABLE)

    {
      MDL_context *conflicting_ctx= conflicting_ticket->get_ctx();

      /*
        If thread which holds conflicting lock is waiting on table-level
        lock or some other non-MDL resource we might need to wake it up
        by calling code outside of MDL.
      */
      ctx->get_owner()->
        notify_shared_lock(conflicting_ctx->get_owner(),
                           conflicting_ctx->get_needs_thr_lock_abort());
    }
  }
}


/**
  Notify threads holding scoped IX locks which conflict with a pending S lock.

  @param  ctx  MDL_context for current thread.
*/

void MDL_scoped_lock::notify_conflicting_locks(MDL_context *ctx)
{
  Ticket_iterator it(m_granted);
  MDL_ticket *conflicting_ticket;

  while ((conflicting_ticket= it++))
  {
    if (conflicting_ticket->get_ctx() != ctx &&
        conflicting_ticket->get_type() == MDL_INTENTION_EXCLUSIVE)

    {
      MDL_context *conflicting_ctx= conflicting_ticket->get_ctx();

      /*
        Thread which holds global IX lock can be a handler thread for
        insert delayed. We need to kill such threads in order to get
        global shared lock. We do this my calling code outside of MDL.
      */
      ctx->get_owner()->
        notify_shared_lock(conflicting_ctx->get_owner(),
                           conflicting_ctx->get_needs_thr_lock_abort());
    }
  }
}


/**
  Acquire one lock with waiting for conflicting locks to go away if needed.

  @param mdl_request [in/out] Lock request object for lock to be acquired

  @param lock_wait_timeout [in] Seconds to wait before timeout.

  @retval  FALSE   Success. MDL_request::ticket points to the ticket
                   for the lock.
  @retval  TRUE    Failure (Out of resources or waiting is aborted),
*/

bool
MDL_context::acquire_lock(MDL_request *mdl_request, ulong lock_wait_timeout)
{
  MDL_lock *lock;
  MDL_ticket *ticket;
  struct timespec abs_timeout;
  MDL_wait::enum_wait_status wait_status;
  /* Do some work outside the critical section. */
  set_timespec(abs_timeout, lock_wait_timeout);

  if (try_acquire_lock_impl(mdl_request, &ticket))
    return TRUE;

  if (mdl_request->ticket)
  {
    /*
      We have managed to acquire lock without waiting.
      MDL_lock, MDL_context and MDL_request were updated
      accordingly, so we can simply return success.
    */
    return FALSE;
  }

  /*
    Our attempt to acquire lock without waiting has failed.
    As a result of this attempt we got MDL_ticket with m_lock
    member pointing to the corresponding MDL_lock object which
    has MDL_lock::m_rwlock write-locked.
  */
  lock= ticket->m_lock;

  lock->m_waiting.add_ticket(ticket);

  /*
    Once we added a pending ticket to the waiting queue,
    we must ensure that our wait slot is empty, so
    that our lock request can be scheduled. Do that in the
    critical section formed by the acquired write lock on MDL_lock.
  */
  m_wait.reset_status();

  if (lock->needs_notification(ticket))
    lock->notify_conflicting_locks(this);

  mysql_prlock_unlock(&lock->m_rwlock);

  will_wait_for(ticket);

  /* There is a shared or exclusive lock on the object. */
  DEBUG_SYNC(get_thd(), "mdl_acquire_lock_wait");

  find_deadlock();

  if (lock->needs_notification(ticket))
  {
    struct timespec abs_shortwait;
    set_timespec(abs_shortwait, 1);
    wait_status= MDL_wait::EMPTY;

    while (cmp_timespec(abs_shortwait, abs_timeout) <= 0)
    {
      /* abs_timeout is far away. Wait a short while and notify locks. */
      wait_status= m_wait.timed_wait(m_owner, &abs_shortwait, FALSE,
                                     mdl_request->key.get_wait_state_name());

      if (wait_status != MDL_wait::EMPTY)
        break;

      mysql_prlock_wrlock(&lock->m_rwlock);
      lock->notify_conflicting_locks(this);
      mysql_prlock_unlock(&lock->m_rwlock);
      set_timespec(abs_shortwait, 1);
    }
    if (wait_status == MDL_wait::EMPTY)
      wait_status= m_wait.timed_wait(m_owner, &abs_timeout, TRUE,
                                     mdl_request->key.get_wait_state_name());
  }
  else
    wait_status= m_wait.timed_wait(m_owner, &abs_timeout, TRUE,
                                   mdl_request->key.get_wait_state_name());

  done_waiting_for();

  if (wait_status != MDL_wait::GRANTED)
  {
    lock->remove_ticket(&MDL_lock::m_waiting, ticket);
    MDL_ticket::destroy(ticket);
    switch (wait_status)
    {
    case MDL_wait::VICTIM:
      my_error(ER_LOCK_DEADLOCK, MYF(0));
      break;
    case MDL_wait::TIMEOUT:
      my_error(ER_LOCK_WAIT_TIMEOUT, MYF(0));
      break;
    case MDL_wait::KILLED:
      break;
    default:
      DBUG_ASSERT(0);
      break;
    }
    return TRUE;
  }

  /*
    We have been granted our request.
    State of MDL_lock object is already being appropriately updated by a
    concurrent thread (@sa MDL_lock:reschedule_waiters()).
    So all we need to do is to update MDL_context and MDL_request objects.
  */
  DBUG_ASSERT(wait_status == MDL_wait::GRANTED);

  m_tickets[mdl_request->duration].push_front(ticket);

  mdl_request->ticket= ticket;

  return FALSE;
}


extern "C" int mdl_request_ptr_cmp(const void* ptr1, const void* ptr2)
{
  MDL_request *req1= *(MDL_request**)ptr1;
  MDL_request *req2= *(MDL_request**)ptr2;
  return req1->key.cmp(&req2->key);
}


/**
  Acquire exclusive locks. There must be no granted locks in the
  context.

  This is a replacement of lock_table_names(). It is used in
  RENAME, DROP and other DDL SQL statements.

  @param  mdl_requests  List of requests for locks to be acquired.

  @param lock_wait_timeout  Seconds to wait before timeout.

  @note The list of requests should not contain non-exclusive lock requests.
        There should not be any acquired locks in the context.

  @note Assumes that one already owns scoped intention exclusive lock.

  @retval FALSE  Success
  @retval TRUE   Failure
*/

bool MDL_context::acquire_locks(MDL_request_list *mdl_requests,
                                ulong lock_wait_timeout)
{
  MDL_request_list::Iterator it(*mdl_requests);
  MDL_request **sort_buf, **p_req;
  MDL_savepoint mdl_svp= mdl_savepoint();
  ssize_t req_count= static_cast<ssize_t>(mdl_requests->elements());

  if (req_count == 0)
    return FALSE;

  /* Sort requests according to MDL_key. */
  if (! (sort_buf= (MDL_request **)my_malloc(req_count *
                                             sizeof(MDL_request*),
                                             MYF(MY_WME))))
    return TRUE;

  for (p_req= sort_buf; p_req < sort_buf + req_count; p_req++)
    *p_req= it++;

  my_qsort(sort_buf, req_count, sizeof(MDL_request*),
           mdl_request_ptr_cmp);

  for (p_req= sort_buf; p_req < sort_buf + req_count; p_req++)
  {
    if (acquire_lock(*p_req, lock_wait_timeout))
      goto err;
  }
  my_free(sort_buf);
  return FALSE;

err:
  /*
    Release locks we have managed to acquire so far.
    Use rollback_to_savepoint() since there may be duplicate
    requests that got assigned the same ticket.
  */
  rollback_to_savepoint(mdl_svp);
  /* Reset lock requests back to its initial state. */
  for (req_count= p_req - sort_buf, p_req= sort_buf;
       p_req < sort_buf + req_count; p_req++)
  {
    (*p_req)->ticket= NULL;
  }
  my_free(sort_buf);
  return TRUE;
}


/**
  Upgrade a shared metadata lock.

  Used in ALTER TABLE.

  @param mdl_ticket         Lock to upgrade.
  @param new_type           Lock type to upgrade to.
  @param lock_wait_timeout  Seconds to wait before timeout.

  @note In case of failure to upgrade lock (e.g. because upgrader
        was killed) leaves lock in its original state (locked in
        shared mode).

  @note There can be only one upgrader for a lock or we will have deadlock.
        This invariant is ensured by the fact that upgradeable locks SU, SNW
        and SNRW are not compatible with each other and themselves.

  @retval FALSE  Success
  @retval TRUE   Failure (thread was killed)
*/

bool
MDL_context::upgrade_shared_lock(MDL_ticket *mdl_ticket,
                                 enum_mdl_type new_type,
                                 ulong lock_wait_timeout)
{
  MDL_request mdl_xlock_request;
  MDL_savepoint mdl_svp= mdl_savepoint();
  bool is_new_ticket;

  DBUG_ENTER("MDL_context::upgrade_shared_lock");
  DEBUG_SYNC(get_thd(), "mdl_upgrade_lock");

  /*
    Do nothing if already upgraded. Used when we FLUSH TABLE under
    LOCK TABLES and a table is listed twice in LOCK TABLES list.
  */
  if (mdl_ticket->has_stronger_or_equal_type(new_type))
    DBUG_RETURN(FALSE);

  /* Only allow upgrades from SHARED_UPGRADABLE/NO_WRITE/NO_READ_WRITE */
  DBUG_ASSERT(mdl_ticket->m_type == MDL_SHARED_UPGRADABLE ||
              mdl_ticket->m_type == MDL_SHARED_NO_WRITE ||
              mdl_ticket->m_type == MDL_SHARED_NO_READ_WRITE);

  mdl_xlock_request.init(&mdl_ticket->m_lock->key, new_type,
                         MDL_TRANSACTION);

  if (acquire_lock(&mdl_xlock_request, lock_wait_timeout))
    DBUG_RETURN(TRUE);

  is_new_ticket= ! has_lock(mdl_svp, mdl_xlock_request.ticket);

  /* Merge the acquired and the original lock. @todo: move to a method. */
  mysql_prlock_wrlock(&mdl_ticket->m_lock->m_rwlock);
  if (is_new_ticket)
    mdl_ticket->m_lock->m_granted.remove_ticket(mdl_xlock_request.ticket);
  /*
    Set the new type of lock in the ticket. To update state of
    MDL_lock object correctly we need to temporarily exclude
    ticket from the granted queue and then include it back.
  */
  mdl_ticket->m_lock->m_granted.remove_ticket(mdl_ticket);
  mdl_ticket->m_type= new_type;
  mdl_ticket->m_lock->m_granted.add_ticket(mdl_ticket);

  mysql_prlock_unlock(&mdl_ticket->m_lock->m_rwlock);

  if (is_new_ticket)
  {
    m_tickets[MDL_TRANSACTION].remove(mdl_xlock_request.ticket);
    MDL_ticket::destroy(mdl_xlock_request.ticket);
  }

  DBUG_RETURN(FALSE);
}


/**
  A fragment of recursive traversal of the wait-for graph
  in search for deadlocks. Direct the deadlock visitor to all
  contexts that own the lock the current node in the wait-for
  graph is waiting for.
  As long as the initial node is remembered in the visitor,
  a deadlock is found when the same node is seen twice.
*/

bool MDL_lock::visit_subgraph(MDL_ticket *waiting_ticket,
                              MDL_wait_for_graph_visitor *gvisitor)
{
  MDL_ticket *ticket;
  MDL_context *src_ctx= waiting_ticket->get_ctx();
  bool result= TRUE;

  mysql_prlock_rdlock(&m_rwlock);

  /* Must be initialized after taking a read lock. */
  Ticket_iterator granted_it(m_granted);
  Ticket_iterator waiting_it(m_waiting);

  /*
    MDL_lock's waiting and granted queues and MDL_context::m_waiting_for
    member are updated by different threads when the lock is granted
    (see MDL_context::acquire_lock() and MDL_lock::reschedule_waiters()).
    As a result, here we may encounter a situation when MDL_lock data
    already reflects the fact that the lock was granted but
    m_waiting_for member has not been updated yet.

    For example, imagine that:

    thread1: Owns SNW lock on table t1.
    thread2: Attempts to acquire SW lock on t1,
             but sees an active SNW lock.
             Thus adds the ticket to the waiting queue and
             sets m_waiting_for to point to the ticket.
    thread1: Releases SNW lock, updates MDL_lock object to
             grant SW lock to thread2 (moves the ticket for
             SW from waiting to the active queue).
             Attempts to acquire a new SNW lock on t1,
             sees an active SW lock (since it is present in the
             active queue), adds ticket for SNW lock to the waiting
             queue, sets m_waiting_for to point to this ticket.

    At this point deadlock detection algorithm run by thread1 will see that:
    - Thread1 waits for SNW lock on t1 (since m_waiting_for is set).
    - SNW lock is not granted, because it conflicts with active SW lock
      owned by thread 2 (since ticket for SW is present in granted queue).
    - Thread2 waits for SW lock (since its m_waiting_for has not been
      updated yet!).
    - SW lock is not granted because there is pending SNW lock from thread1.
      Therefore deadlock should exist [sic!].

    To avoid detection of such false deadlocks we need to check the "actual"
    status of the ticket being waited for, before analyzing its blockers.
    We do this by checking the wait status of the context which is waiting
    for it. To avoid races this has to be done under protection of
    MDL_lock::m_rwlock lock.
  */
  if (src_ctx->m_wait.get_status() != MDL_wait::EMPTY)
  {
    result= FALSE;
    goto end;
  }

  /*
    To avoid visiting nodes which were already marked as victims of
    deadlock detection (or whose requests were already satisfied) we
    enter the node only after peeking at its wait status.
    This is necessary to avoid active waiting in a situation
    when previous searches for a deadlock already selected the
    node we're about to enter as a victim (see the comment
    in MDL_context::find_deadlock() for explanation why several searches
    can be performed for the same wait).
    There is no guarantee that the node isn't chosen a victim while we
    are visiting it but this is OK: in the worst case we might do some
    extra work and one more context might be chosen as a victim.
  */
  if (gvisitor->enter_node(src_ctx))
    goto end;

  /*
    We do a breadth-first search first -- that is, inspect all
    edges of the current node, and only then follow up to the next
    node. In workloads that involve wait-for graph loops this
    has proven to be a more efficient strategy [citation missing].
  */
  while ((ticket= granted_it++))
  {
    /* Filter out edges that point to the same node. */
    if (ticket->get_ctx() != src_ctx &&
        ticket->is_incompatible_when_granted(waiting_ticket->get_type()) &&
        gvisitor->inspect_edge(ticket->get_ctx()))
    {
      goto end_leave_node;
    }
  }

  while ((ticket= waiting_it++))
  {
    /* Filter out edges that point to the same node. */
    if (ticket->get_ctx() != src_ctx &&
        ticket->is_incompatible_when_waiting(waiting_ticket->get_type()) &&
        gvisitor->inspect_edge(ticket->get_ctx()))
    {
      goto end_leave_node;
    }
  }

  /* Recurse and inspect all adjacent nodes. */
  granted_it.rewind();
  while ((ticket= granted_it++))
  {
    if (ticket->get_ctx() != src_ctx &&
        ticket->is_incompatible_when_granted(waiting_ticket->get_type()) &&
        ticket->get_ctx()->visit_subgraph(gvisitor))
    {
      goto end_leave_node;
    }
  }

  waiting_it.rewind();
  while ((ticket= waiting_it++))
  {
    if (ticket->get_ctx() != src_ctx &&
        ticket->is_incompatible_when_waiting(waiting_ticket->get_type()) &&
        ticket->get_ctx()->visit_subgraph(gvisitor))
    {
      goto end_leave_node;
    }
  }

  result= FALSE;

end_leave_node:
  gvisitor->leave_node(src_ctx);

end:
  mysql_prlock_unlock(&m_rwlock);
  return result;
}


/**
  Traverse a portion of wait-for graph which is reachable
  through the edge represented by this ticket and search
  for deadlocks.

  @retval TRUE  A deadlock is found. A pointer to deadlock
                 victim is saved in the visitor.
  @retval FALSE
*/

bool MDL_ticket::accept_visitor(MDL_wait_for_graph_visitor *gvisitor)
{
  return m_lock->visit_subgraph(this, gvisitor);
}


/**
  A fragment of recursive traversal of the wait-for graph of
  MDL contexts in the server in search for deadlocks.
  Assume this MDL context is a node in the wait-for graph,
  and direct the visitor to all adjacent nodes. As long
  as the starting node is remembered in the visitor, a
  deadlock is found when the same node is visited twice.
  One MDL context is connected to another in the wait-for
  graph if it waits on a resource that is held by the other
  context.

  @retval TRUE  A deadlock is found. A pointer to deadlock
                victim is saved in the visitor.
  @retval FALSE
*/

bool MDL_context::visit_subgraph(MDL_wait_for_graph_visitor *gvisitor)
{
  bool result= FALSE;

  mysql_prlock_rdlock(&m_LOCK_waiting_for);

  if (m_waiting_for)
    result= m_waiting_for->accept_visitor(gvisitor);

  mysql_prlock_unlock(&m_LOCK_waiting_for);

  return result;
}


/**
  Try to find a deadlock. This function produces no errors.

  @note If during deadlock resolution context which performs deadlock
        detection is chosen as a victim it will be informed about the
        fact by setting VICTIM status to its wait slot.

  @retval TRUE  A deadlock is found.
  @retval FALSE No deadlock found.
*/

void MDL_context::find_deadlock()
{
  while (1)
  {
    /*
      The fact that we use fresh instance of gvisitor for each
      search performed by find_deadlock() below is important,
      the code responsible for victim selection relies on this.
    */
    Deadlock_detection_visitor dvisitor(this);
    MDL_context *victim;

    if (! visit_subgraph(&dvisitor))
    {
      /* No deadlocks are found! */
      break;
    }

    victim= dvisitor.get_victim();

    /*
      Failure to change status of the victim is OK as it means
      that the victim has received some other message and is
      about to stop its waiting/to break deadlock loop.
      Even when the initiator of the deadlock search is
      chosen the victim, we need to set the respective wait
      result in order to "close" it for any attempt to
      schedule the request.
      This is needed to avoid a possible race during
      cleanup in case when the lock request on which the
      context was waiting is concurrently satisfied.
    */
    (void) victim->m_wait.set_status(MDL_wait::VICTIM);
    victim->unlock_deadlock_victim();

    if (victim == this)
      break;
    /*
      After adding a new edge to the waiting graph we found that it
      creates a loop (i.e. there is a deadlock). We decided to destroy
      this loop by removing an edge, but not the one that we added.
      Since this doesn't guarantee that all loops created by addition
      of the new edge are destroyed, we have to repeat the search.
    */
  }
}


/**
  Release lock.

  @param duration Lock duration.
  @param ticket   Ticket for lock to be released.

*/

void MDL_context::release_lock(enum_mdl_duration duration, MDL_ticket *ticket)
{
  MDL_lock *lock= ticket->m_lock;
  DBUG_ENTER("MDL_context::release_lock");
  DBUG_PRINT("enter", ("db=%s name=%s", lock->key.db_name(),
                                        lock->key.name()));

  DBUG_ASSERT(this == ticket->get_ctx());
  mysql_mutex_assert_not_owner(&LOCK_open);

  lock->remove_ticket(&MDL_lock::m_granted, ticket);

  m_tickets[duration].remove(ticket);
  MDL_ticket::destroy(ticket);

  DBUG_VOID_RETURN;
}


/**
  Release lock with explicit duration.

  @param ticket   Ticket for lock to be released.

*/

void MDL_context::release_lock(MDL_ticket *ticket)
{
  DBUG_ASSERT(ticket->m_duration == MDL_EXPLICIT);

  release_lock(MDL_EXPLICIT, ticket);
}


/**
  Release all locks associated with the context. If the sentinel
  is not NULL, do not release locks stored in the list after and
  including the sentinel.

  Statement and transactional locks are added to the beginning of
  the corresponding lists, i.e. stored in reverse temporal order.
  This allows to employ this function to:
  - back off in case of a lock conflict.
  - release all locks in the end of a statment or transaction
  - rollback to a savepoint.
*/

void MDL_context::release_locks_stored_before(enum_mdl_duration duration,
                                              MDL_ticket *sentinel)
{
  MDL_ticket *ticket;
  Ticket_iterator it(m_tickets[duration]);
  DBUG_ENTER("MDL_context::release_locks_stored_before");

  if (m_tickets[duration].is_empty())
    DBUG_VOID_RETURN;

  while ((ticket= it++) && ticket != sentinel)
  {
    DBUG_PRINT("info", ("found lock to release ticket=%p", ticket));
    release_lock(duration, ticket);
  }

  DBUG_VOID_RETURN;
}


/**
  Release all explicit locks in the context which correspond to the
  same name/object as this lock request.

  @param ticket    One of the locks for the name/object for which all
                   locks should be released.
*/

void MDL_context::release_all_locks_for_name(MDL_ticket *name)
{
  /* Use MDL_ticket::m_lock to identify other locks for the same object. */
  MDL_lock *lock= name->m_lock;

  /* Remove matching lock tickets from the context. */
  MDL_ticket *ticket;
  Ticket_iterator it_ticket(m_tickets[MDL_EXPLICIT]);

  while ((ticket= it_ticket++))
  {
    DBUG_ASSERT(ticket->m_lock);
    if (ticket->m_lock == lock)
      release_lock(MDL_EXPLICIT, ticket);
  }
}


/**
  Downgrade an EXCLUSIVE or SHARED_NO_WRITE lock to shared metadata lock.

  @param type  Type of lock to which exclusive lock should be downgraded.
*/

void MDL_ticket::downgrade_lock(enum_mdl_type type)
{
  mysql_mutex_assert_not_owner(&LOCK_open);

  /*
    Do nothing if already downgraded. Used when we FLUSH TABLE under
    LOCK TABLES and a table is listed twice in LOCK TABLES list.
    Note that this code might even try to "downgrade" a weak lock
    (e.g. SW) to a stronger one (e.g SNRW). So we can't even assert
    here that target lock is weaker than existing lock.
  */
  if (m_type == type || !has_stronger_or_equal_type(type))
    return;

  /* Only allow downgrade from EXCLUSIVE and SHARED_NO_WRITE. */
  DBUG_ASSERT(m_type == MDL_EXCLUSIVE ||
              m_type == MDL_SHARED_NO_WRITE);

  mysql_prlock_wrlock(&m_lock->m_rwlock);
  /*
    To update state of MDL_lock object correctly we need to temporarily
    exclude ticket from the granted queue and then include it back.
  */
  m_lock->m_granted.remove_ticket(this);
  m_type= type;
  m_lock->m_granted.add_ticket(this);
  m_lock->reschedule_waiters();
  mysql_prlock_unlock(&m_lock->m_rwlock);
}


/**
  Auxiliary function which allows to check if we have some kind of lock on
  a object. Returns TRUE if we have a lock of a given or stronger type.

  @param mdl_namespace Id of object namespace
  @param db            Name of the database
  @param name          Name of the object
  @param mdl_type      Lock type. Pass in the weakest type to find
                       out if there is at least some lock.

  @return TRUE if current context contains satisfied lock for the object,
          FALSE otherwise.
*/

bool
MDL_context::is_lock_owner(MDL_key::enum_mdl_namespace mdl_namespace,
                           const char *db, const char *name,
                           enum_mdl_type mdl_type)
{
  MDL_request mdl_request;
  enum_mdl_duration not_unused;
  /* We don't care about exact duration of lock here. */
  mdl_request.init(mdl_namespace, db, name, mdl_type, MDL_TRANSACTION);
  MDL_ticket *ticket= find_ticket(&mdl_request, &not_unused);

  DBUG_ASSERT(ticket == NULL || ticket->m_lock);

  return ticket;
}


/**
  Check if we have any pending locks which conflict with existing shared lock.

  @pre The ticket must match an acquired lock.

  @return TRUE if there is a conflicting lock request, FALSE otherwise.
*/

bool MDL_ticket::has_pending_conflicting_lock() const
{
  return m_lock->has_pending_conflicting_lock(m_type);
}


/**
  Releases metadata locks that were acquired after a specific savepoint.

  @note Used to release tickets acquired during a savepoint unit.
  @note It's safe to iterate and unlock any locks after taken after this
        savepoint because other statements that take other special locks
        cause a implicit commit (ie LOCK TABLES).
*/

void MDL_context::rollback_to_savepoint(const MDL_savepoint &mdl_savepoint)
{
  DBUG_ENTER("MDL_context::rollback_to_savepoint");

  /* If savepoint is NULL, it is from the start of the transaction. */
  release_locks_stored_before(MDL_STATEMENT, mdl_savepoint.m_stmt_ticket);
  release_locks_stored_before(MDL_TRANSACTION, mdl_savepoint.m_trans_ticket);

  DBUG_VOID_RETURN;
}


/**
  Release locks acquired by normal statements (SELECT, UPDATE,
  DELETE, etc) in the course of a transaction. Do not release
  HANDLER locks, if there are any.

  This method is used at the end of a transaction, in
  implementation of COMMIT (implicit or explicit) and ROLLBACK.
*/

void MDL_context::release_transactional_locks()
{
  DBUG_ENTER("MDL_context::release_transactional_locks");
  release_locks_stored_before(MDL_STATEMENT, NULL);
  release_locks_stored_before(MDL_TRANSACTION, NULL);
  DBUG_VOID_RETURN;
}


void MDL_context::release_statement_locks()
{
  DBUG_ENTER("MDL_context::release_transactional_locks");
  release_locks_stored_before(MDL_STATEMENT, NULL);
  DBUG_VOID_RETURN;
}


/**
  Does this savepoint have this lock?

  @retval TRUE  The ticket is older than the savepoint or
                is an LT, HA or GLR ticket. Thus it belongs
                to the savepoint or has explicit duration.
  @retval FALSE The ticket is newer than the savepoint.
                and is not an LT, HA or GLR ticket.
*/

bool MDL_context::has_lock(const MDL_savepoint &mdl_savepoint,
                           MDL_ticket *mdl_ticket)
{
  MDL_ticket *ticket;
  /* Start from the beginning, most likely mdl_ticket's been just acquired. */
  MDL_context::Ticket_iterator s_it(m_tickets[MDL_STATEMENT]);
  MDL_context::Ticket_iterator t_it(m_tickets[MDL_TRANSACTION]);

  while ((ticket= s_it++) && ticket != mdl_savepoint.m_stmt_ticket)
  {
    if (ticket == mdl_ticket)
      return FALSE;
  }

  while ((ticket= t_it++) && ticket != mdl_savepoint.m_trans_ticket)
  {
    if (ticket == mdl_ticket)
      return FALSE;
  }
  return TRUE;
}


/**
  Change lock duration for transactional lock.

  @param ticket   Ticket representing lock.
  @param duration Lock duration to be set.

  @note This method only supports changing duration of
        transactional lock to some other duration.
*/

void MDL_context::set_lock_duration(MDL_ticket *mdl_ticket,
                                    enum_mdl_duration duration)
{
  DBUG_ASSERT(mdl_ticket->m_duration == MDL_TRANSACTION &&
              duration != MDL_TRANSACTION);

  m_tickets[MDL_TRANSACTION].remove(mdl_ticket);
  m_tickets[duration].push_front(mdl_ticket);
#ifndef DBUG_OFF
  mdl_ticket->m_duration= duration;
#endif
}


/**
  Set explicit duration for all locks in the context.
*/

void MDL_context::set_explicit_duration_for_all_locks()
{
  int i;
  MDL_ticket *ticket;

  /*
    In the most common case when this function is called list
    of transactional locks is bigger than list of locks with
    explicit duration. So we start by swapping these two lists
    and then move elements from new list of transactional
    locks and list of statement locks to list of locks with
    explicit duration.
  */

  m_tickets[MDL_EXPLICIT].swap(m_tickets[MDL_TRANSACTION]);

  for (i= 0; i < MDL_EXPLICIT; i++)
  {
    Ticket_iterator it_ticket(m_tickets[i]);

    while ((ticket= it_ticket++))
    {
      m_tickets[i].remove(ticket);
      m_tickets[MDL_EXPLICIT].push_front(ticket);
    }
  }

#ifndef DBUG_OFF
  Ticket_iterator exp_it(m_tickets[MDL_EXPLICIT]);

  while ((ticket= exp_it++))
    ticket->m_duration= MDL_EXPLICIT;
#endif
}


/**
  Set transactional duration for all locks in the context.
*/

void MDL_context::set_transaction_duration_for_all_locks()
{
  MDL_ticket *ticket;

  /*
    In the most common case when this function is called list
    of explicit locks is bigger than two other lists (in fact,
    list of statement locks is always empty). So we start by
    swapping list of explicit and transactional locks and then
    move contents of new list of explicit locks to list of
    locks with transactional duration.
  */

  DBUG_ASSERT(m_tickets[MDL_STATEMENT].is_empty());

  m_tickets[MDL_TRANSACTION].swap(m_tickets[MDL_EXPLICIT]);

  Ticket_iterator it_ticket(m_tickets[MDL_EXPLICIT]);

  while ((ticket= it_ticket++))
  {
    m_tickets[MDL_EXPLICIT].remove(ticket);
    m_tickets[MDL_TRANSACTION].push_front(ticket);
  }

#ifndef DBUG_OFF
  Ticket_iterator trans_it(m_tickets[MDL_TRANSACTION]);

  while ((ticket= trans_it++))
    ticket->m_duration= MDL_TRANSACTION;
#endif
}

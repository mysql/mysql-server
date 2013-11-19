/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:

#ifndef TokuDB_cachetable_internal_h
#define TokuDB_cachetable_internal_h

#ident "$Id$"
/*
COPYING CONDITIONS NOTICE:

  This program is free software; you can redistribute it and/or modify
  it under the terms of version 2 of the GNU General Public License as
  published by the Free Software Foundation, and provided that the
  following conditions are met:

      * Redistributions of source code must retain this COPYING
        CONDITIONS NOTICE, the COPYRIGHT NOTICE (below), the
        DISCLAIMER (below), the UNIVERSITY PATENT NOTICE (below), the
        PATENT MARKING NOTICE (below), and the PATENT RIGHTS
        GRANT (below).

      * Redistributions in binary form must reproduce this COPYING
        CONDITIONS NOTICE, the COPYRIGHT NOTICE (below), the
        DISCLAIMER (below), the UNIVERSITY PATENT NOTICE (below), the
        PATENT MARKING NOTICE (below), and the PATENT RIGHTS
        GRANT (below) in the documentation and/or other materials
        provided with the distribution.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
  02110-1301, USA.

COPYRIGHT NOTICE:

  TokuDB, Tokutek Fractal Tree Indexing Library.
  Copyright (C) 2007-2013 Tokutek, Inc.

DISCLAIMER:

  This program is distributed in the hope that it will be useful, but
  WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  General Public License for more details.

UNIVERSITY PATENT NOTICE:

  The technology is licensed by the Massachusetts Institute of
  Technology, Rutgers State University of New Jersey, and the Research
  Foundation of State University of New York at Stony Brook under
  United States of America Serial No. 11/760379 and to the patents
  and/or patent applications resulting from it.

PATENT MARKING NOTICE:

  This software is covered by US Patent No. 8,185,551.
  This software is covered by US Patent No. 8,489,638.

PATENT RIGHTS GRANT:

  "THIS IMPLEMENTATION" means the copyrightable works distributed by
  Tokutek as part of the Fractal Tree project.

  "PATENT CLAIMS" means the claims of patents that are owned or
  licensable by Tokutek, both currently or in the future; and that in
  the absence of this license would be infringed by THIS
  IMPLEMENTATION or by using or running THIS IMPLEMENTATION.

  "PATENT CHALLENGE" shall mean a challenge to the validity,
  patentability, enforceability and/or non-infringement of any of the
  PATENT CLAIMS or otherwise opposing any of the PATENT CLAIMS.

  Tokutek hereby grants to you, for the term and geographical scope of
  the PATENT CLAIMS, a non-exclusive, no-charge, royalty-free,
  irrevocable (except as stated in this section) patent license to
  make, have made, use, offer to sell, sell, import, transfer, and
  otherwise run, modify, and propagate the contents of THIS
  IMPLEMENTATION, where such license applies only to the PATENT
  CLAIMS.  This grant does not include claims that would be infringed
  only as a consequence of further modifications of THIS
  IMPLEMENTATION.  If you or your agent or licensee institute or order
  or agree to the institution of patent litigation against any entity
  (including a cross-claim or counterclaim in a lawsuit) alleging that
  THIS IMPLEMENTATION constitutes direct or contributory patent
  infringement, or inducement of patent infringement, then any rights
  granted to you under this License shall terminate as of the date
  such litigation is filed.  If you or your agent or exclusive
  licensee institute or order or agree to the institution of a PATENT
  CHALLENGE, then Tokutek may terminate any rights granted to you
  under this License.
*/

#ident "Copyright (c) 2007-2013 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."

#include "background_job_manager.h"
#include <portability/toku_random.h>
#include <util/frwlock.h>
#include <util/kibbutz.h>
#include <util/nb_mutex.h>
#include <util/partitioned_counter.h>

//////////////////////////////////////////////////////////////////////////////
//
// This file contains the classes and structs that make up the cachetable.
// The structs are:
//  - cachefile
//  - ctpair
//  - pair_list
//  - cachefile_list
//  - checkpointer
//  - evictor
//  - cleaner
//
// The rest of this comment assumes familiarity with the locks used in these
// classes/structs and what the locks protect. Nevertheless, here is 
// a list of the locks that we have:
//  - pair_list->list_lock
//  - pair_list->pending_lock_expensive
//  - pair_list->pending_lock_cheap
//  - cachefile_list->lock
//  - PAIR->mutex
//  - PAIR->value_rwlock
//  - PAIR->disk_nb_mutex
//
// Here are rules for how the locks interact:
//  - To grab any of the pair_list's locks, or the cachefile_list's lock,
//      the cachetable must be in existence
//  - To grab the PAIR mutex, we must know the PAIR will not dissappear:
//   - the PAIR must be pinned (value_rwlock or disk_nb_mutex is held)
//   - OR, the pair_list's list lock is held
//  - As a result, to get rid of a PAIR from the pair_list, we must hold
//     both the pair_list's list_lock and the PAIR's mutex
//  - To grab PAIR->value_rwlock, we must hold the PAIR's mutex
//  - To grab PAIR->disk_nb_mutex, we must hold the PAIR's mutex
//      and hold PAIR->value_rwlock
//
// Now let's talk about ordering. Here is an order from outer to inner (top locks must be grabbed first)
//  - pair_list->pending_lock_expensive
//  - pair_list->list_lock
//  - cachefile_list->lock
//  - PAIR->mutex
//  - pair_list->pending_lock_cheap <-- after grabbing this lock, 
//                                      NO other locks 
//                                      should be grabbed.
//  - when grabbing PAIR->value_rwlock or PAIR->disk_nb_mutex,
//     if the acquisition will not block, then it does not matter if any other locks held,
//     BUT if the acquisition will block, then NO other locks may be held besides
//     PAIR->mutex.
// 
// HERE ARE TWO EXAMPLES:
// To pin a PAIR on a client thread, the following must be done:
//  - first grab the list lock and find the PAIR
//  - with the list lock grabbed, grab PAIR->mutex
//  - with PAIR->mutex held:
//   - release list lock
//   - pin PAIR
//   - with PAIR pinned, grab pending_lock_cheap,
//   - copy and clear PAIR->checkpoint_pending,
//   - resolve checkpointing if necessary
//   - return to user.
//  The list lock may be held while pinning the PAIR if 
//  the PAIR has no contention. Otherwise, we may have
//  get a deadlock with another thread that has the PAIR pinned,
//  tries to pin some other PAIR, and in doing so, grabs the list lock.
//
// To unpin a PAIR on a client thread:
//  - because the PAIR is pinned, we don't need the pair_list's list_lock
//  - so, simply acquire PAIR->mutex
//  - unpin the PAIR
//  - return
//
//////////////////////////////////////////////////////////////////////////////
class evictor;
class pair_list;

///////////////////////////////////////////////////////////////////////////////
//
// Maps to a file on disk.
//
struct cachefile {
    CACHEFILE next;
    CACHEFILE prev;
    // these next two fields are protected by cachetable's list lock
    // they are managed whenever we add or remove a pair from
    // the cachetable. As of Riddler, this linked list is only used to
    // make cachetable_flush_cachefile more efficient
    PAIR cf_head; // doubly linked list that is NOT circular
    uint32_t num_pairs; // count on number of pairs in the cachetable belong to this cachefile

    bool for_checkpoint; //True if part of the in-progress checkpoint

    // If set and the cachefile closes, the file will be removed.
    // Clients must not operate on the cachefile after setting this,
    // nor attempt to open any cachefile with the same fname (dname)
    // until this cachefile has been fully closed and unlinked.
    bool unlink_on_close;
    int fd;       /* Bug: If a file is opened read-only, then it is stuck in read-only.  If it is opened read-write, then subsequent writers can write to it too. */
    CACHETABLE cachetable;
    struct fileid fileid;
    // the filenum is used as an identifer of the cachefile
    // for logging and recovery
    FILENUM filenum;
    // number used to generate hashes for blocks in the cachefile
    // used in toku_cachetable_hash
    // this used to be the filenum.fileid, but now it is separate
    uint32_t hash_id;
    char *fname_in_env; /* Used for logging */

    void *userdata;
    void (*log_fassociate_during_checkpoint)(CACHEFILE cf, void *userdata); // When starting a checkpoint we must log all open files.
    void (*close_userdata)(CACHEFILE cf, int fd, void *userdata, bool lsnvalid, LSN); // when closing the last reference to a cachefile, first call this function. 
    void (*free_userdata)(CACHEFILE cf, void *userdata); // when closing the last reference to a cachefile, first call this function. 
    void (*begin_checkpoint_userdata)(LSN lsn_of_checkpoint, void *userdata); // before checkpointing cachefiles call this function.
    void (*checkpoint_userdata)(CACHEFILE cf, int fd, void *userdata); // when checkpointing a cachefile, call this function.
    void (*end_checkpoint_userdata)(CACHEFILE cf, int fd, void *userdata); // after checkpointing cachefiles call this function.
    void (*note_pin_by_checkpoint)(CACHEFILE cf, void *userdata); // add a reference to the userdata to prevent it from being removed from memory
    void (*note_unpin_by_checkpoint)(CACHEFILE cf, void *userdata); // add a reference to the userdata to prevent it from being removed from memory
    BACKGROUND_JOB_MANAGER bjm;
};


///////////////////////////////////////////////////////////////////////////////
//
//  The pair represents the data stored in the cachetable.
//
struct ctpair {
    // these fields are essentially constants. They do not change.
    CACHEFILE cachefile;
    CACHEKEY key;
    uint32_t fullhash;
    CACHETABLE_FLUSH_CALLBACK flush_callback;
    CACHETABLE_PARTIAL_EVICTION_EST_CALLBACK pe_est_callback;
    CACHETABLE_PARTIAL_EVICTION_CALLBACK pe_callback;
    CACHETABLE_CLEANER_CALLBACK cleaner_callback;
    CACHETABLE_CLONE_CALLBACK clone_callback;
    CACHETABLE_CHECKPOINT_COMPLETE_CALLBACK checkpoint_complete_callback;
    void *write_extraargs;

    // access to these fields are protected by disk_nb_mutex
    void* cloned_value_data; // cloned copy of value_data used for checkpointing
    long cloned_value_size; // size of cloned_value_data, used for accounting of size_current
    void* disk_data; // data used to fetch/flush value_data to and from disk.

    // access to these fields are protected by value_rwlock
    void* value_data; // data used by client threads, FTNODEs and ROLLBACK_LOG_NODEs
    PAIR_ATTR attr;
    enum cachetable_dirty dirty;

    // protected by PAIR->mutex
    uint32_t count;        // clock count
    uint32_t refcount; // if > 0, then this PAIR is referenced by
                       // callers to the cachetable, and therefore cannot 
                       // be evicted
    uint32_t num_waiting_on_refs; // number of threads waiting on refcount to go to zero
    toku_cond_t refcount_wait; // cond used to wait for refcount to go to zero

    // locks
    toku::frwlock value_rwlock;
    struct nb_mutex disk_nb_mutex; // single writer, protects disk_data, is used for writing cloned nodes for checkpoint
    toku_mutex_t* mutex; // gotten from the pair list

    // Access to checkpoint_pending is protected by two mechanisms,
    // the value_rwlock and the pair_list's pending locks (expensive and cheap).
    // checkpoint_pending may be true of false. 
    // Here are the rules for reading/modifying this bit.
    //  - To transition this field from false to true during begin_checkpoint,
    //   we must be holding both of the pair_list's pending locks.
    //  - To transition this field from true to false during end_checkpoint,
    //   we must be holding the value_rwlock.
    //  - For a non-checkpoint thread to read the value, we must hold both the
    //   value_rwlock and one of the pair_list's pending locks
    //  - For the checkpoint thread to read the value, we must 
    //   hold the value_rwlock
    //
    bool checkpoint_pending; // If this is on, then we have got to resolve checkpointing modifying it.

    // these are variables that are only used to transfer information to background threads
    // we cache them here to avoid a malloc. In the future, we should investigate if this
    // is necessary, as having these fields here is not technically necessary
    long size_evicting_estimate;
    evictor* ev;
    pair_list* list;

    // A PAIR is stored in a pair_list (which happens to be PAIR->list).
    // These variables are protected by the list lock in the pair_list
    //
    // clock_next,clock_prev represent a circular doubly-linked list.
    PAIR clock_next,clock_prev; // In clock.
    PAIR hash_chain;

    // pending_next,pending_next represent a non-circular doubly-linked list.
    PAIR pending_next;
    PAIR pending_prev;

    // cf_next, cf_prev represent a non-circular doubly-linked list.
    // entries in linked list for PAIRs in a cachefile, these are protected
    // by the list lock of the PAIR's pair_list. They are used to make
    // cachetable_flush_cachefile cheaper so that we don't need
    // to search the entire cachetable to find a particular cachefile's
    // PAIRs
    PAIR cf_next;
    PAIR cf_prev;
};

//
// This initializes the fields and members of the pair.
//
void pair_init(PAIR p,
    CACHEFILE cachefile,
    CACHEKEY key,
    void *value,
    PAIR_ATTR attr,
    enum cachetable_dirty dirty,
    uint32_t fullhash,
    CACHETABLE_WRITE_CALLBACK write_callback,
    evictor *ev,
    pair_list *list);


///////////////////////////////////////////////////////////////////////////////
//
//  The pair list maintains the set of PAIR's that make up
//  the cachetable.
//
class pair_list {
public:
    //
    // the following fields are protected by the list lock
    // 
    uint32_t m_n_in_table; // number of pairs in the hash table
    uint32_t m_table_size; // number of buckets in the hash table
    uint32_t m_num_locks;
    PAIR *m_table; // hash table
    toku_mutex_aligned_t *m_mutexes; 
    // 
    // The following fields are the heads of various linked lists.
    // They also protected by the list lock, but their 
    // usage is not as straightforward. For each of them,
    // only ONE thread is allowed iterate over them with 
    // a read lock on the list lock. All other threads
    // that want to modify elements in the lists or iterate over
    // the lists must hold the write list lock. Here is the
    // association between what threads may hold a read lock
    // on the list lock while iterating:
    //  - clock_head -> eviction thread (evictor)
    //  - cleaner_head -> cleaner thread (cleaner)
    //  - pending_head -> checkpoint thread (checkpointer)
    //
    PAIR m_clock_head; // of clock . head is the next thing to be up for decrement. 
    PAIR m_cleaner_head; // for cleaner thread. head is the next thing to look at for possible cleaning.
    PAIR m_checkpoint_head; // for begin checkpoint to iterate over PAIRs and mark as pending_checkpoint
    PAIR m_pending_head; // list of pairs marked with checkpoint_pending

    // this field is public so we are still POD

    // usage of this lock is described above
    toku_pthread_rwlock_t m_list_lock;
    //
    // these locks are the "pending locks" referenced 
    // in comments about PAIR->checkpoint_pending. There
    // are two of them, but both serve the same purpose, which
    // is to protect the transition of a PAIR's checkpoint pending
    // value from false to true during begin_checkpoint.
    // We use two locks, because threads that want to read the
    // checkpoint_pending value may hold a lock for varying periods of time.
    // Threads running eviction may need to protect checkpoint_pending
    // while writing a node to disk, which is an expensive operation,
    // so it uses pending_lock_expensive. Client threads that
    // want to pin PAIRs will want to protect checkpoint_pending
    // just long enough to read the value and wipe it out. This is
    // a cheap operation, and as a result, uses pending_lock_cheap.
    //
    // By having two locks, and making begin_checkpoint first 
    // grab pending_lock_expensive and then pending_lock_cheap,
    // we ensure that threads that want to pin nodes can grab
    // only pending_lock_cheap, and never block behind threads
    // holding pending_lock_expensive and writing a node out to disk
    //
    toku_pthread_rwlock_t m_pending_lock_expensive;
    toku_pthread_rwlock_t m_pending_lock_cheap;
    void init();
    void destroy();
    void evict_completely(PAIR pair);
    void evict_from_cachetable(PAIR pair);
    void evict_from_cachefile(PAIR pair);
    void add_to_cachetable_only(PAIR p);
    void put(PAIR pair);
    PAIR find_pair(CACHEFILE file, CACHEKEY key, uint32_t hash);
    void pending_pairs_remove (PAIR p);
    void verify();
    void get_state(int *num_entries, int *hash_size);
    void read_list_lock();
    void read_list_unlock();
    void write_list_lock();
    void write_list_unlock();
    void read_pending_exp_lock();
    void read_pending_exp_unlock();
    void write_pending_exp_lock();
    void write_pending_exp_unlock();
    void read_pending_cheap_lock();
    void read_pending_cheap_unlock();
    void write_pending_cheap_lock();
    void write_pending_cheap_unlock();
    toku_mutex_t* get_mutex_for_pair(uint32_t fullhash);
    void pair_lock_by_fullhash(uint32_t fullhash);
    void pair_unlock_by_fullhash(uint32_t fullhash);

private:
    void pair_remove (PAIR p);
    void remove_from_hash_chain(PAIR p);
    void add_to_cf_list (PAIR p);
    void add_to_clock (PAIR p);
    void add_to_hash_chain(PAIR p);
};

///////////////////////////////////////////////////////////////////////////////
//
// Wrapper for the head of our cachefile list.
//
class cachefile_list {
public:
    void init();
    void destroy();
    void read_lock();
    void read_unlock();
    void write_lock();
    void write_unlock();
    int cachefile_of_iname_in_env(const char *iname_in_env, CACHEFILE *cf);
    int cachefile_of_filenum(FILENUM filenum, CACHEFILE *cf);
    void add_cf_unlocked(CACHEFILE newcf);
    void add_stale_cf(CACHEFILE newcf);
    void remove_cf(CACHEFILE cf);
    void remove_stale_cf_unlocked(CACHEFILE cf);
    FILENUM reserve_filenum();
    uint32_t get_new_hash_id_unlocked();
    CACHEFILE find_cachefile_unlocked(struct fileid* fileid);
    CACHEFILE find_stale_cachefile_unlocked(struct fileid* fileid);
    void verify_unused_filenum(FILENUM filenum);
    bool evict_some_stale_pair(evictor* ev);
    void free_stale_data(evictor* ev);
    // access to these fields are protected by the lock
    CACHEFILE m_active_head; // head of CACHEFILEs that are active
    CACHEFILE m_stale_head; // head of CACHEFILEs that are stale
    CACHEFILE m_stale_tail; // tail of CACHEFILEs that are stale
    FILENUM m_next_filenum_to_use;
    uint32_t m_next_hash_id_to_use;
    toku_pthread_rwlock_t m_lock; // this field is publoc so we are still POD
private:    
    CACHEFILE find_cachefile_in_list_unlocked(CACHEFILE start, struct fileid* fileid);
};


///////////////////////////////////////////////////////////////////////////////
//
//  The checkpointer handles starting and finishing checkpoints of the 
//  cachetable's data.
//
class checkpointer {
public:
    void init(pair_list *_pl, TOKULOGGER _logger, evictor *_ev, cachefile_list *files);
    void destroy();
    void set_checkpoint_period(uint32_t new_period);
    uint32_t get_checkpoint_period();
    int shutdown();
    bool has_been_shutdown();
    void begin_checkpoint();
    void add_background_job();
    void remove_background_job();
    void end_checkpoint(void (*testcallback_f)(void*),  void* testextra);
    TOKULOGGER get_logger();
    // used during begin_checkpoint
    void increment_num_txns();
private:
    uint32_t m_checkpoint_num_txns;   // how many transactions are in the checkpoint
    TOKULOGGER m_logger;
    LSN m_lsn_of_checkpoint_in_progress;
    uint32_t m_checkpoint_num_files; // how many cachefiles are in the checkpoint
    struct minicron m_checkpointer_cron; // the periodic checkpointing thread
    cachefile_list *m_cf_list;
    pair_list *m_list;
    evictor *m_ev;
    
    // variable used by the checkpoint thread to know
    // when all work induced by cloning on client threads is done
    BACKGROUND_JOB_MANAGER m_checkpoint_clones_bjm;
    // private methods for begin_checkpoint    
    void update_cachefiles();
    void log_begin_checkpoint();
    void turn_on_pending_bits();
    // private methods for end_checkpoint    
    void fill_checkpoint_cfs(CACHEFILE* checkpoint_cfs);
    void checkpoint_pending_pairs();
    void checkpoint_userdata(CACHEFILE* checkpoint_cfs);
    void log_end_checkpoint();
    void end_checkpoint_userdata(CACHEFILE* checkpoint_cfs);
    void remove_cachefiles(CACHEFILE* checkpoint_cfs);
    
    // Unit test struct needs access to private members.
    friend struct checkpointer_test;
};

//
// This is how often we want the eviction thread
// to run, in seconds.
//
const int EVICTION_PERIOD = 1;

///////////////////////////////////////////////////////////////////////////////
//
// The evictor handles the removal of pairs from the pair list/cachetable.
//
class evictor {
public:
    void init(long _size_limit, pair_list* _pl, cachefile_list* _cf_list, KIBBUTZ _kibbutz, uint32_t eviction_period);
    void destroy();
    void add_pair_attr(PAIR_ATTR attr);
    void remove_pair_attr(PAIR_ATTR attr);    
    void change_pair_attr(PAIR_ATTR old_attr, PAIR_ATTR new_attr);
    void add_to_size_current(long size);
    void remove_from_size_current(long size);
    uint64_t reserve_memory(double fraction, uint64_t upper_bound);
    void release_reserved_memory(uint64_t reserved_memory);
    void run_eviction_thread();
    void do_partial_eviction(PAIR p, bool pair_mutex_held);
    void evict_pair(PAIR p, bool checkpoint_pending);
    void wait_for_cache_pressure_to_subside();
    void signal_eviction_thread();
    bool should_client_thread_sleep();
    bool should_client_wake_eviction_thread();
    // function needed for testing
    void get_state(long *size_current_ptr, long *size_limit_ptr);
    void fill_engine_status();
private:
    void run_eviction();
    bool run_eviction_on_pair(PAIR p);
    void try_evict_pair(PAIR p);
    void decrease_size_evicting(long size_evicting_estimate);
    bool should_sleeping_clients_wakeup();
    bool eviction_needed();

    // We have some intentional races with these variables because we're ok with reading something a little bit old.
    // Provide some hooks for reading variables in an unsafe way so that there are function names we can stick in a valgrind suppression.
    int64_t unsafe_read_size_current(void) const;
    int64_t unsafe_read_size_evicting(void) const;

    pair_list* m_pl;
    cachefile_list* m_cf_list;
    int64_t m_size_current;            // the sum of the sizes of the pairs in the cachetable
    // changes to these two values are protected
    // by ev_thread_lock
    int64_t m_size_reserved;           // How much memory is reserved (e.g., by the loader)
    int64_t m_size_evicting;           // the sum of the sizes of the pairs being written

    // these are constants
    int64_t m_low_size_watermark; // target max size of cachetable that eviction thread aims for
    int64_t m_low_size_hysteresis; // if cachetable grows to this size, client threads wake up eviction thread upon adding data
    int64_t m_high_size_watermark; // if cachetable grows to this size, client threads sleep upon adding data
    int64_t m_high_size_hysteresis; // if > cachetable size, then sleeping client threads may wake up

    // used to calculate random numbers
    struct random_data m_random_data;
    char m_random_statebuf[64];

    // mutex that protects fields listed immedietly below
    toku_mutex_t m_ev_thread_lock;
    // the eviction thread
    toku_pthread_t m_ev_thread;
    // condition variable that controls the sleeping period
    // of the eviction thread
    toku_cond_t m_ev_thread_cond;
    // number of client threads that are currently sleeping
    // due to an over-subscribed cachetable
    uint32_t m_num_sleepers;
    // states if the eviction thread should run. set to true
    // in init, set to false during destroy
    bool m_run_thread;
    // bool that states if the eviction thread is currently running
    bool m_ev_thread_is_running;
    // period which the eviction thread sleeps
    uint32_t m_period_in_seconds;
    // condition variable on which client threads wait on when sleeping
    // due to an over-subscribed cachetable
    toku_cond_t m_flow_control_cond;

    // variables for engine status
    PARTITIONED_COUNTER m_size_nonleaf;
    PARTITIONED_COUNTER m_size_leaf;
    PARTITIONED_COUNTER m_size_rollback;
    PARTITIONED_COUNTER m_size_cachepressure;
    PARTITIONED_COUNTER m_wait_pressure_count;
    PARTITIONED_COUNTER m_wait_pressure_time;
    PARTITIONED_COUNTER m_long_wait_pressure_count;
    PARTITIONED_COUNTER m_long_wait_pressure_time;
    
    KIBBUTZ m_kibbutz;

    // this variable is ONLY used for testing purposes
    uint64_t m_num_eviction_thread_runs;
    friend class evictor_test_helpers;
    friend class evictor_unit_test;
};

///////////////////////////////////////////////////////////////////////////////
//
// Iterates over the clean head in the pair list, calling the cleaner
// callback on each node in that list.
//
class cleaner {
public:
    void init(uint32_t cleaner_iterations, pair_list* _pl, CACHETABLE _ct);
    void destroy(void);
    uint32_t get_iterations(void);
    void set_iterations(uint32_t new_iterations);
    uint32_t get_period_unlocked(void);
    void set_period(uint32_t new_period);
    int run_cleaner(void);
    
private:
    pair_list* m_pl;
    CACHETABLE m_ct;
    struct minicron m_cleaner_cron; // the periodic cleaner thread
    uint32_t m_cleaner_iterations; // how many times to run the cleaner per
                                  // cleaner period (minicron has a
                                  // minimum period of 1s so if you want
                                  // more frequent cleaner runs you must
                                  // use this)
};

///////////////////////////////////////////////////////////////////////////////
//
// The cachetable is as close to an ENV as we get.
//
struct cachetable {
    pair_list list;
    cleaner cl;
    evictor ev;
    checkpointer cp;
    cachefile_list cf_list;
    
    KIBBUTZ client_kibbutz; // pool of worker threads and jobs to do asynchronously for the client.
    KIBBUTZ ct_kibbutz; // pool of worker threads and jobs to do asynchronously for the cachetable
    KIBBUTZ checkpointing_kibbutz; // small pool for checkpointing cloned pairs

    char *env_dir;
};

#endif // End of header guardian.

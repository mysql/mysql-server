/* -*- mode: C; c-basic-offset: 4 -*- */
#ident "Copyright (c) 2007-8 Tokutek Inc.  All rights reserved."

#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."

#if !defined(TOKU_LOCKTREE_H)
#define TOKU_LOCKTREE_H

/**
   \file  locktree.h
   \brief Lock trees: header and comments
  
   Lock trees are toku-struct's for granting long-lived locks to transactions.
   See more details on the design document.

   TODO: If the various range trees are inconsistent with
   each other, due to some system error like failed malloc,
   we defer to the db panic handler. Pass in another parameter to do this.
*/
#include <stdbool.h>
#include <db.h>
#include <brttypes.h>
#include <rangetree.h>
#include <lth.h>
#include <rth.h>
#include <idlth.h>
#include <omt.h>
#include "toku_pthread.h"
#include "toku_assert.h"

#if defined(__cplusplus)
extern "C" {
#endif

/** Errors returned by lock trees */
typedef enum {
    TOKU_LT_INCONSISTENT=-1,  /**< The member data are in an inconsistent state */
} TOKU_LT_ERROR;

typedef int (*toku_dbt_cmp)(DB *,const DBT*,const DBT*);

/** Convert error codes into a human-readable error message */
char* toku_lt_strerror(TOKU_LT_ERROR r /**< Error code */) 
                       __attribute__((const,pure));

#if !defined(TOKU_LOCKTREE_DEFINE)
#define TOKU_LOCKTREE_DEFINE
typedef struct __toku_lock_tree toku_lock_tree;
#endif

#if !defined(TOKU_LTH_DEFINE)
#define TOKU_LTH_DEFINE
typedef struct __toku_lth toku_lth;
#endif

#define TOKU_LT_USE_BORDERWRITE 1

typedef struct __toku_ltm toku_ltm;

/** \brief The lock tree structure */
struct __toku_lock_tree {
    /** Lock tree manager */
    toku_ltm* mgr;
    /** The database for which this locktree will be handling locks */
    DB*                 db;
    toku_range_tree*    borderwrite; /**< See design document */
    toku_rth*           rth;         /**< Stores local(read|write)set tables */
    /** Whether lock escalation is allowed. */
    bool                lock_escalation_allowed;
    /** Function to retrieve the key compare function from the database. */
    toku_dbt_cmp compare_fun;
    /** The number of references held by DB instances and transactions to this lock tree*/
    uint32_t          ref_count;
    /** DICTIONARY_ID associated with the lock tree */
    DICTIONARY_ID      dict_id;
    OMT                dbs; //The extant dbs using this lock tree.
    OMT                lock_requests;
    toku_rth*          txns_to_unlock; // set of txn's that could not release their locks because there was no db for the comparison function

    toku_pthread_mutex_t mutex;
    bool mutex_locked;

    /** A temporary area where we store the results of various find on 
        the range trees that this lock tree owns 
    Memory ownership: 
     - tree->buf is an array of toku_range's, which the lt owns
       The contents of tree->buf are volatile (this is a buffer space
       that we pass around to various functions, and every time we
       invoke a new function, its previous contents may become 
       meaningless)
     - tree->buf[i].left, .right are toku_points (ultimately a struct), 
       also owned by lt. We gave a pointer only to this memory to the 
       range tree earlier when we inserted a range, but the range tree
       does not own it!
     - tree->buf[i].{left,right}.key_payload is owned by
       the lt, we made copies from the DB at some point
    */
    toku_range*         buf;      
    uint32_t            buflen;      /**< The length of buf */
    toku_range*         bw_buf;
    uint32_t            bw_buflen;
    toku_range*         verify_buf;
    uint32_t            verify_buflen;
};

typedef enum {
    LTM_LOCKS_LIMIT,                // number of locks allowed (obsolete)
    LTM_LOCKS_CURR,                 // number of locks in existence
    LTM_LOCK_MEMORY_LIMIT,          // maximum amount of memory allowed for locks 
    LTM_LOCK_MEMORY_CURR,           // maximum amount of memory allowed for locks 
    LTM_LOCK_ESCALATION_SUCCESSES,  // number of times lock escalation succeeded 
    LTM_LOCK_ESCALATION_FAILURES,   // number of times lock escalation failed
    LTM_READ_LOCK,                  // number of times read lock taken successfully
    LTM_READ_LOCK_FAIL,             // number of times read lock denied
    LTM_OUT_OF_READ_LOCKS,          // number of times read lock denied for out_of_locks
    LTM_WRITE_LOCK,                 // number of times write lock taken successfully
    LTM_WRITE_LOCK_FAIL,            // number of times write lock denied
    LTM_OUT_OF_WRITE_LOCKS,         // number of times write lock denied for out_of_locks
    LTM_LT_CREATE,                  // number of locktrees created
    LTM_LT_CREATE_FAIL,             // number of locktrees unable to be created
    LTM_LT_DESTROY,                 // number of locktrees destroyed
    LTM_LT_NUM,                     // number of locktrees (should be created - destroyed)
    LTM_LT_NUM_MAX,                 // max number of locktrees that have existed simultaneously
    LTM_STATUS_NUM_ROWS
} ltm_status_entry;

typedef struct {
    BOOL initialized;
    TOKU_ENGINE_STATUS_ROW_S status[LTM_STATUS_NUM_ROWS];
} LTM_STATUS_S, *LTM_STATUS;

struct __toku_ltm {
    /** The maximum number of locks allowed for the environment. */
    uint64_t          locks_limit;
    /** The current number of locks for the environment. */
    uint64_t          curr_locks;
    /** The maximum amount of memory for locks allowed for the environment. */
    uint64_t          lock_memory_limit;
    /** The current amount of memory for locks for the environment. */
    uint64_t          curr_lock_memory;
    /** Status / accountability information */
    LTM_STATUS_S       status;
    /** The list of lock trees it manages. */
    toku_lth*          lth;
    /** List of lock-tree DB mappings. Upon a request for a lock tree given
        a DB, if an object for that DB exists in this list, then the lock tree
        is retrieved from this list, otherwise, a new lock tree is created
        and the new mapping of DB and Lock tree is stored here */
    toku_idlth*        idlth;
    /** The panic function */
    int               (*panic)(DB*, int);

    toku_pthread_mutex_t mutex;
    bool mutex_locked;

    struct timeval lock_wait_time;
};

extern const DBT* const toku_lt_infinity;     /**< Special value denoting 
                                                   +infty */
extern const DBT* const toku_lt_neg_infinity; /**< Special value denoting 
                                                   -infty */

/**

   \brief A 2D BDB-inspired point.

   Observe the toku_point, and marvel! 
   It makes the pair (key, data) into a 1-dimensional point,
   on which a total order is defined by toku_lt_point_cmp.
   Additionally, we have points at +infty and -infty as
   key_payload = (void*) toku_lt_infinity or 
   key_payload = (void*) toku_lt_neg infinity 
 */
struct __toku_point {
    toku_lock_tree* lt;           /**< The lock tree, where toku_lt_point_cmp 
                                       is defined */
    void*           key_payload;  /**< The key ... */
    uint32_t        key_len;      /**< and its length */
};
#if !defined(__TOKU_POINT)
#define __TOKU_POINT
typedef struct __toku_point toku_point;
#endif

/**
   Create a lock tree.  Should be called only inside DB->open.

   \param ptree          We set *ptree to the newly allocated tree.
   
   \return
   - 0       Success
   - EINVAL  If any pointer or function argument is NULL.
   - EINVAL  If payload_capacity is 0.
   - May return other errors due to system calls.

   A pre-condition is that no pointer parameter can be NULL;
   this pre-condition is assert(3)'ed.
   A future check is that it should return EINVAL for already opened db
   or already closed db. 
   If this library is ever exported to users, we will use error datas 
   instead.
 */
int toku_lt_create(toku_lock_tree** ptree,
                   toku_ltm* mgr,
                   toku_dbt_cmp compare_fun);

/**
   Closes and frees a lock tree.
   It will free memory used by the tree, and all keys/datas
   from all internal structures.
   It handles the case of transactions that are still active
   when lt_close is invoked: it can throw away other tables, but 
   it keeps lists of selfread and selfwrite, and frees the memory
   pointed to by the DBTs contained in the selfread and selfwrite.

   \param tree    The tree to free.
   
   \return 
   - 0       Success.

   It asserts that the tree != NULL. 
   If this library is ever exported to users, we will use error datas instead.

*/
int toku_lt_close(toku_lock_tree* tree);

/**
   Acquires a read lock on a single key (or key/data).

   \param tree    The lock tree for the db.
   \param txn     The TOKU Transaction this lock is for.
   \param key     The key this lock is for.
   \param data   The data this lock is for.

   \return
   - 0                   Success.
   - DB_LOCK_NOTGRANTED  If there is a conflict in getting the lock.
                         This can only happen if some other transaction has
                         a write lock that overlaps this point.
   - ENOMEM              If adding the lock would exceed the maximum
                         memory allowed for payloads.

   The following is asserted: 
     (tree == NULL || txn == NULL || key == NULL);
   If this library is ever exported to users, we will use EINVAL instead.

   In BDB, txn can actually be NULL (mixed operations with transactions and 
   no transactions). This can cause conflicts, nobody was able (so far) 
   to verify that MySQL does or does not use this.
*/
int toku_lt_acquire_read_lock(toku_lock_tree* tree, DB* db, TXNID txn,
                              const DBT* key);

/*
   Acquires a read lock on a key range (or key/data range).  (Closed range).

   \param tree            The lock tree for the db.
   \param txn             The TOKU Transaction this lock is for.
                          Note that txn == NULL is not supported at this time.
   \param key_left        The left end key of the range.
   \param key_right       The right end key of the range.

   \return
   - 0                   Success.
   - DB_LOCK_NOTGRANTED  If there is a conflict in getting the lock.
                         This can only happen if some other transaction has
                         a write lock that overlaps this range.
   - EDOM                if (key_left) >  (key_right)
                         (According to the db's comparison function.)
   - ENOMEM              If adding the lock would exceed the maximum
                         memory allowed for payloads.

    The following is asserted, but if this library is ever exported to users,
    EINVAL should be used instead:
     If (tree == NULL || txn == NULL ||
         key_left == NULL || key_right == NULL) or

    Memory: It is safe to free keys and datas after this call.
    If the lock tree needs to hold onto the key or data, it will make copies
    to its local memory.

    In BDB, txn can actually be NULL (mixed operations with transactions and 
    no transactions). This can cause conflicts, nobody was able (so far) 
    to verify that MySQL does or does not use this.
 */
int toku_lt_acquire_range_read_lock(toku_lock_tree* tree, DB* db, TXNID txn,
				    const DBT* key_left,
				    const DBT* key_right);

/**
   Acquires a write lock on a single key (or key/data).

   \param tree   The lock tree for the db.
   \param txn    The TOKU Transaction this lock is for.
                 Note that txn == NULL is not supported at this time.
   \param key    The key this lock is for.
   \param data   The data this lock is for.

    \return
    - 0                   Success.
    - DB_LOCK_NOTGRANTED  If there is a conflict in getting the lock.
                          This can only happen if some other transaction has
                          a write (or read) lock that overlaps this point.
    - ENOMEM              If adding the lock would exceed the maximum
                           memory allowed for payloads.

    The following is asserted, but if this library is ever exported to users,
    EINVAL should be used instead:
    If (tree == NULL || txn == NULL || key == NULL)

   Memory:
        It is safe to free keys and datas after this call.
        If the lock tree needs to hold onto the key or data, it will make copies
        to its local memory.
*/
int toku_lt_acquire_write_lock(toku_lock_tree* tree, DB* db, TXNID txn,
                               const DBT* key);

 //In BDB, txn can actually be NULL (mixed operations with transactions and no transactions).
 //This can cause conflicts, I was unable (so far) to verify that MySQL does or does not use
 //this.
/*
 * Acquires a write lock on a key range (or key/data range).  (Closed range).
 * Params:
 *      tree            The lock tree for the db.
 *      txn             The TOKU Transaction this lock is for.
 *      key_left        The left end key of the range.
 *      key_right       The right end key of the range.
 * Returns:
 *      0                   Success.
 *      DB_LOCK_NOTGRANTED  If there is a conflict in getting the lock.
 *                          This can only happen if some other transaction has
 *                          a write (or read) lock that overlaps this range.
 *      EINVAL              If (tree == NULL || txn == NULL ||
 *                              key_left == NULL || key_right == NULL) or
 *      ERANGE              If (key_left) >  (key_right)
 *                          (According to the db's comparison functions.
 *      ENOSYS              THis is not yet implemented.  Till it is, it will return ENOSYS,
 *                            if other errors do not occur first.
 *      ENOMEM              If adding the lock would exceed the maximum
 *                          memory allowed for payloads.
 * Asserts:
 *      The EINVAL and ERANGE cases described will use assert to abort instead of returning errors.
 *      If this library is ever exported to users, we will use error datas instead.
 * Memory:
 *      It is safe to free keys and datas after this call.
 *      If the lock tree needs to hold onto the key or data, it will make copies
 *      to its local memory.
 * *** Note that txn == NULL is not supported at this time.
 */
int toku_lt_acquire_range_write_lock(toku_lock_tree* tree, DB* db, TXNID txn,
				     const DBT* key_left,
				     const DBT* key_right);

 //In BDB, txn can actually be NULL (mixed operations with transactions and no transactions).
 //This can cause conflicts, I was unable (so far) to verify that MySQL does or does not use
 //this.
/**
   Releases all the locks owned by a transaction.
   This is used when a transaction aborts/rolls back/commits.

   \param tree        The lock tree for the db.
   \param txn         The transaction to release all locks for.
                      Note that txn == NULL is not supported at this time.

   \return
   - 0           Success.
   - EINVAL      If (tree == NULL || txn == NULL).
   - EINVAL      If panicking.
 */
int toku_lt_unlock_txn(toku_lock_tree* tree, TXNID txn);

void toku_lt_retry_lock_requests(toku_lock_tree *tree);

void toku_lt_add_ref(toku_lock_tree* tree);

int toku_lt_remove_ref(toku_lock_tree* tree);

void toku_lt_remove_db_ref(toku_lock_tree* tree, DB *db);

toku_range_tree* toku_lt_ifexist_selfread(toku_lock_tree* tree, TXNID txn);

toku_range_tree* toku_lt_ifexist_selfwrite(toku_lock_tree* tree, TXNID txn);

void toku_lt_verify(toku_lock_tree *tree, DB *db);

int toku_lt_point_cmp(const toku_point* x, const toku_point* y);

/* Lock tree manager functions begin here */

/**
    Creates a lock tree manager.
    
    \param pmgr      A buffer for the new lock tree manager.
    \param locks_limit    The maximum number of locks.

    \return
    - 0 on success.
    - EINVAL if any pointer parameter is NULL.
    - May return other errors due to system calls.
*/
int toku_ltm_create(toku_ltm** pmgr,
                    uint32_t locks_limit,
                    uint64_t lock_memory_limit,
                    int   (*panic)(DB*, int));

/** Open the lock tree manager */
int toku_ltm_open(toku_ltm *mgr);

/**
    Closes and frees a lock tree manager..
    
    \param mgr  The lock tree manager.

    \return
    - 0 on success.
    - EINVAL if any pointer parameter is NULL.
    - May return other errors due to system calls.
*/
int toku_ltm_close(toku_ltm* mgr);

/**
    Sets the maximum number of locks on the lock tree manager.
    
    \param mgr       The lock tree manager to which to set locks_limit.
    \param locks_limit    The new maximum number of locks.

    \return
    - 0 on success.
    - EINVAL if tree is NULL or locks_limit is 0
    - EDOM   if locks_limit is less than the number of locks held by any lock tree
         held by the manager
*/
int toku_ltm_set_max_locks(toku_ltm* mgr, uint32_t locks_limit);

int toku_ltm_get_max_locks(toku_ltm* mgr, uint32_t* locks_limit);

int toku_ltm_set_max_lock_memory(toku_ltm* mgr, uint64_t lock_memory_limit);

int toku_ltm_get_max_lock_memory(toku_ltm* mgr, uint64_t* lock_memory_limit);

void toku_ltm_get_status(toku_ltm* mgr, LTM_STATUS s);

// set the default lock timeout. units are milliseconds
void toku_ltm_set_lock_wait_time(toku_ltm *mgr, uint64_t lock_wait_time_msec);

// get the default lock timeout
void toku_ltm_get_lock_wait_time(toku_ltm *mgr, uint64_t *lock_wait_time_msec);

/**
    Gets a lock tree for a given DB with id dict_id
*/
int toku_ltm_get_lt(toku_ltm* mgr, toku_lock_tree** ptree, DICTIONARY_ID dict_id, DB *dbp, toku_dbt_cmp compare_fun);

void toku_ltm_invalidate_lt(toku_ltm* mgr, DICTIONARY_ID dict_id);

void toku_ltm_incr_lock_memory(void *extra, size_t s);
void toku_ltm_decr_lock_memory(void *extra, size_t s);

typedef enum {
    LOCK_REQUEST_INIT = 0,
    LOCK_REQUEST_PENDING = 1,
    LOCK_REQUEST_COMPLETE = 2,
} toku_lock_request_state;

// TODO: use DB_LOCK_READ/WRITE instead?
typedef enum {
    LOCK_REQUEST_UNKNOWN = 0,
    LOCK_REQUEST_READ = 1,
    LOCK_REQUEST_WRITE = 2,
} toku_lock_type;

typedef struct {
    DB *db;
    TXNID txnid;
    const DBT *key_left; const DBT *key_right;
    DBT key_left_copy, key_right_copy;
    toku_lock_type type;
    toku_lock_tree *tree;
    int complete_r;
    toku_lock_request_state state;
    toku_pthread_cond_t wait;
    bool wait_initialized;
} toku_lock_request;

// a lock request contains the db, the key range, the lock type, and the transaction id that describes a potential row range lock.
// the typical use case is:
// - initialize a lock request
// - start to try to acquire the lock
// - do something else
// - wait for the lock request to be resolved on the wait condition variable and a timeout.
// - destroy the lock request
// a lock request is resolved when its state is no longer pending, or when it becomes granted, or timedout, or deadlocked.
// when resolved, the state of the lock request is changed and any waiting threads are awakened.

// initialize a lock request (default initializer).
void toku_lock_request_default_init(toku_lock_request *lock_request);

// initialize the lock request parameters.
// this API allows a lock request to be reused.
void toku_lock_request_set(toku_lock_request *lock_request, DB *db, TXNID txnid, const DBT *key_left, const DBT *key_right, toku_lock_type type);

// initialize and set the parameters for a lock request.  it is equivalent to _default_init followed by _set.
void toku_lock_request_init(toku_lock_request *lock_request, DB *db, TXNID txnid, const DBT *key_left, const DBT *key_right, toku_lock_type type);

// destroy a lock request.
void toku_lock_request_destroy(toku_lock_request *lock_request);

// try to acquire a lock described by a lock request. 
// if the lock is granted, then set the lock request state to granted
// otherwise, add the lock request to the lock request tree and check for deadlocks
// returns 0 (success), DB_LOCK_NOTGRANTED, DB_LOCK_DEADLOCK
int toku_lock_request_start(toku_lock_request *lock_request, toku_lock_tree *tree, bool copy_keys_if_not_granted);

// sleep on the lock request until it becomes resolved or the wait time occurs.
// if the wait time is not specified, then wait for as long as it takes.
int toku_lock_request_wait(toku_lock_request *lock_request, toku_lock_tree *tree, struct timeval *wait_time);

int toku_lock_request_wait_with_default_timeout(toku_lock_request *lock_request, toku_lock_tree *tree);

// wakeup any threads that are waiting on a lock request.
void toku_lock_request_wakeup(toku_lock_request *lock_request, toku_lock_tree *tree);

// returns the lock request state
toku_lock_request_state toku_lock_request_get_state(toku_lock_request *lock_request);

// try to acquire a lock described by a lock request. if the lock is granted then return success.
// otherwise wait on the lock request until the lock request is resolved (either granted or
// deadlocks), or the given timer has expired.
// returns 0 (success), DB_LOCK_NOTGRANTED
int toku_lt_acquire_lock_request_with_timeout(toku_lock_tree *tree, toku_lock_request *lock_request, struct timeval *wait_time);

int toku_lt_acquire_lock_request_with_default_timeout(toku_lock_tree *tree, toku_lock_request *lock_request);

#include "txnid_set.h"

// internal function that finds all transactions that conflict with a given lock request
// for read lock requests
//     conflicts = all transactions in the BWT that conflict with the lock request
// for write lock requests
//     conflicts = all transactions in the GRT that conflict with the lock request UNION
//                 all transactions in the BWT that conflict with the lock request
// adds all of the conflicting transactions to the conflicts transaction set
// returns an error code (0 == success)
int toku_lt_get_lock_request_conflicts(toku_lock_tree *tree, toku_lock_request *lock_request, txnid_set *conflicts);

#if defined(__cplusplus)
}
#endif

#endif

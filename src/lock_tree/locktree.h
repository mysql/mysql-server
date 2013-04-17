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

#include <db.h>
#include <brttypes.h>
#include <rangetree.h>
#include <lth.h>
#include <rth.h>
#include <idlth.h>
#include <omt.h>

#include "toku_assert.h"

#if defined(__cplusplus)
extern "C" {
#endif

/** Errors returned by lock trees */
typedef enum {
    TOKU_LT_INCONSISTENT=-1,  /**< The member data are in an inconsistent state */
} TOKU_LT_ERROR;

typedef int (*toku_dbt_cmp)(DB*,const DBT*,const DBT*);

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
    /** The database for which this locktree will be handling locks */
    DB*                 db;
    toku_range_tree*    borderwrite; /**< See design document */
    toku_rth*           rth;         /**< Stores local(read|write)set tables */
    /**
        Stores a list of transactions to unlock when it is safe.
        When we get a PUT or a GET, the comparison function is valid
        and we can delete locks held in txns_to_unlock, even if txns_still_locked
        is nonempty.
    */
    toku_rth*           txns_to_unlock; 
    /** Stores a list of transactions that hold locks. txns_still_locked = rth - txns_to_unlock
        rth != txns_still_locked + txns_to_unlock, we may get an unlock call for a txn that has
        no locks in rth.
        When txns_still_locked becomes empty, we can throw away the contents of the lock tree
        quickly. */
    toku_rth*           txns_still_locked;
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
    /** Whether lock escalation is allowed. */
    BOOL                lock_escalation_allowed;
    /** Lock tree manager */
    toku_ltm* mgr;
    /** Function to retrieve the key compare function from the database. */
    toku_dbt_cmp (*get_compare_fun_from_db)(DB*);
    /** The key compare function */
    int               (*compare_fun)(DB*,const DBT*,const DBT*);
    /** The panic function */
    int               (*panic)(DB*, int);
    /** The user malloc function */
    void*             (*malloc) (size_t);
    /** The user free function */
    void              (*free)   (void*);
    /** The user realloc function */
    void*             (*realloc)(void*, size_t);
    /** The number of references held by DB instances and transactions to this lock tree*/
    uint32_t          ref_count;
    /** DICTIONARY_ID associated with the lock tree */
    DICTIONARY_ID      dict_id;
    TXNID              table_lock_owner;
    BOOL               table_is_locked;
    OMT                dbs; //The extant dbs using this lock tree.
};


// accountability
typedef struct ltm_status {
    uint32_t   lock_escalation_successes;  // number of times lock escalation succeeded 
    uint32_t   lock_escalation_failures;   // number of times lock escalation failed
    uint64_t   read_lock;                  // number of times read lock taken successfully
    uint64_t   read_lock_fail;             // number of times read lock denied
    uint64_t   out_of_read_locks;          // number of times read lock denied for out_of_locks
    uint64_t   write_lock;                 // number of times write lock taken successfully
    uint64_t   write_lock_fail;            // number of times write lock denied
    uint64_t   out_of_write_locks;         // number of times write lock denied for out_of_locks
} LTM_STATUS_S, *LTM_STATUS;



struct __toku_ltm {
    /** The maximum number of locks allowed for the environment. */
    uint32_t          max_locks;
    /** The current number of locks for the environment. */
    uint32_t          curr_locks;
    /** The maximum amount of memory for locks allowed for the environment. */
    uint64_t          max_lock_memory;
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
    /** Function to retrieve the key compare function from the database. */
    toku_dbt_cmp (*get_compare_fun_from_db)(DB*);
    /** The panic function */
    int               (*panic)(DB*, int);
    /** The user malloc function */
    void*             (*malloc) (size_t);
    /** The user free function */
    void              (*free)   (void*);
    /** The user realloc function */
    void*             (*realloc)(void*, size_t);
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
   \param get_compare_fun_from_db    Accessor for the key compare function.
   \param panic          The function to cause the db to panic.  
                         i.e., godzilla_rampage()
   \param payload_capacity The maximum amount of memory to use for dbt payloads.
   \param user_malloc    A user provided malloc(3) function.
   \param user_free      A user provided free(3) function.
   \param user_realloc   A user provided realloc(3) function.
   
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
                   int   (*panic)(DB*, int), 
                   toku_ltm* mgr,
                   toku_dbt_cmp (*get_compare_fun_from_db)(DB*),
                   void* (*user_malloc) (size_t),
                   void  (*user_free)   (void*),
                   void* (*user_realloc)(void*, size_t));

/**
    Gets a lock tree for a given DB with id dict_id
*/
int toku_ltm_get_lt(toku_ltm* mgr, toku_lock_tree** ptree, 
                    DICTIONARY_ID dict_id, DB *db);

void toku_ltm_invalidate_lt(toku_ltm* mgr, DICTIONARY_ID dict_id);

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
int toku_lt_unlock(toku_lock_tree* tree, TXNID txn);

/* Lock tree manager functions begin here */
/**
    Creates a lock tree manager..
    
    \param pmgr      A buffer for the new lock tree manager.
    \param max_locks    The maximum number of locks.
    \param user_malloc  A user provided malloc(3) function.
    \param user_free    A user provided free(3) function.
    \param user_realloc A user provided realloc(3) function.

    \return
    - 0 on success.
    - EINVAL if any pointer parameter is NULL.
    - May return other errors due to system calls.
*/
int toku_ltm_create(toku_ltm** pmgr,
                    uint32_t max_locks,
                    uint64_t max_lock_memory,
                    int   (*panic)(DB*, int), 
                    toku_dbt_cmp (*get_compare_fun_from_db)(DB*),
                    void* (*user_malloc) (size_t),
                    void  (*user_free)   (void*),
                    void* (*user_realloc)(void*, size_t));

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
    
    \param mgr       The lock tree manager to which to set max_locks.
    \param max_locks    The new maximum number of locks.

    \return
    - 0 on success.
    - EINVAL if tree is NULL or max_locks is 0
    - EDOM   if max_locks is less than the number of locks held by any lock tree
         held by the manager
*/
int toku_ltm_set_max_locks(toku_ltm* mgr, uint32_t max_locks);

int toku_ltm_get_max_lock_memory(toku_ltm* mgr, uint64_t* max_lock_memory);

int toku_ltm_set_max_lock_memory(toku_ltm* mgr, uint64_t max_lock_memory);

void toku_ltm_get_status(toku_ltm* mgr, uint32_t * max_locks, uint32_t * curr_locks, 
			 uint64_t *max_lock_memory, uint64_t *curr_lock_memory,
			 LTM_STATUS s);

int toku_ltm_get_max_locks(toku_ltm* mgr, uint32_t* max_locks);

void toku_lt_add_ref(toku_lock_tree* tree);

int toku_lt_remove_ref(toku_lock_tree* tree);

void toku_lt_remove_db_ref(toku_lock_tree* tree, DB *db);

int toku_lt_point_cmp(const toku_point* x, const toku_point* y);

toku_range_tree* toku_lt_ifexist_selfread(toku_lock_tree* tree, TXNID txn);

toku_range_tree* toku_lt_ifexist_selfwrite(toku_lock_tree* tree, TXNID txn);

void toku_lt_verify(toku_lock_tree *tree, DB *db);

#if defined(__cplusplus)
}
#endif

#endif

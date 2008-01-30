/* -*- mode: C; c-basic-offset: 4 -*- */
#ident "Copyright (c) 2007-8 Tokutek Inc.  All rights reserved."

#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."

/**
   \file  locktree.h
   \brief Lock trees: header and comments
  
   Lock trees are toku-struct's for granting long-lived locks to transactions.
   See more details on the design document.

   TODO: If the various range trees are inconsistent with
   each other, due to some system error like failed malloc,
   we defer to the db panic handler
      TODO: Pass in another parameter to do this.
*/

#include <assert.h>
#include <db.h>
#include <brttypes.h>
#include <rangetree.h>

/** The lock tree structure */
typedef struct {
    /** The database for which this locktree will be handling locks */
    DB*                 db;
    /** Whether the db supports duplicate */
    BOOL                duplicates;
    toku_range_tree*    mainread;    /**< See design document */
    toku_range_tree*    borderwrite; /**< See design document */
    //TODO: Remove this tree and have one per transaction.
    toku_range_tree*    selfread;
    //TODO: Remove this tree and have one per transaction.
    toku_range_tree*    selfwrite;
    /** A temporary area where we store the results of various find on 
        the range trees that this lock tree owns */
    toku_range*         buf;      
    unsigned            buflen;      /**< The length of buf */
    /** The maximum amount of memory to be used for DBT payloads. */
    size_t              payload_capacity;
    /** The current amount of memory used for DBT payloads. */
    size_t              payload_used;
    /** The key compare function */
    int                 (*compare_fun)(DB*,const DBT*,const DBT*);
    /** The data compare function */
    int                 (*dup_compare)(DB*,const DBT*,const DBT*);
    /** The panic function */
    void                (*panic)(DB*);
    /** The user malloc function */
    void*               (*malloc) (size_t);
    /** The user free function */
    void                (*free)   (void*);
    /** The user realloc function */
    void*               (*realloc)(void*, size_t);
} toku_lock_tree;


extern const DBT* const toku_lt_infinity;     /**< Special value denoting 
                                                   +infty */
extern const DBT* const toku_lt_neg_infinity; /**< Special value denoting 
                                                   -infty */

/**
   Observe the toku_point, and marvel! 
   It makes the pair (key, data) into a 1-dimensional point,
   on which a total order is defined by toku_lt_point_cmp.
   Additionally, we have points at +infty and -infty as
   key_payload = (void*) toku_lt_infinity or 
   key_payload = (void*) toku_lt_neg infinity 
 */
typedef struct {
    toku_lock_tree* lt;           /**< The lock tree, where toku_lt_point_cmp 
                                       is defined */
    void*           key_payload;  /**< The key ... */
    u_int32_t       key_len;      /**< and its length */
    void*           data_payload; /**< The data ... */
    u_int32_t       data_len;     /**< and its length */
} toku_point;

/**
   A comparison function between toku_point's.
   It is implemented as a wrapper of db compare and dup_compare functions,
   but it checks whether the point is +/- infty.
   Parameters are of type toku_point.
   Return values conform to cmp from qsort(3).
 */
int toku_lt_point_cmp(void* a, void* b);

/**
   Create a lock tree.  Should be called only inside DB->open.

   \param ptree:         We set *ptree to the newly allocated tree.
   \param db:            This is the db that the lock tree will be performing 
                         locking for.
   \param duplicates     Whether the db supports duplicates.
   \param compare_fun    The key compare function.
   \param dup_compare    The data compare function.
   \param panic          The function to cause the db to panic.  i.e. godzilla_rampage()
   \param payload_capacity The maximum amount of memory to use for dbt payloads.
   \param user_malloc    A user provided malloc(3) function.
   \param user_free      A user provided free(3) function.
   \param user_realloc   A user provided realloc(3) function.
   
   \return
   - 0:      Success
   - EINVAL: If any pointer or function argument is NULL.
   - EINVAL: If payload_capacity is 0.
   - May return other errors due to system calls.

   A pre-condition is that no pointer parameter can be NULL;
   this pre-condition is assert(3)'ed.
   A future check is that it should return EINVAL for already opened db
   or already closed db. 
   If this library is ever exported to users, we will use error datas 
   instead.
 */
int toku_lt_create(toku_lock_tree** ptree, DB* db, BOOL duplicates,
                   void(*panic)(DB*), size_t payload_capacity,
                   int (*compare_fun)(DB*,const DBT*,const DBT*),
                   int (*dup_compare)(DB*,const DBT*,const DBT*),
                   void* (*user_malloc) (size_t),
                   void  (*user_free)   (void*),
                   void* (*user_realloc)(void*, size_t));


/**
   Closes and frees a lock tree.
   It will free memory used by the tree, and all keys/datas
   from all internal structures.
   It handles the case of transactions that are still active
   when lt_close is invoked: it can throw away other tables, but 
   it keeps lists of selfread and selfwrite, and frees the memory
   pointed to by the DBTs contained in the selfread and selfwrite.

   \param tree:   The tree to free.
   
   \return 
   - 0:      Success.

   It asserts that the tree != NULL. 
   If this library is ever exported to users, we will use error datas instead.

*/
int toku_lt_close(toku_lock_tree* tree);

/*
 * Acquires a read lock on a single key (or key/data).
 * Params:
 *      tree:   The lock tree for the db.
 *      txn:    The TOKU Transaction this lock is for.
 *      key:    The key this lock is for.
 *      data:  The data this lock is for.
 * Returns:
 *      0:                  Success.
 *      DB_LOCK_NOTGRANTED: If there is a conflict in getting the lock.
 *                          This can only happen if some other transaction has
 *                          a write lock that overlaps this point.
 *      EINVAL:             If (tree == NULL || txn == NULL || key == NULL) or
 *                             (tree->db is dupsort && data == NULL) or
 *                             (tree->db is nodup   && data != NULL) or
 *                             (tree->db is dupsort && key != data &&
 *                                  (key == toku_lt_infinity ||
 *                                   key == toku_lt_neg_infinity))
 *      ENOMEM:             If adding the lock would exceed the maximum
 *                          memory allowed for payloads.
 * Asserts:
 *      The EINVAL cases described will use assert to abort instead of returning errors.
 *      If this library is ever exported to users, we will use error datas instead.
 * Memory:
 *      It is safe to free keys and datas after this call.
 *      If the lock tree needs to hold onto the key or data, it will make copies
 *      to its local memory.
 * *** Note that txn == NULL is not supported at this time.
 */
int toku_lt_acquire_read_lock(toku_lock_tree* tree, DB_TXN* txn,
                              const DBT* key, const DBT* data);

 //In BDB, txn can actually be NULL (mixed operations with transactions and no transactions).
 //This can cause conflicts, I was unable (so far) to verify that MySQL does or does not use
 //this.
/*
 * Acquires a read lock on a key range (or key/data range).  (Closed range).
 * Params:
 *      tree:           The lock tree for the db.
 *      txn:            The TOKU Transaction this lock is for.
 *      key_left:       The left end key of the range.
 *      data_left:     The left end data of the range.
 *      key_right:      The right end key of the range.
 *      data_right:    The right end data of the range.
 * Returns:
 *      0:                  Success.
 *      DB_LOCK_NOTGRANTED: If there is a conflict in getting the lock.
 *                          This can only happen if some other transaction has
 *                          a write lock that overlaps this range.
 *      EINVAL:             If (tree == NULL || txn == NULL ||
 *                              key_left == NULL || key_right == NULL) or
 *                             (tree->db is dupsort &&
 *                               (data_left == NULL || data_right == NULL)) or
 *                             (tree->db is nodup   &&
 *                               (data_left != NULL || data_right != NULL)) or
 *                             (tree->db is dupsort && key_left != data_left &&
 *                                  (key_left == toku_lt_infinity ||
 *                                   key_left == toku_lt_neg_infinity)) or
 *                             (tree->db is dupsort && key_right != data_right &&
 *                                  (key_right == toku_lt_infinity ||
 *                                   key_right == toku_lt_neg_infinity))
 *      EDOM:             In a DB_DUPSORT db:
 *                            If (key_left, data_left) >  (key_right, data_right) or
 *                          In a nodup db:      if (key_left) >  (key_right)
 *                          (According to the db's comparison functions.
 *      ENOMEM:             If adding the lock would exceed the maximum
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
int toku_lt_acquire_range_read_lock(toku_lock_tree* tree, DB_TXN* txn,
                                   const DBT* key_left,  const DBT* data_left,
                                   const DBT* key_right, const DBT* data_right);

 //In BDB, txn can actually be NULL (mixed operations with transactions and no transactions).
 //This can cause conflicts, I was unable (so far) to verify that MySQL does or does not use
 //this.
/*
 * Acquires a write lock on a single key (or key/data).
 * Params:
 *      tree:   The lock tree for the db.
 *      txn:    The TOKU Transaction this lock is for.
 *      key:    The key this lock is for.
 *      data:  The data this lock is for.
 * Returns:
 *      0:                  Success.
 *      DB_LOCK_NOTGRANTED: If there is a conflict in getting the lock.
 *                          This can only happen if some other transaction has
 *                          a write (or read) lock that overlaps this point.
 *      EINVAL:             If (tree == NULL || txn == NULL || key == NULL) or
 *                             (tree->db is dupsort && data == NULL) or
 *                             (tree->db is nodup   && data != NULL)
 *                             (tree->db is dupsort && key != data &&
 *                                  (key == toku_lt_infinity ||
 *                                   key == toku_lt_neg_infinity))
 *      ENOMEM:             If adding the lock would exceed the maximum
 *                          memory allowed for payloads.
 * Asserts:
 *      The EINVAL cases described will use assert to abort instead of returning errors.
 *      If this library is ever exported to users, we will use error datas instead.
 * Memory:
 *      It is safe to free keys and datas after this call.
 *      If the lock tree needs to hold onto the key or data, it will make copies
 *      to its local memory.
 * *** Note that txn == NULL is not supported at this time.
 */
int toku_lt_acquire_write_lock(toku_lock_tree* tree, DB_TXN* txn,
                               const DBT* key, const DBT* data);

 //In BDB, txn can actually be NULL (mixed operations with transactions and no transactions).
 //This can cause conflicts, I was unable (so far) to verify that MySQL does or does not use
 //this.
/*
 * ***************NOTE: This will not be implemented before Feb 1st because
 * ***************      MySQL does not use DB->del on DB_DUPSORT dbs.
 * ***************      The only operation that requires a write range lock is
 * ***************      DB->del on DB_DUPSORT dbs.
 * Acquires a write lock on a key range (or key/data range).  (Closed range).
 * Params:
 *      tree:           The lock tree for the db.
 *      txn:            The TOKU Transaction this lock is for.
 *      key_left:       The left end key of the range.
 *      key_right:      The right end key of the range.
 *      data_left:     The left end data of the range.
 *      data_right:    The right end data of the range.
 * Returns:
 *      0:                  Success.
 *      DB_LOCK_NOTGRANTED: If there is a conflict in getting the lock.
 *                          This can only happen if some other transaction has
 *                          a write (or read) lock that overlaps this range.
 *      EINVAL:             If (tree == NULL || txn == NULL ||
 *                              key_left == NULL || key_right == NULL) or
 *                             (tree->db is dupsort &&
 *                               (data_left == NULL || data_right == NULL)) or
 *                             (tree->db is nodup   &&
 *                               (data_left != NULL || data_right != NULL))
 or
 *                             (tree->db is dupsort && key_left != data_left &&
 *                                  (key_left == toku_lt_infinity ||
 *                                   key_left == toku_lt_neg_infinity)) or
 *                             (tree->db is dupsort && key_right != data_right &&
 *                                  (key_right == toku_lt_infinity ||
 *                                   key_right == toku_lt_neg_infinity))
 *      ERANGE:             In a DB_DUPSORT db:
 *                            If (key_left, data_left) >  (key_right, data_right) or
 *                          In a nodup db:      if (key_left) >  (key_right)
 *                          (According to the db's comparison functions.
 *      ENOSYS:             THis is not yet implemented.  Till it is, it will return ENOSYS,
 *                            if other errors do not occur first.
 *      ENOMEM:             If adding the lock would exceed the maximum
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
int toku_lt_acquire_range_write_lock(toku_lock_tree* tree, DB_TXN* txn,
                                   const DBT* key_left,  const DBT* data_left,
                                   const DBT* key_right, const DBT* data_right);

 //In BDB, txn can actually be NULL (mixed operations with transactions and no transactions).
 //This can cause conflicts, I was unable (so far) to verify that MySQL does or does not use
 //this.
/*
 * Releases all the locks owned by a transaction.
 * This is used when a transaction aborts/rolls back/commits.
 * Params:
 *      tree:       The lock tree for the db.
 *      txn:        The transaction to release all locks for.
 * Returns:
 *      0:          Success.
 *      EINVAL:     If (tree == NULL || txn == NULL) or
 *                  if toku_lt_unlock has already been called on this txn.
 * *** Note that txn == NULL is not supported at this time.
 */
int toku_lt_unlock(toku_lock_tree* tree, DB_TXN* txn);

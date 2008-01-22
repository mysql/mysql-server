/* -*- mode: C; c-basic-offset: 4 -*- */
#ident "Copyright (c) 2007-8 Tokutek Inc.  All rights reserved."

#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."

#include <assert.h>
#include <db.h>
#include <brttypes.h>
#include <rangetree.h>

typedef struct {
    DB*                 db;
    BOOL                duplicates;
    toku_range_tree*    mainread;
    toku_range_tree*    borderwrite;
} toku_lock_tree;

extern DBT* toku_lt_infinity;
extern DBT* toku_lt_neg_infinity;

/*
 * key_data = (void*)toku_lt_infinity is how we refer to the infinities.
 */
typedef struct {
    toku_lock_tree* lt;
    void*           key_payload;
    u_int32_t       key_len;
    void*           data_payload;
    u_int32_t       data_len;
} toku_point;

/*
 * Wrapper of db compare and dup_compare functions.
 * In addition, also uses toku_lt_infinity and toku_lt_neg_infinity.
 * Parameters are of type toku_point.
 *
 * Return values conform to cmp from quicksort(3).
 */
int __toku_lt_point_cmp(void* a, void* b);

/*
 * Create a lock tree.  Should be called only inside DB->open.
 * Params:
 *      ptree:  We set *ptree to the newly allocated tree.
 *      db:     This is the db that the lock tree will be performing locking for.
 *              We will get flags to determine if it is a nodup or dupsort db.
 *              db should NOT be opened yet, but should have the DB_DUPSORT flag
 *              set appropriately.
 *              We will use the key compare (and if appropriate the data compare)
 *              functions in order to understand ranges.
 * Returns:
 *      0:      Success
 *      EINVAL: If (ptree == NULL || db == NULL).
 *              FutureChecks: Try to return EINVAL for already opened db,
 *              or already closed db.
 *      May return other errors due to system calls.
 * Asserts:
 *      The EINVAL cases described will use assert to abort instead of returning errors.
 *      If this library is ever exported to users, we will use error datas instead.
 */
int toku_lt_create(toku_lock_tree** ptree, DB* db);

/*
 * Closes/Frees a lock tree.
 * Params:
 *      tree:   The tree to free.
 * Returns:
 *      0:      Success.
 *      EINVAL: If (tree == NULL)
 *      May return other errors due to system calls.
 * Asserts:
 *      The EINVAL cases described will use assert to abort instead of returning errors.
 *      If this library is ever exported to users, we will use error datas instead.
 * Memory:
 *      This will free memory used by the tree, and all keys/datas
 *      from all internal structures.
 */
int toku_lt_close(toku_lock_tree* tree);
 //NOTE: Must handle case of transactions still active.
 //Need to keep lists of selfread and selfwrite.
 //can throw away other tables, but must go through each
 //of the selfread and selfwrite tables and free the memory
 //pointed to by the DBTs contained.

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
 * Asserts:
 *      The EINVAL cases described will use assert to abort instead of returning errors.
 *      If this library is ever exported to users, we will use error datas instead.
 * Memory:
 *      It is safe to free keys and datas after this call.
 *      If the lock tree needs to hold onto the key or data, it will make copies
 *      to its local memory.
 * *** Note that txn == NULL is not supported at this time.
 */
int toku_lt_acquire_read_lock(toku_lock_tree* tree, DB_TXN* txn, DBT* key, DBT* data);

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
 *      ERANGE:             In a DB_DUPSORT db:
 *                            If (key_left, data_left) >  (key_right, data_right) or
 *                          In a nodup db:      if (key_left) >  (key_right)
 *                          (According to the db's comparison functions.
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
                                    DBT* key_left, DBT* data_left,
                                    DBT* key_right, DBT* data_right);

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
 * Asserts:
 *      The EINVAL cases described will use assert to abort instead of returning errors.
 *      If this library is ever exported to users, we will use error datas instead.
 * Memory:
 *      It is safe to free keys and datas after this call.
 *      If the lock tree needs to hold onto the key or data, it will make copies
 *      to its local memory.
 * *** Note that txn == NULL is not supported at this time.
 */
int toku_lt_acquire_write_lock(toku_lock_tree* tree, DB_TXN* txn, DBT* key, DBT* data);

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
                                    DBT* key_left,  DBT* data_left,
                                    DBT* key_right, DBT* data_right);

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

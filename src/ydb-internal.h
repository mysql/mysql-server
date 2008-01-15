#ifndef YDB_INTERNAL_H
#define YDB_INTERNAL_H

#ident "Copyright (c) 2007 Tokutek Inc.  All rights reserved."

#include "../include/db.h"
#include "../newbrt/brttypes.h"
#include "../newbrt/brt.h"
#include "../newbrt/list.h"

struct db_header {
    int n_databases; // Or there can be >=1 named databases.  This is the count.
    char *database_names; // These are the names
    BRT  *database_brts;  // These 
};

struct __toku_db_internal {
    DB *db; // A pointer back to the DB.
    int freed;
    struct db_header *header;
    int database_number; // -1 if it is the single unnamed database.  Nonnengative number otherwise.
    char *full_fname;
    char *database_name;
    //int fd;
    u_int32_t open_flags;
    int open_mode;
    BRT brt;
    FILENUM fileid;
    struct list associated; // All the associated databases.  The primary is the head of the list.
    DB *primary;            // For secondary (associated) databases, what is the primary?  NULL if not a secondary.
    int(*associate_callback)(DB*, const DBT*, const DBT*, DBT*); // For secondary, the callback function for associate.  NULL if not secondary
    int associate_is_immutable; // If this DB is a secondary then this field indicates that the index never changes due to updates.
};

#if DB_VERSION_MAJOR == 4 && DB_VERSION_MINOR == 1
typedef void (*toku_env_errcall_t)(const char *, char *);
#elif DB_VERSION_MAJOR == 4 && DB_VERSION_MINOR >= 3
typedef void (*toku_env_errcall_t)(const DB_ENV *, const char *, const char *);
#else
#error
#endif

struct __toku_db_env_internal {
    int is_panicked;
    int ref_count;
    u_int32_t open_flags;
    int open_mode;
    toku_env_errcall_t errcall;
    void *errfile;
    const char *errpfx;
    char *dir;                  /* A malloc'd copy of the directory. */
    char *tmp_dir;
    char *lg_dir;
    char **data_dirs;
    u_int32_t n_data_dirs;
    //void (*noticecall)(DB_ENV *, db_notices);
    unsigned long cachetable_size;
    CACHETABLE cachetable;
    TOKULOGGER logger;
};

struct __toku_db_txn_internal {
    //TXNID txnid64; /* A sixty-four bit txn id. */
    TOKUTXN tokutxn;
    DB_TXN *parent;
};

struct __toku_dbc_internal {
    BRT_CURSOR c;
    DB_TXN *txn;
};

typedef struct __toku_lock_tree {
    DB* db;
    //Some Red Black tree
} toku_lock_tree;


/*
 * Create a lock tree
 * If it is a nodup database, it uses just the regular compare,
 * else it uses both regular and dup compare.
*/
int toku_lock_tree_create(toku_lock_tree** tree, DB* db);

/*
 * Closes/Frees a lock tree.
*/
int toku_lock_tree_close(toku_lock_tree* tree);

/*
 * Obtains a read lock on a single key (or key/value)
 * Returns 0 on success
 * Error: DB_LOCK_NOTGRANTED if cannot get the read lock.
 *        This occurs if someone (else) has a write lock on it or
 *        a region that encompasses it.
 */
int toku_lock_tree_get_read_lock(toku_lock_tree* tree, DB_TXN* txn,
                                 DBT* key, DBT* value);

/*
 * Obtains a read lock on a key (or key/value) range.
 * Returns 0 on success
 * Error: DB_LOCK_NOTGRANTED if cannot get the read lock.
 *        This occurs if someone (else) has a write or read lock in the range
 *        or in an overlapping range.
 */
int toku_lock_tree_get_read_range_lock(toku_lock_tree* tree, DB_TXN* txn,
                                       DBT* key_from, DBT* key_to,
                                       DBT* value_from, DBT* value_to);

/*
 * Obtains a write lock on a single key (or key/value)
 * Returns 0 on success
 * Error: DB_LOCK_NOTGRANTED if cannot get the read lock.
 *        This occurs if someone (else) has a read or write lock on it or
 *        a region that encompasses it.
 */
int toku_lock_tree_get_write_lock(toku_lock_tree* tree, DB_TXN* txn,
                                  DBT* key, DBT* value);

/*
 * Obtains a write lock on a key (or key/value) range.
 * Returns 0 on success
 * Error: DB_LOCK_NOTGRANTED if cannot get the read lock.
 *        This occurs if someone (else) has a read or write lock in the range
 *        or in an overlapping range.
 */
int toku_lock_tree_get_write_range_lock(toku_lock_tree* tree, DB_TXN* txn,
                                        DBT* key_from, DBT* key_to,
                                        DBT* value_from, DBT* value_to);

/*
 * Releases all the locks owned by a transaction.
 * Used when the transaction commits or rolls back.
 */
int toku_lock_tree_free_locks(toku_lock_tree* tree, DB_TXN* txn);

//These are special symbols for top/bottom.
//They can be used both for keys and values.
extern const DBT* toku_dbt_positive_infinity;
extern const DBT* toku_dbt_negative_infinity;
/*
    This will be defined in the lock table .c file.
static const DBT toku_dbt_top_placeholder;
const DBT* toku_dbt_top = &toku_dbt_top_placeholder;
static const DBT toku_dbt_bottom_placeholder;
const DBT* toku_dbt_bottom = &toku_dbt_bottom_placeholder;
*/
//Need some way to represent TOP and BOTTOM, and (key,top) and (key,bottom), so therefore it should be easier
//for the lock tree to understand what keys and values are instead of just a single item.

/*
Whenever a lock fails rollback (just current op) and quit
NODUP in DB
    get
        DEFAULT (in key)
            lock key
            run
        DB_GET_BOTH (in key)
            lock key
            run
    c_get
        DB_CURRENT (in void)
            No Lock (Must already be locked by virtue of cursor pointing to it.)
            run
        DB_FIRST
            run (out key)
            if found    lock bottom<->key
            else        lock bottom<->top
        DB_LAST
            run (out key)
            if found    lock key<->top
            else        lock bottom<->top
        DB_GET_BOTH (in key)
            lock key
            run
        DB_GET_BOTH_RANGE (in key)
            ******** I guess we don't care what the behavior is!  Assuming it acts as DB_GET_BOTH and not DB_SET
            lock key
            run
        DB_NEXT
            if (!initialized) call DB_FIRST, return
            API_getoldcurrent (out o_key)
            run (out key)
            if found    lock o_key<->key
            else        lock o_key<->top
        DB_PREV
            if (!initialized) call DB_LAST, return
            API_getoldcurrent (out o_key)
            run (out key)
            if found    lock key<->o_key
            else        lock bottom<->o_key
        DB_SET (in key)
            lock key
            run
        DB_SET_RANGE (in i_key)
            run (out key)
            if found
                if (i_key != key)   lock i_key<->key
                else                lock i_key
            else
                lock i_key<->top
        DB_NEXT_NODUP   *** Does this return EINVAL?
            call DB_NEXT, return
        DB_NEXT_DUP
            EINVAL? or just return notfound (or if not init, EINVAL)?
        DB_PREV_NODUP
            call DB_PREV, return
    Put
        Default/0 (in key)
            writelock key
            run
        NoOverwrite
            writelock key
            run
            
            optimization alternative
                get
                if found done
                else call YesOverwrite
        YesOverwrite
            writelock key
            run
    Del
        Default/0 (in key)
            writelock key
            run
    
            optimization alternative
                get
                if !found done
                else call DeleteAny
        DeleteAny
            writelock key
            run
    c_del
        API_getoldcurrent (out key)
        writelock key
        run
DUPSORT db
    get
        DEFAULT (in key)
            run (out key, out value)
            if found    lock (key,bottom)<->(key, value)
            else        lock (key,bottom)<->(key, top)
        DB_GET_BOTH (in key, in value)
            lock (key,value)
            run
    c_get
        DB_CURRENT (in void)
            No Lock (Must already be locked by virtue of cursor pointing to it.)
            run
        DB_FIRST
            run (out key, out value)
            if found    lock (bottom,bottom)<->(key,value)
            else        lock (bottom,bottom)<->(top,top)
        DB_LAST
            run (out key, out value)
            if found    lock (key,value)<->(top,top)
            else        lock (bottom,bottom)<->(top,top)
        DB_GET_BOTH (in key, in value)
            lock (key,value)
            run
        DB_GET_BOTH_RANGE (in key, in i_value)
            *** verify it doesn't leave the key
            run (out value)
            if found
                if (value != i_value)   lock (key,i_value)<->(key,value)
                else                    lock (key,value)
            else                        lock (key,i_value)<->(key,top)
        DB_NEXT
            if (!initialized) call DB_FIRST, return
            API_getoldcurrent (out o_key,out o_value)
            run (out key, out value)
            if found    lock (o_key,o_value)<->(key,value)
            else        lock (o_key,o_value)<->(top,top)
        DB_PREV
            if (!initialized) call DB_LAST, return
            API_getoldcurrent (out o_key, out o_value)
            run (out key, out value)
            if found    lock (key,value)<->(o_key,o_value)
            else        lock (bottom,bottom)<->(o_key,o_value)
        DB_SET (in key)
            run (out value)
            if found    lock (key,value)
            else        lock (key,bottom)<->(key,top)
        DB_SET_RANGE (in i_key)
            run (out key, out value)
            if found    lock (i_key, bottom)<->(key,value)
            else        lock (i_key, bottom)<->(top,top)
        DB_NEXT_NODUP   *** Does this return EINVAL?
            ***Alg that overblocks
                if (!initialized) call DB_FIRST, return
                API_getoldcurrent (out o_key,out o_value)
                run (out key, out value)
                if found    lock (o_key,o_value)<->(key,value)
                else        lock (o_key,o_value)<->(top,top)
                
            ***'Optimization to not overblock'.. may require a 'supertop' instead of top however.
                if (!initialized) call DB_FIRST, return
                API_getoldcurrent (out o_key,out o_value)
                run (out key, out value)
                if found    lock (o_key,top)<->(key,value)     ********** Can I just use regular top?  I think so!  Won't hurt reads.. check writes
                else        lock (o_key,top)<->(top,top)       ********** Can I just use regular top?  I think so!  Won't hurt reads.. check wirtes
        DB_NEXT_DUP
            API_getoldcurrent (out o_value)
            run (out key, out value)  //key == o_key so no need for it.
            if found    lock (key,o_value)<->(key,value)
            else        lock (key,o_value)<->(key,top)
        DB_PREV_NODUP
            ***Alg that overblocks
                if (!initialized) call DB_LAST, return
                API_getoldcurrent (out o_key,out o_value)
                run (out key, out value)
                if found    lock (key,value)<->(o_key,o_value)
                else        lock (bottom,bottom)<->(o_key,o_value)
                
            ***'Optimization to not overblock'.. may require a 'superbottom' instead of bottom however.
                if (!initialized) call DB_FIRST, return
                API_getoldcurrent (out o_key,out o_value)
                run (out key, out value)
                if found    lock (key,value)<->(o_key,bottom)       ********** Can I just use regular bottom?  I think so!  Won't hurt reads.. check writes
                else        lock (bottom,bottom)<->(o_key,bottom)   ********** Can I just use regular bottom?  I think so!  Won't hurt reads.. check wirtes
    Put
        Default/0
            EINVAL (EASY)
        NoOverwrite (in key, in value)
            writelock (key,value)
            run
            
            optimization alternative
                get DB_GET_BOTH (key,value)
                if found done
                else call YesOverwrite(key,value)
        YesOverwrite
            writelock (key,value)
            run
    Del
        Default/0 (in key)
            writelock (key,bottom)<->(key,top)  //Technically conflicts with NEXT_NODUP and PREV_NODUP, but ONLY with the same txn,
                                                //so no problem, since anyone holding top/bottom has a read lock inside that key as well.
            run
    
            optimization alternative
                get
                if !found done
                else call DeleteAny
        DeleteAny
            writelock (key,bottom)<->(key,top)  //Technically conflicts with NEXT_NODUP and PREV_NODUP, but ONLY with the same txn,
                                                //so no problem, since anyone holding top/bottom has a read lock inside that key as well.
            run
    c_del
        API_getoldcurrent (out key, out value)
        writelock (key,value)
        run
        
*/


/*
Whenever a lock fails rollback (just current op) and quit

Several forms of locks.
Write Locks
    Exact key (exists in db)
    Exact key (May or may not exist in db)
    open range between two entries (two sides will be exact and exist or not)
Read Locks
    Exact key (exists in db)
    Exact key (May or may not exist in db)
    open range between two entries (two sides will be exact and exist or not)

Locking an exact key inside an open range requires you to split the range.
i.e.
Things 'underlined' with '-'s are read locks.
Things 'underlined' with '+'s are write locks
Our standard database is All letters of the alphabet. (Values are equal to the keys.
We may show a subset of that if it is not necessary for the example.

A B   C
  -(-)

Add lock on B1 changes the lock tree to:

A B   B1   C
  -(-)--(-) 
A


NODUP in DB
    get
        DEFAULT (in key)
            Input: key.  Returns key and value if (key,vale) is in the db.  Else returns DB_NOTFOUND.
            Procedure
                run the regular 'get' call.
                If found, acquire exact read lock on key.
                else, acquire open range at key.
            Ex
                get (B) transforms
                    A   B   C
                to
                    A   B   C
                        -
                
                get (B1) transforms
                    A   B   C
                to
                    A   B   C
                         (-)
        DB_GET_BOTH (in key)
            Input: (key,value).  Retrieves key from db.  If it exists, and tests value found for equality with input value,
            Procedure
                Run the regular 'get' call.
                If found    lock exact key
                else        lock open range for key
                test for equality/return
            Ex
                getDB_GET_BOTH (B,B) transforms
                    (A,A)   (B,B)   (C,C)
                to
                    (A,A)   (B,B)   (C,C)
                            -----
                getDB_GET_BOTH (B,V) transforms
                    (A,A)   (B,B)   (C,C)
                to
                    (A,A)   (B,B)   (C,C)
                            -----
                getDB_GET_BOTH (B1,V) transforms
                    (A,A)   (B,B)   (C,C)
                to
                    (A,A)   (B,B)   (C,C)
                                 (-)
    c_get
        DB_CURRENT (in void)
            Returns what the cursor is already pointing to.
            Note that a single cursor is binded to a single transaction.
            Whatever the cursor points to must already be locked since we managed
            to get a cursor to point to it.
            
            Nothing needs to be done to get this to work.
        DB_FIRST
            Retrieves the minimum element in the db.
            Procedure
                Run c_getDB_FIRST (retrieves key,value)
                if found    lock key
                else        lock entire db (open range from bottom to top)
            Ex
                cgetDB_FIRST transforms
                    A   B   C
                to
                    A   B   C
                    -
                cgetDB_FIRST transforms
                    (empty db)
                to
                    (empty db)
                    (--------)
        DB_LAST
            Retrieves the maximum element in the db.
            Procedure
                Run c_getDB_LAST (retrieves key,value)
                if found    lock key
                else        lock entire db (open range from bottom to top)
            Ex
                cgetDB_LAST transforms
                    A   B   C ..... X   Y   Z
                to
                    A   B   C ..... X   Y   Z
                                            -
                cgetDB_LAST transforms
                    (empty db)
                to
                    (empty db)
                    (--------)
        DB_GET_BOTH (in key)
            Runs getDB_GET_BOTH, and if it is found and value matches, returns a cursor to it.
            See getDB_GET_BOTH.
        DB_GET_BOTH_RANGE (in key)
            For non-dupsort databases, this is the same as c_getDB_GET_BOTH
        DB_NEXT
            On an initialized cursor, does a successor query on the current element.
            On an uninitialized cursor, it calls c_getDB_FIRST
            Note that what the cursor originally points to is locked prior to this call.
            Procedure
                if (!initialized) call DB_FIRST, return
                run cgetDB_NEXT (retrieves key,value)
                if found    lock key, locks open range before key
                else        lock open range at end of db
            Ex
                cgetDB_NEXT transforms 
                    A   (B)   C
                        ---
                to
                    A   B   (C)
                        -(-)---
                cgetDB_NEXT transforms 
                    A   B   (C)
                        -(-)---
                to
                    A   B   C
                        -(-)-(-)
        DB_PREV
            Opposite of cgetDB_NEXT (runs DB_LAST if uninitialized, runs pred query otherwise)
            Note that what the cursor originally points to is locked prior to this call.
            Procedure
                if (!initialized) call DB_LAST, return
                run cgetDB_PREV (retrieves key,value)
                if found    lock key, locks open range after key
                else        lock open range at start of db
            Ex
                cgetDB_PREV transforms 
                    A   (B)   C
                        ---
                to
                    (A)   B   C
                    ---(-)-
                cgetDB_PREV transforms 
                    (A)   B   C
                    ---(-)-
                to
                    A   B   C
                 (-)-(-)-
        DB_SET (in key)
            Runs get, and if it is found, returns a cursor to it.
            See get.
        DB_SET_RANGE (in i_key)
            Retrieves the least upper bound of input (key) as a cursor.
            Procedure
                run cgetDB_SET_RANGE(i_key), retrieves key or returns DB_NOTFOUND
                if found
                    if (i_key != key)   lock key, lock open range before key
                    else                lock key
                else                    lock open range at i_key
            Ex
                cgetDB_SET_RANGE(B) transforms
                    A   B   C
                to
                    A   (B)   C
                        ---
                cgetDB_SET_RANGE(B1) transforms
                    A   B   C
                to
                    A   B   (C)
                         (-)---
                cgetDB_SET_RANGE(Z1) transforms
                    A   ... X   Y   Z
                to
                    A   ... X   Y   Z
                                     (-)
        DB_NEXT_NODUP
            Same as cgetDB_NEXT (for non dupsort dbs)
            call DB_NEXT, return
        DB_NEXT_DUP
            returns EINVAL for non dupsort dbs
        DB_PREV_NODUP
            Same as cgetDB_PREV (for non dupsort dbs)
    Put
        Default/0 (in key)
            Identical to putDB_YESOVERWRITE for non dupsort dbs.
        DB_NODUPDATA
            (Currently not implemented)
            Invalid for non dupsort dbs.
        DB_NOOVERWRITE
            Inserts (key,value) into the db if the key does not exist in the db.  Returns error otherwise.
            Procedure
                call get (key) (This may create read locks)
                If found, return DB_KEYEXISTS
                else    call putDB_YESOVERWRITE
            Ex
                putDB_NOOVERWRITE (A) transforms
                    A   B   C
                to
                    A   B   C
                    -
                and returns DB_KEYEXIST
                
                putDB_NOOVERWRITE (A1) transforms
                    A   B   C
                to
                    A   A1  B   C
                        --
                        ++
        DB_YESOVERWRITE
            Inserts (key,value) into the db.  If it already exists, it will overwrite it.
            Procedure
                writelock key
                Run putDB_YESOVERWRITE
            Ex
                putDB_YESOVERWRITE (A) transforms
                    A   B   C
                to
                    A   B   C
                    +
                putDB_YESOVERWRITE (A1) transforms
                    A   B   C
                to
                    A   A1  B   C
                        ++
    Del
        Default/0 (in key)
            Delete key, but report whether it was there in the first place.
            Procedure
                call get (key) (This may create read locks)
                if !found, return DB_NOTFOUND
                else    call delDB_DELETE_ANY
            Ex
                del(A) transforms
                    A   B   C
                to
                    [A]   B   C     [] in this case means it is not in the db.
                    ---
                    +++
                
                del(A1) transforms
                    A   B   C
                to
                    A   B   C
                     (-)
        DB_DELETE_ANY
            Delete key from db.  Do not report (thus saving a search).
            Procedure
                writelock key
                run the actual delete
            delDB_DELETE_ANY (A) transforms
                A   B   C
            to
                [A] B   C
                +++
            delDB_DELETE_ANY (A1) transforms
                A   B   C
            to
                A   [A1]    B   C
                    ++++
    c_del
        Deletes what a cursor is pointing to from the db.
        Procedure
            API_getoldcurrent (out key)  (gets the old value of the key)
            writelock key
            delete it.
        Ex
            c_del transforms
                A   (B) C
                    ---
            to
                A   ([B])   C
                    +++++
DUPSORT db
    get
        DEFAULT (in key)
            Searches for the lowest value for the given key.  (Returns key,value if found) else returns DB_NOTFOUND.
            Procedure
                run the regular 'get' call.
                If found, acquire exact read lock on (key,value) and open range before
                else, acquire open range at (key,value).
                
                NOTE: Since it finds the first one.. do we also need to lock the open range between (key,bottom) and (key,value)???
            Ex
                get (B) transforms
                    A,V   B,V   C,V
                to
                    A,V   B,V   C,V
                       (-)---
                
                get (B1) transforms
                    A   B   C
                to
                    A   B   C
                         (-)
        DB_GET_BOTH (in key)
            Input: (key,value).  Retrieves key from db.  If it exists, and tests value found for equality with input value,
            Procedure
                Run the regular 'get' call.
                If found    lock exact key
                else        lock open range for key
                test for equality/return
            Ex
                getDB_GET_BOTH (B,B) transforms
                    (A,A)   (B,B)   (C,C)
                to
                    (A,A)   (B,B)   (C,C)
                            -----
                getDB_GET_BOTH (B,V) transforms
                    (A,A)   (B,B)   (C,C)
                to
                    (A,A)   (B,B)   (C,C)
                            -----
                getDB_GET_BOTH (B1,V) transforms
                    (A,A)   (B,B)   (C,C)
                to
                    (A,A)   (B,B)   (C,C)
                                 (-)
    c_get
        DB_CURRENT (in void)
            Returns what the cursor is already pointing to.
            Note that a single cursor is binded to a single transaction.
            Whatever the cursor points to must already be locked since we managed
            to get a cursor to point to it.
            
            Nothing needs to be done to get this to work.
        DB_FIRST
            Retrieves the minimum element in the db.
            Procedure
                Run c_getDB_FIRST (retrieves key,value)
                if found    lock key
                else        lock entire db (open range from bottom to top)
            Ex
                cgetDB_FIRST transforms
                    A   B   C
                to
                    A   B   C
                    -
                cgetDB_FIRST transforms
                    (empty db)
                to
                    (empty db)
                    (--------)
        DB_LAST
            Retrieves the maximum element in the db.
            Procedure
                Run c_getDB_LAST (retrieves key,value)
                if found    lock key
                else        lock entire db (open range from bottom to top)
            Ex
                cgetDB_LAST transforms
                    A   B   C ..... X   Y   Z
                to
                    A   B   C ..... X   Y   Z
                                            -
                cgetDB_LAST transforms
                    (empty db)
                to
                    (empty db)
                    (--------)
        DB_GET_BOTH (in key)
            Runs getDB_GET_BOTH, and if it is found and value matches, returns a cursor to it.
            See getDB_GET_BOTH.
        DB_GET_BOTH_RANGE (in key)   ************************* 3 cases.. did not find ->read open interval, found exact->lock exact, found different ->lock exact and open before
            For non-dupsort databases, this is the same as c_getDB_GET_BOTH
        DB_NEXT
            On an initialized cursor, does a successor query on the current element.
            On an uninitialized cursor, it calls c_getDB_FIRST
            Note that what the cursor originally points to is locked prior to this call.
            Procedure
                if (!initialized) call DB_FIRST, return
                run cgetDB_NEXT (retrieves key,value)
                if found    lock key, locks open range before key
                else        lock open range at end of db
            Ex
                cgetDB_NEXT transforms 
                    A   (B)   C
                        ---
                to
                    A   B   (C)
                        -(-)---
                cgetDB_NEXT transforms 
                    A   B   (C)
                        -(-)---
                to
                    A   B   C
                        -(-)-(-)
        DB_PREV
            Opposite of cgetDB_NEXT (runs DB_LAST if uninitialized, runs pred query otherwise)
            Note that what the cursor originally points to is locked prior to this call.
            Procedure
                if (!initialized) call DB_LAST, return
                run cgetDB_PREV (retrieves key,value)
                if found    lock key, locks open range after key
                else        lock open range at start of db
            Ex
                cgetDB_PREV transforms 
                    A   (B)   C
                        ---
                to
                    (A)   B   C
                    ---(-)-
                cgetDB_PREV transforms 
                    (A)   B   C
                    ---(-)-
                to
                    A   B   C
                 (-)-(-)-
        DB_SET (in key)
            Runs get, and if it is found, returns a cursor to it.
            See get.
        DB_SET_RANGE (in i_key)  **************ALWAYS key AND open range
            Retrieves the least upper bound of input (key) as a cursor.
            Procedure
                run cgetDB_SET_RANGE(i_key), retrieves key or returns DB_NOTFOUND
                if found
                    if (i_key != key)   lock key, lock open range before key
                    else                lock key
                else                    lock open range at i_key
            Ex
                cgetDB_SET_RANGE(B) transforms
                    A   B   C
                to
                    A   (B)   C
                        ---
                cgetDB_SET_RANGE(B1) transforms
                    A   B   C
                to
                    A   B   (C)
                         (-)---
                cgetDB_SET_RANGE(Z1) transforms
                    A   ... X   Y   Z
                to
                    A   ... X   Y   Z
                                     (-)
        DB_NEXT_NODUP ***** lock the exact, and open range from new till prev(new).
            Same as cgetDB_NEXT (for non dupsort dbs)
            call DB_NEXT, return
        DB_NEXT_DUP  ***** same as DB_NEXT (for nodups) except if key of successor is different, lock open range.
            returns EINVAL for non dupsort dbs
        DB_PREV_NODUP  *** trivially create from DB_NEXT_NODUP
            Same as cgetDB_PREV (for non dupsort dbs)
    Put
        Default/0 (in key)  *** EINVAL
            Identical to putDB_YESOVERWRITE for non dupsort dbs.
        DB_NODUPDATA **** delete cursor put NO_DUPDATA
            (Currently not implemented)
            Invalid for non dupsort dbs.
        DB_NOOVERWRITE   *** if exists, lock ANY (probably first) and return.
            Inserts (key,value) into the db if the key does not exist in the db.  Returns error otherwise.
            Procedure
                call get (key) (This may create read locks)
                If found, return DB_KEYEXISTS
                else    call putDB_YESOVERWRITE
            Ex
                putDB_NOOVERWRITE (A) transforms
                    A   B   C
                to
                    A   B   C
                    -
                and returns DB_KEYEXIST
                
                putDB_NOOVERWRITE (A1) transforms
                    A   B   C
                to
                    A   A1  B   C
                        --
                        ++
        DB_YESOVERWRITE *** 'same' as nodups 
            Inserts (key,value) into the db.  If it already exists, it will overwrite it.
            Procedure
                writelock key
                Run putDB_YESOVERWRITE
            Ex
                putDB_YESOVERWRITE (A) transforms
                    A   B   C
                to
                    A   B   C
                    +
                putDB_YESOVERWRITE (A1) transforms
                    A   B   C
                to
                    A   A1  B   C
                        ++
    Del
        Default/0 (in key)  *** find first 'b', then do pred, and then do nextnodup (don't use infinities yet)
            Delete key, but report whether it was there in the first place.
            Procedure
                call get (key) (This may create read locks)
                if !found, return DB_NOTFOUND
                else    call delDB_DELETE_ANY
            Ex
                del(A) transforms
                    A   B   C
                to
                    [A]   B   C     [] in this case means it is not in the db.
                    ---
                    +++
                
                del(A1) transforms
                    A   B   C
                to
                    A   B   C
                     (-)
        DB_DELETE_ANY **** need infinities. lock infinities
            Delete key from db.  Do not report (thus saving a search).
            Procedure
                writelock key
                run the actual delete
            delDB_DELETE_ANY (A) transforms
                A   B   C
            to
                [A] B   C
                +++
            delDB_DELETE_ANY (A1) transforms
                A   B   C
            to
                A   [A1]    B   C
                    ++++
    c_del *** same as nodups
        Deletes what a cursor is pointing to from the db.
        Procedure
            API_getoldcurrent (out key)  (gets the old value of the key)
            writelock key
            delete it.
        Ex
            c_del transforms
                A   (B) C
                    ---
            to
                A   ([B])   C
                    +++++
*/



#endif

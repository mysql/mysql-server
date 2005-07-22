/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1997-2004
 *	Sleepycat Software.  All rights reserved.
 *
 * $Id: TestLockVec.java,v 1.7 2004/01/28 03:36:34 bostic Exp $
 */

/*
 * test of DbEnv.lock_vec()
 */

package com.sleepycat.test;

import com.sleepycat.db.*;
import java.io.FileNotFoundException;

public class TestLockVec
{
    public static int locker1;
    public static int locker2;

    public static void gdb_pause()
    {
        try {
            System.err.println("attach gdb and type return...");
            System.in.read(new byte[10]);
        }
        catch (java.io.IOException ie) {
        }
    }

    public static void main(String[] args)
    {
        try {
            DbEnv dbenv1 = new DbEnv(0);
            DbEnv dbenv2 = new DbEnv(0);
            dbenv1.open(".",
                       Db.DB_CREATE | Db.DB_INIT_LOCK | Db.DB_INIT_MPOOL, 0);
            dbenv2.open(".",
                       Db.DB_CREATE | Db.DB_INIT_LOCK | Db.DB_INIT_MPOOL, 0);
            locker1 = dbenv1.lockId();
            locker2 = dbenv1.lockId();
            Db db1 = new Db(dbenv1, 0);
            db1.open(null, "my.db", null, Db.DB_BTREE, Db.DB_CREATE, 0);
            Db db2 = new Db(dbenv2, 0);
            db2.open(null, "my.db", null, Db.DB_BTREE, 0, 0);

            // populate our database, just two elements.
            Dbt Akey = new Dbt("A".getBytes());
            Dbt Adata = new Dbt("Adata".getBytes());
            Dbt Bkey = new Dbt("B".getBytes());
            Dbt Bdata = new Dbt("Bdata".getBytes());

            // We don't allow Dbts to be reused within the
            // same method call, so we need some duplicates.
            Dbt Akeyagain = new Dbt("A".getBytes());
            Dbt Bkeyagain = new Dbt("B".getBytes());

            db1.put(null, Akey, Adata, 0);
            db1.put(null, Bkey, Bdata, 0);

            Dbt notInDatabase = new Dbt("C".getBytes());

            /* make sure our check mechanisms work */
            int expectedErrs = 0;

            lock_check_free(dbenv2, Akey);
            try {
                lock_check_held(dbenv2, Bkey, Db.DB_LOCK_READ);
            }
            catch (DbException dbe1) {
                expectedErrs += 1;
            }
            DbLock tmplock = dbenv1.lockGet(locker1, Db.DB_LOCK_NOWAIT,
                                            Akey, Db.DB_LOCK_READ);
            lock_check_held(dbenv2, Akey, Db.DB_LOCK_READ);
            try {
                lock_check_free(dbenv2, Akey);
            }
            catch (DbException dbe2) {
                expectedErrs += 2;
            }
            if (expectedErrs != 1+2) {
                System.err.println("lock check mechanism is broken");
                System.exit(1);
            }
            dbenv1.lockPut(tmplock);

            /* Now on with the test, a series of lock_vec requests,
             * with checks between each call.
             */

            System.out.println("get a few");
            /* Request: get A(W), B(R), B(R) */
            DbLockRequest[] reqs = new DbLockRequest[3];

            reqs[0] = new DbLockRequest(Db.DB_LOCK_GET, Db.DB_LOCK_WRITE,
                                        Akey, null);
            reqs[1] = new DbLockRequest(Db.DB_LOCK_GET, Db.DB_LOCK_READ,
                                        Bkey, null);
            reqs[2] = new DbLockRequest(Db.DB_LOCK_GET, Db.DB_LOCK_READ,
                                        Bkeyagain, null);

            dbenv1.lockVector(locker1, Db.DB_LOCK_NOWAIT, reqs, 0, 3);

            /* Locks held: A(W), B(R), B(R) */
            lock_check_held(dbenv2, Bkey, Db.DB_LOCK_READ);
            lock_check_held(dbenv2, Akey, Db.DB_LOCK_WRITE);

            System.out.println("put a couple");
            /* Request: put A, B(first) */
            reqs[0].setOp(Db.DB_LOCK_PUT);
            reqs[1].setOp(Db.DB_LOCK_PUT);

            dbenv1.lockVector(locker1, Db.DB_LOCK_NOWAIT, reqs, 0, 2);

            /* Locks held: B(R) */
            lock_check_free(dbenv2, Akey);
            lock_check_held(dbenv2, Bkey, Db.DB_LOCK_READ);

            System.out.println("put one more, test index offset");
            /* Request: put B(second) */
            reqs[2].setOp(Db.DB_LOCK_PUT);

            dbenv1.lockVector(locker1, Db.DB_LOCK_NOWAIT, reqs, 2, 1);

            /* Locks held: <none> */
            lock_check_free(dbenv2, Akey);
            lock_check_free(dbenv2, Bkey);

            System.out.println("get a few");
            /* Request: get A(R), A(R), B(R) */
            reqs[0] = new DbLockRequest(Db.DB_LOCK_GET, Db.DB_LOCK_READ,
                                        Akey, null);
            reqs[1] = new DbLockRequest(Db.DB_LOCK_GET, Db.DB_LOCK_READ,
                                        Akeyagain, null);
            reqs[2] = new DbLockRequest(Db.DB_LOCK_GET, Db.DB_LOCK_READ,
                                        Bkey, null);
            dbenv1.lockVector(locker1, Db.DB_LOCK_NOWAIT, reqs, 0, 3);

            /* Locks held: A(R), B(R), B(R) */
            lock_check_held(dbenv2, Akey, Db.DB_LOCK_READ);
            lock_check_held(dbenv2, Bkey, Db.DB_LOCK_READ);

            System.out.println("try putobj");
            /* Request: get B(R), putobj A */
            reqs[1] = new DbLockRequest(Db.DB_LOCK_GET, Db.DB_LOCK_READ,
                                        Bkey, null);
            reqs[2] = new DbLockRequest(Db.DB_LOCK_PUT_OBJ, 0,
                                        Akey, null);
            dbenv1.lockVector(locker1, Db.DB_LOCK_NOWAIT, reqs, 1, 2);

            /* Locks held: B(R), B(R) */
            lock_check_free(dbenv2, Akey);
            lock_check_held(dbenv2, Bkey, Db.DB_LOCK_READ);

            System.out.println("get one more");
            /* Request: get A(W) */
            reqs[0] = new DbLockRequest(Db.DB_LOCK_GET, Db.DB_LOCK_WRITE,
                                        Akey, null);
            dbenv1.lockVector(locker1, Db.DB_LOCK_NOWAIT, reqs, 0, 1);

            /* Locks held: A(W), B(R), B(R) */
            lock_check_held(dbenv2, Akey, Db.DB_LOCK_WRITE);
            lock_check_held(dbenv2, Bkey, Db.DB_LOCK_READ);

            System.out.println("putall");
            /* Request: putall */
            reqs[0] = new DbLockRequest(Db.DB_LOCK_PUT_ALL, 0,
                                        null, null);
            dbenv1.lockVector(locker1, Db.DB_LOCK_NOWAIT, reqs, 0, 1);

            lock_check_free(dbenv2, Akey);
            lock_check_free(dbenv2, Bkey);
            db1.close(0);
            dbenv1.close(0);
            db2.close(0);
            dbenv2.close(0);
            System.out.println("done");
        }
        catch (DbLockNotGrantedException nge) {
            System.err.println("Db Exception: " + nge);
        }
        catch (DbException dbe) {
            System.err.println("Db Exception: " + dbe);
        }
        catch (FileNotFoundException fnfe) {
            System.err.println("FileNotFoundException: " + fnfe);
        }

    }

    /* Verify that the lock is free, throw an exception if not.
     * We do this by trying to grab a write lock (no wait).
     */
    static void lock_check_free(DbEnv dbenv, Dbt dbt)
        throws DbException
    {
        DbLock tmplock = dbenv.lockGet(locker2, Db.DB_LOCK_NOWAIT,
                                       dbt, Db.DB_LOCK_WRITE);
        dbenv.lockPut(tmplock);
    }

    /* Verify that the lock is held with the mode, throw an exception if not.
     * If we have a write lock, we should not be able to get the lock
     * for reading.  If we have a read lock, we should be able to get
     * it for reading, but not writing.
     */
    static void lock_check_held(DbEnv dbenv, Dbt dbt, int mode)
        throws DbException
    {
        DbLock never = null;

        try {
            if (mode == Db.DB_LOCK_WRITE) {
                never = dbenv.lockGet(locker2, Db.DB_LOCK_NOWAIT,
                                      dbt, Db.DB_LOCK_READ);
            }
            else if (mode == Db.DB_LOCK_READ) {
                DbLock rlock = dbenv.lockGet(locker2, Db.DB_LOCK_NOWAIT,
                                             dbt, Db.DB_LOCK_READ);
                dbenv.lockPut(rlock);
                never = dbenv.lockGet(locker2, Db.DB_LOCK_NOWAIT,
                                      dbt, Db.DB_LOCK_WRITE);
            }
            else {
                throw new DbException("lock_check_held bad mode");
            }
        }
        catch (DbLockNotGrantedException nge) {
            /* We expect this on our last lock_get call */
        }

        /* make sure we failed */
        if (never != null) {
          try {
            dbenv.lockPut(never);
          }
          catch (DbException dbe2) {
            System.err.println("Got some real troubles now");
            System.exit(1);
          }
          throw new DbException("lock_check_held: lock was not held");
        }
    }

}

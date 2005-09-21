/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2000-2004
 *	Sleepycat Software.  All rights reserved.
 *
 * $Id: TestGetSetMethods.java,v 1.7 2004/01/28 03:36:34 bostic Exp $
 */

/*
 * Do some regression tests for simple get/set access methods
 * on DbEnv, DbTxn, Db.  We don't currently test that they have
 * the desired effect, only that they operate and return correctly.
 */
package com.sleepycat.test;

import com.sleepycat.db.*;
import java.io.FileNotFoundException;

public class TestGetSetMethods
{
    public void testMethods()
        throws DbException, FileNotFoundException
    {
        DbEnv dbenv = new DbEnv(0);
        DbTxn dbtxn;
        byte[][] conflicts = new byte[10][10];

        dbenv.setTimeout(0x90000000,
                          Db.DB_SET_LOCK_TIMEOUT);
        dbenv.setLogBufferSize(0x1000);
        dbenv.setLogDir(".");
        dbenv.setLogMax(0x10000000);
        dbenv.setLogRegionMax(0x100000);
        dbenv.setLockConflicts(conflicts);
        dbenv.setLockDetect(Db.DB_LOCK_DEFAULT);
        // exists, but is deprecated:
        // dbenv.set_lk_max(0);
        dbenv.setLockMaxLockers(100);
        dbenv.setLockMaxLocks(10);
        dbenv.setLockMaxObjects(1000);
        dbenv.setMemoryPoolMapSize(0x10000);
        dbenv.setTestAndSetSpins(1000);

        // Need to open the environment so we
        // can get a transaction.
        //
        dbenv.open(".", Db.DB_CREATE | Db.DB_INIT_TXN |
                   Db.DB_INIT_LOCK | Db.DB_INIT_LOG |
                   Db.DB_INIT_MPOOL,
                   0644);

        dbtxn = dbenv.txnBegin(null, Db.DB_TXN_NOWAIT);
        dbtxn.setTimeout(0xA0000000, Db.DB_SET_TXN_TIMEOUT);
        dbtxn.abort();

        dbenv.close(0);

        // We get a db, one for each type.
        // That's because once we call (for instance)
        // setBtreeMinKey, DB 'knows' that this is a
        // Btree Db, and it cannot be used to try Hash
        // or Recno functions.
        //
        Db db_bt = new Db(null, 0);
        db_bt.setBtreeMinKey(100);
        db_bt.setCacheSize(0x100000, 0);
        db_bt.close(0);

        Db db_h = new Db(null, 0);
        db_h.setHashFillFactor(0x10);
        db_h.setHashNumElements(100);
        db_h.setByteOrder(0);
        db_h.setPageSize(0x10000);
        db_h.close(0);

        Db db_re = new Db(null, 0);
        db_re.setRecordDelimiter('@');
        db_re.setRecordPad(10);
        db_re.setRecordSource("re.in");
        db_re.setRecordLength(1000);
        db_re.close(0);

        Db db_q = new Db(null, 0);
        db_q.setQueueExtentSize(200);
        db_q.close(0);
    }

    public static void main(String[] args)
    {
        try {
            TestGetSetMethods tester = new TestGetSetMethods();
            tester.testMethods();
        }
        catch (Exception e) {
            System.err.println("TestGetSetMethods: Exception: " + e);
        }
    }
}

/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1997-2004
 *	Sleepycat Software.  All rights reserved.
 *
 * $Id: TestStat.java,v 1.10 2004/01/28 03:36:34 bostic Exp $
 */

/*
 * Simple test for get/put of specific values.
 */

package com.sleepycat.test;

import com.sleepycat.db.*;
import java.io.FileNotFoundException;

public class TestStat
{
    public static void main(String[] args)
    {
        int envflags =
            Db.DB_INIT_MPOOL | Db.DB_INIT_LOCK | Db.DB_INIT_LOG |
            Db.DB_INIT_REP | Db.DB_INIT_TXN | Db.DB_CREATE;
        try {
            DbEnv dbenv = new DbEnv(0);
            dbenv.open(".", envflags, 0);

            // Use a separate environment that has no activity
            // to do the replication stats.  We don't want to get
            // into configuring a real replication environment here.
            DbEnv repenv = new DbEnv(0);
            repenv.open(".", envflags, 0);

            // Keep a couple transactions open so DbTxnStat active
            // array will have some entries.
            DbTxn dbtxn1 = dbenv.txnBegin(null, 0);
            DbTxn dbtxn2 = dbenv.txnBegin(dbtxn1, 0);
            Db db = new Db(dbenv, 0);
            db.open(null, "my.db", null, Db.DB_BTREE, Db.DB_CREATE, 0);

            TestUtil.populate(db);
            System.out.println("BtreeStat:");
            DbBtreeStat stat = (DbBtreeStat)db.stat(0);
            System.out.println("  bt_magic: " + stat.bt_magic);

            System.out.println("LogStat:");
            DbLogStat logstat = dbenv.logStat(0);
            System.out.println("  st_magic: " + logstat.st_magic);
            System.out.println("  st_cur_file: " + logstat.st_cur_file);

            System.out.println("TxnStat:");
            DbTxnStat txnstat = dbenv.txnStat(0);
            System.out.println("  st_ncommits: " + txnstat.st_ncommits);
            System.out.println("  st_nactive: " + txnstat.st_nactive);

            DbTxnStat.Active active0 = txnstat.st_txnarray[0];
            DbTxnStat.Active active1 = txnstat.st_txnarray[1];
            if (active0.txnid != active1.parentid &&
                active1.txnid != active0.parentid) {
              System.out.println("Missing PARENT/CHILD txn relationship:");
              System.out.println("  st_active[0].txnid: " + active0.txnid);
              System.out.println("  st_active[0].parentid: " +
                                 active0.parentid);
              System.out.println("  st_active[1].txnid: " + active1.txnid);
              System.out.println("  st_active[1].parentid: " +
                                 active1.parentid);
            }

	    System.out.println("DbMpoolStat:");
	    DbMpoolStat mpstat = dbenv.memoryPoolStat(0);
	    System.out.println("  st_gbytes: " + mpstat.st_gbytes);

	    System.out.println("DbMpoolFileStat:");
	    DbMpoolFStat[] mpfstat = dbenv.memoryPoolFileStat(0);
	    System.out.println("  num files: " + mpfstat.length);

            System.out.println("RepStat:");
            DbRepStat repstat = repenv.replicationStat(0);
            System.out.println("  st_status: " + repstat.st_status);
            System.out.println("  st_log_duplication: " +
                               repstat.st_log_duplicated);

            System.out.println("finished test");
        }
        catch (DbException dbe) {
            System.err.println("Db Exception: " + dbe);
        }
        catch (FileNotFoundException fnfe) {
            System.err.println("FileNotFoundException: " + fnfe);
        }
    }
}

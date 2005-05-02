/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1997-2002
 *	Sleepycat Software.  All rights reserved.
 *
 * $Id: TestStat.java,v 1.1 2002/08/16 19:35:56 dda Exp $
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
            Db.DB_INIT_MPOOL | Db.DB_INIT_LOCK |
            Db.DB_INIT_LOG | Db.DB_INIT_TXN | Db.DB_CREATE;
        try {
            DbEnv dbenv = new DbEnv(0);
            dbenv.open(".", envflags, 0);
            Db db = new Db(dbenv, 0);
            db.open(null, "my.db", null, Db.DB_BTREE, Db.DB_CREATE, 0);

            TestUtil.populate(db);
            System.out.println("BtreeStat:");
            DbBtreeStat stat = (DbBtreeStat)db.stat(0);
            System.out.println("  bt_magic: " + stat.bt_magic);

            System.out.println("LogStat:");
            DbLogStat logstat = dbenv.log_stat(0);
            System.out.println("  st_magic: " + logstat.st_magic);
            System.out.println("  st_cur_file: " + logstat.st_cur_file);

            System.out.println("RepStat:");
            DbRepStat repstat = dbenv.rep_stat(0);
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

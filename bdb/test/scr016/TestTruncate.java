/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1997-2002
 *	Sleepycat Software.  All rights reserved.
 *
 * $Id: TestTruncate.java,v 1.4 2002/01/23 14:29:52 bostic Exp $
 */

/*
 * Simple test for get/put of specific values.
 */

package com.sleepycat.test;

import com.sleepycat.db.*;
import java.io.FileNotFoundException;

public class TestTruncate
{
    public static void main(String[] args)
    {
        try {
            Db db = new Db(null, 0);
            db.open(null, "my.db", null, Db.DB_BTREE, Db.DB_CREATE, 0644);

            // populate our massive database.
            Dbt keydbt = new Dbt("key".getBytes());
            Dbt datadbt = new Dbt("data".getBytes());
            db.put(null, keydbt, datadbt, 0);

            // Now, retrieve.  We could use keydbt over again,
            // but that wouldn't be typical in an application.
            Dbt goodkeydbt = new Dbt("key".getBytes());
            Dbt badkeydbt = new Dbt("badkey".getBytes());
            Dbt resultdbt = new Dbt();
            resultdbt.set_flags(Db.DB_DBT_MALLOC);

            int ret;

            if ((ret = db.get(null, goodkeydbt, resultdbt, 0)) != 0) {
                System.out.println("get: " + DbEnv.strerror(ret));
            }
            else {
                String result =
                    new String(resultdbt.get_data(), 0, resultdbt.get_size());
                System.out.println("got data: " + result);
            }

            if ((ret = db.get(null, badkeydbt, resultdbt, 0)) != 0) {
                // We expect this...
                System.out.println("get using bad key: " + DbEnv.strerror(ret));
            }
            else {
                String result =
                    new String(resultdbt.get_data(), 0, resultdbt.get_size());
                System.out.println("*** got data using bad key!!: " + result);
            }

            // Now, truncate and make sure that it's really gone.
            System.out.println("truncating data...");
            int nrecords = db.truncate(null, 0);
            System.out.println("truncate returns " + nrecords);
            if ((ret = db.get(null, goodkeydbt, resultdbt, 0)) != 0) {
                // We expect this...
                System.out.println("after trunctate get: " +
                                   DbEnv.strerror(ret));
            }
            else {
                String result =
                    new String(resultdbt.get_data(), 0, resultdbt.get_size());
                System.out.println("got data: " + result);
            }

            db.close(0);
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

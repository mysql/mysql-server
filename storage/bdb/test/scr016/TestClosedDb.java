/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1997-2004
 *	Sleepycat Software.  All rights reserved.
 *
 * $Id: TestClosedDb.java,v 1.8 2004/01/28 03:36:34 bostic Exp $
 */

/*
 * Close the Db, and make sure operations after that fail gracefully.
 */

package com.sleepycat.test;

import com.sleepycat.db.*;
import java.io.FileNotFoundException;

public class TestClosedDb
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
            resultdbt.setFlags(Db.DB_DBT_MALLOC);

            int ret;

            // Close the db - subsequent operations should fail
            // by throwing an exception.
            db.close(0);
            try {
                db.get(null, goodkeydbt, resultdbt, 0);
                System.out.println("Error - did not expect to get this far.");
            }
            catch (IllegalArgumentException dbe) {
                System.out.println("Got expected exception: " + dbe);
            }
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

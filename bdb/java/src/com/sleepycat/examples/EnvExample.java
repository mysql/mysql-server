/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1997, 1998, 1999, 2000
 *	Sleepycat Software.  All rights reserved.
 *
 * $Id: EnvExample.java,v 11.7 2000/09/25 13:16:51 dda Exp $
 */

package com.sleepycat.examples;

import com.sleepycat.db.*;
import java.io.FileNotFoundException;
import java.io.OutputStream;

/*
 * An example of a program using DbEnv to configure its DB
 * environment.
 *
 * For comparison purposes, this example uses a similar structure
 * as examples/ex_env.c and examples_cxx/EnvExample.cpp.
 */
public class EnvExample
{
    private static final String progname = "EnvExample";
    private static final String DATABASE_HOME = "/tmp/database";

    private static void db_application()
        throws DbException
    {
        // Do something interesting...
        // Your application goes here.
    }

    private static void db_setup(String home, String data_dir,
                                  OutputStream errs)
         throws DbException, FileNotFoundException
    {
        //
        // Create an environment object and initialize it for error
        // reporting.
        //
        DbEnv dbenv = new DbEnv(0);
        dbenv.set_error_stream(errs);
        dbenv.set_errpfx(progname);

        //
        // We want to specify the shared memory buffer pool cachesize,
        // but everything else is the default.
        //
        dbenv.set_cachesize(0, 64 * 1024, 0);

	// Databases are in a subdirectory.
	dbenv.set_data_dir(data_dir);

	// Open the environment with full transactional support.
        //
        // open() will throw a DbException if there is an error.
        //
        // open is declared to throw a FileNotFoundException, which normally
        // shouldn't occur with the DB_CREATE option.
        //
        dbenv.open(DATABASE_HOME,
                   Db.DB_CREATE | Db.DB_INIT_LOCK | Db.DB_INIT_LOG |
                   Db.DB_INIT_MPOOL | Db.DB_INIT_TXN, 0);

        try {

            // Start your application.
            db_application();

        }
        finally {

            // Close the environment.  Doing this in the
            // finally block ensures it is done, even if
            // an error is thrown.
            //
            dbenv.close(0);
        }
    }

    private static void db_teardown(String home, String data_dir,
                                    OutputStream errs)
         throws DbException, FileNotFoundException
    {
	// Remove the shared database regions.

        DbEnv dbenv = new DbEnv(0);

        dbenv.set_error_stream(errs);
        dbenv.set_errpfx(progname);
	dbenv.set_data_dir(data_dir);
        dbenv.remove(home, 0);
    }

    public static void main(String[] args)
    {
        //
        // All of the shared database files live in /tmp/database,
        // but data files live in /database.
        //
        // Using Berkeley DB in C/C++, we need to allocate two elements
        // in the array and set config[1] to NULL.  This is not
        // necessary in Java.
        //
        String home = DATABASE_HOME;
        String config = "/database/files";

        try {
            System.out.println("Setup env");
            db_setup(home, config, System.err);

            System.out.println("Teardown env");
            db_teardown(home, config, System.err);
        }
        catch (DbException dbe) {
            System.err.println(progname + ": environment open: " + dbe.toString());
            System.exit (1);
        }
        catch (FileNotFoundException fnfe) {
            System.err.println(progname +
                               ": unexpected open environment error  " + fnfe);
            System.exit (1);
        }
    }

}

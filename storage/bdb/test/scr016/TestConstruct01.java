/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2000-2004
 *	Sleepycat Software.  All rights reserved.
 *
 * $Id: TestConstruct01.java,v 1.10 2004/01/28 03:36:34 bostic Exp $
 */

/*
 * Do some regression tests for constructors.
 * Run normally (without arguments) it is a simple regression test.
 * Run with a numeric argument, it repeats the regression a number
 * of times, to try to determine if there are memory leaks.
 */

package com.sleepycat.test;

import com.sleepycat.db.*;
import java.io.File;
import java.io.IOException;
import java.io.FileNotFoundException;

public class TestConstruct01
{
    public static final String CONSTRUCT01_DBNAME  =       "construct01.db";
    public static final String CONSTRUCT01_DBDIR   =       "/tmp";
    public static final String CONSTRUCT01_DBFULLPATH   =
	CONSTRUCT01_DBDIR + "/" + CONSTRUCT01_DBNAME;

    private int itemcount;	// count the number of items in the database
    public static boolean verbose_flag = false;

    public static void ERR(String a)
    {
	System.out.println("FAIL: " + a);
	System.err.println("FAIL: " + a);
	sysexit(1);
    }

    public static void DEBUGOUT(String s)
    {
	System.out.println(s);
    }

    public static void VERBOSEOUT(String s)
    {
        if (verbose_flag)
            System.out.println(s);
    }

    public static void sysexit(int code)
    {
	System.exit(code);
    }

    private static void check_file_removed(String name, boolean fatal,
					   boolean force_remove_first)
    {
	File f = new File(name);
	if (force_remove_first) {
	    f.delete();
	}
	if (f.exists()) {
	    if (fatal)
		System.out.print("FAIL: ");
	    System.out.print("File \"" + name + "\" still exists after run\n");
	    if (fatal)
		sysexit(1);
	}
    }


    // Check that key/data for 0 - count-1 are already present,
    // and write a key/data for count.  The key and data are
    // both "0123...N" where N == count-1.
    //
    // For some reason on Windows, we need to open using the full pathname
    // of the file when there is no environment, thus the 'has_env'
    // variable.
    //
    void rundb(Db db, int count, boolean has_env, TestOptions options)
	throws DbException, FileNotFoundException
    {
	String name;

	if (has_env)
	    name = CONSTRUCT01_DBNAME;
	else
	    name = CONSTRUCT01_DBFULLPATH;

	db.set_error_stream(System.err);

	// We don't really care about the pagesize, but we do want
	// to make sure adjusting Db specific variables works before
	// opening the db.
	//
	db.set_pagesize(1024);
	db.open(null, name, null, Db.DB_BTREE,
		(count != 0) ? 0 : Db.DB_CREATE, 0664);


	// The bit map of keys we've seen
	long bitmap = 0;

	// The bit map of keys we expect to see
	long expected = (1 << (count+1)) - 1;

	byte outbuf[] = new byte[count+1];
	int i;
	for (i=0; i<count; i++) {
	    outbuf[i] = (byte)('0' + i);
	    //outbuf[i] = System.out.println((byte)('0' + i);
	}
	outbuf[i++] = (byte)'x';

	/*
	 System.out.println("byte: " + ('0' + 0) + ", after: " +
	 (int)'0' + "=" + (int)('0' + 0) +
	 "," + (byte)outbuf[0]);
	 */

	Dbt key = new Dbt(outbuf, 0, i);
	Dbt data = new Dbt(outbuf, 0, i);

	//DEBUGOUT("Put: " + (char)outbuf[0] + ": " + new String(outbuf, 0, i));
	db.put(null, key, data, Db.DB_NOOVERWRITE);

	// Acquire a cursor for the table.
	Dbc dbcp = db.cursor(null, 0);

	// Walk through the table, checking
	Dbt readkey = new Dbt();
	Dbt readdata = new Dbt();
	Dbt whoknows = new Dbt();

	readkey.set_flags(options.dbt_alloc_flags);
	readdata.set_flags(options.dbt_alloc_flags);

	//DEBUGOUT("Dbc.get");
	while (dbcp.get(readkey, readdata, Db.DB_NEXT) == 0) {
	    String key_string =
              new String(readkey.get_data(), 0, readkey.get_size());
	    String data_string =
              new String(readdata.get_data(), 0, readkey.get_size());
	    //DEBUGOUT("Got: " + key_string + ": " + data_string);
	    int len = key_string.length();
	    if (len <= 0 || key_string.charAt(len-1) != 'x') {
		ERR("reread terminator is bad");
	    }
	    len--;
	    long bit = (1 << len);
	    if (len > count) {
		ERR("reread length is bad: expect " + count + " got "+ len + " (" + key_string + ")" );
	    }
	    else if (!data_string.equals(key_string)) {
		ERR("key/data don't match");
	    }
	    else if ((bitmap & bit) != 0) {
		ERR("key already seen");
	    }
	    else if ((expected & bit) == 0) {
		ERR("key was not expected");
	    }
	    else {
		bitmap |= bit;
		expected &= ~(bit);
		for (i=0; i<len; i++) {
		    if (key_string.charAt(i) != ('0' + i)) {
			System.out.print(" got " + key_string
					 + " (" + (int)key_string.charAt(i)
					 + "), wanted " + i
					 + " (" + (int)('0' + i)
					 + ") at position " + i + "\n");
			ERR("key is corrupt");
		    }
		}
	    }
	}
	if (expected != 0) {
	    System.out.print(" expected more keys, bitmap is: " + expected + "\n");
	    ERR("missing keys in database");
	}

	dbcp.close();
	db.close(0);
    }

    void t1(TestOptions options)
	throws DbException, FileNotFoundException
    {
	Db db = new Db(null, 0);
	rundb(db, itemcount++, false, options);
    }

    void t2(TestOptions options)
	throws DbException, FileNotFoundException
    {
	Db db = new Db(null, 0);
	rundb(db, itemcount++, false, options);
        //	rundb(db, itemcount++, false, options);
        //	rundb(db, itemcount++, false, options);
    }

    void t3(TestOptions options)
	throws DbException, FileNotFoundException
    {
	Db db = new Db(null, 0);
        //	rundb(db, itemcount++, false, options);
	db.set_errpfx("test3");
        for (int i=0; i<100; i++)
            db.set_errpfx("str" + i);
    	rundb(db, itemcount++, false, options);
    }

    void t4(TestOptions options)
	throws DbException, FileNotFoundException
    {
	DbEnv env = new DbEnv(0);
	env.open(CONSTRUCT01_DBDIR, Db.DB_CREATE | Db.DB_INIT_MPOOL, 0);
	Db db = new Db(env, 0);
	/**/
	    //rundb(db, itemcount++, true, options);
	    db.set_errpfx("test4");
	    rundb(db, itemcount++, true, options);
	    /**/
	    env.close(0);
    }

    void t5(TestOptions options)
	throws DbException, FileNotFoundException
    {
	DbEnv env = new DbEnv(0);
	env.open(CONSTRUCT01_DBDIR, Db.DB_CREATE | Db.DB_INIT_MPOOL, 0);
	Db db = new Db(env, 0);
        //	rundb(db, itemcount++, true, options);
	db.set_errpfx("test5");
	rundb(db, itemcount++, true, options);
        /*
	env.close(0);

	// reopen the environment, don't recreate
	env.open(CONSTRUCT01_DBDIR, Db.DB_INIT_MPOOL, 0);
	// Note we cannot reuse the old Db!
        */
	Db anotherdb = new Db(env, 0);

        //	rundb(anotherdb, itemcount++, true, options);
	anotherdb.set_errpfx("test5");
	rundb(anotherdb, itemcount++, true, options);
	env.close(0);
    }

    void t6(TestOptions options)
	throws DbException, FileNotFoundException
    {
	Db db = new Db(null, 0);
        DbEnv dbenv = new DbEnv(0);
        db.close(0);
        dbenv.close(0);

	System.gc();
	System.runFinalization();
    }

    // By design, t7 leaves a db and dbenv open; it should be detected.
    void t7(TestOptions options)
	throws DbException, FileNotFoundException
    {
	Db db = new Db(null, 0);
        DbEnv dbenv = new DbEnv(0);

	System.gc();
	System.runFinalization();
    }

    // remove any existing environment or database
    void removeall(boolean use_db)
    {
	{
	    if (use_db) {
		try {
		    /**/
		    //memory leak for this:
		    Db tmpdb = new Db(null, 0);
		    tmpdb.remove(CONSTRUCT01_DBFULLPATH, null, 0);
		    /**/
		    DbEnv tmpenv = new DbEnv(0);
		    tmpenv.remove(CONSTRUCT01_DBDIR, Db.DB_FORCE);
		}
		catch (DbException dbe) {
                    System.err.println("error during remove: " + dbe);
		}
		catch (FileNotFoundException fnfe) {
                    //expected error:
                    // System.err.println("error during remove: " + fnfe);
		}
	    }
	}
	check_file_removed(CONSTRUCT01_DBFULLPATH, true, !use_db);
	for (int i=0; i<8; i++) {
	    String fname = "__db.00" + i;
	    check_file_removed(fname, true, !use_db);
	}
    }

    boolean doall(TestOptions options)
    {
	itemcount = 0;
	try {
	    removeall((options.testmask & 1) != 0);
	    for (int item=1; item<32; item++) {
		if ((options.testmask & (1 << item)) != 0) {
		    VERBOSEOUT("  Running test " + item + ":");
		    switch (item) {
			case 1:
			    t1(options);
			    break;
			case 2:
			    t2(options);
			    break;
			case 3:
			    t3(options);
			    break;
			case 4:
			    t4(options);
			    break;
			case 5:
			    t5(options);
			    break;
			case 6:
			    t6(options);
			    break;
			case 7:
			    t7(options);
			    break;
			default:
			    ERR("unknown test case: " + item);
			    break;
		    }
		    VERBOSEOUT("  finished.\n");
		}
	    }
	    removeall((options.testmask & 1) != 0);
            options.successcounter++;
	    return true;
	}
	catch (DbException dbe) {
	    ERR("EXCEPTION RECEIVED: " + dbe);
	}
	catch (FileNotFoundException fnfe) {
	    ERR("EXCEPTION RECEIVED: " + fnfe);
	}
	return false;
    }

    public static void main(String args[])
    {
	int iterations = 200;
	int mask = 0x7f;

        // Make sure the database file is removed before we start.
	check_file_removed(CONSTRUCT01_DBFULLPATH, true, true);

	for (int argcnt=0; argcnt<args.length; argcnt++) {
	    String arg = args[argcnt];
	    if (arg.charAt(0) == '-') {
                // keep on lower bit, which means to remove db between tests.
		mask = 1;
		for (int pos=1; pos<arg.length(); pos++) {
		    char ch = arg.charAt(pos);
		    if (ch >= '0' && ch <= '9') {
			mask |= (1 << (ch - '0'));
		    }
                    else if (ch == 'v') {
                        verbose_flag = true;
                    }
		    else {
			ERR("Usage:  construct01 [-testdigits] count");
		    }
		}
                VERBOSEOUT("mask = " + mask);

	    }
	    else {
		try {
		    iterations = Integer.parseInt(arg);
		    if (iterations < 0) {
			ERR("Usage:  construct01 [-testdigits] count");
		    }
		}
		catch (NumberFormatException nfe) {
		    ERR("EXCEPTION RECEIVED: " + nfe);
		}
	    }
	}

        // Run GC before and after the test to give
        // a baseline for any Java memory used.
        //
        System.gc();
        System.runFinalization();
        VERBOSEOUT("gc complete");
        long starttotal = Runtime.getRuntime().totalMemory();
        long startfree = Runtime.getRuntime().freeMemory();

	TestConstruct01 con = new TestConstruct01();
        int[] dbt_flags = { 0, Db.DB_DBT_MALLOC, Db.DB_DBT_REALLOC };
        String[] dbt_flags_name = { "default", "malloc", "realloc" };

        TestOptions options = new TestOptions();
        options.testmask = mask;

        for (int flagiter = 0; flagiter < dbt_flags.length; flagiter++) {
            options.dbt_alloc_flags = dbt_flags[flagiter];

            VERBOSEOUT("Running with DBT alloc flags: " +
                       dbt_flags_name[flagiter]);
            for (int i=0; i<iterations; i++) {
                if (iterations != 0) {
                    VERBOSEOUT("(" + i + "/" + iterations + ") ");
                }
                VERBOSEOUT("construct01 running:");
                if (!con.doall(options)) {
                    ERR("SOME TEST FAILED");
                }
                else {
                    VERBOSEOUT("\nTESTS SUCCESSFUL");
                }

                // We continually run GC during the test to keep
                // the Java memory usage low.  That way we can
                // monitor the total memory usage externally
                // (e.g. via ps) and verify that we aren't leaking
                // memory in the JNI or DB layer.
                //
                System.gc();
                System.runFinalization();
                VERBOSEOUT("gc complete, bytes free == " + Runtime.getRuntime().freeMemory());
            }
        }

        if (options.successcounter == 600) {
            System.out.println("ALL TESTS SUCCESSFUL");
        }
        else {
            System.out.println("***FAIL: " + (600 - options.successcounter) +
                               " tests did not complete");
        }
        long endtotal = Runtime.getRuntime().totalMemory();
        long endfree = Runtime.getRuntime().freeMemory();

        System.out.println("delta for total mem: " + magnitude(endtotal - starttotal));
        System.out.println("delta for free mem: " + magnitude(endfree - startfree));

	return;
    }

    static String magnitude(long value)
    {
        final long max = 10000000;
        for (long scale = 10; scale <= max; scale *= 10) {
            if (value < scale && value > -scale)
                return "<" + scale;
        }
        return ">" + max;
    }

}

class TestOptions
{
    int testmask = 0;           // which tests to run
    int dbt_alloc_flags = 0;    // DB_DBT_* flags to use
    int successcounter =0;
}


/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1997, 1998, 1999, 2000
 *	Sleepycat Software.  All rights reserved.
 *
 * $Id: LockExample.java,v 11.5 2001/01/04 14:23:30 dda Exp $
 */

package com.sleepycat.examples;

import com.sleepycat.db.*;
import java.io.FileNotFoundException;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.io.IOException;
import java.io.PrintStream;
import java.util.Vector;

//
// An example of a program using DbLock and related classes.
//
class LockExample extends DbEnv
{
    private static final String progname = "LockExample";
    private static final String LOCK_HOME = "TESTDIR";

    public LockExample(String home, int maxlocks, boolean do_unlink)
         throws DbException, FileNotFoundException
    {
        super(0);
        if (do_unlink) {
            remove(home, Db.DB_FORCE);
        }
        else {
            set_error_stream(System.err);
            set_errpfx("LockExample");
            if (maxlocks != 0)
                set_lk_max_locks(maxlocks);
            open(home, Db.DB_CREATE|Db.DB_INIT_LOCK, 0);
        }
    }

    // Prompts for a line, and keeps prompting until a non blank
    // line is returned.  Returns null on error.
    //
    static public String askForLine(InputStreamReader reader,
                                    PrintStream out, String prompt)
    {
        String result = "";
        while (result != null && result.length() == 0) {
            out.print(prompt);
            out.flush();
            result = getLine(reader);
        }
        return result;
    }

    // Not terribly efficient, but does the job.
    // Works for reading a line from stdin or a file.
    // Returns null on EOF.  If EOF appears in the middle
    // of a line, returns that line, then null on next call.
    //
    static public String getLine(InputStreamReader reader)
    {
        StringBuffer b = new StringBuffer();
        int c;
        try {
            while ((c = reader.read()) != -1 && c != '\n') {
                if (c != '\r')
                    b.append((char)c);
            }
        }
        catch (IOException ioe) {
            c = -1;
        }

        if (c == -1 && b.length() == 0)
            return null;
        else
            return b.toString();
    }

    public void run()
         throws DbException
    {
        long held;
        int len = 0, locker;
        int ret;
        boolean did_get = false;
        int lockid = 0;
        InputStreamReader in = new InputStreamReader(System.in);
        Vector locks = new Vector();

        //
        // Accept lock requests.
        //
        locker = lock_id();
        for (held = 0;;) {
            String opbuf = askForLine(in, System.out,
                                      "Operation get/release [get]> ");
            if (opbuf == null)
                break;

            try {
                if (opbuf.equals("get")) {
                    // Acquire a lock.
                    String objbuf = askForLine(in, System.out,
                                   "input object (text string) to lock> ");
                    if (objbuf == null)
                        break;

                    String lockbuf;
                    do {
                        lockbuf = askForLine(in, System.out,
                                             "lock type read/write [read]> ");
                        if (lockbuf == null)
                            break;
                        len = lockbuf.length();
                    } while (len >= 1 &&
                             !lockbuf.equals("read") &&
                             !lockbuf.equals("write"));

                    int lock_type;
                    if (len <= 1 || lockbuf.equals("read"))
                        lock_type = Db.DB_LOCK_READ;
                    else
                        lock_type = Db.DB_LOCK_WRITE;

                    Dbt dbt = new Dbt(objbuf.getBytes());

                    DbLock lock;
                    did_get = true;
                    lock = lock_get(locker, Db.DB_LOCK_NOWAIT,
                                    dbt, lock_type);
                    lockid = locks.size();
                    locks.addElement(lock);
                } else {
                    // Release a lock.
                    String objbuf;
                    objbuf = askForLine(in, System.out,
                                        "input lock to release> ");
                    if (objbuf == null)
                        break;

                    lockid = Integer.parseInt(objbuf, 16);
                    if (lockid < 0 || lockid >= locks.size()) {
                        System.out.println("Lock #" + lockid + " out of range");
                        continue;
                    }
                    did_get = false;
                    DbLock lock = (DbLock)locks.elementAt(lockid);
                    lock.put(this);
                }
                System.out.println("Lock #" + lockid + " " +
                                   (did_get ? "granted" : "released"));
                held += did_get ? 1 : -1;
            }
            catch (DbException dbe) {
                switch (dbe.get_errno()) {
                case Db.DB_LOCK_NOTGRANTED:
                    System.out.println("Lock not granted");
                    break;
                case Db.DB_LOCK_DEADLOCK:
                    System.err.println("LockExample: lock_" +
                                       (did_get ? "get" : "put") +
                                       ": returned DEADLOCK");
                    break;
                default:
                    System.err.println("LockExample: lock_get: " + dbe.toString());
                }
            }
        }
        System.out.println();
        System.out.println("Closing lock region " + String.valueOf(held) +
                           " locks held");
    }

    private static void usage()
    {
        System.err.println("usage: LockExample [-u] [-h home] [-m maxlocks]");
        System.exit(1);
    }

    public static void main(String argv[])
    {
        String home = LOCK_HOME;
        boolean do_unlink = false;
        int maxlocks = 0;

        for (int i = 0; i < argv.length; ++i) {
            if (argv[i].equals("-h")) {
                if (++i >= argv.length)
                    usage();
                home = argv[i];
            }
            else if (argv[i].equals("-m")) {
                if (++i >= argv.length)
                    usage();

                try {
                    maxlocks = Integer.parseInt(argv[i]);
                }
                catch (NumberFormatException nfe) {
                    usage();
                }
            }
            else if (argv[i].equals("-u")) {
                do_unlink = true;
            }
            else {
                usage();
            }
        }

        try {
            if (do_unlink) {
                // Create an environment that immediately
                // removes all files.
                LockExample tmp = new LockExample(home, maxlocks, do_unlink);
            }

            LockExample app = new LockExample(home, maxlocks, do_unlink);
            app.run();
            app.close(0);
        }
        catch (DbException dbe) {
            System.err.println(progname + ": " + dbe.toString());
        }
        catch (Throwable t) {
            System.err.println(progname + ": " + t.toString());
        }
        System.out.println("LockExample completed");
    }
}

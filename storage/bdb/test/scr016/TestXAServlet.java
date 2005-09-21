/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1997-2004
 *	Sleepycat Software.  All rights reserved.
 *
 * $Id: TestXAServlet.java,v 1.4 2004/01/28 03:36:34 bostic Exp $
 */

/*
 * Simple test of XA, using WebLogic.
 */

package com.sleepycat.test;

import com.sleepycat.db.*;
import com.sleepycat.db.xa.*;
import java.io.File;
import java.io.FileNotFoundException;
import java.io.IOException;
import java.io.PrintWriter;
import java.util.Hashtable;
import javax.servlet.*;
import javax.servlet.http.*;
import javax.transaction.*;
import javax.transaction.xa.*;
import javax.naming.Context;
import javax.naming.InitialContext;
import javax.naming.NamingException;
import weblogic.transaction.TxHelper;
import weblogic.transaction.TransactionManager;

public class TestXAServlet extends HttpServlet
{
    public static final String ENV_HOME = "TESTXADIR";
    public static final String DEFAULT_URL = "t3://localhost:7001";
    public static String filesep = System.getProperty("file.separator");

    private static TransactionManager tm;
    private static DbXAResource xaresource;
    private static boolean initialized = false;

    /**
     * Utility to remove files recursively.
     */
    public static void removeRecursive(File f)
    {
        if (f.isDirectory()) {
            String[] sub = f.list();
            for (int i=0; i<sub.length; i++)
                removeRecursive(new File(f.getName() + filesep + sub[i]));
        }
        f.delete();
    }

    /**
     * Typically done only once, unless shutdown is invoked.  This
     * sets up directories, and removes any work files from previous
     * runs.  Also establishes a transaction manager that we'll use
     * for various transactions.  Each call opens/creates a new DB
     * environment in our work directory.
     */
    public static synchronized void startup()
    {
        if (initialized)
            return;

        try {
            File dir = new File(ENV_HOME);
            removeRecursive(dir);
            dir.mkdirs();

            System.out.println("Getting context");
            InitialContext ic = getInitialContext(DEFAULT_URL);
            System.out.println("Creating XAResource");
            xaresource = new DbXAResource(ENV_HOME, 77, 0);
            System.out.println("Registering with transaction manager");
            tm = TxHelper.getTransactionManager();
            tm.registerStaticResource("DbXA", xaresource);
            initialized = true;
        }
        catch (Exception e) {
            System.err.println("Exception: " + e);
            e.printStackTrace();
        }
        initialized = true;
    }

    /**
     * Closes the XA resource manager.
     */
    public static synchronized void shutdown(PrintWriter out)
        throws XAException
    {
        if (!initialized)
            return;

        out.println("Closing the resource.");
        xaresource.close(0);
        out.println("Shutdown complete.");
        initialized = false;
     }


    /**
     * Should be called once per chunk of major activity.
     */
    public void initialize()
    {
        startup();
    }

    private static int count = 1;
    private static boolean debugInited = false;
    private Xid bogusXid;

    public static synchronized int incrCount()
    {
         return count++;
    }

    public void debugSetup(PrintWriter out)
        throws ServletException, IOException
    {
        try {
            Db.load_db();
        }
        catch (Exception e) {
            out.println("got exception during load: " + e);
            System.out.println("got exception during load: " + e);
        }
        out.println("The servlet has been restarted, and Berkeley DB is loaded");
        out.println("<p>If you're debugging, you should now start the debugger and set breakpoints.");
    }

    public void doXATransaction(PrintWriter out, String key, String value,
                                String operation)
        throws ServletException, IOException
    {
        try {
            int counter = incrCount();
            if (key == null || key.equals(""))
                    key = "key" + counter;
            if (value == null || value.equals(""))
                    value = "value" + counter;

            out.println("Adding (\"" + key + "\", \"" + value + "\")");

            System.out.println("XA transaction begin");
	    tm.begin();
            System.out.println("getting XA transaction");
            DbXAResource.DbAttach attach = DbXAResource.xa_attach(null, null);
            DbTxn txn = attach.get_txn();
            DbEnv env = attach.get_env();
            Db db = new Db(env, 0);
            db.open(txn, "my.db", null, Db.DB_BTREE, Db.DB_CREATE, 0644);
            System.out.println("DB put " + key);
            db.put(txn,
                   new Dbt(key.getBytes()),
                   new Dbt(value.getBytes()),
                   0);

            if (operation.equals("rollback")) {
                out.println("<p>ROLLBACK");
                System.out.println("XA transaction rollback");
                tm.rollback();
                System.out.println("XA rollback returned");

                // The old db is no good after the rollback
                // since the open was part of the transaction.
                // Get another db for the cursor dump
                //
                db = new Db(env, 0);
                db.open(null, "my.db", null, Db.DB_BTREE, Db.DB_CREATE, 0644);
            }
            else {
                out.println("<p>COMMITTED");
                System.out.println("XA transaction commit");
                tm.commit();
            }

            // Show the current state of the database.
            Dbc dbc = db.cursor(null, 0);
            Dbt gotkey = new Dbt();
            Dbt gotdata = new Dbt();

            out.println("<p>Current database values:");
            while (dbc.get(gotkey, gotdata, Db.DB_NEXT) == 0) {
                out.println("<br>  " + getDbtString(gotkey) + " : "
                                   + getDbtString(gotdata));
            }
            dbc.close();
            db.close(0);
        }
        catch (DbException dbe) {
            System.err.println("Db Exception: " + dbe);
            out.println(" *** Exception received: " + dbe);
            dbe.printStackTrace();
        }
        catch (FileNotFoundException fnfe) {
            System.err.println("FileNotFoundException: " + fnfe);
            out.println(" *** Exception received: " + fnfe);
            fnfe.printStackTrace();
        }
        // Includes SystemException, NotSupportedException, RollbackException
        catch (Exception e) {
            System.err.println("Exception: " + e);
            out.println(" *** Exception received: " + e);
            e.printStackTrace();
        }
    }

    private static Xid getBogusXid()
        throws XAException
    {
        return new DbXid(1, "BOGUS_gtrid".getBytes(),
                         "BOGUS_bqual".getBytes());
    }

    private static String getDbtString(Dbt dbt)
    {
        return new String(dbt.get_data(), 0, dbt.get_size());
    }

    /**
     * doGet is called as a result of invoking the servlet.
     */
    public void doGet(HttpServletRequest req, HttpServletResponse resp)
        throws ServletException, IOException
    {
        try {
            resp.setContentType("text/html");
            PrintWriter out = resp.getWriter();

            String key = req.getParameter("key");
            String value = req.getParameter("value");
            String operation = req.getParameter("operation");

            out.println("<HTML>");
            out.println("<HEAD>");
            out.println("<TITLE>Berkeley DB with XA</TITLE>");
            out.println("</HEAD><BODY>");
            out.println("<a href=\"TestXAServlet" +
                        "\">Database put and commit</a><br>");
            out.println("<a href=\"TestXAServlet?operation=rollback" +
                        "\">Database put and rollback</a><br>");
            out.println("<a href=\"TestXAServlet?operation=close" +
                        "\">Close the XA resource manager</a><br>");
            out.println("<a href=\"TestXAServlet?operation=forget" +
                        "\">Forget an operation (bypasses TM)</a><br>");
            out.println("<a href=\"TestXAServlet?operation=prepare" +
                        "\">Prepare an operation (bypasses TM)</a><br>");
            out.println("<br>");

            if (!debugInited) {
                // Don't initialize XA yet, give the user
                // a chance to attach a debugger if necessary.
                debugSetup(out);
                debugInited = true;
            }
            else {
                initialize();
                if (operation == null)
                    operation = "commit";

                if (operation.equals("close")) {
                    shutdown(out);
                }
                else if (operation.equals("forget")) {
                    // A bogus test, we just make sure the API is callable.
                    out.println("<p>FORGET");
                    System.out.println("XA forget bogus XID (bypass TM)");
                    xaresource.forget(getBogusXid());
                }
                else if (operation.equals("prepare")) {
                    // A bogus test, we just make sure the API is callable.
                    out.println("<p>PREPARE");
                    System.out.println("XA prepare bogus XID (bypass TM)");
                    xaresource.prepare(getBogusXid());
                }
                else {
                    // commit, rollback, prepare, forget
                    doXATransaction(out, key, value, operation);
                }
            }
            out.println("</BODY></HTML>");

            System.out.println("Finished.");
        }
        // Includes SystemException, NotSupportedException, RollbackException
        catch (Exception e) {
            System.err.println("Exception: " + e);
            e.printStackTrace();
        }

    }


    /**
     * From weblogic's sample code:
     *    samples/examples/jta/jmsjdbc/Client.java
     */
    private static InitialContext getInitialContext(String url)
        throws NamingException
    {
        Hashtable env = new Hashtable();
        env.put(Context.INITIAL_CONTEXT_FACTORY,
                "weblogic.jndi.WLInitialContextFactory");
        env.put(Context.PROVIDER_URL, url);
        return new InitialContext(env);
    }

}

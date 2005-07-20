/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1997-2004
 *	Sleepycat Software.  All rights reserved.
 *
 * $Id: TestReplication.java,v 1.7 2004/01/28 03:36:34 bostic Exp $
 */

/*
 * Simple test of replication, merely to exercise the individual
 * methods in the API.  Rather than use TCP/IP, our transport
 * mechanism is just an ArrayList of byte arrays.
 * It's managed like a queue, and synchronization is via
 * the ArrayList object itself and java's wait/notify.
 * It's not terribly extensible, but it's fine for a small test.
 */

package com.sleepycat.test;

import com.sleepycat.db.*;
import java.io.File;
import java.io.FileNotFoundException;
import java.io.IOException;
import java.util.Vector;

public class TestReplication extends Thread
    implements DbRepTransport
{
    public static final String MASTER_ENVDIR = "./master";
    public static final String CLIENT_ENVDIR = "./client";

    private Vector queue = new Vector();
    private DbEnv master_env;
    private DbEnv client_env;

    private static void mkdir(String name)
        throws IOException
    {
        (new File(name)).mkdir();
    }


    // The client thread runs this
    public void run()
    {
        try {
            System.err.println("c10");
            client_env = new DbEnv(0);
            System.err.println("c11");
            client_env.set_rep_transport(1, this);
            System.err.println("c12");
            client_env.open(CLIENT_ENVDIR, Db.DB_CREATE | Db.DB_INIT_MPOOL, 0);
            System.err.println("c13");
            Dbt myid = new Dbt("master01".getBytes());
            System.err.println("c14");
            client_env.rep_start(myid, Db.DB_REP_CLIENT);
            System.err.println("c15");
            DbEnv.RepProcessMessage processMsg = new DbEnv.RepProcessMessage();
            processMsg.envid = 2;
            System.err.println("c20");
            boolean running = true;

            Dbt control = new Dbt();
            Dbt rec = new Dbt();

            while (running) {
                int msgtype = 0;

                System.err.println("c30");
                synchronized (queue) {
                    if (queue.size() == 0) {
                        System.err.println("c40");
                        sleepShort();
                    }
                    else {
                        msgtype = ((Integer)queue.firstElement()).intValue();
                        queue.removeElementAt(0);
                        byte[] data;

                        System.err.println("c50 " + msgtype);

                        switch (msgtype) {
                            case -1:
                                running = false;
                                break;
                            case 1:
                                data = (byte[])queue.firstElement();
                                queue.removeElementAt(0);
                                control.set_data(data);
                                control.set_size(data.length);
                                break;
                            case 2:
                                control.set_data(null);
                                control.set_size(0);
                                break;
                            case 3:
                                data = (byte[])queue.firstElement();
                                queue.removeElementAt(0);
                                rec.set_data(data);
                                rec.set_size(data.length);
                                break;
                            case 4:
                                rec.set_data(null);
                                rec.set_size(0);
                                break;
                        }

                    }
                }
                System.err.println("c60");
                if (msgtype == 3 || msgtype == 4) {
                    System.out.println("client: Got message");
                    client_env.rep_process_message(control, rec,
                                                   processMsg);
                }
            }
            System.err.println("c70");
            Db db = new Db(client_env, 0);
            db.open(null, "x.db", null, Db.DB_BTREE, 0, 0);
            Dbt data = new Dbt();
            System.err.println("c80");
            db.get(null, new Dbt("Hello".getBytes()), data, 0);
            System.err.println("c90");
            System.out.println("Hello " + new String(data.get_data(), data.get_offset(), data.get_size()));
            System.err.println("c95");
            client_env.close(0);
        }
        catch (Exception e) {
            System.err.println("client exception: " + e);
        }
    }

    // Implements DbTransport
    public int send(DbEnv env, Dbt control, Dbt rec, int flags, int envid)
        throws DbException
    {
        System.out.println("Send to " + envid);
        if (envid == 1) {
            System.err.println("Unexpected envid = " + envid);
            return 0;
        }

        int nbytes = 0;

        synchronized (queue) {
            System.out.println("Sending message");
            byte[] data = control.get_data();
            if (data != null && data.length > 0) {
                queue.addElement(new Integer(1));
                nbytes += data.length;
                byte[] newdata = new byte[data.length];
                System.arraycopy(data, 0, newdata, 0, data.length);
                queue.addElement(newdata);
            }
            else
            {
                queue.addElement(new Integer(2));
            }

            data = rec.get_data();
            if (data != null && data.length > 0) {
                queue.addElement(new Integer(3));
                nbytes += data.length;
                byte[] newdata = new byte[data.length];
                System.arraycopy(data, 0, newdata, 0, data.length);
                queue.addElement(newdata);
            }
            else
            {
                queue.addElement(new Integer(4));
            }
            System.out.println("MASTER: sent message");
        }
        return 0;
    }

    public void sleepShort()
    {
        try {
            sleep(100);
        }
        catch (InterruptedException ie)
        {
        }
    }

    public void send_terminator()
    {
        synchronized (queue) {
            queue.addElement(new Integer(-1));
        }
    }

    public void master()
    {
        try {
            master_env = new DbEnv(0);
            master_env.set_rep_transport(2, this);
            master_env.open(MASTER_ENVDIR, Db.DB_CREATE | Db.DB_INIT_MPOOL, 0644);
            System.err.println("10");
            Dbt myid = new Dbt("client01".getBytes());
            master_env.rep_start(myid, Db.DB_REP_MASTER);
            System.err.println("10");
            Db db = new Db(master_env, 0);
            System.err.println("20");
            db.open(null, "x.db", null, Db.DB_BTREE, Db.DB_CREATE, 0644);
            System.err.println("30");
            db.put(null, new Dbt("Hello".getBytes()),
                   new Dbt("world".getBytes()), 0);
            System.err.println("40");
            //DbEnv.RepElectResult electionResult = new DbEnv.RepElectResult();
            //master_env.rep_elect(2, 2, 3, 4, electionResult);
            db.close(0);
            System.err.println("50");
            master_env.close(0);
            send_terminator();
        }
        catch (Exception e) {
            System.err.println("client exception: " + e);
        }
    }

    public static void main(String[] args)
    {
        // The test should only take a few milliseconds.
        // give it 10 seconds before bailing out.
        TimelimitThread t = new TimelimitThread(1000*10);
        t.start();

        try {
            mkdir(CLIENT_ENVDIR);
            mkdir(MASTER_ENVDIR);

            TestReplication rep = new TestReplication();

            // Run the client as a seperate thread.
            rep.start();

            // Run the master.
            rep.master();

            // Wait for the master to finish.
            rep.join();
        }
        catch (Exception e)
        {
            System.err.println("Exception: " + e);
        }
        t.finished();
    }

}

class TimelimitThread extends Thread
{
    long nmillis;
    boolean finished = false;

    TimelimitThread(long nmillis)
    {
        this.nmillis = nmillis;
    }

    public void finished()
    {
        finished = true;
    }

    public void run()
    {
        long targetTime = System.currentTimeMillis() + nmillis;
        long curTime;

        while (!finished &&
               ((curTime = System.currentTimeMillis()) < targetTime)) {
            long diff = targetTime - curTime;
            if (diff > 100)
                diff = 100;
            try {
                sleep(diff);
            }
            catch (InterruptedException ie) {
            }
        }
        System.err.println("");
        System.exit(1);
    }
}

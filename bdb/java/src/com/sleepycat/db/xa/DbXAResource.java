/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1997-2002
 *      Sleepycat Software.  All rights reserved.
 *
 * $Id: DbXAResource.java,v 1.2 2002/08/09 01:54:57 bostic Exp $
 */

package com.sleepycat.db.xa;

import com.sleepycat.db.Db;
import com.sleepycat.db.DbEnv;
import com.sleepycat.db.DbTxn;
import javax.transaction.xa.XAResource;
import javax.transaction.xa.XAException;
import javax.transaction.xa.Xid;

public class DbXAResource implements XAResource
{
    public DbXAResource(String home, int rmid, int flags)
        throws XAException
    {
        this.home = home;
        this.rmid = rmid;

        // We force single-threading for calls to _init/_close.
        // This makes our internal code much easier, and
        // should not be a performance burden.
        synchronized (DbXAResource.class) {
                _init(home, rmid, flags);
        }
    }

    //
    // Alternate constructor for convenience.
    // Uses an rmid that is unique within this JVM,
    // numbered started at 0.
    //
    public DbXAResource(String home)
        throws XAException
    {
        this(home, get_unique_rmid(), 0);
    }

    private native void _init(String home, int rmid, int flags);

    public void close(int flags)
        throws XAException
    {
        // We force single-threading for calls to _init/_close.
        // This makes our internal code much easier, and
        // should not be a performance burden.
        synchronized (DbXAResource.class) {
                _close(home, rmid, flags);
        }
    }

    private native void _close(String home, int rmid, int flags);

    public void commit(Xid xid, boolean onePhase)
        throws XAException
    {
        _commit(xid, rmid, onePhase);
    }

    private native void _commit(Xid xid, int rmid, boolean onePhase);

    public void end(Xid xid, int flags)
        throws XAException
    {
        _end(xid, rmid, flags);
    }

    private native void _end(Xid xid, int rmid, int flags);

    public void forget(Xid xid)
        throws XAException
    {
        _forget(xid, rmid);
    }

    private native void _forget(Xid xid, int rmid);

    public int getTransactionTimeout()
        throws XAException
    {
        return transactionTimeout;
    }

    public boolean isSameRM(XAResource xares)
        throws XAException
    {
        if (!(xares instanceof DbXAResource))
            return false;
        return (this.rmid == ((DbXAResource)xares).rmid);
    }

    public int prepare(Xid xid)
        throws XAException
    {
        return _prepare(xid, rmid);
    }

    private native int _prepare(Xid xid, int rmid);

    public Xid [] recover(int flag)
        throws XAException
    {
        return _recover(rmid, flag);
    }

    private native Xid[] _recover(int rmid, int flags);

    public void rollback(Xid xid)
        throws XAException
    {
        _rollback(xid, rmid);
        System.err.println("DbXAResource.rollback returned");
    }

    private native void _rollback(Xid xid, int rmid);

    public boolean setTransactionTimeout(int seconds)
        throws XAException
    {
        // XXX we are not using the transaction timeout.
        transactionTimeout = seconds;
        return true;
    }

    public void start(Xid xid, int flags)
        throws XAException
    {
        _start(xid, rmid, flags);
    }

    private native void _start(Xid xid, int rmid, int flags);

    private static synchronized int get_unique_rmid()
    {
        return unique_rmid++;
    }

    public interface DbAttach
    {
        public DbEnv get_env();
        public DbTxn get_txn();
    }

    protected static class DbAttachImpl implements DbAttach
    {
        private DbEnv env;
        private DbTxn txn;

        DbAttachImpl(DbEnv env, DbTxn txn)
        {
            this.env = env;
            this.txn = txn;
        }

        public DbTxn get_txn()
        {
            return txn;
        }

        public DbEnv get_env()
        {
            return env;
        }
    }

    public static native DbAttach xa_attach(Xid xid, Integer rmid);

    ////////////////////////////////////////////////////////////////
    //
    // private data
    //
    private long private_dbobj_ = 0;
    private int transactionTimeout = 0;
    private String home;
    private int rmid;

    private static int unique_rmid = 0;

    static
    {
        Db.load_db();
    }
}

/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1997-2002
 *      Sleepycat Software.  All rights reserved.
 *
 * $Id: DbLockRequest.java,v 11.4 2002/01/16 07:45:24 mjc Exp $
 */

package com.sleepycat.db;

public class DbLockRequest
{
    public DbLockRequest(int op, int mode, Dbt obj, DbLock lock)
    {
        this.op = op;
        this.mode = mode;
        this.obj = obj;
        this.lock = lock;
    }

    public int get_op()
    {
        return op;
    }

    public void set_op(int op)
    {
        this.op = op;
    }

    public int get_mode()
    {
        return mode;
    }

    public void set_mode(int mode)
    {
        this.mode = mode;
    }

    public Dbt get_obj()
    {
        return obj;
    }

    public void set_obj(Dbt obj)
    {
        this.obj = obj;
    }

    public DbLock get_lock()
    {
        return lock;
    }

    public void set_lock(DbLock lock)
    {
        this.lock = lock;
    }

    private /* db_lockop_t */ int op;
    private /* db_lockmode_t */ int mode;
    private /* db_timeout_t */ int timeout;
    private Dbt obj;
    private DbLock lock;
}

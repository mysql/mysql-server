/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1997-2002
 *      Sleepycat Software.  All rights reserved.
 *
 * $Id: DbLockNotGrantedException.java,v 11.3 2002/01/11 15:52:36 bostic Exp $
 */

package com.sleepycat.db;

public class DbLockNotGrantedException extends DbException {
    public DbLockNotGrantedException(String message,
                                     int op, int mode, Dbt obj,
                                     DbLock lock, int index)
    {
        super(message, Db.DB_LOCK_NOTGRANTED);
        this.op = op;
        this.mode = mode;
        this.obj = obj;
        this.lock = lock;
        this.index = index;
    }

    public int get_op()
    {
        return op;
    }

    public int get_mode()
    {
        return mode;
    }

    public Dbt get_obj()
    {
        return obj;
    }

    public DbLock get_lock()
    {
        return lock;
    }

    public int get_index()
    {
        return index;
    }

    private int op;
    private int mode;
    private Dbt obj;
    private DbLock lock;
    private int index;

}


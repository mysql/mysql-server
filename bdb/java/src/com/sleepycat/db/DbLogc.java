/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1997-2002
 *      Sleepycat Software.  All rights reserved.
 *
 * $Id: DbLogc.java,v 11.3 2002/01/11 15:52:37 bostic Exp $
 */

package com.sleepycat.db;

/**
 *
 * @author Donald D. Anderson
 */
public class DbLogc
{
    // methods
    //
    public native void close(int flags)
         throws DbException;

    // returns: 0, DB_NOTFOUND, or throws error
    public native int get(DbLsn lsn, Dbt data, int flags)
         throws DbException;

    protected native void finalize()
         throws Throwable;

    // private data
    //
    private long private_dbobj_ = 0;

    static {
        Db.load_db();
    }
}

// end of DbLogc.java

/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1997, 1998, 1999, 2000
 *	Sleepycat Software.  All rights reserved.
 *
 *	$Id: DbLsn.java,v 11.5 2000/09/11 16:21:37 dda Exp $
 */

package com.sleepycat.db;

/**
 *
 * @author Donald D. Anderson
 */
public class DbLsn
{
    // methods
    //
    public DbLsn()
    {
        init_lsn();
    }

    protected native void finalize()
         throws Throwable;

    private native void init_lsn();

    // get/set methods
    //

    // private data
    //
    private long private_dbobj_ = 0;

    static {
        Db.load_db();
    }
}

// end of DbLsn.java

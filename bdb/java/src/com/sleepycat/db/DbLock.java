/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1997-2002
 *      Sleepycat Software.  All rights reserved.
 *
 * $Id: DbLock.java,v 11.8 2002/01/11 15:52:36 bostic Exp $
 */

package com.sleepycat.db;

/**
 *
 * @author Donald D. Anderson
 */
public class DbLock
{
    protected native void finalize()
         throws Throwable;

    // get/set methods
    //

    // private data
    //
    private long private_dbobj_ = 0;

    static {
        Db.load_db();
    }
}

// end of DbLock.java

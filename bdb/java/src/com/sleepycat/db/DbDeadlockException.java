/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1999-2002
 *      Sleepycat Software.  All rights reserved.
 *
 * $Id: DbDeadlockException.java,v 11.6 2002/01/11 15:52:33 bostic Exp $
 */

package com.sleepycat.db;

public class DbDeadlockException extends DbException
{
    // methods
    //

    public DbDeadlockException(String s)
    {
        super(s);
    }

    public DbDeadlockException(String s, int errno)
    {
        super(s, errno);
    }
}

// end of DbDeadlockException.java

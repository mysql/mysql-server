/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2000-2002
 *      Sleepycat Software.  All rights reserved.
 *
 * $Id: DbHash.java,v 11.5 2002/01/11 15:52:36 bostic Exp $
 */

package com.sleepycat.db;

/*
 * This interface is used by DbEnv.set_bt_compare()
 *
 */
public interface DbHash
{
    public abstract int hash(Db db, byte[] data, int len);
}

// end of DbHash.java

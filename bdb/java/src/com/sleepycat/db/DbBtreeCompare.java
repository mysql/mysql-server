/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2000-2002
 *      Sleepycat Software.  All rights reserved.
 *
 * $Id: DbBtreeCompare.java,v 11.6 2002/01/11 15:52:33 bostic Exp $
 */

package com.sleepycat.db;

/*
 * This interface is used by DbEnv.set_bt_compare()
 *
 */
public interface DbBtreeCompare
{
    public abstract int bt_compare(Db db, Dbt dbt1, Dbt dbt2);
}

// end of DbBtreeCompare.java

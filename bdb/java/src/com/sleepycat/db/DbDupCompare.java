/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2000
 *	Sleepycat Software.  All rights reserved.
 *
 *	$Id: DbDupCompare.java,v 11.2 2000/07/04 20:53:19 dda Exp $
 */

package com.sleepycat.db;

/*
 * This interface is used by DbEnv.set_dup_compare()
 * 
 */
public interface DbDupCompare
{
    public abstract int dup_compare(Db db, Dbt dbt1, Dbt dbt2);
}

// end of DbDupCompare.java

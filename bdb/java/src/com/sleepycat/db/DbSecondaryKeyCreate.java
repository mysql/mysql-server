/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1999-2002
 *      Sleepycat Software.  All rights reserved.
 *
 * $Id: DbSecondaryKeyCreate.java,v 11.3 2002/01/11 15:52:40 bostic Exp $
 */

package com.sleepycat.db;

/*
 * This is used as a callback by Db.associate.
 */
public interface DbSecondaryKeyCreate
{
    public int secondary_key_create(Db secondary, Dbt key,
                                    Dbt data, Dbt result)
        throws DbException;
}

// end of DbSecondaryKeyCreate.java

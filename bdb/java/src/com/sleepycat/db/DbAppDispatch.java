/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2000-2002
 *      Sleepycat Software.  All rights reserved.
 *
 * $Id: DbAppDispatch.java,v 11.6 2002/02/26 16:23:02 krinsky Exp $
 */

package com.sleepycat.db;

/*
 * This interface is used by DbEnv.set_app_dispatch()
 *
 */
public interface DbAppDispatch
{
    // The value of recops is one of the Db.DB_TXN_* constants
    public abstract int app_dispatch(DbEnv env, Dbt dbt, DbLsn lsn, int recops);
}

// end of DbAppDispatch.java

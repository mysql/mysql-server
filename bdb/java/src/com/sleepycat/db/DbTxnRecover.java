/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2000
 *	Sleepycat Software.  All rights reserved.
 *
 *	$Id: DbTxnRecover.java,v 11.1 2000/06/29 18:08:17 dda Exp $
 */

package com.sleepycat.db;

/*
 * This interface is used by DbEnv.set_tx_recover()
 * 
 */
public interface DbTxnRecover
{
    // The value of recops is one of the Db.DB_TXN_* constants
    public abstract int tx_recover(DbEnv env, Dbt dbt, DbLsn lsn, int recops);
}

// end of DbBtreeCompare.java

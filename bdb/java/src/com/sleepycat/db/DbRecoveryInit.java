/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1997, 1998, 1999, 2000
 *	Sleepycat Software.  All rights reserved.
 *
 *	$Id: DbRecoveryInit.java,v 11.3 2000/02/14 02:59:56 bostic Exp $
 */

package com.sleepycat.db;

/**
 *
 * @author Donald D. Anderson
 */
public interface DbRecoveryInit
{
    // methods
    //
    public abstract void recovery_init(DbEnv dbenv);
}

// end of DbRecoveryInit.java

/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1999, 2000
 *	Sleepycat Software.  All rights reserved.
 *
 *	$Id: DbEnvFeedback.java,v 11.3 2000/02/14 02:59:56 bostic Exp $
 */

package com.sleepycat.db;

public interface DbEnvFeedback
{
    // methods
    //
    public abstract void feedback(DbEnv env, int opcode, int pct);
}

// end of DbFeedback.java

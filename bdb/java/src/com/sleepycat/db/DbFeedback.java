/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1997-2002
 *      Sleepycat Software.  All rights reserved.
 *
 * $Id: DbFeedback.java,v 11.7 2002/01/11 15:52:35 bostic Exp $
 */

package com.sleepycat.db;

/**
 *
 * @author Donald D. Anderson
 */
public interface DbFeedback
{
    // methods
    //
    public abstract void feedback(Db db, int opcode, int pct);
}

// end of DbFeedback.java

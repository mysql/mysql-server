/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1999-2002
 *      Sleepycat Software.  All rights reserved.
 *
 * $Id: DbMemoryException.java,v 11.7 2002/01/11 15:52:37 bostic Exp $
 */

package com.sleepycat.db;

public class DbMemoryException extends DbException
{
    // methods
    //

    public DbMemoryException(String s)
    {
        super(s);
    }

    public DbMemoryException(String s, int errno)
    {
        super(s, errno);
    }

    public void set_dbt(Dbt dbt)
    {
        this.dbt = dbt;
    }

    public Dbt get_dbt()
    {
        return dbt;
    }

    /* Override of DbException.toString():
     * the extra verbage that comes from DbEnv.strerror(ENOMEM)
     * is not helpful.
     */
    public String toString()
    {
        return getMessage();
    }

    Dbt dbt = null;
}

// end of DbMemoryException.java

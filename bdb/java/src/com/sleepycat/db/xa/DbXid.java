/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1997-2002
 *      Sleepycat Software.  All rights reserved.
 *
 * $Id: DbXid.java,v 1.2 2002/08/09 01:54:58 bostic Exp $
 */

package com.sleepycat.db.xa;

import com.sleepycat.db.DbException;
import com.sleepycat.db.DbTxn;
import javax.transaction.xa.XAException;
import javax.transaction.xa.Xid;

public class DbXid implements Xid
{
    public DbXid(int formatId, byte[] gtrid, byte[] bqual)
        throws XAException
    {
        this.formatId = formatId;
        this.gtrid = gtrid;
        this.bqual = bqual;
    }

    public int getFormatId()
    {
        return formatId;
    }

    public byte[] getGlobalTransactionId()
    {
        return gtrid;
    }

    public byte[] getBranchQualifier()
    {
        return bqual;
    }

    ////////////////////////////////////////////////////////////////
    //
    // private data
    //
    private byte[] gtrid;
    private byte[] bqual;
    private int formatId;
}

/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1997, 1998, 1999, 2000
 *	Sleepycat Software.  All rights reserved.
 *
 *	$Id: DbTxnStat.java,v 11.3 2000/02/14 02:59:56 bostic Exp $
 */

package com.sleepycat.db;

/*
 * This is filled in and returned by the
 * DbTxnMgr.fstat() method.
 */
public class DbTxnStat
{
    public static class Active {
        public int txnid;               // Transaction ID
        public int parentid;            // Transaction ID of parent
        public DbLsn lsn;               // Lsn of the begin record
    };

    public DbLsn st_last_ckp;           // lsn of the last checkpoint
    public DbLsn st_pending_ckp;        // last checkpoint did not finish
    public long st_time_ckp;            // time of last checkpoint (UNIX secs)
    public int st_last_txnid;           // last transaction id given out
    public int st_maxtxns;              // maximum number of active txns
    public int st_naborts;              // number of aborted transactions
    public int st_nbegins;              // number of begun transactions
    public int st_ncommits;             // number of committed transactions
    public int st_nactive;              // number of active transactions
    public int st_maxnactive;           // maximum active transactions
    public Active st_txnarray[];        // array of active transactions
    public int st_region_wait;          // Region lock granted after wait.
    public int st_region_nowait;        // Region lock granted without wait.
    public int st_regsize;              // Region size.
}

// end of DbTxnStat.java

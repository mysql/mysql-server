/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1999, 2000
 *	Sleepycat Software.  All rights reserved.
 *
 *	$Id: DbQueueStat.java,v 11.5 2000/11/07 18:45:27 dda Exp $
 */

package com.sleepycat.db;

/*
 * This is filled in and returned by the
 * Db.stat() method.
 */
public class DbQueueStat
{
    public int qs_magic;		// Magic number.
    public int qs_version;		// Version number.
    public int qs_metaflags;		// Metadata flags.
    public int qs_nkeys;		// Number of unique keys.
    public int qs_ndata;		// Number of data items.
    public int qs_pagesize;		// Page size.
    public int qs_pages;		// Data pages.
    public int qs_re_len;		// Fixed-length record length.
    public int qs_re_pad;		// Fixed-length record pad.
    public int qs_pgfree;		// Bytes free in data pages.
    public int qs_first_recno;		// First not deleted record.
    public int qs_cur_recno;		// Last allocated record number.
}

// end of DbQueueStat.java

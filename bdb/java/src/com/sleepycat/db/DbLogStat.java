/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1998, 1999, 2000
 *	Sleepycat Software.  All rights reserved.
 *
 *	$Id: DbLogStat.java,v 11.3 2000/02/14 02:59:56 bostic Exp $
 */

package com.sleepycat.db;

/*
 * This is filled in and returned by the
 * DbLog.stat() method.
 */
public class DbLogStat
{
    public int st_magic;                // Log file magic number.
    public int st_version;              // Log file version number.
    public int st_mode;                 // Log file mode.
    public int st_lg_max;               // Maximum log file size.
    public int st_w_bytes;              // Bytes to log.
    public int st_w_mbytes;             // Megabytes to log.
    public int st_wc_bytes;             // Bytes to log since checkpoint.
    public int st_wc_mbytes;            // Megabytes to log since checkpoint.
    public int st_wcount;               // Total syncs to the log.
    public int st_scount;               // Total writes to the log.
    public int st_region_wait;          // Region lock granted after wait.
    public int st_region_nowait;        // Region lock granted without wait.
    public int st_cur_file;             // Current log file number.
    public int st_cur_offset;           // Current log file offset.
    public int st_regsize;              // Region size.
}

// end of DbLogStat.java

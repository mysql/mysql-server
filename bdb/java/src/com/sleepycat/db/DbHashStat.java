/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1999, 2000
 *	Sleepycat Software.  All rights reserved.
 *
 *	$Id: DbHashStat.java,v 11.6 2000/05/04 02:54:55 dda Exp $
 */

package com.sleepycat.db;

/*
 * This is filled in and returned by the
 * Db.stat() method.
 */
public class DbHashStat
{
    public int hash_magic;		// Magic number.
    public int hash_version;		// Version number.
    public int hash_metaflags;		// Metadata flags.
    public int hash_nkeys;		// Number of unique keys.
    public int hash_ndata;		// Number of data items.
    public int hash_pagesize;		// Page size.
    public int hash_nelem;		// Original nelem specified.
    public int hash_ffactor;		// Fill factor specified at create.
    public int hash_buckets;		// Number of hash buckets.
    public int hash_free;		// Pages on the free list.
    public int hash_bfree;		// Bytes free on bucket pages.
    public int hash_bigpages;		// Number of big key/data pages.
    public int hash_big_bfree;		// Bytes free on big item pages.
    public int hash_overflows;		// Number of overflow pages.
    public int hash_ovfl_free;		// Bytes free on ovfl pages.
    public int hash_dup;		// Number of dup pages.
    public int hash_dup_free;		// Bytes free on duplicate pages.
}

// end of DbHashStat.java

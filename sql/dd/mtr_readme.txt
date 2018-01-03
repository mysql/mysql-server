/* Copyright (c) 2014, 2017, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is also distributed with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have included with MySQL.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

// Copyright just here to pass server-side git copyright check.

Here are the details of tests that are disabled due
to new data dictionary implementation.

///////////////////////////////////////////////////////////////////
// RELATED TO INNODB SE
///////////////////////////////////////////////////////////////////


/** HIGH/MEDIUM - Marko
  Relates to InnoDB recovery
  Marko: These seem to wait for WL#7141/WL#7016, which is after
  the 1st push.

  WL6378_DDL_ON_READ_ONLY means that the test is executing
  InnoDB in read-only mode (using --innodb-force-recovery),
  which used to allow DDL operations (mainly DROP TABLE, DROP INDEX)
  and block DML. With the Global DD, it also blocks any modification
  of DD tables, thus blocking any DDL.

  WL7141_WL7016_RECOVERY means that the crash recovery and related
  tests must be rewritten in WL#7141 and WL#7016.
*/
i_innodb.innodb_bug16631778            WL6378_DDL_ON_READ_ONLY
i_innodb.innodb-force-recovery-3       WL6378_DDL_ON_READ_ONLY
i_innodb.innodb_bug15878013            WL7141_WL7016_RECOVERY
innodb.alter_crash                     WL7141_WL7016_RECOVERY
i_innodb.innodb_bug14676345            WL6795_WL7016_RECOVERY
innodb.innodb_wl6501_crash_1           WL6795_WL7016_RECOVERY
innodb.innodb_wl6501_crash_2           WL6795_WL7016_RECOVERY
innodb.innodb_wl6501_crash_6           WL6795_WL7016_RECOVERY
innodb.innodb_wl6501_crash_7           WL6795_WL7016_RECOVERY
innodb.innodb_wl6501_crash_8           WL6795_WL7016_RECOVERY
innodb_zip.wl6501                      WL6795_WL7016_RECOVERY
innodb_zip.wl6501_1                    WL6795_WL7016_RECOVERY
innodb_zip.wl6501_debug                WL6795_WL7016_RECOVERY
innodb_zip.wl6501_error_1              WL6795_WL7016_RECOVERY
innodb_zip.wl6501_crash_3              WL6795_WL7016_RECOVERY
innodb_zip.wl6501_crash_4              WL6795_WL7016_RECOVERY
innodb_zip.wl6501_crash_5              WL6795_WL7016_RECOVERY

// A single test case is disabled. More info in the test case.
innodb.partition                       WL6378_ALTER_PARTITION_TABLESPACE

///////////////////////////////////////////////////////////////////
// TEST COMMENTED IN-LINE WITHIN .test FILES
///////////////////////////////////////////////////////////////////

/** MEDIUM - Joh
  Needs understanding of test case scenario and bit more
  involved study to re-write these tests. We may or may
  not be successful in re-write, need to check.
*/
main.lock_sync                         WL6378_DEBUG_SYNC


///////////////////////////////////////////////////////////////////
// TESTS DISABLED DUE TO WL6599
///////////////////////////////////////////////////////////////////

/*
  Metadata of IS tables of thread pool plugin seem to be missing
  from DD tables.
*/
thread_pool.thread_pool_i_s : Enabled by WL9495.

// Hangs after 5 contineous run using ./mtr --repeat=30 - Thayu
// Raised Bug#25508568 to track this.
i_innodb.innodb_bug14150372 : WL6599_INNODB_SPORADIC

Restrictions OR waiting for WL/Bug fixes:
=========================================

/*
  We cannot call ANALYZE TABLE under innodb read only mode now.
  This would be a restrictions with wl6599. It is recommended
  to use 'information_schema_stats=latest' to get latest
  statistics in read only mode.

  Not sure if we can remove the ANALYZE statements in the test
  case. Need to check with Satya (Innodb)?

  "Bug#21611899 creating table on non-innodb engine when
   innodb-read-only option is set"
  
*/
i_innodb.innodb_bug16083211   : WL6599_ANALYZE_READONLY

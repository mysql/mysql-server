/* Copyright (c) 2014, 2016, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA */

// Copyright just here to pass server-side git copyright check.

Here are the details of tests that are disabled due
to new data dictionary implementation.

////////////////////////////////////////////////////////////
Following are the tests that mostly pass or will be
disabled due to some other dependencies.
////////////////////////////////////////////////////////////

/** MEDIUM - Joh
  Needs understanding of test case scenario and bit more
  involved study to re-write these tests. We may or may
  not be successful in re-write, need to check.
*/
main.mdl_sync                          WL6378_DEBUG_SYNC
main.lock_sync                         WL6378_DEBUG_SYNC


/** LOW
  Deals with upgrade scenarios.
  Need to re-visit once we have WL6392
*/
i_main.mysql_upgrade                   WL6378_UPGRADE
main.mysql_upgrade                     WL6378_UPGRADE
sysschema.mysqldump                    WL6378_UPGRADE


/** LOW
  Test deals with CREATE/DROP/ALTER on innodb_index_stats
  innodb_table_stats DD tables. As DD tables structures
  should not be changed, this should be avoided.

  Can we re-write the test to avoid this ?

  NOTE: we must block DDL to these tables. Revisit after
  WL6391.
*/
main.system_mysql_db_fix50030          WL6378_DDL_ON_DD_TABLE
main.system_mysql_db_fix40123          WL6378_DDL_ON_DD_TABLE
main.system_mysql_db_fix50117          WL6378_DDL_ON_DD_TABLE


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

/* LOW
  Direct modification of system tables which will be disallowed.
  Revisit after WL6391
*/
i_main.plugin_auth                     WL6378_MODIFIES_SYSTEM_TABLE

/*
  Allow dump/restore of innodb_index_stats and innodb_table_stats.
  See Bug#22655287
*/
main.mysqldump                         WL6378_DDL_ON_DD_TABLE


/*
  WL#6599
  Disabled for valgrind because of I_S timeout. Retry after WL#6599
*/
main.get_table_share

///////////////////////////////////////////////////////////////////
// TEST DISABLED BY OTHER WL TREE (not WL#6378 stuff)
///////////////////////////////////////////////////////////////////

/* HIGH/MEDIUM - Marko
  Tests with result difference
*/
// WL#7811/WL#7743/WL#7141 TODO: Adjust for TRUNCATE (WL#7899/WL#6795)
innodb_zip.wl6501_scale_1                      : WL6795_WL7016_RECOVERY

(16:16:46) marko: innodb_zip.wl6501_scale_1 needs some WL#6795 (http://wl.no.oracle.com/?tid=6795) adjustment
(16:16:58) marko: or re-recording (warnings on TRUNCATE)

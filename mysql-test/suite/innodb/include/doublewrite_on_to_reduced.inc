--let BACKUP=$MYSQL_TMP_DIR/datadir_backup
--let ORIG=$MYSQL_TMP_DIR/datadir_orig
if ($O_DIRECT) {
--let $restart_parameters = "restart: --datadir=$BACKUP/data --innodb-flush-method=O_DIRECT"
}

if (!$O_DIRECT) {
--let $restart_parameters = "restart: --datadir=$BACKUP/data"
}

--let $ERROR_LOG=$MYSQL_TMP_DIR/error.log

--disable_query_log
call mtr.add_suppression("Database page [0-9]+:1 contained only zeroes.");
call mtr.add_suppression("Header page consists of zero bytes");
call mtr.add_suppression("Checksum mismatch in datafile");
call mtr.add_suppression("but the innodb_page_size start-up parameter is");
call mtr.add_suppression("Database page corruption");
call mtr.add_suppression("Wrong server version");
--enable_query_log

--let INNODB_PAGE_SIZE=`select @@innodb_page_size`
--let MYSQLD_DATADIR=`select @@datadir`

--let $ARGS=--innodb_doublewrite=detect_only --log-error=$ERROR_LOG --datadir=$BACKUP/data

SHOW VARIABLES LIKE 'innodb_doublewrite';
SHOW VARIABLES LIKE 'innodb_fil_make_page_dirty_debug';
SHOW VARIABLES LIKE 'innodb_saved_page_number_debug';

--echo # Wait for purge to complete
--source include/wait_innodb_all_purged.inc

create table t1 (f1 int primary key, f2 blob) engine=innodb STATS_PERSISTENT=0;

START TRANSACTION;
INSERT INTO t1 VALUES(1, repeat('#',12));
INSERT INTO t1 VALUES(2, repeat('+',12));
INSERT INTO t1 VALUES(3, repeat('/',12));
INSERT INTO t1 VALUES(4, repeat('-',12));
INSERT INTO t1 VALUES(5, repeat('.',12));
COMMIT WORK;

# Slow shutdown and restart to make sure ibuf merge is finished
SET GLOBAL innodb_fast_shutdown = 0;
--source include/shutdown_mysqld.inc

--echo # Take backup of original datadir
--mkdir $BACKUP
--mkdir $ORIG
--force-cpdir $MYSQLD_DATADIR $BACKUP
--force-cpdir $MYSQLD_DATADIR $ORIG

--replace_result $BACKUP BACKUP " --innodb-flush-method=O_DIRECT" ""
--source include/start_mysqld.inc

--echo # ---------------------------------------------------------------
--echo # Test Begin: Test if recovery works if first page of user
--echo # tablespace is full of zeroes.

SHOW VARIABLES LIKE 'innodb_doublewrite';

--echo # Ensure that dirty pages of table t1 is flushed.
FLUSH TABLES t1 FOR EXPORT;
UNLOCK TABLES;

SELECT space FROM INFORMATION_SCHEMA.INNODB_TABLESPACEs
WHERE name = 'test/t1' INTO @space_id;

--source truncate_redo_log_and_stop_truncating.inc
SET GLOBAL innodb_limit_optimistic_insert_debug=2;

SELECT @@innodb_doublewrite;
BEGIN;
INSERT INTO t1 VALUES (6, repeat('%', 12));
INSERT INTO t1 SELECT f1+8,f2 FROM t1;
INSERT INTO t1 SELECT f1+16,f2 FROM t1;
INSERT INTO t1 SELECT f1+32,f2 FROM t1;
COMMIT;

SET GLOBAL innodb_doublewrite=detect_only;
SELECT @@innodb_doublewrite;

BEGIN;
INSERT INTO t1 VALUES (0, repeat('%', 12));
INSERT INTO t1 SELECT f1+64,f2 FROM t1;

--echo # Make the first page dirty for table t1
SET GLOBAL innodb_saved_page_number_debug = 0;

--source include/expect_crash.inc
# Set the page dirty and force a flush to disk
--error CR_SERVER_LOST
SET GLOBAL innodb_fil_make_page_dirty_debug = @space_id;

--echo # Make the first page (page_no=0) of the user tablespace
--echo # full of zeroes.
--let IBD_FILE=$BACKUP/data/test/t1.ibd
--let PAGE_NUM="0"
--let ALL_ZEROES=1
--source corrupt_page.inc

--error 1,2,17,-2147483645
--exec $MYSQLD_CMD $ARGS

--let ABORT_ON=NOT_FOUND
--let SEARCH_PATTERN=Database page corruption of tablespace .*test.*t1.ibd space_id: \d+ page_num: \d+. Cannot recover it from the doublewrite buffer because it was written in detect_only-doublewrite mode
--let SEARCH_FILE= $ERROR_LOG
--source include/search_pattern_multiline.inc
--remove_file $ERROR_LOG

--echo # restoring original datadir
--force-rmdir $BACKUP/data
--force-cpdir $ORIG/data $BACKUP/data
--replace_result $BACKUP BACKUP " --innodb-flush-method=O_DIRECT" ""
--source include/start_mysqld.inc

--echo # Test End - 1

--echo # ---------------------------------------------------------------
--echo # Test Begin: Test if recovery works if first page of user
--echo # tablespace is corrupted.

SHOW VARIABLES LIKE 'innodb_doublewrite';

SELECT space from INFORMATION_SCHEMA.INNODB_TABLESPACES
WHERE name = 'test/t1' INTO @space_id;

--echo # Ensure that dirty pages of table t1 is flushed.
FLUSH TABLES t1 FOR EXPORT;
UNLOCK TABLES;

--source truncate_redo_log_and_stop_truncating.inc

SELECT @@innodb_doublewrite;
BEGIN;
INSERT INTO t1 VALUES (6, repeat('%', 12));
INSERT INTO t1 SELECT f1+8,f2 FROM t1;
INSERT INTO t1 SELECT f1+16,f2 FROM t1;
INSERT INTO t1 SELECT f1+32,f2 FROM t1;
COMMIT;

SET GLOBAL innodb_doublewrite=detect_only;
SELECT @@innodb_doublewrite;

BEGIN;
INSERT INTO t1 VALUES (0, repeat('%', 12));

--echo # Make the first page dirty for table t1
SET GLOBAL innodb_saved_page_number_debug = 0;

--source include/expect_crash.inc
# Set the page dirty and force a flush to disk
--error CR_SERVER_LOST
SET GLOBAL innodb_fil_make_page_dirty_debug = @space_id;

--echo # Corrupt the first page (page_no=0) of the user tablespace.
--let IBD_FILE=$BACKUP/data/test/t1.ibd
--let PAGE_NUM="0"
--let ALL_ZEROES=0
--source corrupt_page.inc

--error 1,2,17,-2147483645
--exec $MYSQLD_CMD $ARGS

--let ABORT_ON=NOT_FOUND
--let SEARCH_PATTERN=Database page corruption of tablespace .*test.*t1.ibd space_id: \d+ page_num: \d+. Cannot recover it from the doublewrite buffer because it was written in detect_only-doublewrite mode
--let SEARCH_FILE= $ERROR_LOG
--source include/search_pattern_multiline.inc
--remove_file $ERROR_LOG

--echo # restoring original datadir
--force-rmdir $BACKUP/data
--force-cpdir $ORIG/data $BACKUP/data
--replace_result $BACKUP BACKUP " --innodb-flush-method=O_DIRECT" ""
--source include/start_mysqld.inc

--echo # Test End - 2
--echo # ---------------------------------------------------------------
--echo # Test Begin: Test if recovery works if 2nd page of user
--echo # tablespace is full of zeroes.

SHOW VARIABLES LIKE 'innodb_doublewrite';

SELECT space FROM INFORMATION_SCHEMA.INNODB_TABLESPACES
WHERE name = 'test/t1' INTO @space_id;

--echo # Ensure that dirty pages of table t1 is flushed.
FLUSH TABLES t1 FOR EXPORT;
UNLOCK TABLES;

--source truncate_redo_log_and_stop_truncating.inc

SELECT @@innodb_doublewrite;
BEGIN;
INSERT INTO t1 VALUES (6, repeat('%', 12));
INSERT INTO t1 SELECT f1+8,f2 FROM t1;
INSERT INTO t1 SELECT f1+16,f2 FROM t1;
INSERT INTO t1 SELECT f1+32,f2 FROM t1;
COMMIT;

SET GLOBAL innodb_doublewrite=detect_only;
SELECT @@innodb_doublewrite;

BEGIN;
INSERT INTO t1 VALUES (0, repeat('%', 12));

--echo # Make the 2nd page dirty for table t1
SET GLOBAL innodb_saved_page_number_debug = 1;

# Set the page dirty and force a flush to disk
--source include/expect_crash.inc
--error CR_SERVER_LOST
SET GLOBAL innodb_fil_make_page_dirty_debug = @space_id;

--echo # Make the 2nd page (page_no=1) of the tablespace all zeroes.
--let IBD_FILE=$BACKUP/data/test/t1.ibd
--let PAGE_NUM=1
--let ALL_ZEROES=1
--source corrupt_page.inc

--error 1,2,17,-2147483645
--exec $MYSQLD_CMD $ARGS

--let ABORT_ON=NOT_FOUND
--let SEARCH_PATTERN=Database page corruption of tablespace .*test.*t1.ibd space_id: \d+ page_num: \d+. Cannot recover it from the doublewrite buffer because it was written in detect_only-doublewrite mode
--let SEARCH_FILE= $ERROR_LOG
--source include/search_pattern_multiline.inc
--remove_file $ERROR_LOG

--echo # restoring original datadir
--force-rmdir $BACKUP/data
--force-cpdir $ORIG/data $BACKUP/data
--replace_result $BACKUP BACKUP " --innodb-flush-method=O_DIRECT" ""
--source include/start_mysqld.inc

--echo # Test End - 3
--echo # ---------------------------------------------------------------
--echo # Test Begin: Test if recovery works if 2nd page of user
--echo # tablespace is corrupted.

SHOW VARIABLES LIKE 'innodb_doublewrite';

SELECT space FROM INFORMATION_SCHEMA.INNODB_TABLESPACES
WHERE name = 'test/t1' into @space_id;

#--echo # Ensure that dirty pages of table t1 is flushed.
FLUSH TABLES t1 FOR EXPORT;
UNLOCK TABLES;

--source truncate_redo_log_and_stop_truncating.inc

SELECT @@innodb_doublewrite;
BEGIN;
INSERT INTO t1 VALUES (6, repeat('%', 12));
INSERT INTO t1 SELECT f1+8,f2 FROM t1;
INSERT INTO t1 SELECT f1+16,f2 FROM t1;
INSERT INTO t1 SELECT f1+32,f2 FROM t1;
COMMIT;

SET GLOBAL innodb_doublewrite=detect_only;
SELECT @@innodb_doublewrite;

BEGIN;
INSERT INTO t1 VALUES (0, repeat('%', 12));

--echo # Make the 2nd page dirty for table t1
SET GLOBAL innodb_saved_page_number_debug = 1;

--source include/expect_crash.inc
# Set the page dirty and force a flush to disk
--error CR_SERVER_LOST
SET GLOBAL innodb_fil_make_page_dirty_debug = @space_id;

--echo # Corrupt the 2nd page (page_no=1) of the user tablespace.
--let IBD_FILE=$BACKUP/data/test/t1.ibd
--let PAGE_NUM=1
--let ALL_ZEROES=0
--source corrupt_page.inc

--error 1,2,17,-2147483645
--exec $MYSQLD_CMD $ARGS

--let ABORT_ON=NOT_FOUND
--let SEARCH_PATTERN=Database page corruption of tablespace .*test.*t1.ibd space_id: \d+ page_num: \d+. Cannot recover it from the doublewrite buffer because it was written in detect_only-doublewrite mode
--let SEARCH_FILE= $ERROR_LOG
--source include/search_pattern_multiline.inc
--remove_file $ERROR_LOG

--echo # restoring original datadir
--force-rmdir $BACKUP/data
--force-cpdir $ORIG/data $BACKUP/data
--replace_result $BACKUP BACKUP " --innodb-flush-method=O_DIRECT" ""
--source include/start_mysqld.inc

--echo # Test End - 4
--echo # ---------------------------------------------------------------
--echo # Test Begin: Test if recovery works if first page of
--echo # system tablespace is full of zeroes.

SHOW VARIABLES LIKE 'innodb_doublewrite';

SET GLOBAL innodb_master_thread_disabled_debug=1;

SELECT @@innodb_doublewrite;
BEGIN;
INSERT INTO t1 VALUES (6, repeat('%', 12));
INSERT INTO t1 SELECT f1+8,f2 FROM t1;
INSERT INTO t1 SELECT f1+16,f2 FROM t1;
INSERT INTO t1 SELECT f1+32,f2 FROM t1;
COMMIT;

SET GLOBAL innodb_doublewrite=detect_only;
SELECT @@innodb_doublewrite;

BEGIN;
INSERT INTO t1 VALUES (0, repeat('%', 12));

--source truncate_redo_log_and_stop_truncating.inc

--echo # Make the first page dirty for system tablespace
SET GLOBAL innodb_saved_page_number_debug = 0;

# Set the page dirty and force a flush to disk
--source include/expect_crash.inc
--error CR_SERVER_LOST
SET GLOBAL innodb_fil_make_page_dirty_debug = 0;

--echo # Make the first page (page_no=0) of the system tablespace
--echo # all zeroes.
--let IBD_FILE=$BACKUP/data/ibdata1
--let PAGE_NUM="0"
--let ALL_ZEROES=1
--source corrupt_page.inc

--error 1,2,17,-2147483645
--exec $MYSQLD_CMD $ARGS

--let ABORT_ON=NOT_FOUND
--let SEARCH_PATTERN=Database page corruption of tablespace .*ibdata1 space_id: \d+ page_num: \d+. Cannot recover it from the doublewrite buffer because it was written in detect_only-doublewrite mode
--let SEARCH_FILE= $ERROR_LOG
--source include/search_pattern_multiline.inc
--remove_file $ERROR_LOG

--echo # restoring original datadir
--force-rmdir $BACKUP/data
--force-cpdir $ORIG/data $BACKUP/data
--replace_result $BACKUP BACKUP " --innodb-flush-method=O_DIRECT" ""
--source include/start_mysqld.inc

--echo # Test End - 5
--echo # ---------------------------------------------------------------
--echo # Test Begin: Test if recovery works if first page of
--echo # system tablespace is corrupted.

SHOW VARIABLES LIKE 'innodb_doublewrite';

SET GLOBAL innodb_master_thread_disabled_debug=1;

SELECT @@innodb_doublewrite;
BEGIN;
INSERT INTO t1 VALUES (6, repeat('%', 12));
INSERT INTO t1 SELECT f1+8,f2 FROM t1;
INSERT INTO t1 SELECT f1+16,f2 FROM t1;
INSERT INTO t1 SELECT f1+32,f2 FROM t1;
COMMIT;

SET GLOBAL innodb_doublewrite=detect_only;
SELECT @@innodb_doublewrite;

BEGIN;
INSERT INTO t1 VALUES (0, repeat('%', 12));

--source truncate_redo_log_and_stop_truncating.inc

--echo # Make the first page dirty for system tablespace
SET GLOBAL innodb_saved_page_number_debug = 0;

# Set the page dirty and force a flush to disk
--source include/expect_crash.inc
--error CR_SERVER_LOST
SET GLOBAL innodb_fil_make_page_dirty_debug = 0;

--echo # Corrupt the first page (page_no=0) of the system tablespace.
--let IBD_FILE=$BACKUP/data/ibdata1
--let PAGE_NUM="0"
--source corrupt_page.inc

--error 1,2,17,-2147483645
--exec $MYSQLD_CMD $ARGS

--let ABORT_ON=NOT_FOUND
--let SEARCH_PATTERN=Database page corruption of tablespace .*ibdata1 space_id: \d+ page_num: \d+. Cannot recover it from the doublewrite buffer because it was written in detect_only-doublewrite mode
--let SEARCH_FILE= $ERROR_LOG
--source include/search_pattern_multiline.inc
--remove_file $ERROR_LOG


--echo # restoring original datadir
--force-rmdir $BACKUP/data
--force-cpdir $ORIG/data $BACKUP/data
--replace_result $BACKUP BACKUP " --innodb-flush-method=O_DIRECT" ""
--source include/start_mysqld.inc

--echo # Test End - 6
--echo # ---------------------------------------------------------------
--echo # Test Begin: Test if recovery works if 2nd page of
--echo # system tablespace is full of zeroes.

SHOW VARIABLES LIKE 'innodb_doublewrite';

SET GLOBAL innodb_master_thread_disabled_debug=1;

SELECT @@innodb_doublewrite;
BEGIN;
INSERT INTO t1 VALUES (6, repeat('%', 12));
INSERT INTO t1 SELECT f1+8,f2 FROM t1;
INSERT INTO t1 SELECT f1+16,f2 FROM t1;
INSERT INTO t1 SELECT f1+32,f2 FROM t1;
COMMIT;

SET GLOBAL innodb_doublewrite=detect_only;
SELECT @@innodb_doublewrite;

BEGIN;
INSERT INTO t1 VALUES (0, repeat('%', 12));

--source truncate_redo_log_and_stop_truncating.inc

--echo # Make the second page dirty for system tablespace
SET GLOBAL innodb_saved_page_number_debug = 1;

# Set the page dirty and force a flush to disk
--source include/expect_crash.inc
--error CR_SERVER_LOST
SET GLOBAL innodb_fil_make_page_dirty_debug = 0;

--echo # Make the 2nd page (page_no=1) of the system tablespace
--echo # all zeroes.
--let IBD_FILE=$BACKUP/data/ibdata1
--let PAGE_NUM=1
--let ALL_ZEROES=1
--source corrupt_page.inc

--error 1,2,17,-2147483645
--exec $MYSQLD_CMD $ARGS

--let ABORT_ON=NOT_FOUND
--let SEARCH_PATTERN=Database page corruption of tablespace .*ibdata1 space_id: \d+ page_num: \d+. Cannot recover it from the doublewrite buffer because it was written in detect_only-doublewrite mode
--let SEARCH_FILE= $ERROR_LOG
--source include/search_pattern_multiline.inc
--remove_file $ERROR_LOG

--echo # restoring original datadir
--force-rmdir $BACKUP/data
--force-cpdir $ORIG/data $BACKUP/data
--replace_result $BACKUP BACKUP " --innodb-flush-method=O_DIRECT" ""
--source include/start_mysqld.inc

--echo # Test End - 7
--echo # ---------------------------------------------------------------
--echo # Test Begin: Test if recovery works if 2nd page of
--echo # system tablespace is corrupted.

SHOW VARIABLES LIKE 'innodb_doublewrite';

SET GLOBAL innodb_master_thread_disabled_debug=1;

SELECT @@innodb_doublewrite;
BEGIN;
INSERT INTO t1 VALUES (6, repeat('%', 12));
INSERT INTO t1 SELECT f1+8,f2 FROM t1;
INSERT INTO t1 SELECT f1+16,f2 FROM t1;
INSERT INTO t1 SELECT f1+32,f2 FROM t1;
COMMIT;

SET GLOBAL innodb_doublewrite=detect_only;
SELECT @@innodb_doublewrite;

BEGIN;
INSERT INTO t1 VALUES (0, repeat('%', 12));

--source truncate_redo_log_and_stop_truncating.inc

--echo # Make the second page dirty for system tablespace
SET GLOBAL innodb_saved_page_number_debug = 1;

# Set the page dirty and force a flush to disk
--source include/expect_crash.inc
--error CR_SERVER_LOST
SET GLOBAL innodb_fil_make_page_dirty_debug = 0;

--echo # Make the 2nd page (page_no=1) of the system tablespace
--echo # all zeroes.
--let IBD_FILE=$BACKUP/data/ibdata1
--let PAGE_NUM=1
--source corrupt_page.inc

--error 1,2,17,-2147483645
--exec $MYSQLD_CMD $ARGS

--let ABORT_ON=NOT_FOUND
--let SEARCH_PATTERN=Database page corruption of tablespace .*ibdata1 space_id: \d+ page_num: \d+. Cannot recover it from the doublewrite buffer because it was written in detect_only-doublewrite mode
--let SEARCH_FILE= $ERROR_LOG
--source include/search_pattern_multiline.inc
--remove_file $ERROR_LOG

--echo # restoring original datadir
--force-rmdir $BACKUP/data
--force-cpdir $ORIG/data $BACKUP/data

--let $restart_parameters =
--source include/start_mysqld.inc

--echo # Test End - 8
--echo # ---------------------------------------------------------------

DROP TABLE t1;
--force-rmdir $BACKUP
--force-rmdir $ORIG

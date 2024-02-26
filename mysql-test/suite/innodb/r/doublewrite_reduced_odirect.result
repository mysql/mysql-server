###########################################
# Test with O_DIRECT enabled
###########################################
SHOW VARIABLES LIKE 'innodb_doublewrite';
Variable_name	Value
innodb_doublewrite	ON
SHOW VARIABLES LIKE 'innodb_fil_make_page_dirty_debug';
Variable_name	Value
innodb_fil_make_page_dirty_debug	4294967295
SHOW VARIABLES LIKE 'innodb_saved_page_number_debug';
Variable_name	Value
innodb_saved_page_number_debug	0
# Wait for purge to complete
create table t1 (f1 int primary key, f2 blob) engine=innodb STATS_PERSISTENT=0;
START TRANSACTION;
INSERT INTO t1 VALUES(1, repeat('#',12));
INSERT INTO t1 VALUES(2, repeat('+',12));
INSERT INTO t1 VALUES(3, repeat('/',12));
INSERT INTO t1 VALUES(4, repeat('-',12));
INSERT INTO t1 VALUES(5, repeat('.',12));
COMMIT WORK;
SET GLOBAL innodb_fast_shutdown = 0;
# Take backup of original datadir
# restart: --innodb_doublewrite=detect_only --datadir=BACKUP/data
# ---------------------------------------------------------------
# Test Begin: Test if recovery works if first page of user
# tablespace is full of zeroes.
SHOW VARIABLES LIKE 'innodb_doublewrite';
Variable_name	Value
innodb_doublewrite	DETECT_ONLY
# Ensure that dirty pages of table t1 is flushed.
FLUSH TABLES t1 FOR EXPORT;
UNLOCK TABLES;
SELECT space FROM INFORMATION_SCHEMA.INNODB_TABLESPACEs
WHERE name = 'test/t1' INTO @space_id;
SET GLOBAL innodb_checkpoint_disabled=1;
SET GLOBAL innodb_limit_optimistic_insert_debug=2;
BEGIN;
INSERT INTO t1 VALUES (6, repeat('%', 12));
INSERT INTO t1 SELECT f1+8,f2 FROM t1;
SET GLOBAL innodb_checkpoint_disabled = 1;
# Make the first page dirty for table t1
SET GLOBAL innodb_saved_page_number_debug = 0;
SET GLOBAL innodb_fil_make_page_dirty_debug = @space_id;
ERROR HY000: Lost connection to MySQL server during query
# Make the first page (page_no=0) of the user tablespace
# full of zeroes.
# restoring original datadir
# restart: --innodb_doublewrite=detect_only --datadir=BACKUP/data
# Test End - 1
# ---------------------------------------------------------------
# Test Begin: Test if recovery works if first page of user
# tablespace is corrupted.
SHOW VARIABLES LIKE 'innodb_doublewrite';
Variable_name	Value
innodb_doublewrite	DETECT_ONLY
SELECT space from INFORMATION_SCHEMA.INNODB_TABLESPACES
WHERE name = 'test/t1' INTO @space_id;
# Ensure that dirty pages of table t1 is flushed.
FLUSH TABLES t1 FOR EXPORT;
UNLOCK TABLES;
SET GLOBAL innodb_checkpoint_disabled=1;
BEGIN;
INSERT INTO t1 VALUES (6, repeat('%', 12));
SET GLOBAL innodb_checkpoint_disabled = 1;
# Make the first page dirty for table t1
SET GLOBAL innodb_saved_page_number_debug = 0;
SET GLOBAL innodb_fil_make_page_dirty_debug = @space_id;
ERROR HY000: Lost connection to MySQL server during query
# Corrupt the first page (page_no=0) of the user tablespace.
# restoring original datadir
# restart: --innodb_doublewrite=detect_only --datadir=BACKUP/data
# Test End - 2
# ---------------------------------------------------------------
# Test Begin: Test if recovery works if 2nd page of user
# tablespace is full of zeroes.
SHOW VARIABLES LIKE 'innodb_doublewrite';
Variable_name	Value
innodb_doublewrite	DETECT_ONLY
SELECT space FROM INFORMATION_SCHEMA.INNODB_TABLESPACES
WHERE name = 'test/t1' INTO @space_id;
# Ensure that dirty pages of table t1 is flushed.
FLUSH TABLES t1 FOR EXPORT;
UNLOCK TABLES;
SET GLOBAL innodb_checkpoint_disabled=1;
BEGIN;
INSERT INTO t1 VALUES (6, repeat('%', 400));
SET GLOBAL innodb_checkpoint_disabled = 1;
# Make the 2nd page dirty for table t1
SET GLOBAL innodb_saved_page_number_debug = 1;
SET GLOBAL innodb_fil_make_page_dirty_debug = @space_id;
ERROR HY000: Lost connection to MySQL server during query
# Make the 2nd page (page_no=1) of the tablespace all zeroes.
# restoring original datadir
# restart: --innodb_doublewrite=detect_only --datadir=BACKUP/data
# Test End - 3
# ---------------------------------------------------------------
# Test Begin: Test if recovery works if 2nd page of user
# tablespace is corrupted.
SHOW VARIABLES LIKE 'innodb_doublewrite';
Variable_name	Value
innodb_doublewrite	DETECT_ONLY
SELECT space FROM INFORMATION_SCHEMA.INNODB_TABLESPACES
WHERE name = 'test/t1' into @space_id;
FLUSH TABLES t1 FOR EXPORT;
UNLOCK TABLES;
SET GLOBAL innodb_checkpoint_disabled=1;
BEGIN;
INSERT INTO t1 VALUES (6, repeat('%', 400));
# Make the 2nd page dirty for table t1
SET GLOBAL innodb_saved_page_number_debug = 1;
SET GLOBAL innodb_checkpoint_disabled = 1;
SET GLOBAL innodb_fil_make_page_dirty_debug = @space_id;
ERROR HY000: Lost connection to MySQL server during query
# Corrupt the 2nd page (page_no=1) of the user tablespace.
# restoring original datadir
# restart: --innodb_doublewrite=detect_only --datadir=BACKUP/data
# Test End - 4
# ---------------------------------------------------------------
# Test Begin: Test if recovery works if first page of
# system tablespace is full of zeroes.
SHOW VARIABLES LIKE 'innodb_doublewrite';
Variable_name	Value
innodb_doublewrite	DETECT_ONLY
SET GLOBAL innodb_master_thread_disabled_debug=1;
BEGIN;
INSERT INTO t1 VALUES (6, repeat('%', 400));
SET GLOBAL innodb_checkpoint_disabled = 1;
# Make the first page dirty for system tablespace
SET GLOBAL innodb_saved_page_number_debug = 0;
SET GLOBAL innodb_fil_make_page_dirty_debug = 0;
ERROR HY000: Lost connection to MySQL server during query
# Make the first page (page_no=0) of the system tablespace
# all zeroes.
# restoring original datadir
# restart: --innodb_doublewrite=detect_only --datadir=BACKUP/data
# Test End - 5
# ---------------------------------------------------------------
# Test Begin: Test if recovery works if first page of
# system tablespace is corrupted.
SHOW VARIABLES LIKE 'innodb_doublewrite';
Variable_name	Value
innodb_doublewrite	DETECT_ONLY
SET GLOBAL innodb_master_thread_disabled_debug=1;
BEGIN;
INSERT INTO t1 VALUES (6, repeat('%', 400));
SET GLOBAL innodb_checkpoint_disabled = 1;
# Make the first page dirty for system tablespace
SET GLOBAL innodb_saved_page_number_debug = 0;
SET GLOBAL innodb_fil_make_page_dirty_debug = 0;
ERROR HY000: Lost connection to MySQL server during query
# Corrupt the first page (page_no=0) of the system tablespace.
# restoring original datadir
# restart: --innodb_doublewrite=detect_only --datadir=BACKUP/data
# Test End - 6
# ---------------------------------------------------------------
# Test Begin: Test if recovery works if 2nd page of
# system tablespace is full of zeroes.
SHOW VARIABLES LIKE 'innodb_doublewrite';
Variable_name	Value
innodb_doublewrite	DETECT_ONLY
SET GLOBAL innodb_master_thread_disabled_debug=1;
BEGIN;
INSERT INTO t1 VALUES (6, repeat('%', 400));
SET GLOBAL innodb_checkpoint_disabled = 1;
# Make the second page dirty for system tablespace
SET GLOBAL innodb_saved_page_number_debug = 1;
SET GLOBAL innodb_fil_make_page_dirty_debug = 0;
ERROR HY000: Lost connection to MySQL server during query
# Make the 2nd page (page_no=1) of the system tablespace
# all zeroes.
# restoring original datadir
# restart: --innodb_doublewrite=detect_only --datadir=BACKUP/data
# Test End - 7
# ---------------------------------------------------------------
# Test Begin: Test if recovery works if 2nd page of
# system tablespace is corrupted.
SHOW VARIABLES LIKE 'innodb_doublewrite';
Variable_name	Value
innodb_doublewrite	DETECT_ONLY
SET GLOBAL innodb_master_thread_disabled_debug=1;
BEGIN;
INSERT INTO t1 VALUES (6, repeat('%', 400));
SET GLOBAL innodb_checkpoint_disabled = 1;
# Make the second page dirty for system tablespace
SET GLOBAL innodb_saved_page_number_debug = 1;
SET GLOBAL innodb_fil_make_page_dirty_debug = 0;
ERROR HY000: Lost connection to MySQL server during query
# Make the 2nd page (page_no=1) of the system tablespace
# all zeroes.
# restoring original datadir
# restart
# Test End - 8
# ---------------------------------------------------------------
DROP TABLE t1;

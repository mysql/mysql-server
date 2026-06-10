################################################################################
# === Purpose ===
# This test ensures that server does not modify the charset and collation of
# temporal types and comparison of temporal values with other charsets are
# handled smoothly by the server.
#
# === References ===
# Bug#114830: MySQL converts collation of date data type in ibd but data dictionary
################################################################################

CREATE TABLE test (
   dt datetime primary key,
   datetxt varchar(10) GENERATED ALWAYS AS (DATE(dt)) STORED,
   timetxt varchar(10) GENERATED ALWAYS AS (TIME(dt)) STORED
) ENGINE=InnoDB;

SHOW CREATE TABLE test;

# Flush all pages to disk before running ibd2sdi
FLUSH TABLE test FOR EXPORT;
UNLOCK TABLES;

################################################################################
# Test - 1: Verify that collation of temporal types remains as
#           my_charset_numeric (collation_id 8) in the SDI.
################################################################################
--let $MYSQLD_DATADIR=`SELECT @@DATADIR`
--exec $IBD2SDI $MYSQLD_DATADIR/test/test.ibd -d $MYSQL_TMP_DIR/test.json 2>&1

--echo #
--echo # Verify that the collation_id is my_charset_numeric in the SDI.
--echo #
--echo # Expect 1 match
--let $grep_file=$MYSQL_TMP_DIR/test.json
--let $grep_pattern= \"collation_id\": 8,
--source include/grep_pattern.inc

--echo #
--echo # Verify that the collation_id does not change after ALTER TABLE
--echo # ENGINE=InnoDB
--echo #
ALTER TABLE test ENGINE=InnoDB;
FLUSH TABLE test FOR EXPORT;
UNLOCK TABLES;
--exec $IBD2SDI $MYSQLD_DATADIR/test/test.ibd -d $MYSQL_TMP_DIR/test.json 2>&1

--echo # Expect 1 match
--let $grep_file=$MYSQL_TMP_DIR/test.json
--let $grep_pattern= \"collation_id\": 8,
--source include/grep_pattern.inc

--echo #
--echo # Verify that the collation_id does not change after ALTER TABLE CONVERT
--echo # TO CHARACTER SET
--echo #
ALTER TABLE test CONVERT TO CHARACTER SET utf8mb4 COLLATE utf8mb4_0900_ai_ci;
FLUSH TABLE test FOR EXPORT;
UNLOCK TABLES;
--exec $IBD2SDI $MYSQLD_DATADIR/test/test.ibd -d $MYSQL_TMP_DIR/test.json 2>&1

--echo # Expect 1 match
--let $grep_file=$MYSQL_TMP_DIR/test.json
--let $grep_pattern= \"collation_id\": 8,
--source include/grep_pattern.inc

################################################################################
# Test - 2: Verify that comparision with varchar columns are handled properly.
################################################################################
--echo #
--echo # Insert some data and verify that comparision with varchar columns are handled properly.
--echo #
INSERT INTO test(dt) VALUES("1997-11-30 12:30:45"),("1999-12-13 10:11:53");
SELECT * FROM test;

--echo #
--echo # Verify that there are two rows in the table
--echo #
SELECT COUNT(*)=2 AS EXPECTED_RESULT FROM test WHERE DATE(dt) = datetxt;
SELECT COUNT(*)=2 AS EXPECTED_RESULT FROM test WHERE TIME(dt) = timetxt;

SELECT * FROM test WHERE DATE(dt) = datetxt;
SELECT * FROM test WHERE TIME(dt) = timetxt;

--echo #
--echo # Change the character set of the table to utf8mb4_unicode_ci
--echo #
ALTER TABLE test CONVERT TO CHARACTER SET utf8mb4 collate utf8mb4_unicode_ci;

--echo #
--echo # Verify that the comparision works after changing the charset of the table
--echo #
SELECT * FROM test WHERE DATE(dt) = datetxt;
SELECT * FROM test WHERE TIME(dt) = timetxt;

--echo #
--echo # Verify that the comparision works after changing the datatype of dt column
--echo #
ALTER TABLE test MODIFY dt TIMESTAMP(6);
INSERT INTO test(dt) VALUES("1999-08-15 11:38:25.123456"),("2007-07-29 11:15:34.245147");


--echo #
--echo # Change the character set of the table to utf32_unicode_ci
--echo #
ALTER TABLE test CONVERT TO CHARACTER SET utf32 COLLATE utf32_unicode_ci;

--echo #
--echo # Verify that there are four rows in the table
--echo # Note: We use STUBSTRING_INDEX to do the comparision as the
--echo #       TIME(dt) returns decimal value as well.
--echo #
SELECT COUNT(*)=4 AS EXPECTED_RESULT FROM test WHERE DATE(dt) = datetxt;
SELECT COUNT(*)=4 AS EXPECTED_RESULT FROM test WHERE SUBSTRING_INDEX(TIME(dt),'.',1) = timetxt;

--echo #
--echo # Verify that the comparision works after changing the charset of the table
--echo #
SELECT * FROM test WHERE DATE(dt) = datetxt;
SELECT * FROM test WHERE SUBSTRING_INDEX(TIME(dt),'.',1) = timetxt;

# Cleanup
--remove_file $MYSQL_TMP_DIR/test.json
DROP TABLE test;

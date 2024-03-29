--echo #
--echo # Bug#30787535 : FULLTEXT INDEX TABLES CREATED IN ENCRYPTED SCHEMA ARE NOT ENCRYPTED
--echo #

################################################################################
# For an encrypted table, this test case checks physical ibd file for :
#	- table
#	- fts table created for an FTS index on table
#	- Other aux tables created for FTS index
#  to see if they are encrypted.
################################################################################

--echo #########
--echo # SETUP #
--echo #########
--echo
let datadir=`SELECT @@datadir`;
let search_pattern=supremum;

--echo #########################################################################
--echo # 1 : WITH KEYRING
--echo #########################################################################

--echo # Create a new 'unencrypted' table
CREATE TABLE t1 (id INT UNSIGNED AUTO_INCREMENT NOT NULL PRIMARY KEY,
                 C1 TEXT(500),
                 C2 VARCHAR(200),
                 C3 VARCHAR(200)) ENCRYPTION='N' ENGINE=InnoDB;

# Make sure all pages are flushed
set global innodb_buf_flush_list_now = 1;

--echo # ---------------------------------------------------------------
--echo # Test 1 : t1 un-encrypted, FTS tables should also be unencrypted
--echo # ---------------------------------------------------------------
--echo # Check that tablespace file is not encrypted
--let ts_name=test/t1.ibd
--source include/if_encrypted.inc

CREATE FULLTEXT INDEX idx ON t1(C1);

# Make sure all pages are flushed
set global innodb_buf_flush_list_now = 1;

--let ts_name = `select NAME from information_schema.innodb_tables where name like "%index_1%";`
--let ts_name = $ts_name.ibd
--echo # Check that FTS tablespaces file is not encrypted
--source include/if_encrypted.inc

--echo # ---------------------------------------------------------------
--echo # Test 1 : t1 encrypted, FTS tables should also be unencrypted
--echo # ---------------------------------------------------------------
# Now, change the encryption property of table
ALTER TABLE t1 ENCRYPTION='Y';

--echo # Check that tablespace file is encrypted now
--let ts_name=test/t1.ibd
--source include/if_encrypted.inc

--let ts_name = `select NAME from information_schema.innodb_tables where name like "%index_1%";`
--let ts_name = $ts_name.ibd
--echo # Check that FTS tablespace file is encrypted now
--source include/if_encrypted.inc

--echo ###########
--echo # CLEANUP #
--echo ###########
DROP TABLE test.t1;
remove_file $MYSQLTEST_VARDIR/tmpfile.txt;

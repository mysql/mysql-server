#
# Bug#30787535 : FULLTEXT INDEX TABLES CREATED IN ENCRYPTED SCHEMA ARE NOT ENCRYPTED
#
#########
# SETUP #
#########

#########################################################################
# RESTART 1 : WITH KEYRING PLUGIN
#########################################################################
# Create a new 'unencrypted' table
CREATE TABLE t1 (id INT UNSIGNED AUTO_INCREMENT NOT NULL PRIMARY KEY,
C1 TEXT(500),
C2 VARCHAR(200),
C3 VARCHAR(200)) ENCRYPTION='N' ENGINE=InnoDB;
set global innodb_buf_flush_list_now = 1;
# ---------------------------------------------------------------
# Test 1 : t1 un-encrypted, FTS tables should also be unencrypted
# ---------------------------------------------------------------
# Check that tablespace file is not encrypted
# Print result
table space is Unencrypted.
CREATE FULLTEXT INDEX idx ON t1(C1);
Warnings:
Warning	124	InnoDB rebuilding table to add column FTS_DOC_ID
set global innodb_buf_flush_list_now = 1;
# Check that FTS tablespaces file is not encrypted
# Print result
table space is Unencrypted.
# ---------------------------------------------------------------
# Test 1 : t1 encrypted, FTS tables should also be unencrypted
# ---------------------------------------------------------------
ALTER TABLE t1 ENCRYPTION='Y';
# Check that tablespace file is encrypted now
# Print result
table space is Encrypted.
# Check that FTS tablespace file is encrypted now
# Print result
table space is Encrypted.
###########
# CLEANUP #
###########
DROP TABLE test.t1;
# Restarting server without keyring to restore server state
# restart: 

include/master-slave.inc
Warnings:
Note	####	Sending passwords in plain text without SSL/TLS is extremely insecure.
Note	####	Storing MySQL user name or password information in the master info repository is not secure and is therefore not recommended. Please consider using the USER and PASSWORD connection options for START SLAVE; see the 'START SLAVE Syntax' in the MySQL Manual for more information.
[connection master]
include/assert.inc [The value for transaction_write_set_extraction must be XXHASH64]
CREATE TABLE t1 (a BINARY(1) PRIMARY KEY);
SET @debug_saved= @@GLOBAL.DEBUG;
SET @@GLOBAL.DEBUG= '+d,PKE_assert_single_primary_key_generated_insert';
INSERT INTO t1 VALUES(1);
SET @@GLOBAL.DEBUG= @debug_saved;
SET @@GLOBAL.DEBUG= '+d,PKE_assert_single_primary_key_generated_update';
UPDATE t1 SET a=3 WHERE a=1;
SET @@GLOBAL.DEBUG= @debug_saved;
include/rpl_sync.inc
DROP TABLE t1;
include/rpl_sync.inc
CREATE TABLE t1(a BINARY(1), b BINARY(1), PRIMARY KEY(a, b));
SET @@GLOBAL.DEBUG= @debug_saved;
SET @@GLOBAL.DEBUG= '+d,PKE_assert_multi_primary_key_generated_insert';
INSERT INTO t1 VALUE(1, 2);
SET @@GLOBAL.DEBUG= @debug_saved;
SET @@GLOBAL.DEBUG= '+d,PKE_assert_multi_primary_key_generated_update';
UPDATE t1 SET a=3 WHERE a=1;
SET @@GLOBAL.DEBUG= @debug_saved;
include/rpl_sync.inc
DROP TABLE t1;
include/rpl_sync.inc
CREATE TABLE t1 (c1 BINARY(1) PRIMARY KEY, c2 BINARY(1) NOT NULL UNIQUE, c3 BINARY(1) NOT NULL UNIQUE);
SET @@GLOBAL.DEBUG= @debug_saved;
SET @@GLOBAL.DEBUG= '+d,PKE_assert_single_primary_unique_key_generated_insert';
INSERT INTO t1 VALUES (1, 2, 3);
SET @@GLOBAL.DEBUG= @debug_saved;
SET @@GLOBAL.DEBUG= '+d,PKE_assert_single_primary_unique_key_generated_update';
UPDATE t1 SET c1=5 WHERE c1=1;
include/rpl_sync.inc
DROP TABLE t1;
include/rpl_sync.inc
CREATE TABLE t1 (a BINARY(1), d BINARY(1), b BINARY(1) NOT NULL UNIQUE, c BINARY(1) NOT NULL UNIQUE, PRIMARY KEY(a, d));
SET @@GLOBAL.DEBUG= @debug_saved;
SET @@GLOBAL.DEBUG= '+d,PKE_assert_multi_primary_unique_key_generated_insert';
INSERT INTO t1 VALUES(1, 2, 3, 4);
SET @@GLOBAL.DEBUG= @debug_saved;
SET @@GLOBAL.DEBUG= '+d,PKE_assert_multi_primary_unique_key_generated_update';
UPDATE t1 SET a=5 WHERE a=1;
SET @@GLOBAL.DEBUG= @debug_saved;
include/rpl_sync.inc
DROP TABLE t1;
include/rpl_sync.inc
CREATE TABLE t1 (a BINARY(1) PRIMARY KEY);
CREATE TABLE t2 (b BINARY(1) PRIMARY KEY);
CREATE TABLE t3 (c1 BINARY(1), c2 BINARY(1) NOT NULL UNIQUE, PRIMARY KEY(c1, c2), FOREIGN KEY(c1) REFERENCES t1(a), FOREIGN KEY(c2) REFERENCES t2(b));
INSERT INTO t1 VALUES (1);
INSERT INTO t2 VALUES (5);
SET @@GLOBAL.DEBUG= @debug_saved;
SET @@GLOBAL.DEBUG= '+d,PKE_assert_multi_foreign_key_generated_insert';
INSERT INTO t3 values(1,5);
SET @@GLOBAL.DEBUG= @debug_saved;
INSERT INTO t1 VALUES (3);
include/rpl_sync.inc
SET @@GLOBAL.DEBUG= '+d,PKE_assert_multi_foreign_key_generated_update';
UPDATE t3 SET c1=3 WHERE c1=1;
include/rpl_sync.inc
SET @@GLOBAL.DEBUG= @debug_saved;
DROP TABLE t3;
DROP TABLE t2;
DROP TABLE t1;
include/rpl_sync.inc
CREATE TABLE t1 (c1 BINARY(1) PRIMARY KEY, c2 BINARY(1), UNIQUE KEY(c2));
CREATE TABLE t2 (x1 BINARY(1) PRIMARY KEY, x2 BINARY(1), FOREIGN KEY (x2) REFERENCES t1(c2));
INSERT INTO t1 VALUES (1,1);
SET @@GLOBAL.DEBUG= @debug_saved;
SET @@GLOBAL.DEBUG= '+d,PKE_assert_foreign_key_on_referenced_unique_key_parent_generated_insert';
INSERT INTO t1 VALUES (2,2);
SET @@GLOBAL.DEBUG= @debug_saved;
SET @@GLOBAL.DEBUG= '+d,PKE_assert_foreign_key_on_referenced_unique_key_generated_insert';
INSERT INTO t2 VALUES (1,1);
SET @@GLOBAL.DEBUG= @debug_saved;
SET @@GLOBAL.DEBUG= '+d,PKE_assert_foreign_key_on_referenced_unique_key_generated_update';
UPDATE t2 SET x2=2 WHERE x1=1;
SET @@GLOBAL.DEBUG= @debug_saved;
DROP TABLE t2;
DROP TABLE t1;
include/rpl_sync.inc
CREATE TABLE t1 (c1 BINARY(1) PRIMARY KEY, c2 BINARY(1), KEY(c2));
CREATE TABLE t2 (x1 BINARY(1) PRIMARY KEY, x2 BINARY(1), FOREIGN KEY (x2) REFERENCES t1(c2));
INSERT INTO t1 VALUES (1,1);
SET @@GLOBAL.DEBUG= @debug_saved;
SET @@GLOBAL.DEBUG= '+d,PKE_assert_foreign_key_on_referenced_non_unique_key_parent_generated_insert';
INSERT INTO t1 VALUES (2,2);
SET @@GLOBAL.DEBUG= @debug_saved;
SET @@GLOBAL.DEBUG= '+d,PKE_assert_foreign_key_on_referenced_non_unique_key_generated_insert';
INSERT INTO t2 VALUES (1,1);
SET @@GLOBAL.DEBUG= @debug_saved;
SET @@GLOBAL.DEBUG= '+d,PKE_assert_foreign_key_on_referenced_non_unique_key_generated_update';
UPDATE t2 SET x2=2 WHERE x1=1;
SET @@GLOBAL.DEBUG= @debug_saved;
DROP TABLE t2;
DROP TABLE t1;
include/rpl_sync.inc
include/rpl_end.inc

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
CREATE TABLE t1 (a BINARY(1), b BINARY(1) PRIMARY KEY INVISIBLE);
SELECT * FROM t1;
a
SET @debug_saved= @@GLOBAL.DEBUG;
SET @@GLOBAL.DEBUG= '+d,PKE_assert_single_primary_key_generated_insert';
INSERT INTO t1 (a, b) VALUES(0, 1);
SET @@GLOBAL.DEBUG= @debug_saved;
SET @@GLOBAL.DEBUG= '+d,PKE_assert_single_primary_key_generated_update';
UPDATE t1 SET b=3 WHERE b=1;
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
CREATE TABLE t1 (a BINARY(1), d BINARY(1), b BINARY(1) NOT NULL UNIQUE INVISIBLE, c BINARY(1) NOT NULL UNIQUE INVISIBLE, PRIMARY KEY(a, d));
SELECT * FROM t1;
a	d
SET @@GLOBAL.DEBUG= @debug_saved;
SET @@GLOBAL.DEBUG= '+d,PKE_assert_multi_primary_unique_key_generated_insert';
INSERT INTO t1 (a, d, b, c) VALUES (1, 2, 3, 4);
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
CREATE TABLE t1 (c1 BINARY(1) PRIMARY KEY, c2 BINARY(1) INVISIBLE, UNIQUE KEY(c2));
SELECT * FROM t1;
c1
CREATE TABLE t2 (x1 BINARY(1) PRIMARY KEY, x2 BINARY(1) INVISIBLE, FOREIGN KEY (x2) REFERENCES t1(c2));
SELECT * FROM t2;
x1
INSERT INTO t1 (c1, c2) VALUES (1,1);
SET @@GLOBAL.DEBUG= @debug_saved;
SET @@GLOBAL.DEBUG= '+d,PKE_assert_foreign_key_on_referenced_unique_key_parent_generated_insert';
INSERT INTO t1 (c1, c2) VALUES (2,2);
SET @@GLOBAL.DEBUG= @debug_saved;
SET @@GLOBAL.DEBUG= '+d,PKE_assert_foreign_key_on_referenced_unique_key_generated_insert';
INSERT INTO t2 (x1, x2) VALUES (1,1);
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
CREATE TABLE t1(a BINARY(1), b BINARY(1), c BINARY(1), PRIMARY KEY (a,b,c));
CREATE TABLE t2(d BINARY(1), e BINARY(1), f BINARY(1), PRIMARY KEY (d,e,f), CONSTRAINT foreign_key_t2_t1 FOREIGN KEY (d,e,f) REFERENCES t1(a,b,c));
INSERT INTO t1 VALUES (1,2,3);
INSERT INTO t1 VALUES (4,2,3);
SET @@GLOBAL.DEBUG= @debug_saved;
SET @@GLOBAL.DEBUG= '+d,PKE_assert_multi_column_foreign_key_on_multiple_column_primary_key_insert';
INSERT INTO t2 VALUES (1,2,3);
SET @@GLOBAL.DEBUG= @debug_saved;
SET @@GLOBAL.DEBUG= '+d,PKE_assert_multi_column_foreign_key_on_multiple_column_primary_key_update';
UPDATE t2 SET d=4 WHERE d=1;
SET @@GLOBAL.DEBUG= @debug_saved;
DROP TABLE t2;
DROP TABLE t1;
include/rpl_sync.inc
CREATE TABLE t1 (a BINARY(1) PRIMARY KEY, b BINARY(1) NOT NULL, c BINARY(1) NOT NULL, UNIQUE KEY key_b_c(b,c));
CREATE TABLE t2 (d BINARY(1) PRIMARY KEY, e BINARY(1) NOT NULL, f BINARY(1) NOT NULL, UNIQUE KEY key_e_f(e,f), FOREIGN KEY(e,f) REFERENCES t1(b,c));
SET @@GLOBAL.DEBUG= @debug_saved;
SET @@GLOBAL.DEBUG= '+d,PKE_assert_multiple_column_unique_key_insert';
INSERT INTO t1 VALUES (1,2,3);
SET @@GLOBAL.DEBUG= @debug_saved;
INSERT INTO t1 VALUES (4,5,6);
SET @@GLOBAL.DEBUG= @debug_saved;
SET @@GLOBAL.DEBUG= '+d,PKE_assert_multi_column_foreign_key_on_multiple_column_unique_key_insert';
INSERT INTO t2 VALUES (1,2,3);
SET @@GLOBAL.DEBUG= @debug_saved;
SET @@GLOBAL.DEBUG= '+d,PKE_assert_multi_column_foreign_key_on_multiple_column_unique_key_update';
UPDATE t2 SET d=4,e=5,f=6 WHERE d=1;
SET @@GLOBAL.DEBUG= @debug_saved;
DROP TABLE t2;
DROP TABLE t1;
include/rpl_sync.inc
include/rpl_end.inc

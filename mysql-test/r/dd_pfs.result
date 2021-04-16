create schema s;
create table s.t1(i int);
create table s.t2(i int) tablespace innodb_system;
drop schema s;
###############################################################################
include/assert.inc ['CREATE SCHEMA s' will allocate 2 objects.]
include/assert.inc ['CREATE SCHEMA s' will free 2 objects.]
include/assert.inc ['CREATE SCHEMA s' will leave 0 objects.]
###############################################################################
include/assert.inc ['CREATE TABLE s.t_myisam(i int) engine myisam' will allocate 9 objects.]
include/assert.inc ['CREATE TABLE s.t_myisam(i int) engine myisam' will free 6 objects.]
include/assert.inc ['CREATE TABLE s.t_myisam(i int) engine myisam' will leave 3 objects.]
###############################################################################
include/assert.inc ['CREATE TABLE s.t_innodb_1(i int) TABLESPACE innodb_system' will allocate 52 objects.]
include/assert.inc ['CREATE TABLE s.t_innodb_1(i int) TABLESPACE innodb_system' will free 52 objects.]
include/assert.inc ['CREATE TABLE s.t_innodb_1(i int) TABLESPACE innodb_system' will leave 0 objects.]
###############################################################################
include/assert.inc ['CREATE TABLE s.t_innodb_2(i int) TABLESPACE innodb_system' will allocate 52 objects.]
include/assert.inc ['CREATE TABLE s.t_innodb_2(i int) TABLESPACE innodb_system' will free 52 objects.]
include/assert.inc ['CREATE TABLE s.t_innodb_2(i int) TABLESPACE innodb_system' will leave 0 objects.]
###############################################################################
include/assert.inc ['SELECT * FROM s.t_myisam' will allocate 0 objects.]
include/assert.inc ['SELECT * FROM s.t_myisam' will free 0 objects.]
include/assert.inc ['SELECT * FROM s.t_myisam' will leave 0 objects.]
###############################################################################
include/assert.inc ['SELECT * FROM s.t_innodb_1' will allocate 12 objects.]
include/assert.inc ['SELECT * FROM s.t_innodb_1' will free 2 objects.]
include/assert.inc ['SELECT * FROM s.t_innodb_1' will leave 10 objects.]
###############################################################################
include/assert.inc ['SELECT * FROM s.t_innodb_2' will allocate 12 objects.]
include/assert.inc ['SELECT * FROM s.t_innodb_2' will free 2 objects.]
include/assert.inc ['SELECT * FROM s.t_innodb_2' will leave 10 objects.]
###############################################################################
include/assert.inc ['SELECT * FROM s.t_innodb_1' will allocate 0 objects.]
include/assert.inc ['SELECT * FROM s.t_innodb_1' will free 0 objects.]
include/assert.inc ['SELECT * FROM s.t_innodb_1' will leave 0 objects.]
###############################################################################
include/assert.inc ['DROP TABLE s.t_innodb_1' will allocate 7 objects.]
include/assert.inc ['DROP TABLE s.t_innodb_1' will free 17 objects.]
include/assert.inc ['DROP TABLE s.t_innodb_1' will leave -10 objects.]
###############################################################################
include/assert.inc ['DROP TABLE s.t_innodb_2' will allocate 7 objects.]
include/assert.inc ['DROP TABLE s.t_innodb_2' will free 17 objects.]
include/assert.inc ['DROP TABLE s.t_innodb_2' will leave -10 objects.]
###############################################################################
include/assert.inc ['DROP SCHEMA s' will allocate 2 objects.]
include/assert.inc ['DROP SCHEMA s' will free 5 objects.]
include/assert.inc ['DROP SCHEMA s' will leave -3 objects.]
###############################################################################
include/assert.inc ['SELECT * FROM test.tables' will allocate 168 objects.]
include/assert.inc ['SELECT * FROM test.tables' will free 0 objects.]
include/assert.inc ['SELECT * FROM test.tables' will leave 168 objects.]
We will see that the total size allocated is about 40K.
###############################################################################
include/assert.inc ['' will allocate 151 + 151 objects.]
include/assert.inc ['' will free 151 + 151 objects.]
###############################################################################
include/assert.inc ['DROP SCHEMA s' will allocate 601 objects.]
include/assert.inc ['DROP SCHEMA s' will free 2202 objects.]
include/assert.inc ['DROP SCHEMA s' will leave -1601 objects.]
CREATE SCHEMA s1;
CREATE TABLE s1.t_myisam(i INT) ENGINE=MYISAM;
CREATE VIEW s1.v1 AS SELECT * FROM s1.t_myisam;
SELECT * FROM s1.v1;
i
###############################################################################
include/assert.inc ['DROP SCHEMA s1' will allocate 17 objects.]
include/assert.inc ['DROP SCHEMA s1' will free 26 objects.]
include/assert.inc ['DROP SCHEMA s1' will leave -9 objects.]
CREATE SCHEMA s2;
CREATE TABLE s2.t_innodb(i INT) TABLESPACE=innodb_system;
CREATE VIEW v2 AS SELECT * FROM s2.t_innodb;
SELECT * FROM v2;
i
###############################################################################
include/assert.inc ['DROP SCHEMA s2' will allocate 19 objects.]
include/assert.inc ['DROP SCHEMA s2' will free 33 objects.]
include/assert.inc ['DROP SCHEMA s2' will leave -14 objects.]
DROP VIEW v2;
CREATE SCHEMA s3;
CREATE TABLE s3.t_innodb(i INT) TABLESPACE=innodb_system;
CREATE VIEW s3.v3 AS SELECT * FROM s3.t_innodb;
SELECT * FROM s3.v3;
i
###############################################################################
include/assert.inc ['DROP SCHEMA s3' will allocate 13 objects.]
include/assert.inc ['DROP SCHEMA s3' will free 30 objects.]
include/assert.inc ['DROP SCHEMA s3' will leave -17 objects.]
CREATE SCHEMA s4;
CREATE VIEW s4.v4 AS SELECT 1 AS i;
CREATE VIEW s4.v5 AS SELECT * FROM s4.v4;
SELECT * FROM s4.v5;
i
1
###############################################################################
include/assert.inc ['DROP SCHEMA s4' will allocate 7 objects.]
include/assert.inc ['DROP SCHEMA s4' will free 18 objects.]
include/assert.inc ['DROP SCHEMA s4' will leave -11 objects.]

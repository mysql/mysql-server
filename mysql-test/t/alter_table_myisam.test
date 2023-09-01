--source include/force_myisam_default.inc
--source include/have_myisam.inc

SET sql_mode = 'NO_ENGINE_SUBSTITUTION';

create table t1 (bandID MEDIUMINT UNSIGNED NOT NULL PRIMARY KEY, payoutID SMALLINT UNSIGNED NOT NULL) engine=myisam;
insert into t1 (bandID,payoutID) VALUES (1,6),(2,6),(3,4),(4,9),(5,10),(6,1),(7,12),(8,12);
alter table t1 add column new_col int, order by payoutid,bandid;
select * from t1;
alter table t1 order by bandid,payoutid;
select * from t1;
drop table t1;

CREATE TABLE t1 (
  id int(11) unsigned NOT NULL default '0',
  category_id tinyint(4) unsigned NOT NULL default '0',
  type_id tinyint(4) unsigned NOT NULL default '0',
  body text NOT NULL,
  user_id int(11) unsigned NOT NULL default '0',
  status enum('new','old') NOT NULL default 'new',
  PRIMARY KEY (id)
) ENGINE=MyISAM;

ALTER TABLE t1 ORDER BY t1.id, t1.status, t1.type_id, t1.user_id, t1.body;
DROP TABLE t1;

#
# The following combination found a hang-bug in MyISAM
#

CREATE TABLE t1 (AnamneseId int(10) unsigned NOT NULL auto_increment,B BLOB,PRIMARY KEY (AnamneseId)) engine=myisam;
insert into t1 values (null,"hello");
LOCK TABLES t1 WRITE;
ALTER TABLE t1 ADD Column new_col int not null;
UNLOCK TABLES;
OPTIMIZE TABLE t1;
DROP TABLE t1;

# Disable/Enable keys supported by Myisam only
# ALTER TABLE ... ENABLE/DISABLE KEYS

create table t1 (n1 int not null, n2 int, n3 int, n4 float,
                unique(n1),
                key (n1, n2, n3, n4), 
                key (n2, n3, n4, n1), 
                key (n3, n4, n1, n2), 
                key (n4, n1, n2, n3) ) engine=Myisam;
alter table t1 disable keys;
show keys from t1;
#let $1=10000;
let $1=10;
while ($1) 
{
 eval insert into t1 values($1,RAND()*1000,RAND()*1000,RAND());
 dec $1;
}
alter table t1 enable keys;
show keys from t1;
drop table t1;

#
# Test ALTER TABLE ENABLE/DISABLE keys when things are locked
#

CREATE TABLE t1 (
  Host varchar(16) binary NOT NULL default '',
  User varchar(16) binary NOT NULL default '',
  PRIMARY KEY  (Host,User)
) ENGINE=MyISAM;

ALTER TABLE t1 DISABLE KEYS;
LOCK TABLES t1 WRITE;
INSERT INTO t1 VALUES ('localhost','root'),('localhost',''),('games','monty');
SHOW INDEX FROM t1;
ALTER TABLE t1 ENABLE KEYS;
UNLOCK TABLES;
CHECK TABLES t1;
DROP TABLE t1;

#
# Test with two keys
#
CREATE TABLE t1 (
  Host varchar(16) binary NOT NULL default '',
  User varchar(16) binary NOT NULL default '',
  PRIMARY KEY  (Host,User),
  KEY  (Host)
) ENGINE=MyISAM;

ALTER TABLE t1 DISABLE KEYS;
SHOW INDEX FROM t1;
LOCK TABLES t1 WRITE;
INSERT INTO t1 VALUES ('localhost','root'),('localhost','');
SHOW INDEX FROM t1;
ALTER TABLE t1 ENABLE KEYS;
SHOW INDEX FROM t1;
UNLOCK TABLES;
CHECK TABLES t1;

# Test RENAME with LOCK TABLES
LOCK TABLES t1 WRITE;
ALTER TABLE t1 RENAME t2;
UNLOCK TABLES;
--sorted_result
select * from t2;
DROP TABLE t2;

#
# Test disable keys with locking
#
CREATE TABLE t1 (
  Host varchar(16) binary NOT NULL default '',
  User varchar(16) binary NOT NULL default '',
  PRIMARY KEY  (Host,User),
  KEY  (Host)
) ENGINE=MyISAM;

LOCK TABLES t1 WRITE;
ALTER TABLE t1 DISABLE KEYS;
SHOW INDEX FROM t1;
DROP TABLE t1;


#
# BUG#6236 - ALTER TABLE MODIFY should set implicit NOT NULL on PK columns
#
drop table if exists t1, t2;
create table t1 ( a varchar(10) not null primary key ) engine=myisam;
create table t2 ( a varchar(10) not null primary key ) engine=merge union=(t1);
flush tables;
alter table t1 modify a varchar(10);
show create table t2;
flush tables;
alter table t1 modify a varchar(10) not null;
show create table t2;
drop table if exists t1, t2;

# The following is also part of bug #6236 (CREATE TABLE didn't properly count
# not null columns for primary keys)

create table t1 (a int, b int, c int, d int, e int, f int, g int, h int,i int, primary key (a,b,c,d,e,f,g,i,h)) engine=MyISAM;
insert into t1 (a) values(1);
--replace_column 7 X 8 X 9 X 10 X 11 X 12 X 13 X 14 X
show table status like 't1';
alter table t1 modify a int;
--replace_column 7 X 8 X 9 X 10 X 11 X 12 X 13 X 14 X
show table status like 't1';
drop table t1;
create table t1 (a int not null, b int not null, c int not null, d int not null, e int not null, f int not null, g int not null, h int not null,i int not null, primary key (a,b,c,d,e,f,g,i,h)) engine=MyISAM;
insert into t1 (a) values(1);
--replace_column 7 X 8 X 9 X 10 X 11 X 12 X 13 X 14 X
show table status like 't1';
drop table t1;

#
# BUG 12207 alter table ... discard table space on MyISAM table causes ERROR 2013 (HY000)
#
# Some platforms (Mac OS X, Windows) will send the error message using small letters.
CREATE TABLE T12207(a int) engine=MyISAM;
--replace_result t12207 T12207
--error ER_ILLEGAL_HA
ALTER TABLE T12207 DISCARD TABLESPACE;
DROP TABLE T12207;

#
# Disable/Enable keys supported by Myisam only
# Bug #24395: ALTER TABLE DISABLE KEYS doesn't work when modifying the table
#
# This problem happens if the data change is compatible.
# Changing to the same type is compatible for example.
#
--disable_warnings
drop table if exists t1;
--enable_warnings
create table t1 (a int, key(a)) engine=myisam;
show indexes from t1;
--echo "this used not to disable the index"
alter table t1 modify a int, disable keys;
show indexes from t1;

alter table t1 enable keys;
show indexes from t1;

alter table t1 modify a bigint, disable keys;
show indexes from t1;

alter table t1 enable keys;
show indexes from t1;

alter table t1 add b char(10), disable keys;
show indexes from t1;

alter table t1 add c decimal(10,2), enable keys;
show indexes from t1;

--echo "this however did"
alter table t1 disable keys;
show indexes from t1;

desc t1;

alter table t1 add d decimal(15,5);
--echo "The key should still be disabled"
show indexes from t1;

drop table t1;

--echo "Now will test with one unique index"
create table t1(a int, b char(10), unique(a)) engine=myisam;
show indexes from t1;
alter table t1 disable keys;
show indexes from t1;
alter table t1 enable keys;

--echo "If no copy on noop change, this won't touch the data file"
--echo "Unique index, no change"
alter table t1 modify a int, disable keys;
show indexes from t1;

--echo "Change the type implying data copy"
--echo "Unique index, no change"
alter table t1 modify a bigint, disable keys;
show indexes from t1;

alter table t1 modify a bigint;
show indexes from t1;

alter table t1 modify a int;
show indexes from t1;

drop table t1;

--echo "Now will test with one unique and one non-unique index"
create table t1(a int, b char(10), unique(a), key(b)) engine=myisam;
show indexes from t1;
alter table t1 disable keys;
show indexes from t1;
alter table t1 enable keys;


--echo "If no copy on noop change, this won't touch the data file"
--echo "The non-unique index will be disabled"
alter table t1 modify a int, disable keys;
show indexes from t1;
alter table t1 enable keys;
show indexes from t1;

--echo "Change the type implying data copy"
--echo "The non-unique index will be disabled"
alter table t1 modify a bigint, disable keys;
show indexes from t1;

--echo "Change again the type, but leave the indexes as_is"
alter table t1 modify a int;
show indexes from t1;
--echo "Try the same. When data is no copied on similar tables, this is noop"

alter table t1 modify a int;
show indexes from t1;

drop table t1;

#
# ROW_FORMAT=FIXED supported by Myisam only
# BUG#23404 - ROW_FORMAT=FIXED option is lost is an index is added to the
# table
#
CREATE TABLE t1(a INT) ENGINE=MyISAM ROW_FORMAT=FIXED;
CREATE INDEX i1 ON t1(a);
SHOW CREATE TABLE t1;
DROP INDEX i1 ON t1;
SHOW CREATE TABLE t1;
DROP TABLE t1;

#
# Disable/Enable keys supported by Myisam only
# Bug#24219 - ALTER TABLE ... RENAME TO ... , DISABLE KEYS leads to crash
#
--disable_warnings
DROP TABLE IF EXISTS bug24219;
DROP TABLE IF EXISTS bug24219_2;
--enable_warnings

CREATE TABLE bug24219 (a INT, INDEX(a)) ENGINE=MyISAM;

SHOW INDEX FROM bug24219;

ALTER TABLE bug24219 RENAME TO bug24219_2, DISABLE KEYS;

SHOW INDEX FROM bug24219_2;

DROP TABLE bug24219_2;

#
# Bug#25262 Auto Increment lost when changing Engine type
#

create table t1(id int(8) primary key auto_increment) engine=heap;

insert into t1 values (null);
insert into t1 values (null);

select * from t1;

# Set auto increment to 50
alter table t1 auto_increment = 50;

# Alter to myisam
alter table t1 engine = myisam;

# This insert should get id 50
insert into t1 values (null);
select * from t1;

# Alter to heap again
alter table t1 engine = heap;
insert into t1 values (null);
select * from t1;

drop table t1;

#
# Bug#27507: Wrong DATETIME value was allowed by ALTER TABLE in the
#            NO_ZERO_DATE mode.
set sql_mode= default;
create table t1(f1 int) engine=myisam;
alter table t1 add column f2 datetime not null, add column f21 date not null;
insert into t1 values(1,'2000-01-01','2000-01-01');
--error 1292
alter table t1 add column f3 datetime not null;
--error 1292
alter table t1 add column f3 date not null;
--error 1292
alter table t1 add column f4 datetime not null default '2002-02-02',
  add column f41 date not null;
alter table t1 add column f4 datetime not null default '2002-02-02',
  add column f41 date not null default '2002-02-02';
select * from t1;
drop table t1;

#
# Bug#6073 "ALTER table minor glich": ALTER TABLE complains that an index
# without # prefix is not allowed for TEXT columns, while index
# is defined with prefix.
#
create table t1 (t varchar(255) default null, key t (t(80)))
engine=myisam default charset=latin1;
alter table t1 change t t text;
drop table t1;

#
# pack_keys and max_data_length options are meant for Myisam tables
# Bug#39372 "Smart" ALTER TABLE not so smart after all.
#
create table t1(f1 int not null, f2 int not null, key  (f1), key (f2)) engine=myisam;
let $count= 50;
--disable_query_log
while ($count)
{
  EVAL insert into t1 values (1,1),(1,1),(1,1),(1,1),(1,1);
  EVAL insert into t1 values (2,2),(2,2),(2,2),(2,2),(2,2);
  dec $count ;
}
--enable_query_log

select index_length into @unpaked_keys_size from
information_schema.tables where table_name='t1';
alter table t1 pack_keys=1;
select index_length into @paked_keys_size from
information_schema.tables where table_name='t1';
select (@unpaked_keys_size > @paked_keys_size);

select max_data_length into @orig_max_data_length from
information_schema.tables where table_name='t1';
alter table t1 max_rows=100;
select max_data_length into @changed_max_data_length from
information_schema.tables where table_name='t1';
select (@orig_max_data_length > @changed_max_data_length);

drop table t1;

#
# Bug#43508: Renaming timestamp or date column triggers table copy
#

CREATE TABLE t1 (f1 TIMESTAMP NULL DEFAULT NULL,
                 f2 INT(11) DEFAULT NULL) ENGINE=MYISAM DEFAULT CHARSET=utf8mb3;

INSERT INTO t1 VALUES (NULL, NULL), ("2009-10-09 11:46:19", 2);

--echo this should affect no rows as there is no real change
--enable_info
ALTER TABLE t1 CHANGE COLUMN f1 f1_no_real_change TIMESTAMP NULL DEFAULT NULL;
--disable_info
DROP TABLE t1;

--echo #
--echo # Test for bug #12652385 - "61493: REORDERING COLUMNS TO POSITION
--echo #                           FIRST CAN CAUSE DATA TO BE CORRUPTED".
--echo #
--echo # Use MyISAM engine as the fact that InnoDB doesn't support
--echo # in-place ALTER TABLE in cases when columns are being renamed
--echo # hides some bugs.
create table t1 (i int, j int) engine=myisam;
insert into t1 value (1, 2);
--echo # First, test for original problem described in the bug report.
select * from t1;
--echo # Change of column order by the below ALTER TABLE statement should
--echo # affect both column names and column contents.
alter table t1 modify column j int first;
select * from t1;
--echo # Now test for similar problem with the same root.
--echo # The below ALTER TABLE should change not only the name but
--echo # also the value for the last column of the table.
alter table t1 drop column i, add column k int default 0;
select * from t1;
--echo # Clean-up.
drop table t1;

--echo # Additional coverage for refactoring which is made as part
--echo # of fix for bug #27480 "Extend CREATE TEMPORARY TABLES privilege
--echo # to allow temp table operations".
--echo #
--echo # At some point the below test case failed on assertion.

--disable_warnings
DROP TABLE IF EXISTS t1;
--enable_warnings

CREATE TEMPORARY TABLE t1 (i int) ENGINE=MyISAM;

--error ER_ILLEGAL_HA
ALTER TABLE t1 DISCARD TABLESPACE;

DROP TABLE t1;

--echo #
--echo # 3) Test coverage for handling of RENAME INDEX clause in
--echo #    various storage engines and using different ALTER
--echo #    algorithm.
--echo #
--echo # 3.a) Test coverage for simple storage engines (MyISAM/Heap).
create table t1 (i int, key k(i)) engine=myisam;
insert into t1 values (1); 
create table t2 (i int, key k(i)) engine=memory;
insert into t2 values (1); 
--echo # MyISAM and Heap should be able to handle key renaming in-place.
alter table t1 algorithm=inplace, rename key k to kk;
alter table t2 algorithm=inplace, rename key k to kk;
show create table t1;
show create table t2;
--echo # So by default in-place algorithm should be chosen.
--echo # (ALTER TABLE should report 0 rows affected).
--enable_info
alter table t1 rename key kk to kkk; 
alter table t2 rename key kk to kkk; 
--disable_info
show create table t1;
show create table t2;
--echo # Copy algorithm should work as well.
alter table t1 algorithm=copy, rename key kkk to kkkk;
alter table t2 algorithm=copy, rename key kkk to kkkk;
show create table t1;
show create table t2;
--echo # When renaming is combined with other in-place operation
--echo # it still works as expected (i.e. works in-place).
alter table t1 algorithm=inplace, rename key kkkk to k, alter column i set default 100; 
alter table t2 algorithm=inplace, rename key kkkk to k, alter column i set default 100; 
show create table t1;
show create table t2;
--echo # Combining with non-inplace operation results in the whole ALTER
--echo # becoming non-inplace.
--error ER_ALTER_OPERATION_NOT_SUPPORTED
alter table t1 algorithm=inplace, rename key k to kk, add column j int; 
--error ER_ALTER_OPERATION_NOT_SUPPORTED
alter table t2 algorithm=inplace, rename key k to kk, add column j int; 
drop table t1, t2;

--echo #
--echo # WL#5534 Online ALTER, Phase 1
--echo #

--echo # Single thread tests.
--echo # See innodb_mysql_sync.test for multi thread tests.

CREATE TABLE m1(a INT PRIMARY KEY, b INT) engine=MyISAM;
INSERT INTO m1 VALUES (1,1), (2,2);

--echo #
--echo # 1: Test ALGORITHM keyword
--echo #

--echo # --enable_info allows us to see how many rows were updated
--echo # by ALTER TABLE. in-place will show 0 rows, while copy > 0.

--enable_info
ALTER TABLE m1 ENABLE KEYS;
ALTER TABLE m1 ENABLE KEYS, ALGORITHM= DEFAULT;
ALTER TABLE m1 ENABLE KEYS, ALGORITHM= COPY;
ALTER TABLE m1 ENABLE KEYS, ALGORITHM= INPLACE;
--disable_info

--echo #
--echo # 4: Test LOCK keyword
--echo #

ALTER TABLE m1 ENABLE KEYS, LOCK= DEFAULT;
--error ER_ALTER_OPERATION_NOT_SUPPORTED
ALTER TABLE m1 ENABLE KEYS, LOCK= NONE;
--error ER_ALTER_OPERATION_NOT_SUPPORTED
ALTER TABLE m1 ENABLE KEYS, LOCK= SHARED;
ALTER TABLE m1 ENABLE KEYS, LOCK= EXCLUSIVE;

--echo #
--echo # 5: Test ALGORITHM + LOCK
--echo #

--enable_info
--error ER_ALTER_OPERATION_NOT_SUPPORTED
ALTER TABLE m1 ENABLE KEYS, ALGORITHM= INPLACE, LOCK= NONE;
--error ER_ALTER_OPERATION_NOT_SUPPORTED
ALTER TABLE m1 ENABLE KEYS, ALGORITHM= INPLACE, LOCK= SHARED;
ALTER TABLE m1 ENABLE KEYS, ALGORITHM= INPLACE, LOCK= EXCLUSIVE;
--error ER_ALTER_OPERATION_NOT_SUPPORTED_REASON
ALTER TABLE m1 ENABLE KEYS, ALGORITHM= COPY, LOCK= NONE;
# This works because the lock will be SNW for the copy phase.
# It will still require exclusive lock for actually enabling keys.
ALTER TABLE m1 ENABLE KEYS, ALGORITHM= COPY, LOCK= SHARED;
ALTER TABLE m1 ENABLE KEYS, ALGORITHM= COPY, LOCK= EXCLUSIVE;
--disable_info

DROP TABLE m1;

# Disable/Enable keys supported by Myisam only
--echo #
--echo # 6: Possible deadlock involving thr_lock.c
--echo #

CREATE TABLE t1(a INT PRIMARY KEY, b INT) ENGINE=MyISAM;
INSERT INTO t1 VALUES (1,1), (2,2);

START TRANSACTION;
INSERT INTO t1 VALUES (3,3);

--echo # Connection con1
connect (con1, localhost, root);
--echo # Sending:
--send ALTER TABLE t1 DISABLE KEYS

--echo # Connection default
connection default;
--echo # Waiting until ALTER TABLE is blocked.
let $wait_condition=
  SELECT COUNT(*) = 1 FROM information_schema.processlist
  WHERE state = "Waiting for table metadata lock" AND
        info = "ALTER TABLE t1 DISABLE KEYS";
--source include/wait_condition.inc
UPDATE t1 SET b = 4;
COMMIT;

--echo # Connection con1
connection con1;
--echo # Reaping: ALTER TABLE t1 DISABLE KEYS
--reap
disconnect con1;
--source include/wait_until_disconnected.inc

--echo # Connection default
connection default;
DROP TABLE t1;

--echo #
--echo # 7: Which operations require copy and which can be done in-place?
--echo #
--echo # Test which ALTER TABLE operations are done in-place and
--echo # which operations are done using temporary table copy.
--echo #
--echo # --enable_info allows us to see how many rows were updated
--echo # by ALTER TABLE. in-place will show 0 rows, while copy > 0.
--echo #

--echo # Single operation tests

CREATE TABLE tm1(a INT NOT NULL, b INT, c INT) engine=MyISAM;
CREATE TABLE tm2(a INT PRIMARY KEY AUTO_INCREMENT, b INT, c INT) engine=MyISAM;
INSERT INTO tm1 VALUES (1,1,1), (2,2,2);
INSERT INTO tm2 VALUES (1,1,1), (2,2,2);

--enable_info
ALTER TABLE tm1;

ALTER TABLE tm1 ADD COLUMN d VARCHAR(200);
ALTER TABLE tm1 ADD COLUMN d2 VARCHAR(200);
ALTER TABLE tm1 ADD COLUMN e ENUM('a', 'b') FIRST;
ALTER TABLE tm1 ADD COLUMN f INT AFTER a;
ALTER TABLE tm1 ADD INDEX im1(b);
ALTER TABLE tm1 ADD UNIQUE INDEX im2 (c);
ALTER TABLE tm1 ADD FULLTEXT INDEX im3 (d);
ALTER TABLE tm1 ADD FULLTEXT INDEX im4 (d2);

# Bug#14140038 INCONSISTENT HANDLING OF FULLTEXT INDEXES IN ALTER TABLE
ALTER TABLE tm1 ADD PRIMARY KEY(a); 
ALTER TABLE tm1 DROP INDEX im3;
ALTER TABLE tm1 DROP COLUMN d2;
 
ALTER TABLE tm1 ADD CONSTRAINT fm1 FOREIGN KEY (b) REFERENCES tm2(a);
ALTER TABLE tm1 ALTER COLUMN b SET DEFAULT 1;
ALTER TABLE tm1 ALTER COLUMN b DROP DEFAULT;

# This will set both ALTER_COLUMN_NAME and COLUMN_DEFAULT_VALUE
ALTER TABLE tm1 CHANGE COLUMN f g INT;
ALTER TABLE tm1 CHANGE COLUMN g h VARCHAR(20);
ALTER TABLE tm1 MODIFY COLUMN e ENUM('a', 'b', 'c');
ALTER TABLE tm1 MODIFY COLUMN e INT;

# This will set both ALTER_COLUMN_ORDER and COLUMN_DEFAULT_VALUE
ALTER TABLE tm1 MODIFY COLUMN e INT AFTER h;
ALTER TABLE tm1 MODIFY COLUMN e INT FIRST;

# This will set both ALTER_COLUMN_NOT_NULLABLE and COLUMN_DEFAULT_VALUE
ALTER TABLE tm1 MODIFY COLUMN c INT NOT NULL;

# This will set both ALTER_COLUMN_NULLABLE and COLUMN_DEFAULT_VALUE
ALTER TABLE tm1 MODIFY COLUMN c INT NULL;

# This will set both ALTER_COLUMN_EQUAL_PACK_LENGTH and COLUMN_DEFAULT_VALUE
ALTER TABLE tm1 MODIFY COLUMN h VARCHAR(30);
ALTER TABLE tm1 MODIFY COLUMN h VARCHAR(30) AFTER d;

ALTER TABLE tm1 DROP COLUMN h;
ALTER TABLE tm1 DROP INDEX im2; 
ALTER TABLE tm1 DROP PRIMARY KEY; 
ALTER TABLE tm1 DROP FOREIGN KEY fm1; 
ALTER TABLE tm1 RENAME TO tm3; 
ALTER TABLE tm3 RENAME TO tm1; 
ALTER TABLE tm1 ORDER BY b;
ALTER TABLE tm1 CONVERT TO CHARACTER SET utf16;
ALTER TABLE tm1 DEFAULT CHARACTER SET utf8mb3;
ALTER TABLE tm1 FORCE;
ALTER TABLE tm1 AUTO_INCREMENT 3;
ALTER TABLE tm1 AVG_ROW_LENGTH 10;
ALTER TABLE tm1 CHECKSUM 1;
ALTER TABLE tm1 COMMENT 'test';
ALTER TABLE tm1 MAX_ROWS 100; 
ALTER TABLE tm1 MIN_ROWS 1;
ALTER TABLE tm1 PACK_KEYS 1;
DROP TABLE tm1,tm2;

--disable_info

--echo #
--echo # 8: Scenario in which ALTER TABLE was returning an unwarranted
--echo #    ER_ILLEGAL_HA error at some point during work on this WL.
--echo #

CREATE TABLE tm1(i INT DEFAULT 1) engine=MyISAM;
ALTER TABLE tm1 ADD INDEX ii1(i), ALTER COLUMN i DROP DEFAULT;
DROP TABLE tm1; 


--echo #
--echo #BUG#20106553: ALTER TABLE WHICH CHANGES INDEX COMMENT IS NOT
--echo #              LONGER INPLACE/FAST OPERATION.

--echo #Without the patch, the ALTER TABLE to change the index
--echo #comment using INPLACE algorithm reports an error.

CREATE TABLE t1(fld1 int, key key1(fld1) COMMENT 'test') ENGINE= MyISAM;
SHOW INDEX FROM t1;
ALTER TABLE t1 DROP INDEX key1, ADD INDEX key1(fld1), ALGORITHM=INPLACE;
SHOW INDEX FROM t1;
DROP TABLE t1;

# Alter the comment, but keep the same comment length
CREATE TABLE t1(fld1 int, key key1(fld1) COMMENT 'old comment') ENGINE=MyISAM;
SHOW INDEX FROM t1;
ALTER TABLE t1 DROP INDEX key1, ADD INDEX key1(fld1) COMMENT 'new comment',
ALGORITHM= INPLACE;
SHOW INDEX FROM t1;
DROP TABLE t1;

--echo #
--echo # BUG#20106837: ALTER TABLE WHICH DROPS AND ADDS THE SAME FULLTEXT
--echo #               INDEX IS NOT INPLACE/FAST.

CREATE TABLE t1(fld1 varchar(200), FULLTEXT(fld1)) ENGINE=MyISAM;
INSERT INTO t1 VALUES('ABCD');

--enable_info
--echo #Without patch, it was not fast a INPLACE ALTER.
ALTER TABLE t1 DROP INDEX fld1, ADD FULLTEXT INDEX fld1(fld1);
--disable_info

--echo #Without patch, reports an error 'ER_ALTER_OPERATION_NOT_SUPPORTED'.
ALTER TABLE t1 ALGORITHM=INPLACE, DROP INDEX fld1,
ADD FULLTEXT INDEX fld1(fld1);
DROP TABLE t1;

--echo #
--echo # Bug#20146455: FIND_KEY_CI RETURNS NULL, CAUSES CRASH IN
--echo #               FILL_ALTER_INPLACE_INFO
--echo #

CREATE TABLE t1 (a INT PRIMARY KEY, b INT,
                 FOREIGN KEY (b) REFERENCES t1(a)) ENGINE= MyISAM;
ALTER TABLE t1 RENAME INDEX b TO w, ADD FOREIGN KEY (b) REFERENCES t1(a);
SHOW CREATE TABLE t1;
DROP TABLE t1;

--echo # WL#10761 : ALTER TABLE RENAME COLUMN
--echo #
CREATE TABLE t2(a INT, b VARCHAR(30), c FLOAT) ENGINE=MyIsam;
SHOW CREATE TABLE t2;
INSERT INTO t2 VALUES(1,'abcd',1.234);

# Rename multiple columns with MyIsam Engine
ALTER TABLE t2 RENAME COLUMN a TO d, RENAME COLUMN b TO e, RENAME COLUMN c to f;
SHOW CREATE TABLE t2;
SELECT * FROM t2;

# View, Trigger and SP
CREATE VIEW v1 AS SELECT d,e,f FROM t2;
CREATE TRIGGER trg1 BEFORE UPDATE on t2 FOR EACH ROW SET NEW.d=OLD.d + 10;
CREATE PROCEDURE sp1() INSERT INTO t2(d) VALUES(10);
ALTER TABLE t2 RENAME COLUMN d TO g;
SHOW CREATE TABLE t2;
SHOW CREATE VIEW v1;
--error ER_VIEW_INVALID
SELECT * FROM v1;
--error ER_BAD_FIELD_ERROR
UPDATE t2 SET f = f + 10;
--error ER_BAD_FIELD_ERROR
CALL sp1();
DROP TRIGGER trg1;
DROP PROCEDURE sp1;
DROP TABLE t2;
DROP VIEW v1;

--echo #
--echo # Basic test coverage for ALGORITHM=INSTANT support on SQL-layer.
--echo #

--echo #
--echo # 1) For MyISAM tables we support INSTANT algorithm for metadata-only
--echo #    changes as well.
--echo #
CREATE TABLE t1 (i INT, j ENUM('a', 'b'), KEY(i)) ENGINE=MyISAM;
ALTER TABLE t1 ALTER COLUMN i SET DEFAULT 10, ALGORITHM=INSTANT;
SHOW CREATE TABLE t1;
ALTER TABLE t1 ALTER COLUMN i DROP DEFAULT, ALGORITHM=INSTANT;
SHOW CREATE TABLE t1;
ALTER TABLE t1 MODIFY COLUMN j ENUM('a', 'b', 'c', 'd', 'e'), ALGORITHM=INSTANT;
SHOW CREATE TABLE t1;
ALTER TABLE t1 CHANGE COLUMN i k INT, ALGORITHM=INSTANT;
SHOW CREATE TABLE t1;
ALTER TABLE t1 RENAME INDEX i TO k, ALGORITHM=INSTANT;
SHOW CREATE TABLE t1;
ALTER TABLE t1 RENAME TO t2, ALGORITHM=INSTANT;
SHOW CREATE TABLE t2;

--echo #
--echo # 2) And you can still use ALGORITHM=INPLACE for the same operations
--echo #    for MyISAM tables too.
--echo #
ALTER TABLE t2 RENAME TO t1, ALGORITHM=INPLACE;
SHOW CREATE TABLE t1;
ALTER TABLE t1 ALTER COLUMN k SET DEFAULT 11, ALGORITHM=INPLACE;
SHOW CREATE TABLE t1;
ALTER TABLE t1 ALTER COLUMN k DROP DEFAULT, ALGORITHM=INPLACE;
SHOW CREATE TABLE t1;
ALTER TABLE t1 MODIFY COLUMN j ENUM('a', 'b', 'c', 'd', 'e', 'f', 'g'), ALGORITHM=INPLACE;
SHOW CREATE TABLE t1;
ALTER TABLE t1 CHANGE COLUMN k i INT, ALGORITHM=INPLACE;
SHOW CREATE TABLE t1;
ALTER TABLE t1 RENAME INDEX k TO i, ALGORITHM=INPLACE;
SHOW CREATE TABLE t1;

--echo #
--echo # 3) Indeed, some options are not supported as INSTANT
--echo #
--error ER_ALTER_OPERATION_NOT_SUPPORTED
ALTER TABLE t1 ADD COLUMN l INT, ALGORITHM=INSTANT;
--error ER_ALTER_OPERATION_NOT_SUPPORTED
ALTER TABLE t1 ALTER COLUMN i SET DEFAULT 12, DROP COLUMN j, ALGORITHM=INSTANT;
DROP TABLE t1;

--echo # Tests added for coverage.
CREATE TABLE t1(fld1 VARCHAR(3), KEY(fld1)) ENGINE=MYISAM;

--echo # Conversion of unpacked keys to packed keys reports
--echo # error for INPLACE Alter.
--error ER_ALTER_OPERATION_NOT_SUPPORTED
ALTER TABLE t1 MODIFY fld1 VARCHAR(10), ALGORITHM=INPLACE;

--echo # Succeeds with index rebuild.
ALTER TABLE t1 MODIFY fld1 VARCHAR(10), ALGORITHM=COPY;
DROP TABLE t1;


--echo # Tests added for coverage.
CREATE TABLE t1(fld1 VARCHAR(3), KEY(fld1)) ENGINE=MYISAM;

--echo # Conversion of unpacked keys to packed keys reports
--echo # error for INPLACE Alter.
--error ER_ALTER_OPERATION_NOT_SUPPORTED
ALTER TABLE t1 MODIFY fld1 VARCHAR(10), ALGORITHM=INPLACE;

--echo # Succeeds with index rebuild.
ALTER TABLE t1 MODIFY fld1 VARCHAR(10), ALGORITHM=COPY;

--echo # Cleanup.
DROP TABLE t1;



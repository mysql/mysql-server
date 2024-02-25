#
# MySQL Bug#15276: MySQL ignores collation-server
#
show variables like 'collation_server';

#
# Bug#18004 Connecting crashes server when default charset is UCS2
#
show variables like "%character_set_ser%";
--disable_warnings
DROP TABLE IF EXISTS t1;
--enable_warnings
create table t1 (a int);
drop table t1;

--echo End of 4.1 tests

#
# Bug #28925 GROUP_CONCAT inserts wrong separators for a ucs2 column
# Check that GROUP_CONCAT works fine with --default-character-set=ucs2
#
create table t1 (a char(1) character set latin1);
insert into t1 values ('a'),('b'),('c');
select hex(group_concat(a)) from t1;
drop table t1;
#
# Bug #27643: query failed : 1114 (The table '' is full)
#
# Check that HASH indexes ignore trailing spaces when comparing 
# strings with the ucs2_bin collation

CREATE TABLE t1(col1 VARCHAR(32) CHARACTER SET ucs2 COLLATE ucs2_bin NOT NULL, 
                col2 VARCHAR(32) CHARACTER SET ucs2 COLLATE ucs2_bin NOT NULL, 
                UNIQUE KEY key1 USING HASH (col1, col2)) ENGINE=MEMORY;
INSERT INTO t1 VALUES('A', 'A'), ('B', 'B'), ('C', 'C');
--error ER_DUP_ENTRY
INSERT INTO t1 VALUES('A ', 'A ');
DROP TABLE t1;

--echo End of 5.0 tests

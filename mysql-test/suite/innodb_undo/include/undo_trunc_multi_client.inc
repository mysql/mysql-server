#
# WL#6965: Truncate UNDO logs.
#

--source include/not_valgrind.inc
--source include/big_test.inc

#-----------------------------------------------------------------------------
#
# Perform DML action using multiple clients and multiple undo tablespace.
#
#
use test;
create table t1
        (keyc int, c1 char(100), c2 char(100),
         primary key(keyc)) engine = innodb;
create table t2
        (keyc int, c1 char(100), c2 char(100),
         primary key(keyc)) engine = innodb;
#
delimiter |;
CREATE PROCEDURE populate_t1()
BEGIN
        DECLARE i INT DEFAULT 1;
        while (i <= 40000) DO
                insert into t1 values (i, 'a', 'b');
                SET i = i + 1;
        END WHILE;
END |
delimiter ;|
#
delimiter |;
CREATE PROCEDURE populate_t2()
BEGIN
        DECLARE i INT DEFAULT 1;
        while (i <= 40000) DO
                insert into t2 values (i, 'a', 'b');
                SET i = i + 1;
        END WHILE;
END |
delimiter ;|
#
#
connect (con1,localhost,root,,);
connect (con2,localhost,root,,);
#
#
connection con1;
begin;
call populate_t1();
delete from t1 where keyc < 20000;
update t1 set c1 = 'mysql' where  keyc > 20000;
update t1 set c2 = 'oracle' where  keyc > 20000;
#
connection con2;
begin;
call populate_t2();
delete from t2 where keyc < 20000;
update t2 set c1 = 'mysql' where  keyc > 20000;
update t2 set c2 = 'oracle' where  keyc > 20000;
#
connection con1;
commit;
connection con2;
commit;
#
connection default;
drop table t1, t2;
drop PROCEDURE populate_t1;
drop PROCEDURE populate_t2;
#
SET GLOBAL innodb_fast_shutdown=0;
--let $shutdown_server_timeout= 180
--source include/restart_mysqld.inc

let SEARCH_FILE = $MYSQLTEST_VARDIR/log/mysqld.1.err;
let SEARCH_PATTERN = Truncating UNDO tablespace;
--source include/search_pattern.inc

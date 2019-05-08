#
# WL#6965: Truncate UNDO logs during recovery.
#
SET GLOBAL innodb_fast_shutdown = 0;
# restart
SET GLOBAL innodb_undo_log_truncate = 1;
SET GLOBAL innodb_purge_rseg_truncate_frequency = 1;
SET GLOBAL innodb_rollback_segments = 3;
SET GLOBAL innodb_max_undo_log_size = 11534336;
SET GLOBAL log_error_verbosity = 3;
SET GLOBAL innodb_fast_shutdown = 0;
#
# Create procedure to populate the test-bed for Undo Truncate Recover tests
#
CREATE PROCEDURE populate_t1()
BEGIN
DECLARE i INT DEFAULT 1;
while (i <= 40000) DO
insert into t1 values (i, 'a', 'b');
SET i = i + 1;
END WHILE;
END |
#
# 2. Crash after checkpoint, before ddl log.
#
create table t1
(keyc int, c1 char(100), c2 char(100),
primary key(keyc)) engine = innodb;
begin;
call populate_t1();
delete from t1 where keyc < 20000;
update t1 set c1 = 'mysql' where  keyc > 20000;
update t1 set c2 = 'oracle' where  keyc > 20000;
set global debug = "+d,ib_undo_trunc_before_ddl_log_start";
commit;
Pattern "ib_undo_trunc_before_ddl_log_start" found
# restart
drop table t1;
#
# Cleanup
#
drop PROCEDURE populate_t1;

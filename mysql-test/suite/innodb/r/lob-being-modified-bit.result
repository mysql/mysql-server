create table t1 (f1 longblob) engine=innodb;
show create table t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `f1` longblob
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
insert into t1 values (repeat('x', 1024*16*4));
select count(*) from t1;
count(*)
1
select left(f1, 40) from t1;
left(f1, 40)
xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx
select right(f1, 40) from t1;
right(f1, 40)
xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx
select length(f1) from t1;
length(f1)
65536
# Connection con1:
start transaction;
set debug_sync = 'blob_write_middle SIGNAL go_flush WAIT_FOR go_insert';
insert into t1 values (repeat('y', 1024*16*4));;
# Connection default:
set debug_sync = 'now WAIT_FOR go_flush';
SET GLOBAL innodb_log_checkpoint_now = ON;
SET GLOBAL innodb_page_cleaner_disabled_debug = 1;
SET GLOBAL innodb_dict_stats_disabled_debug = 1;
SET GLOBAL innodb_master_thread_disabled_debug = 1;
SET GLOBAL innodb_checkpoint_disabled = 1;
set debug_sync = 'now SIGNAL go_insert';
# Connection con1:
commit;
# Connection default:
# Kill and restart
select count(*) from t1;
count(*)
2
select left(f1, 40) from t1;
left(f1, 40)
xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx
yyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyy
select right(f1, 40) from t1;
right(f1, 40)
xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx
yyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyy
select length(f1) from t1;
length(f1)
65536
65536
drop table t1;

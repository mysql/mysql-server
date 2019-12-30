#
# Bug #29440408 ASSERTION FAILURE: BUF0BUF.CC:4040:M_MODE ==
# PAGE_FETCH::POSSIBLY_FREED || !BLOC
#
create table t1 (f1 int primary key, f2 longblob) engine=innodb row_format=compressed;
insert into t1 values (1, repeat('x', 64*1024));
delete from t1 where f1 = 1;
# Connection con1:
SET DEBUG_SYNC = 'blob_write_middle SIGNAL do_purge WAIT_FOR continue_insert';
insert into t1 values (1, repeat('y', 128*1024));
# Connection default:
set DEBUG_SYNC = 'now WAIT_FOR do_purge';
# Kill and restart
drop table t1;

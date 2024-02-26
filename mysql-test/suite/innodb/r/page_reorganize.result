#
# Bug# 20005279 ASSERT !OTHER_LOCK, LOCK_MOVE_REORGANIZE_PAGE()
#
create table t1 (f1 int auto_increment primary key,
f2 char(255)) engine=innodb;
start transaction;
commit;
start transaction;
select f1, f2 from t1 where f1 = 20 for update;
f1	f2
20	~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
# Connection con1:
select f1 from t1 where f1 = 20 for update;
# Connection default:
SET DEBUG='+d,do_page_reorganize,do_lock_reverse_page_reorganize';
insert into t1(f2) values (repeat('+', 100));
SET DEBUG='-d,do_page_reorganize,do_lock_reverse_page_reorganize';
commit;
# Connection con1:
f1
20
# Connection default:
drop table t1;

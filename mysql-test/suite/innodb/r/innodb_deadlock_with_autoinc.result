#
# Bug #21983865  UNEXPECTED DEADLOCK WITH INNODB_AUTOINC_LOCK_MODE=0
#
create table t1(f1 int not null auto_increment primary key) engine=innodb;
# Hold autoinc_lock on table t1 from connection con1
set debug_sync='ib_after_row_insert SIGNAL others WAIT_FOR continue_others';
insert into t1 values(NULL);
# Create 40 connections and make it to wait for autoinc_lock on table t1.
# Release the auto_inc lock on table t1 for connection con1
set debug_sync='now SIGNAL continue_others';
# Now all 40 connections can finish the insert operation on t1.
drop table t1;

SET GLOBAL innodb_file_per_table=0;
create table bug56947(a int not null) engine = innodb;
SET DEBUG='+d,ib_rebuild_cannot_rename';
alter table bug56947 add unique index (a);
ERROR HY000: Got error 11 - 'InnoDB error' from storage engine
SET DEBUG='-d,ib_rebuild_cannot_rename';
check table bug56947;
Table	Op	Msg_type	Msg_text
test.bug56947	check	status	OK
drop table bug56947;
SET @@global.innodb_file_per_table=DEFAULT;

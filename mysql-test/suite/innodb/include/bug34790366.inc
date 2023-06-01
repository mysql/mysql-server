--echo # check src files are present
--file_exists $MYSQLD_DATADIR/test/src.cfg
--file_exists $MYSQLD_DATADIR/test/src.ibd

--echo # prep for test
--eval create table t3 (c1 int) charset=$charset;
--eval create table t4 (c1 int, c2 set('ab', 'cd'), c3 enum('pq', 'rs')) charset=$charset;
--eval create table t5 (c1 int, c2 set('ab', 'cd', 'ef'), c3 enum('pq', 'rs', 'tu')) charset=$charset;
--eval create table t6 (c1 int, c2 set('ab'), c3 enum('pq')) charset=$charset;
--eval create table t7 (c1 int, c2 set('ab', '22'), c3 enum('pq', 'tu')) charset=$charset;
--eval create table t8 (c1 int, c2 set('ab', 'cd'), c3 enum('rs', 'tu')) charset=$charset;

alter table t4 drop column c2, drop column c3, algorithm=instant;
alter table t5 drop column c2, drop column c3, algorithm=instant;
alter table t6 drop column c2, drop column c3, algorithm=instant;
alter table t7 drop column c2, drop column c3, algorithm=instant;
alter table t8 drop column c2, drop column c3, algorithm=instant;

alter table t3 discard tablespace;
alter table t4 discard tablespace;
alter table t5 discard tablespace;
alter table t6 discard tablespace;
alter table t7 discard tablespace;
alter table t8 discard tablespace;

--echo # prep cfg and ibd
--copy_file $MYSQLD_DATADIR/test/src.cfg $MYSQLD_DATADIR/test/t3.cfg
--copy_file $MYSQLD_DATADIR/test/src.ibd $MYSQLD_DATADIR/test/t3.ibd

--copy_file $MYSQLD_DATADIR/test/src.cfg $MYSQLD_DATADIR/test/t4.cfg
--copy_file $MYSQLD_DATADIR/test/src.ibd $MYSQLD_DATADIR/test/t4.ibd

--copy_file $MYSQLD_DATADIR/test/src.cfg $MYSQLD_DATADIR/test/t5.cfg
--copy_file $MYSQLD_DATADIR/test/src.ibd $MYSQLD_DATADIR/test/t5.ibd

--copy_file $MYSQLD_DATADIR/test/src.cfg $MYSQLD_DATADIR/test/t6.cfg
--copy_file $MYSQLD_DATADIR/test/src.ibd $MYSQLD_DATADIR/test/t6.ibd

--copy_file $MYSQLD_DATADIR/test/src.cfg $MYSQLD_DATADIR/test/t7.cfg
--copy_file $MYSQLD_DATADIR/test/src.ibd $MYSQLD_DATADIR/test/t7.ibd

--copy_file $MYSQLD_DATADIR/test/src.cfg $MYSQLD_DATADIR/test/t8.cfg
--copy_file $MYSQLD_DATADIR/test/src.ibd $MYSQLD_DATADIR/test/t8.ibd

--echo # do test
alter table t3 import tablespace;
alter table t4 import tablespace;
--error ER_INNODB_IMPORT_DROP_COL_METADATA_MISMATCH
alter table t5 import tablespace;
--error ER_INNODB_IMPORT_DROP_COL_METADATA_MISMATCH
alter table t6 import tablespace;
--error ER_INNODB_IMPORT_DROP_COL_METADATA_MISMATCH
alter table t7 import tablespace;
--error ER_INNODB_IMPORT_DROP_COL_METADATA_MISMATCH
alter table t8 import tablespace;

select * from t3;
check table t3;

select * from t4;
check table t4;

--echo # Failed import check table reports corrupt
check table t5;
check table t6;
check table t7;
check table t8;

--echo # cleanup
drop table t3;
drop table t4;
drop table t5;
drop table t6;
drop table t7;
drop table t8;

--echo # import failed, delete tablespace file:
--remove_file $MYSQLD_DATADIR/test/t5.ibd
--remove_file $MYSQLD_DATADIR/test/t5.cfg
--remove_file $MYSQLD_DATADIR/test/t6.ibd
--remove_file $MYSQLD_DATADIR/test/t6.cfg
--remove_file $MYSQLD_DATADIR/test/t7.ibd
--remove_file $MYSQLD_DATADIR/test/t7.cfg
--remove_file $MYSQLD_DATADIR/test/t8.ibd
--remove_file $MYSQLD_DATADIR/test/t8.cfg

--echo # files cleanup
--remove_file $MYSQLD_DATADIR/test/src.cfg
--remove_file $MYSQLD_DATADIR/test/src.ibd

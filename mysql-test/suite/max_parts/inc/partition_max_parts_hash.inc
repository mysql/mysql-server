# Need more as 16k open files per process (ulimit -n) on Unix systems.
--source include/have_partition_open_file_limit.inc
##### max rows to be inserted
let $maxrows=65535;

eval create table t2 (a int not null, primary key(a)) engine=$engine
partition by hash (a) partitions 8192;
show create table t2;
let $count= $maxrows;
--echo $count inserts;
--disable_query_log
while ($count)
{
eval insert into t2 values ($count);
dec $count;
}
--enable_query_log
select count(*) from t2;
select count(*) from t2 partition (p0);
select count(*) from t2 partition (p10);
select count(*) from t2 partition (p100);
select count(*) from t2 partition (p1000);
select count(*) from t2 partition (p4000);
select count(*) from t2 partition (p8000);
select count(*) from t2 partition (p8191);

select * from t2 partition (p0);
select * from t2 partition (p10);
select * from t2 partition (p100);
select * from t2 partition (p1000);
select * from t2 partition (p4000);
select * from t2 partition (p8000);
select * from t2 partition (p8191);

delete from t2 partition (p8191);
select * from t2 partition (p8191);
insert into t2 partition (p8191) values (8191), (65535);
select * from t2 partition (p8191);
update t2 partition (p8191) set a=16383 where a= 8191;
select * from t2 partition (p8191);

write_file $MYSQL_TMP_DIR/data01;
8191,
57343,
EOF
replace_result $MYSQL_TMP_DIR MYSQL_TMP_DIR;
eval load data infile '$MYSQL_TMP_DIR/data01' into table t2 partition (p8191) fields terminated by ',';
remove_file $MYSQL_TMP_DIR/data01;
select * from t2 partition (p8191);

drop table t2;


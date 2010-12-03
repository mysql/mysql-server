# Test if the engine does autocommit in LOAD DATA INFILE, or not
# (NDB wants to do, others don't).

eval SET SESSION DEFAULT_STORAGE_ENGINE = $engine_type;

--disable_warnings
drop table if exists t1;
--enable_warnings

let $load_file= $MYSQLTEST_VARDIR/std_data/loaddata2.dat;

# NDB does not support the create option 'Binlog of table with BLOB attribute and no PK'
# So use a dummy PK here.
create table t1 (id int unsigned not null auto_increment primary key, a text, b text);
start transaction;
--replace_result $load_file LOAD_FILE
eval load data infile '$load_file' into table t1 fields terminated by ',' enclosed by '''' (a, b);
commit;
select count(*) from t1;
truncate table t1;
start transaction;
--replace_result $load_file LOAD_FILE
eval load data infile '$load_file' into table t1 fields terminated by ',' enclosed by '''' (a, b);
rollback;
select count(*) from t1;

drop table t1;

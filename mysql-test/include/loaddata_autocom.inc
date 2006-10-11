# Test if the engine does autocommit in LOAD DATA INFILE, or not
# (NDB wants to do, others don't).

eval SET SESSION STORAGE_ENGINE = $engine_type;

--disable_warnings
drop table if exists t1;
--enable_warnings

create table t1 (a text, b text);
start transaction;
load data infile '../std_data_ln/loaddata2.dat' into table t1 fields terminated by ',' enclosed by '''';
commit;
select count(*) from t1;
truncate table t1;
start transaction;
load data infile '../std_data_ln/loaddata2.dat' into table t1 fields terminated by ',' enclosed by '''';
rollback;
select count(*) from t1;

drop table t1;

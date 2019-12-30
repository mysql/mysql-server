#
# load tables with some significant amount of data and then truncate it.
#

#-----------------------------------------------------------------------------
#
# create test-bed
#
let $per_table = `select @@innodb_file_per_table`;

let $WL6501_TMP_DIR = `select @@tmpdir`;
let $WL6501_DATA_DIR = `select @@datadir`;
set innodb_strict_mode=OFF;

#-----------------------------------------------------------------------------
#
# create procedure to load data
#
delimiter |;
create procedure populate()
begin
	declare i int default 1;
	while (i <= 5000) do
		insert into t1 values (i, 'a', 'b');
		insert into t2 values (i, 'a', 'b');
		insert into t3 values (i, 'a', 'b');
		set i = i + 1;
	end while;
end|
create procedure populate_small()
begin
	declare i int default 10001;
	while (i <= 12000) do
		insert into t1 values (i, 'c', 'd');
		insert into t2 values (i, 'a', 'b');
		insert into t3 values (i, 'a', 'b');
		set i = i + 1;
	end while;
end|
delimiter ;|

#-----------------------------------------------------------------------------
#
# create and load the tables.
#
eval set global innodb_file_per_table = $wl6501_file_per_table;
--replace_regex /[0-9]+/NUMBER/
eval create table t1
	(i int, c1 char(100), c2 char(100),
	 index c1_idx(c1))
	engine=innodb row_format=$wl6501_row_fmt
	key_block_size=$wl6501_kbs;
eval create table t2
	(i int, c1 char(100), c2 char(100),
	 index c1_idx(c1))
	engine=innodb row_format=$wl6501_row_fmt
	key_block_size=$wl6501_kbs;
eval create temporary table t3
	(i int, c1 char(100), c2 char(100),
	 index c1_idx(c1))
	engine=innodb row_format=$wl6501_row_fmt
	key_block_size=$wl6501_kbs;
#
select count(*) from t1;
select count(*) from t2;
select count(*) from t3;
begin;
call populate();
commit;
select count(*) from t1;
select count(*) from t2;
select count(*) from t3;
#
truncate table t1;
select count(*) from t1;
select count(*) from t2;
select count(*) from t3;
#
call populate_small();
select count(*) from t1;
select count(*) from t2;
select count(*) from t3;
#
truncate table t2;
--echo # WL#7811/WL#7743/WL#7141 TODO: Remove the warnings!
truncate table t3;
select count(*) from t1;
select count(*) from t2;
select count(*) from t3;
#
call populate_small();
select count(*) from t1;
select count(*) from t2;
select count(*) from t3;
#
drop table t1;
drop table t2;
drop table t3;

#-----------------------------------------------------------------------------
#
# drop the procedure
#
drop procedure populate;
drop procedure populate_small;

#-----------------------------------------------------------------------------
#
# remove test-bed
#
eval set global innodb_file_per_table = $per_table;

#
# WL#6737: Integrating InnoDB temporary tables with MySQL Optimizer.
#

SET @optimizer_switch_saved= @@optimizer_switch;
SET @@optimizer_switch="derived_merge=off";

# use sql_buffer_result
explain format = json select sql_buffer_result * from t1;
select sql_buffer_result * from t1;
explain format = json select sql_buffer_result * from t2;
select sql_buffer_result * from t2;

#
# If order by is not simple order
# If not (sort_order only uses fields from head table and the head table is not left join table)
explain format = json select * from t1 left join t2 on t1.b = t2.b order by t1.a;
select * from t1 left join t2 on t1.b = t2.b order by t1.a;

#
# If sorting by a sp or udf function due to their expensive and probably non-deterministic nature.
explain format = json select * from t1 order by func1(a);
select * from t1 order by func1(a);

--source include/turn_off_only_full_group_by.inc

# order by aggregate function
explain format = json select a from t1 group by b order by sum(a);
select a from t1 group by b order by sum(a);

# order by aggregate -- In order to make it simple, we make group by index
explain format = json select a from t7 group by b order by sum(a);
select a from t7 group by b order by sum(a);

#
# a select with join and sql_big_result(only can work with group by or distinct)
explain format = json select sql_big_result distinct * from t1, t2;
select sql_big_result distinct * from t1, t2;
explain format = json select sql_big_result t1.b from t1, t2 group by t1.b;
select sql_big_result t1.b from t1, t2 group by t1.b;
explain format = json select sql_big_result distinct t1.a from t1, t2 group by t1.b;
select sql_big_result distinct t1.a from t1, t2 group by t1.b;

#
# a select with join and sorting
explain format = json select t1.* from t1 inner join t2 on t1.a = t2.a order by t2.a;
select t1.* from t1 inner join t2 on t1.a = t2.a order by t2.a;
explain format = json select t1.* from t1, t2 where t1.a = t2.a order by t1.a;
select t1.* from t1, t2 where t1.a = t2.a order by t1.a;

#
# a select with join and grouping that uses tmp table for grouping
explain format = json select t1.* from t1 inner join t2 where t1.a = t2.a group by t1.a;
select t1.* from t1 inner join t2 where t1.a = t2.a group by t1.a;
explain format = json select t1.* from t1 inner join t2 where t1.a = t2.a group by t2.a;
select t1.* from t1 inner join t2 where t1.a = t2.a group by t2.a;

#
# there is an order by clause and a different group by clause
explain format = json select a from t1 group by a order by a;
select a from t1 group by a order by a;
explain format = json select b from t1 group by b order by a;
select b from t1 group by b order by a;

#
# select distinct that uses tmp table for duplicate elimination
explain format = json select distinct t1.a from t1, t2 where  t1.b = t2.b;
select distinct t1.a from t1, t2 where  t1.b = t2.b;

#
# remove_dup_with_hash_index
explain format = json select distinct a from t1 group by b,a;
select distinct a from t1 group by b,a;

#
# remove_dup_with_compare
set @old_sort_buffer_size = @@sort_buffer_size;
set @@sort_buffer_size = 32804;
explain format = json select distinct b from t3 group by a having count(*) > 1;
select distinct b from t3 group by a having count(*) > 1;
select distinct b from t3 group by a;
set @@sort_buffer_size = @old_sort_buffer_size;

#
# special case for distinct
explain format = json select distinct t4.a,e from t4, t1 order by t4.b;
select distinct t4.a,e from t4, t1 order by t4.b;

#
# union
explain format = json select * from t1 union all select * from t2;
select * from t1 union all select * from t2;

# union distinct with global order by filesort
explain format = json select * from t1 union select * from t2 order by b;
select * from t1 union select * from t2 order by b;

#
# select with a single derived table
explain format = json select tt.* from (select * from t1) as tt;
select tt.* from (select * from t1) as tt;
# select with a derived table that builds index over the derived table
explain format = json select b from (select b from t5 where b > 5) tt;
select b from (select b from t5 where b > 5) tt;
# select with mutiple derived tables
explain format = json select * from (select b from t1) as t1, (select b from t2) as t2;
select * from (select b from t1) as t1, (select b from t2) as t2;
# derived table uses 'ref' scan
explain format = json select * from t1, (select * from t6) as t where t.a =5;
select * from t1, (select * from t6) as t where t.a =5;

#
# materialized semi-join
explain format = json select * from t1 where t1.a in (select a from t2) ;
select * from t1 where t1.a in (select a from t2) ;
# tmp table for semi-join weedout
if (`select locate('materialization', @@optimizer_switch) > 0`)
{
	set optimizer_switch='materialization=off';
}
if (`select locate('firstmatch', @@optimizer_switch) > 0`)
{
	set optimizer_switch='firstmatch=off';
}
if (`select locate('loosescan', @@optimizer_switch) > 0`)
{
	set optimizer_switch='loosescan=off';
}
explain format = json select * from t1 where t1.a in (select a from t2) ;
select * from t1 where t1.a in (select a from t2) ;
set optimizer_switch='default,derived_merge=off';

#
# muti-update
if (!`select @@innodb_read_only`)
{
	explain format = json update t1, t2 set t1.a = 9, t2.a = 10 where t1.b = 'a';
	update t1, t2 set t1.a = 9, t2.a = 10 where t1.b = 'a';
}

#
# count with distinct
explain format = json select count(distinct a) from t1;
select count(distinct a) from t1;

#
# group_concat
explain format = json select b, group_concat(a) from t1;
select b, group_concat(a) from t1;

#
# quick group
select * from t1;
explain format = json select b, count(a), sum(a), avg(a), std(a),
	min(a), max(a), bit_and(a), bit_or(a) from t1 group by b;
select b, count(a), sum(a), avg(a), std(a), min(a),
	max(a), bit_and(a), bit_or(a) from t1 group by b;

#
# choose innodb to create tmp table
--disable_warnings
select sql_big_result * from information_schema.global_variables where variable_name like 'internal_tmp_disk_storage_engine';
--enable_warnings

#
# union distinct and all
select 1 union distinct select 1 union all select 1;

#
# enable and disable indexes
select alias1.`col_int_key` AS field1 from (t8 as alias1) where (5) in
(select 7 union select 1) order by alias1.`col_varchar_key` asc;

#
# Exercises select from multiple non-temporary tables that result in
# insert .... select kind of semantics on intrinsic temporary table.
select count(table2.col_date_key) as field1 from
	(t10 as table1 inner join
 	((t10 as table2 right join
   	t9 as table3 on
   	((table3.pk = table2.pk) or
    	(table3.pk = table2.col_int_key))))
 		on ((table3.pk < table2.col_int_nokey)))
	order by field1 ASC;

#
# trying to create index on blob column
select distinct table1.col_varchar_1024_utf8_key as field1
	from t11 as table1 left join t11 as table2 on
	table1.col_varchar_1024_utf8 = table2.col_varchar_1024_latin1_key
	order by field1;

#
# nulls_equal use-case
select t from t12 group by c;

#
# complex queries resulting in multiple intrinsic temporary tables
select count(outr.col_varchar_nokey) AS x
from t13 as outr2 left join
	(select * from t13) as outr
	on (outr2.col_time_key > outr.col_date_key)
	where outr.pk in
	(select innr.col_int_nokey AS y from
	 (select * from t14) as innr2 left join
	 (select * from t14) AS innr
	 ON (innr2.col_int_nokey = innr.col_int_key)
	 where innr.col_varchar_nokey > innr.col_varchar_key
	 or not outr.pk > 9)
	or outr.col_varchar_key is null;

--source include/restore_sql_mode_after_turn_off_only_full_group_by.inc

SET @@optimizer_switch=@optimizer_switch_saved;


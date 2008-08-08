################################################################################
# t/partition_supported_sql_funcs.inc                                          #
#                                                                              #
# Purpose:                                                                     #
#  Tests frame for allowed sql functions                                       #
#                                                                              #
#                                                                              #
#------------------------------------------------------------------------------#
# Original Author: HH                                                          #
# Original Date: 2006-11-22                                                    #
# Change Author: MattiasJ                                                      #
# Change Date: 2008-05-15                                                      #
# Change: Added $max_8_partitions since ndb only capable of 8 partitions       #
#         and $no_reorg_partition since ndb does not support that              #
################################################################################
--echo -------------------------------------------------------------------------
--echo ---  $sqlfunc  in partition with coltype  $coltype
--echo -------------------------------------------------------------------------
--disable_abort_on_error
--disable_warnings
drop table if exists t1 ;
drop table if exists t2 ;
drop table if exists t3 ;
drop table if exists t4 ;
drop table if exists t5 ;
drop table if exists t6 ;
--enable_warnings
--enable_abort_on_error

let $part_t1= partition by range($sqlfunc) 
(partition p0 values less than (15),
 partition p1 values less than maxvalue);

let $part_t2= partition by list($sqlfunc) 
(partition p0 values in (0,1,2,3,4,5,6,7,8,9,10),
 partition p1 values in (11,12,13,14,15,16,17,18,19,20),
 partition p2 values in (21,22,23,24,25,26,27,28,29,30),
 partition p3 values in (31,32,33,34,35,36,37,38,39,40),
 partition p4 values in (41,42,43,44,45,46,47,48,49,50),
 partition p5 values in (51,52,53,54,55,56,57,58,59,60)
);

let $part_t3= partition by hash($sqlfunc);

let $part_t4= partition by range(colint) 
subpartition by hash($sqlfunc) subpartitions 2 
(partition p0 values less than (15),
 partition p1 values less than maxvalue);

let $part_t5= partition by list(colint)
subpartition by hash($sqlfunc) subpartitions 2 
(partition p0 values in (1,2,3,4,5,6,7,8,9,10),
 partition p1 values in (11,12,13,14,15,16,17,18,19,20),
 partition p2 values in (21,22,23,24,25,26,27,28,29,30),
 partition p3 values in (31,32,33,34,35,36,37,38,39,40),
 partition p4 values in (41,42,43,44,45,46,47,48,49,50),
 partition p5 values in (51,52,53,54,55,56,57,58,59,60)
);
if ($max_8_partitions)
{
let $part_t5= partition by list(colint) 
subpartition by hash($sqlfunc) subpartitions 2 
(partition p0 values in (0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15),
 partition p1 values in (16,17,18,19,20,21,22,23,24,25,26,27,28,29,30),
 partition p2 values in (31,32,33,34,35,36,37,38,39,40,41,42,43,44,45),
 partition p3 values in (46,47,48,49,50,51,52,53,54,55,56,57,58,59,60)
);
}

let $part_t6= partition by range(colint) 
(partition p0 values less than ($valsqlfunc),
 partition p1 values less than maxvalue);

let $part_t55_altered= partition by list(colint)
subpartition by hash($sqlfunc) subpartitions 5 
(partition p0 values in (1,2,3,4,5,6,7,8,9,10),
 partition p1 values in (11,12,13,14,15,16,17,18,19,20),
 partition p2 values in (21,22,23,24,25,26,27,28,29,30),
 partition p3 values in (31,32,33,34,35,36,37,38,39,40),
 partition p4 values in (41,42,43,44,45,46,47,48,49,50),
 partition p5 values in (51,52,53,54,55,56,57,58,59,60)
);
if ($max_8_partitions)
{
let $part_t55_altered= partition by list(colint) 
subpartition by hash($sqlfunc) subpartitions 4 
(partition p0 values in (0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30),
 partition p1 values in (31,32,33,34,35,36,37,38,39,40,41,42,43,44,45,46,47,48,49,50,51,52,53,54,55,56,57,58,59,60)
);
}
--echo -------------------------------------------------------------------------
--echo ---  Create tables with $sqlfunc
--echo -------------------------------------------------------------------------
eval create table t1 (col1 $coltype) engine=$engine 
$part_t1;

eval create table t2 (col1 $coltype) engine=$engine 
$part_t2;

eval create table t3 (col1 $coltype) engine=$engine 
$part_t3;

eval create table t4 (colint int, col1 $coltype) engine=$engine 
$part_t4;

eval create table t5 (colint int, col1 $coltype) engine=$engine 
$part_t5;

eval create table t6 (colint int, col1 $coltype) engine=$engine 
$part_t6;

--echo -------------------------------------------------------------------------
--echo ---  Access tables with $sqlfunc 
--echo -------------------------------------------------------------------------

eval insert into t1 values ($val1);
eval insert into t1 values ($val2);

eval insert into t2 values ($val1);
eval insert into t2 values ($val2);
eval insert into t2 values ($val3);

eval insert into t3 values ($val1);
eval insert into t3 values ($val2);
eval insert into t3 values ($val3);

--replace_result $MYSQLTEST_VARDIR MYSQLTEST_VARDIR
eval load data infile '$MYSQLTEST_VARDIR/std_data/parts/$infile' into table t4;
--replace_result $MYSQLTEST_VARDIR MYSQLTEST_VARDIR
eval load data infile '$MYSQLTEST_VARDIR/std_data/parts/$infile' into table t5;
--replace_result $MYSQLTEST_VARDIR MYSQLTEST_VARDIR
eval load data infile '$MYSQLTEST_VARDIR/std_data/parts/$infile' into table t6;

eval select $sqlfunc from t1 order by col1;

select * from t1 order by col1;
select * from t2 order by col1;
select * from t3 order by col1;
select * from t4 order by colint;
select * from t5 order by colint;
select * from t6 order by colint;

if ($do_long_tests)
{
	eval update t1 set col1=$val4 where col1=$val1;
	eval update t2 set col1=$val4 where col1=$val1;
	eval update t3 set col1=$val4 where col1=$val1;
	eval update t4 set col1=$val4 where col1=$val1;
	eval update t5 set col1=$val4 where col1=$val1;
	eval update t6 set col1=$val4 where col1=$val1;

	select * from t1 order by col1;
	select * from t2 order by col1;
	select * from t3 order by col1;
	select * from t4 order by colint;
	select * from t5 order by colint;
	select * from t6 order by colint;
}

--echo -------------------------------------------------------------------------
--echo ---  Alter tables with $sqlfunc
--echo -------------------------------------------------------------------------

--disable_abort_on_error
--disable_warnings
drop table if exists t11 ;
drop table if exists t22 ;
drop table if exists t33 ;
drop table if exists t44 ;
drop table if exists t55 ;
drop table if exists t66 ;
--enable_warnings
--enable_abort_on_error

eval create table t11 engine=$engine as select * from t1;
eval create table t22 engine=$engine as select * from t2;
eval create table t33 engine=$engine as select * from t3;
eval create table t44 engine=$engine as select * from t4;
eval create table t55 engine=$engine as select * from t5;
eval create table t66 engine=$engine as select * from t6;
eval alter table t11
$part_t1;
eval alter table t22
$part_t2;
eval alter table t33
$part_t3;
eval alter table t44
$part_t4;
eval alter table t55
$part_t5;
eval alter table t66
$part_t6;

select * from t11 order by col1;
select * from t22 order by col1;
select * from t33 order by col1;
select * from t44 order by colint;
select * from t55 order by colint;
select * from t66 order by colint;
 
if ($do_long_tests)
{
        --echo ---------------------------
        --echo ---- some alter table begin
        --echo ---------------------------
        if (!$no_reorg_partition)
        {
        eval alter table t11
        reorganize partition p0,p1 into
        (partition s1 values less than maxvalue);
        select * from t11 order by col1;

        eval alter table t11
        reorganize partition s1 into
        (partition p0 values less than (15),
         partition p1 values less than maxvalue);
        select * from t11 order by col1;
        }

eval alter table t55
$part_t55_altered;
        show create table t55; 
	select * from t55 order by colint;

        if (!$no_reorg_partition)
        {
        eval alter table t66
        reorganize partition p0,p1 into
        (partition s1 values less than maxvalue);
        select * from t66 order by colint;

        eval alter table t66
        reorganize partition s1 into
        (partition p0 values less than ($valsqlfunc),
         partition p1 values less than maxvalue);
        select * from t66 order by colint;
	
        eval alter table t66
        reorganize partition p0,p1 into
        (partition s1 values less than maxvalue);
        select * from t66 order by colint;

        eval alter table t66
        reorganize partition s1 into
        (partition p0 values less than ($valsqlfunc),
         partition p1 values less than maxvalue);
        select * from t66 order by colint;
        }

	let $t1=t1;
	let $t2=t2;
	let $t3=t3;
	let $t4=t4;
	let $t5=t5;
	let $t6=t6;
	--source suite/parts/inc/part_supported_sql_funcs_delete.inc

	let $t1=t11;
	let $t2=t22;
	let $t3=t33;
	let $t4=t44;
	let $t5=t55;
	let $t6=t66;
	--source suite/parts/inc/part_supported_sql_funcs_delete.inc
        --echo -------------------------
        --echo ---- some alter table end
        --echo -------------------------
}
--disable_warnings
drop table if exists t1 ;
drop table if exists t2 ;
drop table if exists t3 ;
drop table if exists t4 ;
drop table if exists t5 ;
drop table if exists t6 ;
drop table if exists t11 ;
drop table if exists t22 ;
drop table if exists t33 ;
drop table if exists t44 ;
drop table if exists t55 ;
drop table if exists t66 ;
--enable_warnings


################################################################################
# t/partition_blocked_sql_funcs.inc                                            #
#                                                                              #
# Purpose:                                                                     #
#  Tests around sql functions                                                  #
#                                                                              #
#                                                                              #
#------------------------------------------------------------------------------#
# Original Author: HH                                                          #
# Original Date: 2006-11-22                                                    #
# Change Author:                                                               #
# Change Date:                                                                 #
# Change:                                                                      #
################################################################################
--echo -------------------------------------------------------------------------
--echo ---  $sqlfunc  in partition with coltype  $coltype
--echo -------------------------------------------------------------------------
--echo must all fail!
--disable_warnings
drop table if exists t1 ;
drop table if exists t2 ;
drop table if exists t3 ;
drop table if exists t4 ;
drop table if exists t5 ;
drop table if exists t6 ;
--enable_warnings

--error ER_PARTITION_FUNCTION_IS_NOT_ALLOWED,ER_PARSE_ERROR
eval create table t1 (col1 $coltype) engine=$engine 
partition by range($sqlfunc) 
(partition p0 values less than (15),
 partition p1 values less than (31));

--error ER_PARTITION_FUNCTION_IS_NOT_ALLOWED,ER_PARSE_ERROR
eval create table t2 (col1 $coltype) engine=$engine 
partition by list($sqlfunc) 
(partition p0 values in (1,2,3,4,5,6,7,8,9,10),
 partition p1 values in (11,12,13,14,15,16,17,18,19,20),
 partition p2 values in (21,22,23,24,25,26,27,28,29,30));

--error ER_PARTITION_FUNCTION_IS_NOT_ALLOWED,ER_PARSE_ERROR
eval create table t3 (col1 $coltype) engine=$engine 
partition by hash($sqlfunc);

--error ER_PARTITION_FUNCTION_IS_NOT_ALLOWED,ER_PARSE_ERROR
eval create table t4 (colint int, col1 $coltype) engine=$engine 
partition by range(colint) 
subpartition by hash($sqlfunc) subpartitions 2 
(partition p0 values less than (15),
 partition p1 values less than (31));

--error ER_PARTITION_FUNCTION_IS_NOT_ALLOWED,ER_PARSE_ERROR
eval create table t5 (colint int, col1 $coltype) engine=$engine 
partition by list(colint)
subpartition by hash($sqlfunc) subpartitions 2 
(partition p0 values in (1,2,3,4,5,6,7,8,9,10),
 partition p1 values in (11,12,13,14,15,16,17,18,19,20),
 partition p2 values in (21,22,23,24,25,26,27,28,29,30));

--error ER_PARTITION_FUNCTION_IS_NOT_ALLOWED,ER_PARSE_ERROR
eval create table t6 (colint int, col1 $coltype) engine=$engine 
partition by range(colint) 
(partition p0 values less than ($valsqlfunc),
 partition p1 values less than maxvalue);

--disable_abort_on_error
--disable_warnings
drop table if exists t11 ;
drop table if exists t22 ;
drop table if exists t33 ;
drop table if exists t44 ;
drop table if exists t55 ;
drop table if exists t66 ;
--enable_warnings

eval create table t11 (col1 $coltype) engine=$engine ;

eval create table t22 (col1 $coltype) engine=$engine ;

eval create table t33 (col1 $coltype) engine=$engine ;

eval create table t44 (colint int, col1 $coltype) engine=$engine ;

eval create table t55 (colint int, col1 $coltype) engine=$engine ;

eval create table t66 (colint int, col1 $coltype) engine=$engine ;

--error ER_PARTITION_FUNCTION_IS_NOT_ALLOWED,ER_PARSE_ERROR
eval alter table t11  
partition by range($sqlfunc) 
(partition p0 values less than (15),
 partition p1 values less than (31));

--error ER_PARTITION_FUNCTION_IS_NOT_ALLOWED,ER_PARSE_ERROR
eval alter table t22 
partition by list($sqlfunc) 
(partition p0 values in (1,2,3,4,5,6,7,8,9,10),
 partition p1 values in (11,12,13,14,15,16,17,18,19,20),
 partition p2 values in (21,22,23,24,25,26,27,28,29,30));

--error ER_PARTITION_FUNCTION_IS_NOT_ALLOWED,ER_PARSE_ERROR
eval alter table t33 
partition by hash($sqlfunc);
--enable_abort_on_error

--error ER_PARTITION_FUNCTION_IS_NOT_ALLOWED,ER_PARSE_ERROR
eval alter table t44 
partition by range(colint) 
subpartition by hash($sqlfunc) subpartitions 2 
(partition p0 values less than (15),
 partition p1 values less than (31));

--error ER_PARTITION_FUNCTION_IS_NOT_ALLOWED,ER_PARSE_ERROR
eval alter table t55 
partition by list(colint)
subpartition by hash($sqlfunc) subpartitions 2 
(partition p0 values in (1,2,3,4,5,6,7,8,9,10),
 partition p1 values in (11,12,13,14,15,16,17,18,19,20),
 partition p2 values in (21,22,23,24,25,26,27,28,29,30));

--error ER_PARTITION_FUNCTION_IS_NOT_ALLOWED,ER_PARSE_ERROR
eval alter table t66  
partition by range(colint) 
(partition p0 values less than ($valsqlfunc),
 partition p1 values less than maxvalue);

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


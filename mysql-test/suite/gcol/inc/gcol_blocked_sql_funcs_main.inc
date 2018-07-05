################################################################################
# inc/gcol_blocked_sql_funcs_main.inc                                          #
#                                                                              #
# Purpose:                                                                     #
#  Tests around sql functions                                                  #
#                                                                              #
#                                                                              #
#------------------------------------------------------------------------------#
# Original Author: Andrey Zhakov                                               #
# Original Date: 2008-08-31                                                    #
# Change Author:                                                               #
# Change Date:                                                                 #
# Change:                                                                      #
################################################################################

#
# NOTE:  All SQL functions should be rejected, otherwise BUG.
#

--echo # RAND()
-- error ER_GENERATED_COLUMN_NAMED_FUNCTION_IS_NOT_ALLOWED
create table t1 (b double generated always as (rand()) virtual);

--echo # LOAD_FILE()
-- error ER_GENERATED_COLUMN_NAMED_FUNCTION_IS_NOT_ALLOWED
create table t1 (a varchar(64), b varchar(1024) generated always as (load_file(a)) virtual);

--echo # CURDATE()
-- error ER_GENERATED_COLUMN_NAMED_FUNCTION_IS_NOT_ALLOWED
create table t1 (a datetime generated always as (curdate()) virtual);

--echo # CURRENT_DATE(), CURRENT_DATE
-- error ER_GENERATED_COLUMN_NAMED_FUNCTION_IS_NOT_ALLOWED
create table t1 (a datetime generated always as (current_date) virtual);
-- error ER_GENERATED_COLUMN_NAMED_FUNCTION_IS_NOT_ALLOWED
create table t1 (a datetime generated always as (current_date()) virtual);

--echo # CURRENT_TIME(), CURRENT_TIME
-- error ER_GENERATED_COLUMN_NAMED_FUNCTION_IS_NOT_ALLOWED
create table t1 (a datetime generated always as (current_time) virtual);
-- error ER_GENERATED_COLUMN_NAMED_FUNCTION_IS_NOT_ALLOWED
create table t1 (a datetime generated always as (current_time()) virtual);

--echo # CURRENT_TIMESTAMP(), CURRENT_TIMESTAMP
-- error ER_GENERATED_COLUMN_NAMED_FUNCTION_IS_NOT_ALLOWED
create table t1 (a datetime generated always as (current_timestamp()) virtual);
-- error ER_GENERATED_COLUMN_NAMED_FUNCTION_IS_NOT_ALLOWED
create table t1 (a datetime generated always as (current_timestamp) virtual);

--echo # CURTIME()
-- error ER_GENERATED_COLUMN_NAMED_FUNCTION_IS_NOT_ALLOWED
create table t1 (a datetime generated always as (curtime()) virtual);

--echo # LOCALTIME(), LOCALTIME
-- error ER_GENERATED_COLUMN_NAMED_FUNCTION_IS_NOT_ALLOWED
create table t1 (a datetime, b varchar(10) generated always as (localtime()) virtual);
-- error ER_GENERATED_COLUMN_NAMED_FUNCTION_IS_NOT_ALLOWED
create table t1 (a datetime, b varchar(10) generated always as (localtime) virtual);

--echo # LOCALTIMESTAMP, LOCALTIMESTAMP()(v4.0.6)
-- error ER_GENERATED_COLUMN_NAMED_FUNCTION_IS_NOT_ALLOWED
create table t1 (a datetime, b varchar(10) generated always as (localtimestamp()) virtual);
-- error ER_GENERATED_COLUMN_NAMED_FUNCTION_IS_NOT_ALLOWED
create table t1 (a datetime, b varchar(10) generated always as (localtimestamp) virtual);

--echo # NOW()
-- error ER_GENERATED_COLUMN_NAMED_FUNCTION_IS_NOT_ALLOWED
create table t1 (a datetime, b varchar(10) generated always as (now()) virtual);

--echo # STATEMENT_DIGEST
-- error ER_GENERATED_COLUMN_NAMED_FUNCTION_IS_NOT_ALLOWED
create table t1 (a int, b int generated always as (statement_digest(a)) virtual);

--echo # STATEMENT_DIGEST_TEXT
-- error ER_GENERATED_COLUMN_NAMED_FUNCTION_IS_NOT_ALLOWED
create table t1 (a int, b int generated always as (statement_digest_text(a)) virtual);

--echo # SYSDATE()
-- error ER_GENERATED_COLUMN_NAMED_FUNCTION_IS_NOT_ALLOWED
create table t1 (a int, b varchar(10) generated always as (sysdate()) virtual);

--echo # UNIX_TIMESTAMP()
-- error ER_GENERATED_COLUMN_NAMED_FUNCTION_IS_NOT_ALLOWED
create table t1 (a datetime, b datetime generated always as (unix_timestamp()) virtual);

--echo # UTC_DATE()
-- error ER_GENERATED_COLUMN_NAMED_FUNCTION_IS_NOT_ALLOWED
create table t1 (a datetime, b datetime generated always as (utc_date()) virtual);

--echo # UTC_TIME()
-- error ER_GENERATED_COLUMN_NAMED_FUNCTION_IS_NOT_ALLOWED
create table t1 (a datetime, b datetime generated always as (utc_time()) virtual);

--echo # UTC_TIMESTAMP()
-- error ER_GENERATED_COLUMN_NAMED_FUNCTION_IS_NOT_ALLOWED
create table t1 (a datetime, b datetime generated always as (utc_timestamp()) virtual);

--echo # MATCH()
if (!$skip_full_text_check)
{
  -- error ER_GENERATED_COLUMN_NAMED_FUNCTION_IS_NOT_ALLOWED
  create table t1 (a varchar(32), b bool generated always as (match a against ('sample text')) virtual);
}

--echo # BENCHMARK()
-- error ER_GENERATED_COLUMN_NAMED_FUNCTION_IS_NOT_ALLOWED
create table t1 (a varchar(1024), b varchar(1024) generated always as (benchmark(a,3)) virtual);

--echo # CONNECTION_ID()
-- error ER_GENERATED_COLUMN_NAMED_FUNCTION_IS_NOT_ALLOWED
create table t1 (a int generated always as (connection_id()) virtual);

--echo # CURRENT_USER(), CURRENT_USER
-- error ER_GENERATED_COLUMN_NAMED_FUNCTION_IS_NOT_ALLOWED
create table t1 (a varchar(32) generated always as (current_user()) virtual);
-- error ER_GENERATED_COLUMN_NAMED_FUNCTION_IS_NOT_ALLOWED
create table t1 (a varchar(32) generated always as (current_user) virtual);

--echo # DATABASE()
-- error ER_GENERATED_COLUMN_NAMED_FUNCTION_IS_NOT_ALLOWED
create table t1 (a varchar(1024), b varchar(1024) generated always as (database()) virtual);

--echo # FOUND_ROWS()
-- error ER_GENERATED_COLUMN_NAMED_FUNCTION_IS_NOT_ALLOWED
create table t1 (a varchar(1024), b varchar(1024) generated always as (found_rows()) virtual);

--echo # GET_LOCK()
-- error ER_GENERATED_COLUMN_NAMED_FUNCTION_IS_NOT_ALLOWED
create table t1 (a varchar(1024), b varchar(1024) generated always as (get_lock(a,10)) virtual);

--echo # IS_FREE_LOCK()
-- error ER_GENERATED_COLUMN_NAMED_FUNCTION_IS_NOT_ALLOWED
create table t1 (a varchar(1024), b varchar(1024) generated always as (is_free_lock(a)) virtual);

--echo # IS_USED_LOCK()
-- error ER_GENERATED_COLUMN_NAMED_FUNCTION_IS_NOT_ALLOWED
create table t1 (a varchar(1024), b varchar(1024) generated always as (is_used_lock(a)) virtual);

--echo # LAST_INSERT_ID()
-- error ER_GENERATED_COLUMN_NAMED_FUNCTION_IS_NOT_ALLOWED
create table t1 (a int generated always as (last_insert_id()) virtual);

--echo # MASTER_POS_WAIT()
-- error ER_GENERATED_COLUMN_NAMED_FUNCTION_IS_NOT_ALLOWED
create table t1 (a varchar(32), b int generated always as (master_pos_wait(a,0,2)) virtual);

--echo # NAME_CONST()
-- error ER_GENERATED_COLUMN_FUNCTION_IS_NOT_ALLOWED
create table t1 (a varchar(32) generated always as (name_const('test',1)) virtual);

--echo # RELEASE_LOCK()
-- error ER_GENERATED_COLUMN_NAMED_FUNCTION_IS_NOT_ALLOWED
create table t1 (a varchar(32), b int generated always as (release_lock(a)) virtual);

--echo # ROW_COUNT()
-- error ER_GENERATED_COLUMN_NAMED_FUNCTION_IS_NOT_ALLOWED
create table t1 (a int generated always as (row_count()) virtual);

--echo # SCHEMA()
-- error ER_GENERATED_COLUMN_NAMED_FUNCTION_IS_NOT_ALLOWED
create table t1 (a varchar(32) generated always as (schema()) virtual);

--echo # SESSION_USER()
-- error ER_GENERATED_COLUMN_NAMED_FUNCTION_IS_NOT_ALLOWED
create table t1 (a varchar(32) generated always as (session_user()) virtual);

--echo # SLEEP()
-- error ER_GENERATED_COLUMN_NAMED_FUNCTION_IS_NOT_ALLOWED
create table t1 (a int, b int generated always as (sleep(a)) virtual);

--echo # SYSTEM_USER()
-- error ER_GENERATED_COLUMN_NAMED_FUNCTION_IS_NOT_ALLOWED
create table t1 (a varchar(32) generated always as (system_user()) virtual);

--echo # USER()
-- error ER_GENERATED_COLUMN_NAMED_FUNCTION_IS_NOT_ALLOWED
create table t1 (a varchar(1024), b varchar(1024) generated always as (user()) virtual);

--echo # UUID_SHORT()
-- error ER_GENERATED_COLUMN_NAMED_FUNCTION_IS_NOT_ALLOWED
create table t1 (a varchar(1024) generated always as (uuid_short()) virtual);

--echo # UUID()
-- error ER_GENERATED_COLUMN_NAMED_FUNCTION_IS_NOT_ALLOWED
create table t1 (a varchar(1024) generated always as (uuid()) virtual);

--echo # VALUES()
-- error ER_GENERATED_COLUMN_NAMED_FUNCTION_IS_NOT_ALLOWED
create table t1 (a varchar(1024), b varchar(1024) generated always as (values(a)) virtual);

--echo # VERSION()
-- error ER_GENERATED_COLUMN_NAMED_FUNCTION_IS_NOT_ALLOWED
create table t1 (a varchar(1024), b varchar(1024) generated always as (version()) virtual);

--echo # ENCRYPT()
-- error ER_GENERATED_COLUMN_NAMED_FUNCTION_IS_NOT_ALLOWED
create table t1 (a varchar(1024), b varchar(1024) generated always as (encrypt(a)) virtual);

--echo # Stored procedures

delimiter //;
create procedure p1()
begin
  select current_user();
end //

create function f1()
returns int
begin
 return 1;
end //

delimiter ;//

-- error ER_GENERATED_COLUMN_NAMED_FUNCTION_IS_NOT_ALLOWED
create table t1 (a int generated always as (p1()) virtual);
-- error ER_GENERATED_COLUMN_NAMED_FUNCTION_IS_NOT_ALLOWED
create table t1 (a int generated always as (f1()) virtual);

drop procedure p1;
drop function f1;

--echo # Unknown functions
-- error ER_GENERATED_COLUMN_NAMED_FUNCTION_IS_NOT_ALLOWED
create table t1 (a int generated always as (f1()) virtual);

--echo #
--echo # GROUP BY FUNCTIONS
--echo #

--echo # AVG()
-- error 1111
create table t1 (a int, b int generated always as (avg(a)) virtual);

--echo # BIT_AND()
-- error 1111
create table t1 (a int, b int generated always as (bit_and(a)) virtual);

--echo # BIT_OR()
-- error 1111
create table t1 (a int, b int generated always as (bit_or(a)) virtual);

--echo # BIT_XOR()
-- error 1111
create table t1 (a int, b int generated always as (bit_xor(a)) virtual);

--echo # COUNT(DISTINCT)
-- error 1111
create table t1 (a int, b int generated always as (count(distinct a)) virtual);

--echo # COUNT()
-- error 1111
create table t1 (a int, b int generated always as (count(a)) virtual);

--echo # GROUP_CONCAT()
-- error 1111
create table t1 (a varchar(32), b int generated always as (group_concat(a,'')) virtual);

--echo # MAX()
-- error 1111
create table t1 (a int, b int generated always as (max(a)) virtual);

--echo # MIN()
-- error 1111
create table t1 (a int, b int generated always as (min(a)) virtual);

--echo # STD()
-- error 1111
create table t1 (a int, b int generated always as (std(a)) virtual);

--echo # STDDEV_POP()
-- error 1111
create table t1 (a int, b int generated always as (stddev_pop(a)) virtual);

--echo # STDDEV_SAMP()
-- error 1111
create table t1 (a int, b int generated always as (stddev_samp(a)) virtual);

--echo # STDDEV()
-- error 1111
create table t1 (a int, b int generated always as (stddev(a)) virtual);

--echo # SUM()
-- error 1111
create table t1 (a int, b int generated always as (sum(a)) virtual);

--echo # VAR_POP()
-- error 1111
create table t1 (a int, b int generated always as (var_pop(a)) virtual);

--echo # VAR_SAMP()
-- error 1111
create table t1 (a int, b int generated always as (var_samp(a)) virtual);

--echo # VARIANCE()
-- error 1111
create table t1 (a int, b int generated always as (variance(a)) virtual);

--echo #
--echo # XML FUNCTIONS
--echo #

--echo # UpdateXML()
-- error ER_GENERATED_COLUMN_NAMED_FUNCTION_IS_NOT_ALLOWED
create table t1 (a varchar(1024), b varchar(1024) generated always as (UpdateXML(a,'/a','<e>fff</e>')) virtual);

--echo #
--echo # Sub-selects
--echo #

create table t1 (a int);
-- error 1064
create table t2 (a int, b int generated always as (select count(*) virtual from t1));
drop table t1;

--echo #
--echo # Long expression

let $tmp_long_string = `SELECT repeat('a',240)`;
eval create table t1 (a int, b varchar(300) generated always as (concat(a,'$tmp_long_string')) virtual);
drop table t1;
let $tmp_long_string = `SELECT repeat('a',243)`;
# Limit is lifted to 64K. TODO write a test for it.
# --error 1470
eval create table t1 (a int, b varchar(300) generated always as (concat(a,'$tmp_long_string')) virtual);
drop table t1;

--echo #
--echo # Constant expression
create table t1 (a int generated always as (PI()) virtual);
drop table t1;

if(! $testing_ndb) {
--echo # bug#21098119: GCOL WITH MATCH/AGAINST -->
--echo # ASSERTION FAILED: TR && TR->TABLE->FILE
--echo #
create table t1 (a int);
--error ER_GENERATED_COLUMN_NAMED_FUNCTION_IS_NOT_ALLOWED
alter table t1 add column r blob generated always
as (match(a) against ('' in boolean mode)) virtual;
drop table t1;
}

# Deprecated functions are not allowed in generation expressions
--echo #
--echo # Bug#26279884 : VIRTUAL COLUMNS ALLOW DEPRECATED FUNCTION TO BE USED
--echo #

--echo # JSON_MERGE()
--error ER_GENERATED_COLUMN_NAMED_FUNCTION_IS_NOT_ALLOWED
CREATE TABLE t2(a JSON, b JSON, c JSON  GENERATED ALWAYS AS  (JSON_MERGE(a,b)));

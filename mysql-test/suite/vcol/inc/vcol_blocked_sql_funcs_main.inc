################################################################################
# inc/vcol_blocked_sql_funcs_main.inc                                          #
#                                                                              #
# Purpose:                                                                     #
#  Tests around sql functions                                                  #
#                                                                              #
#                                                                              #
#------------------------------------------------------------------------------#
# Original Author: Andrey Zhakov                                               #
# Original Date: 2008-08-31                                                    #
# Change Author: Oleksandr Byelkin (Monty program Ab)
# Date: 2009-03-24 
# Change: Syntax changed
################################################################################

#
# NOTE:  All SQL functions should be rejected, otherwise BUG.
#

--echo # RAND()
-- error ER_VIRTUAL_COLUMN_FUNCTION_IS_NOT_ALLOWED
create table t1 (b double as (rand()));

--echo # LOAD_FILE()
-- error ER_VIRTUAL_COLUMN_FUNCTION_IS_NOT_ALLOWED
create table t1 (a varchar(64), b varchar(1024) as (load_file(a)));

--echo # CURDATE()
-- error ER_VIRTUAL_COLUMN_FUNCTION_IS_NOT_ALLOWED
create table t1 (a datetime as (curdate()));

--echo # CURRENT_DATE(), CURRENT_DATE
-- error ER_VIRTUAL_COLUMN_FUNCTION_IS_NOT_ALLOWED
create table t1 (a datetime as (current_date));
-- error ER_VIRTUAL_COLUMN_FUNCTION_IS_NOT_ALLOWED
create table t1 (a datetime as (current_date()));

--echo # CURRENT_TIME(), CURRENT_TIME
-- error ER_VIRTUAL_COLUMN_FUNCTION_IS_NOT_ALLOWED
create table t1 (a datetime as (current_time));
-- error ER_VIRTUAL_COLUMN_FUNCTION_IS_NOT_ALLOWED
create table t1 (a datetime as (current_time()));

--echo # CURRENT_TIMESTAMP(), CURRENT_TIMESTAMP
-- error ER_VIRTUAL_COLUMN_FUNCTION_IS_NOT_ALLOWED
create table t1 (a datetime as (current_timestamp()));
-- error ER_VIRTUAL_COLUMN_FUNCTION_IS_NOT_ALLOWED
create table t1 (a datetime as (current_timestamp));

--echo # CURTIME()
-- error ER_VIRTUAL_COLUMN_FUNCTION_IS_NOT_ALLOWED
create table t1 (a datetime as (curtime()));

--echo # LOCALTIME(), LOCALTIME
-- error ER_VIRTUAL_COLUMN_FUNCTION_IS_NOT_ALLOWED
create table t1 (a datetime, b varchar(10) as (localtime())); 
-- error ER_VIRTUAL_COLUMN_FUNCTION_IS_NOT_ALLOWED
create table t1 (a datetime, b varchar(10) as (localtime)); 

--echo # LOCALTIMESTAMP, LOCALTIMESTAMP()(v4.0.6)
-- error ER_VIRTUAL_COLUMN_FUNCTION_IS_NOT_ALLOWED
create table t1 (a datetime, b varchar(10) as (localtimestamp()));
-- error ER_VIRTUAL_COLUMN_FUNCTION_IS_NOT_ALLOWED
create table t1 (a datetime, b varchar(10) as (localtimestamp));

--echo # NOW()
-- error ER_VIRTUAL_COLUMN_FUNCTION_IS_NOT_ALLOWED
create table t1 (a datetime, b varchar(10) as (now()));

--echo # SYSDATE()
-- error ER_VIRTUAL_COLUMN_FUNCTION_IS_NOT_ALLOWED
create table t1 (a int, b varchar(10) as (sysdate()));

--echo # UNIX_TIMESTAMP()
-- error ER_VIRTUAL_COLUMN_FUNCTION_IS_NOT_ALLOWED
create table t1 (a datetime, b datetime as (unix_timestamp()));

--echo # UTC_DATE()
-- error ER_VIRTUAL_COLUMN_FUNCTION_IS_NOT_ALLOWED
create table t1 (a datetime, b datetime as (utc_date()));

--echo # UTC_TIME()
-- error ER_VIRTUAL_COLUMN_FUNCTION_IS_NOT_ALLOWED
create table t1 (a datetime, b datetime as (utc_time()));

--echo # UTC_TIMESTAMP()
-- error ER_VIRTUAL_COLUMN_FUNCTION_IS_NOT_ALLOWED
create table t1 (a datetime, b datetime as (utc_timestamp()));

--echo # MATCH()
if (!$skip_full_text_checks)
{
  -- error ER_VIRTUAL_COLUMN_FUNCTION_IS_NOT_ALLOWED
  create table t1 (a varchar(32), b bool as (match a against ('sample text')));
}

--echo # BENCHMARK()
-- error ER_VIRTUAL_COLUMN_FUNCTION_IS_NOT_ALLOWED
create table t1 (a varchar(1024), b varchar(1024) as (benchmark(a,3)));

--echo # CONNECTION_ID()
-- error ER_VIRTUAL_COLUMN_FUNCTION_IS_NOT_ALLOWED
create table t1 (a int as (connection_id()));

--echo # CURRENT_USER(), CURRENT_USER
-- error ER_VIRTUAL_COLUMN_FUNCTION_IS_NOT_ALLOWED
create table t1 (a varchar(32) as (current_user()));
-- error ER_VIRTUAL_COLUMN_FUNCTION_IS_NOT_ALLOWED
create table t1 (a varchar(32) as (current_user));

--echo # DATABASE()
-- error ER_VIRTUAL_COLUMN_FUNCTION_IS_NOT_ALLOWED
create table t1 (a varchar(1024), b varchar(1024) as (database()));

--echo # FOUND_ROWS()
-- error ER_VIRTUAL_COLUMN_FUNCTION_IS_NOT_ALLOWED
create table t1 (a varchar(1024), b varchar(1024) as (found_rows()));

--echo # GET_LOCK()
-- error ER_VIRTUAL_COLUMN_FUNCTION_IS_NOT_ALLOWED
create table t1 (a varchar(1024), b varchar(1024) as (get_lock(a,10)));

--echo # IS_FREE_LOCK()
-- error ER_VIRTUAL_COLUMN_FUNCTION_IS_NOT_ALLOWED
create table t1 (a varchar(1024), b varchar(1024) as (is_free_lock(a)));

--echo # IS_USED_LOCK()
-- error ER_VIRTUAL_COLUMN_FUNCTION_IS_NOT_ALLOWED
create table t1 (a varchar(1024), b varchar(1024) as (is_used_lock(a)));

--echo # LAST_INSERT_ID()
-- error ER_VIRTUAL_COLUMN_FUNCTION_IS_NOT_ALLOWED
create table t1 (a int as (last_insert_id()));

--echo # MASTER_POS_WAIT()
-- error ER_VIRTUAL_COLUMN_FUNCTION_IS_NOT_ALLOWED
create table t1 (a varchar(32), b int as (master_pos_wait(a,0,2)));

--echo # NAME_CONST()
-- error ER_VIRTUAL_COLUMN_FUNCTION_IS_NOT_ALLOWED
create table t1 (a varchar(32) as (name_const('test',1)));

--echo # RELEASE_LOCK()
-- error ER_VIRTUAL_COLUMN_FUNCTION_IS_NOT_ALLOWED
create table t1 (a varchar(32), b int as (release_lock(a)));

--echo # ROW_COUNT()
-- error ER_VIRTUAL_COLUMN_FUNCTION_IS_NOT_ALLOWED
create table t1 (a int as (row_count()));

--echo # SCHEMA()
-- error ER_VIRTUAL_COLUMN_FUNCTION_IS_NOT_ALLOWED
create table t1 (a varchar(32) as (schema()));

--echo # SESSION_USER()
-- error ER_VIRTUAL_COLUMN_FUNCTION_IS_NOT_ALLOWED
create table t1 (a varchar(32) as (session_user()));

--echo # SLEEP()
-- error ER_VIRTUAL_COLUMN_FUNCTION_IS_NOT_ALLOWED
create table t1 (a int, b int as (sleep(a)));

--echo # SYSTEM_USER()
-- error ER_VIRTUAL_COLUMN_FUNCTION_IS_NOT_ALLOWED
create table t1 (a varchar(32) as (system_user()));

--echo # USER()
-- error ER_VIRTUAL_COLUMN_FUNCTION_IS_NOT_ALLOWED
create table t1 (a varchar(1024), b varchar(1024) as (user()));

--echo # UUID_SHORT()
-- error ER_VIRTUAL_COLUMN_FUNCTION_IS_NOT_ALLOWED
create table t1 (a varchar(1024) as (uuid_short()));

--echo # UUID()
-- error ER_VIRTUAL_COLUMN_FUNCTION_IS_NOT_ALLOWED
create table t1 (a varchar(1024) as (uuid()));

--echo # VALUES()
-- error ER_VIRTUAL_COLUMN_FUNCTION_IS_NOT_ALLOWED
create table t1 (a varchar(1024), b varchar(1024) as (values(a)));

--echo # VERSION()
-- error ER_VIRTUAL_COLUMN_FUNCTION_IS_NOT_ALLOWED
create table t1 (a varchar(1024), b varchar(1024) as (version())); 

--echo # ENCRYPT()
-- error ER_VIRTUAL_COLUMN_FUNCTION_IS_NOT_ALLOWED
create table t1 (a varchar(1024), b varchar(1024) as (encrypt(a)));

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

-- error ER_VIRTUAL_COLUMN_FUNCTION_IS_NOT_ALLOWED
create table t1 (a int as (p1()));
-- error ER_VIRTUAL_COLUMN_FUNCTION_IS_NOT_ALLOWED
create table t1 (a int as (f1()));

drop procedure p1;
drop function f1;

--echo # Unknown functions
-- error ER_VIRTUAL_COLUMN_FUNCTION_IS_NOT_ALLOWED
create table t1 (a int as (f1()));

--echo #
--echo # GROUP BY FUNCTIONS
--echo #

--echo # AVG()
-- error ER_VIRTUAL_COLUMN_FUNCTION_IS_NOT_ALLOWED
create table t1 (a int, b int as (avg(a)));

--echo # BIT_AND()
-- error ER_VIRTUAL_COLUMN_FUNCTION_IS_NOT_ALLOWED
create table t1 (a int, b int as (bit_and(a)));

--echo # BIT_OR()
-- error ER_VIRTUAL_COLUMN_FUNCTION_IS_NOT_ALLOWED
create table t1 (a int, b int as (bit_or(a)));

--echo # BIT_XOR()
-- error ER_VIRTUAL_COLUMN_FUNCTION_IS_NOT_ALLOWED
create table t1 (a int, b int as (bit_xor(a)));

--echo # COUNT(DISTINCT)
-- error ER_VIRTUAL_COLUMN_FUNCTION_IS_NOT_ALLOWED
create table t1 (a int, b int as (count(distinct a)));

--echo # COUNT()
-- error ER_VIRTUAL_COLUMN_FUNCTION_IS_NOT_ALLOWED
create table t1 (a int, b int as (count(a)));

--echo # GROUP_CONCAT()
-- error ER_VIRTUAL_COLUMN_FUNCTION_IS_NOT_ALLOWED
create table t1 (a varchar(32), b int as (group_concat(a,'')));

--echo # MAX()
-- error ER_VIRTUAL_COLUMN_FUNCTION_IS_NOT_ALLOWED
create table t1 (a int, b int as (max(a)));

--echo # MIN()
-- error ER_VIRTUAL_COLUMN_FUNCTION_IS_NOT_ALLOWED
create table t1 (a int, b int as (min(a)));

--echo # STD()
-- error ER_VIRTUAL_COLUMN_FUNCTION_IS_NOT_ALLOWED
create table t1 (a int, b int as (std(a)));

--echo # STDDEV_POP()
-- error ER_VIRTUAL_COLUMN_FUNCTION_IS_NOT_ALLOWED
create table t1 (a int, b int as (stddev_pop(a)));

--echo # STDDEV_SAMP()
-- error ER_VIRTUAL_COLUMN_FUNCTION_IS_NOT_ALLOWED
create table t1 (a int, b int as (stddev_samp(a)));

--echo # STDDEV()
-- error ER_VIRTUAL_COLUMN_FUNCTION_IS_NOT_ALLOWED
create table t1 (a int, b int as (stddev(a)));

--echo # SUM()
-- error ER_VIRTUAL_COLUMN_FUNCTION_IS_NOT_ALLOWED
create table t1 (a int, b int as (sum(a)));

--echo # VAR_POP()
-- error ER_VIRTUAL_COLUMN_FUNCTION_IS_NOT_ALLOWED
create table t1 (a int, b int as (var_pop(a)));

--echo # VAR_SAMP()
-- error ER_VIRTUAL_COLUMN_FUNCTION_IS_NOT_ALLOWED
create table t1 (a int, b int as (var_samp(a)));

--echo # VARIANCE()
-- error ER_VIRTUAL_COLUMN_FUNCTION_IS_NOT_ALLOWED
create table t1 (a int, b int as (variance(a)));

--echo #
--echo # XML FUNCTIONS
--echo #

--echo # ExtractValue()
-- error ER_VIRTUAL_COLUMN_FUNCTION_IS_NOT_ALLOWED
create table t1 (a varchar(1024), b varchar(1024) as (ExtractValue(a,'//b[$@j]')));

--echo # UpdateXML()
-- error ER_VIRTUAL_COLUMN_FUNCTION_IS_NOT_ALLOWED
create table t1 (a varchar(1024), b varchar(1024) as (UpdateXML(a,'/a','<e>fff</e>')));

--echo #
--echo # Sub-selects
--echo #

create table t1 (a int);
-- error ER_PARSE_ERROR
create table t2 (a int, b int as (select count(*) from t1));
drop table t1;

-- error ER_VIRTUAL_COLUMN_FUNCTION_IS_NOT_ALLOWED
create table t1 (a int, b int as ((select 1)));
-- error ER_VIRTUAL_COLUMN_FUNCTION_IS_NOT_ALLOWED
create table t1 (a int, b int as (a+(select 1)));

--echo #
--echo # SP functions
--echo #

--disable_warnings
drop function if exists sub1;
--enable_warnings
create function sub1(i int) returns int deterministic
  return i+1;
select sub1(1);
-- error ER_VIRTUAL_COLUMN_FUNCTION_IS_NOT_ALLOWED
create table t1 (a int, b int as (a+sub3(1)));
drop function sub1;

--echo #
--echo # Long expression

let $tmp_long_string = `SELECT repeat('a',240)`;
eval create table t1 (a int, b varchar(300) as (concat(a,'$tmp_long_string')));
drop table t1;
let $tmp_long_string = `SELECT repeat('a',243)`;
--error ER_WRONG_STRING_LENGTH
eval create table t1 (a int, b varchar(300) as (concat(a,'$tmp_long_string')));

--echo #
--echo # Constant expression
--error ER_CONST_EXPR_IN_VCOL
create table t1 (a int as (PI()));

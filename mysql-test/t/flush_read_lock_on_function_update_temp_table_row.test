# ==== Purpose ====
#
# Verify that calling a function updating temporary table should be
# compatible with FLUSH TABLES WITH READ LOCK (FTWRL) in row binlog
# formart. Which is split from flush_read_lock.test, because we set
# binlog format to statement for writing a 'function call' top
# statement into binary log if the function contains DML statement(s)
# on temporary table in mixed mode after fixing Bug #28258992. Which
# causes an known timeout issue in statement binlog format. So we can
# just run the script in row format.
#
# ==== Implementation ====
#
# 1. If function used as argument updates temporary tables
#    CALL statement should be compatible with FTWRL.
# 2. DO which calls SF updating temporary table should be
#    compatible with FTWRL.
# 3. SELECT which calls SF updating temporary table should be
#    compatible with FTWRL.
# 4. Ordinary SET which calls SF updating temporary table
#    should be compatible with FTWRL.
#
# ==== References ====
#
# flush_read_lock.test
# Bug #28258992  FUNCTION CALL NOT WRITTEN TO BINLOG IF IT CONTAIN DML ALONG WITH DROP TEMP TABLE

--source include/have_binlog_format_row.inc
# We need the Debug Sync Facility.
--source include/have_debug_sync.inc
# Save the initial number of concurrent sessions.
--source include/count_sessions.inc
--source include/force_myisam_default.inc
--source include/have_myisam.inc

create temporary table t1_temp(i int);
create procedure p2(i int) begin end;
delimiter |;
create function f2_temp() returns int
begin
  insert into t1_temp values (1);
  return 0;
end|
delimiter ;|

connect (con1,localhost,root,,);
connect (con2,localhost,root,,);
connection default;

let $con_aux1=con1;
let $con_aux2=con2;
let $cleanup_stmt2= ;
let $skip_3rd_check= ;

--echo #
--echo # If function used as argument updates temporary tables
--echo # CALL statement should be compatible with FTWRL.
--echo #
let $statement= call p2(f2_temp());
let $cleanup_stmt1= ;
--echo #
--echo # Skip last part of compatibility testing as this statement
--echo # releases metadata locks in non-standard place.
--echo #
let $skip_3rd_check= 1;
--source include/check_ftwrl_compatible.inc
let $skip_3rd_check= ;

--echo #
--echo # DO which calls SF updating temporary table should be
--echo # compatible with FTWRL.
--echo #
let $statement= do f2_temp();
let $cleanup_stmt= delete from t1_temp limit 1;
--source include/check_ftwrl_compatible.inc

--echo #
--echo # SELECT which calls SF updating temporary table should be
--echo # compatible with FTWRL.
--echo #
let $statement= select f2_temp();
let $cleanup_stmt= delete from t1_temp limit 1;
--source include/check_ftwrl_compatible.inc

--echo #
--echo # Ordinary SET which calls SF updating temporary table
--echo # should be compatible with FTWRL.
--echo #
let $statement= set @a:= f2_temp();
let $cleanup_stmt= delete from t1_temp limit 1;
--echo #
--echo # Skip last part of compatibility testing as our helper debug
--echo # sync-point doesn't work for SET statements.
--echo #
let $skip_3rd_check= 1;
--source include/check_ftwrl_compatible.inc
let $skip_3rd_check= ;

--echo #
--echo # Clean-up.
--echo #
drop procedure p2;
drop function f2_temp;
drop temporary tables t1_temp;
disconnect con1;
disconnect con2;

# Check that all connections opened by test cases in this file are really
# gone so execution of other tests won't be affected by their presence.
--source include/wait_until_count_sessions.inc

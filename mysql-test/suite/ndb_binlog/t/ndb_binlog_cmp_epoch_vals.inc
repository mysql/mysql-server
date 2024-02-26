--disable_query_log
--disable_result_log

select @curr_server_epoch:=Variable_value 
from performance_schema.global_status 
where Variable_name ="ndb_last_commit_epoch_server";

select @curr_session_epoch:=Variable_value 
from performance_schema.global_status 
where Variable_name ="ndb_last_commit_epoch_session";

# Wait for latest committed transaction to reach binlog
show binlog events;

--source include/get_ndb_epochs.inc

select max(epoch)
from mysql.ndb_binlog_index into @binlog_max_epoch;

select @binlog_max_epoch;

set @a:= (@init_server_epoch + 0) >= (@init_session_epoch + 0);
set @b:= (@curr_server_epoch + 0) >= (@curr_session_epoch + 0);
set @c:= (@curr_server_epoch + 0) >= (@init_server_epoch + 0);
set @d:= (@curr_session_epoch + 0) >= (@init_session_epoch + 0);

--enable_result_log

select @a as init_server_epoch_includes_session_epoch,
  @b as curr_server_epoch_includes_session_epoch,
  @c as server_epoch_does_not_regress,
  @d as session_epoch_does_not_regress;

select (@binlog_max_epoch+0) = (@curr_session_epoch+0) as
  binlog_contains_last_session_epoch;

--disable_result_log

eval select @e:= (($ndb_latest_applied_binlog_epoch + 0) = @binlog_max_epoch);

--let $problem=query_get_value("SELECT 5 - (@a+@b+@c+@d+@e) AS p", p, 1)

--enable_result_log
if ($problem)
{
  --echo Unexpected result.  Details : 

  SELECT @init_server_epoch, @init_session_epoch,
    @curr_server_epoch, @curr_session_epoch;

  eval select $ndb_latest_applied_binlog_epoch as ndb_latest_applied_binlog_epoch, @binlog_max_epoch;

  SELECT * FROM mysql.ndb_binlog_index;
}

--enable_query_log

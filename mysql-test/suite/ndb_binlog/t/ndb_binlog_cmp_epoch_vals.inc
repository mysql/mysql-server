--disable_query_log
--disable_result_log

select @curr_server_epoch:=Variable_value 
from information_schema.global_status 
where Variable_name ="ndb_last_commit_epoch_server";

select @curr_session_epoch:=Variable_value 
from information_schema.global_status 
where Variable_name ="ndb_last_commit_epoch_session";

show binlog events;

select @binlog_max_epoch:= max(epoch) 
from mysql.ndb_binlog_index;

--enable_result_log

select (@init_server_epoch + 0) >= (@init_session_epoch + 0) 
  as init_server_epoch_includes_session_epoch,
  (@curr_server_epoch + 0) >= (@curr_session_epoch + 0) 
  as curr_server_epoch_includes_session_epoch,
  (@curr_server_epoch + 0) >= (@init_server_epoch + 0) 
  as server_epoch_does_not_regress,
  (@curr_session_epoch + 0) >= (@init_session_epoch + 0) 
  as session_epoch_does_not_regress;

select @binlog_max_epoch = @curr_session_epoch as
  binlog_contains_last_session_epoch;

--enable_query_log
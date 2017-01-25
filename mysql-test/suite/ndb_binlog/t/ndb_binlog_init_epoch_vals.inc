--disable_query_log
--disable_result_log

select @init_server_epoch:=Variable_value 
from performance_schema.global_status 
where Variable_name ="ndb_last_commit_epoch_server";

select @init_session_epoch:=Variable_value 
from performance_schema.global_status 
where Variable_name ="ndb_last_commit_epoch_session";

--enable_query_log
--enable_result_log

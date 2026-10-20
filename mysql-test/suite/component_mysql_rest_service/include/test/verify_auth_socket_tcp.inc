# Verify external-password authentication and auth_socket rejection with
# $mrs_auth_socket_bind_address. The caller must check IPv6 availability first.

--disable_query_log
--disable_result_log
SELECT mysql_rest_service_stop();
eval SET PERSIST mysql_rest_service.http_port=$HTTP_SERVER_PORT;
--enable_result_log
--enable_query_log
--let $restart_parameters=restart: --bind-address=$mrs_auth_socket_bind_address
--source include/restart_mysqld.inc

--let $wait_condition=SELECT VARIABLE_VALUE = 'READY' FROM performance_schema.global_status WHERE VARIABLE_NAME = 'mysql_rest_service.state'
--source include/wait_condition_or_abort.inc
--source ../cmrs/wait_mrs_read_metadata.inc
--let MRS_CLIENT_ARGS=$MRS_CLIENT --url https://[::1]:$HTTP_SERVER_PORT

exec $MRS_CLIENT_ARGS -a basic_json -t POST
  --path /svc/authentication/login -u mrs_socket_external -p mrs_socket_proxy
  --session-type cookie --session-file $MYSQL_TMP_DIR/mrs_socket_external.dat
  --display REQUEST,STATUS;
exec $MRS_CLIENT_ARGS --path /svc/db/fixture/73
  --session-file $MYSQL_TMP_DIR/mrs_socket_external.dat
  --display REQUEST,STATUS;

exec $MRS_CLIENT_ARGS -a basic_json -t POST
  --path /svc/authentication/login -u mrs_socket_match -p nonempty-password
  --session-type cookie --session-file $MYSQL_TMP_DIR/mrs_socket_match.dat
  --expected-status Unauthorized --display REQUEST,STATUS;
exec $MRS_CLIENT_ARGS --path /svc/db/fixture/73
  --session-file $MYSQL_TMP_DIR/mrs_socket_match.dat
  --expected-status Unauthorized --display REQUEST,STATUS;

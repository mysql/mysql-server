# Local password authentication remains usable when the command service cannot
# use TCP. Socket logins that set external_user must still fail closed.

exec $MRS_CLIENT_ARGS -a basic_json -t POST
  --path /svc/authentication/login -u mrs_socket_password -p correct-password
  --session-type cookie --session-file $MYSQL_TMP_DIR/mrs_socket_password.dat
  --display REQUEST,STATUS;
exec $MRS_CLIENT_ARGS --path /svc/db/fixture/73
  --session-file $MYSQL_TMP_DIR/mrs_socket_password.dat
  --exclude-json-pointer /links,/_metadata
  --display REQUEST,STATUS,BODY;

exec $MRS_CLIENT_ARGS -a basic_json -t POST
  --path /svc/authentication/login -u mrs_socket_external -p mrs_socket_proxy
  --session-type cookie --session-file $MYSQL_TMP_DIR/mrs_socket_external_local.dat
  --expected-status Unauthorized --display REQUEST,STATUS;
exec $MRS_CLIENT_ARGS --path /svc/db/fixture/73
  --session-file $MYSQL_TMP_DIR/mrs_socket_external_local.dat
  --expected-status Unauthorized --display REQUEST,STATUS;

exec $MRS_CLIENT_ARGS -a basic_json -t POST
  --path /svc/authentication/login -u mrs_socket_match -p nonempty-password
  --session-type cookie --session-file $MYSQL_TMP_DIR/mrs_socket_match.dat
  --expected-status Unauthorized --display REQUEST,STATUS;
exec $MRS_CLIENT_ARGS --path /svc/db/fixture/73
  --session-file $MYSQL_TMP_DIR/mrs_socket_match.dat
  --expected-status Unauthorized --display REQUEST,STATUS;

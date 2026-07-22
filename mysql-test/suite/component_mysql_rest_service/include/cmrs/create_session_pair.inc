# Create and verify one MRS cookie session and one MRS JWT session.
#
# Required variables:
#   $mrs_session_prefix
#   $mrs_session_service_path
#   $mrs_session_test_path
#   $mrs_session_auth_app
#   $mrs_session_user
#   $mrs_session_password
#
# Outputs:
#   $mrs_cookie_session_file
#   $mrs_jwt_session_file

if (!$mrs_session_prefix)
{
  --die create_session_pair.inc requires mrs_session_prefix
}

--let $mrs_cookie_session_file=$MYSQL_TMP_DIR/$mrs_session_prefix-cookie.dat
--let $mrs_jwt_session_file=$MYSQL_TMP_DIR/$mrs_session_prefix-jwt.dat

--exec $MRS_CLIENT_ARGS -a scram_post --auth-app $mrs_session_auth_app --path $mrs_session_service_path/authentication/login -u $mrs_session_user -p $mrs_session_password --session-type cookie --display none --session-file $mrs_cookie_session_file
--exec $MRS_CLIENT_ARGS -a scram_post --auth-app $mrs_session_auth_app --path $mrs_session_service_path/authentication/login -u $mrs_session_user -p $mrs_session_password --session-type jwt --display none --session-file $mrs_jwt_session_file

--exec $MRS_CLIENT_ARGS --path $mrs_session_test_path --display none --session-file $mrs_cookie_session_file
--exec $MRS_CLIENT_ARGS --path $mrs_session_test_path --display none --session-file $mrs_jwt_session_file

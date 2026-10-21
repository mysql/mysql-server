# Verify the session files produced by create_session_pair.inc are rejected.

--exec $MRS_CLIENT_ARGS --path $mrs_session_test_path --display none --expected-status Unauthorized --session-file $mrs_cookie_session_file
--exec $MRS_CLIENT_ARGS --path $mrs_session_test_path --display none --expected-status Unauthorized --session-file $mrs_jwt_session_file

--remove_file $mrs_cookie_session_file
--remove_file $mrs_jwt_session_file

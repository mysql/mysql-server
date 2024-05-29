if (!$MRS_CLIENT_ARGS)
{
  --die `MRS_CLIENT_ARGS` variable is required.
}

--let $mrs_client_cmd=$MRS_CLIENT_ARGS

if ($mrs_client_arg_path)
{
  #--let $mrs_client_arg_path=`SELECT REPLACE('$mrs_client_arg_path', '$', '%24')`
  --let $mrs_client_arg_path=`SELECT REPLACE($mrs_client_arg_path, '"', '%22')`
  --let $mrs_client_arg_path=`SELECT REPLACE('$mrs_client_arg_path', '{', '%7B')`
  --let $mrs_client_arg_path=`SELECT REPLACE('$mrs_client_arg_path', '}', '%7D')`
  --let $mrs_client_arg_path=`SELECT REPLACE('$mrs_client_arg_path', '[', '%5B')`
  --let $mrs_client_arg_path=`SELECT REPLACE('$mrs_client_arg_path', ']', '%5D')`
  --let $mrs_client_arg_path=`SELECT REPLACE('$mrs_client_arg_path', ' ', '%20')`
  #--let $mrs_client_arg_path=`SELECT REPLACE('$mrs_client_arg_path', '=', '%3D')`
  #--let $mrs_client_arg_path=`SELECT REPLACE('$mrs_client_arg_path', '?', '%3F')`
  --let $mrs_client_arg_path=`SELECT REPLACE('$mrs_client_arg_path', '&', '%26')`

  --let $mrs_client_cmd=$mrs_client_cmd --path $mrs_client_arg_path
}

if ($mrs_client_expected_status)
{
  --let $mrs_client_cmd=$mrs_client_cmd --expected-status $mrs_client_expected_status
}

if ($mrs_client_arg_expected_status)
{
  --let $mrs_client_cmd=$mrs_client_cmd --expected-status $mrs_client_arg_expected_status
}

if ($mrs_client_arg_json_pointer)
{
  --let $mrs_client_cmd=$mrs_client_cmd --json-pointer $mrs_client_arg_json_pointer
}

exec $mrs_client_cmd;
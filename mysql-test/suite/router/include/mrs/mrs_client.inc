if (!$MRS_CLIENT_ARGS)
{
  --die `MRS_CLIENT_ARGS` variable is required.
}

--let $mrs_client_cmd=$MRS_CLIENT_ARGS

if ($mrs_client_arg_path)
{
  --let $mrs_client_arg_path=`SELECT REPLACE(REPLACE($mrs_client_arg_path, '%', '%25'), '$', '%24')`
  --let $mrs_client_arg_path=`SELECT REPLACE('$mrs_client_arg_path', '"', '%22')`
  --let $mrs_client_arg_path=`SELECT REPLACE(REPLACE('$mrs_client_arg_path', '{', '%7B'), '}', '%7D')`
  --let $mrs_client_arg_path=`SELECT REPLACE(REPLACE('$mrs_client_arg_path', '[', '%5B'), ']', '%5D')`
  --let $mrs_client_arg_path=`SELECT REPLACE('$mrs_client_arg_path', ' ', '%20')`
  #--let $mrs_client_arg_path=`SELECT REPLACE('$mrs_client_arg_path', '=', '%3D')`
  #--let $mrs_client_arg_path=`SELECT REPLACE('$mrs_client_arg_path', '?', '%3F')`
  #--let $mrs_client_arg_path=`SELECT REPLACE('$mrs_client_arg_path', '&', '%26')`

  --let $mrs_client_cmd=$mrs_client_cmd --path $mrs_client_arg_path
}

if ($mrs_client_arg_payload)
{
  --let $mrs_client_arg_payload=`SELECT REPLACE(REPLACE($mrs_client_arg_payload, '%', '%25'), '$', '%24')`
  --let $mrs_client_arg_payload=`SELECT REPLACE('$mrs_client_arg_payload', '"', '%22')`
  --let $mrs_client_arg_payload=`SELECT REPLACE(REPLACE('$mrs_client_arg_payload', '{', '%7B'), '}', '%7D')`
  --let $mrs_client_arg_payload=`SELECT REPLACE(REPLACE('$mrs_client_arg_payload', '[', '%5B'), ']', '%5D')`
  --let $mrs_client_arg_payload=`SELECT REPLACE('$mrs_client_arg_payload', ' ', '%20')`
  #--let $mrs_client_arg_payload=`SELECT REPLACE('$mrs_client_arg_payload', '=', '%3D')`
  #--let $mrs_client_arg_payload=`SELECT REPLACE('$mrs_client_arg_payload', '?', '%3F')`
  #--let $mrs_client_arg_payload=`SELECT REPLACE('$mrs_client_arg_payload', '&', '%26')`

  --let $mrs_client_cmd=$mrs_client_cmd --encoded-payload $mrs_client_arg_payload
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

if ($mrs_client_arg_request_type)
{
  --let $mrs_client_cmd=$mrs_client_cmd -t $mrs_client_arg_request_type
}

exec $mrs_client_cmd;
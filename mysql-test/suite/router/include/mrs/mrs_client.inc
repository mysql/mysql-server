if ($mrs_client_exe_args)
{
  --let $mrs_client_cmd=$mrs_client_exe_args  
}

if (!$mrs_client_exe_args)
{
  if (!$MRS_CLIENT_ARGS)
  {
    --die Either `MRS_CLIENT_ARGS` or `mrs_client_exe_args` variable is required.
  }
  --let $mrs_client_cmd=$MRS_CLIENT_ARGS  
}

if ($mrs_client_arg_path)
{
  --let $mrs_client_arg_path=`SELECT REPLACE(REPLACE($mrs_client_arg_path, '%', '%25'), '$', '%24')`
  --let $mrs_client_arg_path=`SELECT REPLACE('$mrs_client_arg_path', '"', '%22')`
  --let $mrs_client_arg_path=`SELECT REPLACE(REPLACE('$mrs_client_arg_path', '{', '%7B'), '}', '%7D')`
  --let $mrs_client_arg_path=`SELECT REPLACE(REPLACE('$mrs_client_arg_path', '(', '%28'), ')', '%29')`
  --let $mrs_client_arg_path=`SELECT REPLACE(REPLACE('$mrs_client_arg_path', '[', '%5B'), ']', '%5D')`
  --let $mrs_client_arg_path=`SELECT REPLACE('$mrs_client_arg_path', ' ', '%20')`
  #--let $mrs_client_arg_path=`SELECT REPLACE('$mrs_client_arg_path', '=', '%3D')`
  #--let $mrs_client_arg_path=`SELECT REPLACE('$mrs_client_arg_path', '?', '%3F')`
  --let $mrs_client_arg_path=`SELECT REPLACE('$mrs_client_arg_path', '&', '%26')`

  --let $mrs_client_cmd=$mrs_client_cmd --path $mrs_client_arg_path
}

if ($mrs_client_arg_payload)
{
  --let $mrs_client_arg_payload=`SELECT REPLACE(REPLACE($mrs_client_arg_payload, '%', '%25'), '$', '%24')`
  --let $mrs_client_arg_payload=`SELECT REPLACE('$mrs_client_arg_payload', '"', '%22')`
  --let $mrs_client_arg_payload=`SELECT REPLACE(REPLACE('$mrs_client_arg_payload', '{', '%7B'), '}', '%7D')`
  --let $mrs_client_arg_payload=`SELECT REPLACE(REPLACE('$mrs_client_arg_payload', '(', '%28'), ')', '%29')`
  --let $mrs_client_arg_payload=`SELECT REPLACE(REPLACE('$mrs_client_arg_payload', '[', '%5B'), ']', '%5D')`
  --let $mrs_client_arg_payload=`SELECT REPLACE('$mrs_client_arg_payload', ' ', '%20')`
  #--let $mrs_client_arg_payload=`SELECT REPLACE('$mrs_client_arg_payload', '=', '%3D')`
  #--let $mrs_client_arg_payload=`SELECT REPLACE('$mrs_client_arg_payload', '?', '%3F')`
  #--let $mrs_client_arg_payload=`SELECT REPLACE('$mrs_client_arg_payload', '&', '%26')`

  --let $mrs_client_cmd=$mrs_client_cmd --encoded-payload $mrs_client_arg_payload
}

if ($mrs_client_arg_expected_status)
{
  --let $mrs_client_cmd=$mrs_client_cmd --expected-status $mrs_client_arg_expected_status
}

if ($mrs_client_arg_json_pointer)
{
  --let $mrs_client_cmd=$mrs_client_cmd --json-pointer $mrs_client_arg_json_pointer
}

if ($mrs_client_arg_exclude_json_pointer)
{
  --let $mrs_client_cmd=$mrs_client_cmd --exclude-json-pointer $mrs_client_arg_exclude_json_pointer
}

if ($mrs_client_arg_request_type)
{
  --let $mrs_client_cmd=$mrs_client_cmd -t $mrs_client_arg_request_type
}

if ($mrs_client_arg_dispaly)
{
  --let $mrs_client_cmd=$mrs_client_cmd --display $mrs_client_arg_dispaly
}

if ($mrs_client_arg_write_format)
{
  --let $mrs_client_cmd=$mrs_client_cmd --write-format $mrs_client_arg_write_format
}

if ($mrs_client_arg_write_to_file)
{
  --let $mrs_client_cmd=$mrs_client_cmd --write-to-file $mrs_client_arg_write_to_file
}

if ($mrs_client_arg_session_file)
{
  --let $mrs_client_cmd=$mrs_client_cmd --session-file $mrs_client_arg_session_file
}

if ($mrs_client_arg_wait_until_found)
{
--let $mrs_client_cmd=$mrs_client_cmd --wait-until-found=$mrs_client_arg_wait_until_found
}

if ($mrs_client_arg_wait_until_status)
{
--let $mrs_client_cmd=$mrs_client_cmd --wait-until-status=$mrs_client_arg_wait_until_status
}

if ($mrs_client_arg_response_type)
{
--let $mrs_client_cmd=$mrs_client_cmd --response-type=$mrs_client_arg_response_type
}

if ($mrs_client_arg_json_schema)
{
--let $mrs_client_cmd=$mrs_client_cmd --json-schema=$mrs_client_arg_json_schema
}

exec $mrs_client_cmd;

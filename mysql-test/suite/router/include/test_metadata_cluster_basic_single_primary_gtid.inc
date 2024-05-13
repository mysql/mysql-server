if (!$test_schema)
{
    --die "Variable 'test_schema' is required."
}

if (!$test_obj1)
{
    --die "Variable 'test_obj1' is required."
}

if (!$test_obj2)
{
    --die "Variable 'test_obj2' is required."
}
START TRANSACTION;
--let $mrs_add_schema=basic_schema
--let $mrs_add_schema_path=/$test_schema
--source ../include/mrs/db_schema/add.inc

--let $mrs_add_db_object=table1
--let $mrs_add_db_object_path=/$test_obj1
--let $mrs_add_db_object_options=$test_options
--source ../include/mrs/db_object/add.inc

--let $mrs_add_db_object=table2
--let $mrs_add_db_object_path=/$test_obj2
--let $mrs_add_db_object_options=$test_options
--source ../include/mrs/db_object/add.inc
COMMIT;
--source include/rpl/sync.inc

--let $mrs_host_and_port=127.0.0.1:$extra_http_port
--let MRS_CLIENT_ARGS=$MRS_CLIENT --url https://$mrs_host_and_port

#--exec $MRS_CLIENT_ARGS --path /svc/$test_schema/$test_obj1 --wait-until-found=60  --request-type post --payload '{"id":1010}'

--exec $MRS_CLIENT_ARGS --path /svc/$test_schema/$test_obj1 --wait-until-found=60
--exec $MRS_CLIENT_ARGS --path /svc/$test_schema/$test_obj1/1
--exec $MRS_CLIENT_ARGS --path /svc/$test_schema/$test_obj1/20
--exec $MRS_CLIENT_ARGS --path /svc/$test_schema/$test_obj1/2 --expected-status 404
exec $MRS_CLIENT_ARGS --path /svc/$test_schema/$test_obj1/20
  --write-to-file $MYSQL_TMP_DIR/gtid.inc
  --write-format mtr
  --request-type delete
  --display RESULT
  --json-pointer /_metadata/gtid;

--source $MYSQL_TMP_DIR/gtid.inc


# Hide GTIDS by setting: BODY,RESULT
--let $asof="\$asof"
exec $MRS_CLIENT_ARGS
  --path '/svc/$test_schema/$test_obj1?q={"id":20,$asof:$mrs_result}'
  --display BODY,RESULT
  --json-pointer /count;

--exec $MRS_CLIENT_ARGS --path /svc/$test_schema/$test_obj2
--exec $MRS_CLIENT_ARGS --path /svc/$test_schema/$test_obj2/1
--exec $MRS_CLIENT_ARGS --path /svc/$test_schema/$test_obj2/200 --expected-status 404

# Save resulting GTID in file: gtid.inc, variable: $mrs_result
exec $MRS_CLIENT_ARGS --path /svc/$test_schema/$test_obj2
  --write-to-file $MYSQL_TMP_DIR/gtid.inc
  --write-format mtr
  --request-type post
  --json-pointer /_metadata/gtid
  --display RESULT
  --payload '{"id":3000, "name":"PUT item", "date":"1920-12-1"}';

# Place GTID in $mrs_result
--source $MYSQL_TMP_DIR/gtid.inc

--exec $MRS_CLIENT_ARGS --path '/svc/$test_schema/$test_obj2?q={"id":3000,$asof:$mrs_result}' --display BODY,RESULT

--exec $MRS_CLIENT_ARGS --expected-status 400 --path '/svc/$test_schema/$test_obj2?q={"id":3001,$asof:"3E11FA47-71CA-11E1-9E33-C80AA9429562:1-5"}' --display BODY,RESULT
exec $MRS_CLIENT_ARGS
  --expected-status 400
  --request-type delete
  --path '/svc/$test_schema/$test_obj2?q={"id":3001,"$asof":"3E11FA47-71CA-11E1-9E33-C80AA9429561:1-5"}'
  --display BODY,RESULT;


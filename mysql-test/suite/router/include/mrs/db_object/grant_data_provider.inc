# Grant the component data-provider role the privileges required by a REST
# object or one of its referenced objects.

--let $_mrs_data_provider_query_log=$ENABLE_QUERY_LOG
--let $_mrs_data_provider_result_log=$ENABLE_RESULT_LOG
--disable_query_log
--disable_result_log

if (!$_mrs_data_provider_grant_schema)
{
  --die '$_mrs_data_provider_grant_schema' variable is required.
}

if (!$_mrs_data_provider_grant_object)
{
  --die '$_mrs_data_provider_grant_object' variable is required.
}

if (!$_mrs_data_provider_grant_type)
{
  --die '$_mrs_data_provider_grant_type' variable is required.
}

--let $_mrs_data_provider_grant=
--let $_mrs_data_provider_object_exists=0
--let $_mrs_data_provider_backing_object_required=0

if ($_mrs_data_provider_grant_type=="TABLE")
{
  --let $_mrs_data_provider_backing_object_required=1
  --let $_mrs_data_provider_object_exists=`SELECT COUNT(*) FROM information_schema.tables WHERE table_schema='$_mrs_data_provider_grant_schema' AND table_name='$_mrs_data_provider_grant_object'`
  if ($_mrs_data_provider_object_exists)
  {
    --let $_mrs_data_provider_privileges=`SELECT CONCAT_WS(',', IF(FIND_IN_SET('CREATE', '$_mrs_data_provider_grant_operations'), 'INSERT', NULL), IF(FIND_IN_SET('READ', '$_mrs_data_provider_grant_operations'), 'SELECT', NULL), IF(FIND_IN_SET('UPDATE', '$_mrs_data_provider_grant_operations'), 'UPDATE', NULL), IF(FIND_IN_SET('DELETE', '$_mrs_data_provider_grant_operations'), 'DELETE', NULL))`
    if ($_mrs_data_provider_privileges)
    {
      --let $_mrs_data_provider_grant=GRANT $_mrs_data_provider_privileges ON `$_mrs_data_provider_grant_schema`.`$_mrs_data_provider_grant_object` TO 'mysql_rest_service_data_provider'
    }
  }
}

if ($_mrs_data_provider_grant_type=="PROCEDURE")
{
  --let $_mrs_data_provider_backing_object_required=1
  --let $_mrs_data_provider_object_exists=`SELECT COUNT(*) FROM information_schema.routines WHERE routine_schema='$_mrs_data_provider_grant_schema' AND routine_name='$_mrs_data_provider_grant_object' AND routine_type='PROCEDURE'`
  if ($_mrs_data_provider_object_exists)
  {
    --let $_mrs_data_provider_grant=GRANT EXECUTE ON PROCEDURE `$_mrs_data_provider_grant_schema`.`$_mrs_data_provider_grant_object` TO 'mysql_rest_service_data_provider'
  }
}

if ($_mrs_data_provider_grant_type=="FUNCTION")
{
  --let $_mrs_data_provider_backing_object_required=1
  --let $_mrs_data_provider_object_exists=`SELECT COUNT(*) FROM information_schema.routines WHERE routine_schema='$_mrs_data_provider_grant_schema' AND routine_name='$_mrs_data_provider_grant_object' AND routine_type='FUNCTION'`
  if ($_mrs_data_provider_object_exists)
  {
    --let $_mrs_data_provider_grant=GRANT EXECUTE ON FUNCTION `$_mrs_data_provider_grant_schema`.`$_mrs_data_provider_grant_object` TO 'mysql_rest_service_data_provider'
  }
}

# Missing backing objects must be explicit in metadata-only fixtures.
if ($_mrs_data_provider_backing_object_required)
{
  if (!$_mrs_data_provider_object_exists)
  {
    if (!$mrs_data_provider_allow_missing_object)
    {
      --die Data-provider object $_mrs_data_provider_grant_schema.$_mrs_data_provider_grant_object must exist.
    }
  }
}

if ($_mrs_data_provider_grant)
{
  if ($_mrs_object_definition_transaction)
  {
    --let $_mrs_data_provider_grants=$_mrs_data_provider_grant;$_mrs_data_provider_grants
  }
  if (!$_mrs_object_definition_transaction)
  {
    --eval $_mrs_data_provider_grant
  }
  if ($_mrs_data_provider_result_log)
  {
    --echo # Configured data-provider access for $_mrs_data_provider_grant_type $_mrs_data_provider_grant_schema.$_mrs_data_provider_grant_object
  }
}

--let $_mrs_data_provider_grant_schema=
--let $_mrs_data_provider_grant_object=
--let $_mrs_data_provider_grant_type=
--let $_mrs_data_provider_grant_operations=
--let $mrs_data_provider_allow_missing_object=
--let $_mrs_data_provider_backing_object_required=
--let $_mrs_data_provider_object_exists=
--let $_mrs_data_provider_privileges=
--let $_mrs_data_provider_grant=

if ($_mrs_data_provider_result_log)
{
  --enable_result_log
}
if ($_mrs_data_provider_query_log)
{
  --enable_query_log
}

--let $_mrs_data_provider_result_log=
--let $_mrs_data_provider_query_log=

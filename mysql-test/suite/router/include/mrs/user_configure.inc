# ==== Purpose ====
#
# Configure MySQL Server account to be used by MRS while
# accessing the data on the server. Where:
#
# * $mrs_user_meta - account used to access mrs schema.
# * $mrs_user_data - account used to access schemas and table
#                    registred by user in MRS.
#
# ==== Usage ====
#
# [--let $mrs_user_meta=some_account@%]
# [--let $mrs_user_data=some_account@%]
# --source ../include/mrs/user_configure.inc
#


if (!$mrs_user_meta)
{
  if (!$mrs_user_data)
  {
    --die At least one of two MRS user, must be specified for the script.
  }  
}

--disable_query_log
--disable_result_log

if ($mrs_user_meta)
{
  eval GRANT 'mysql_rest_service_meta_provider' TO $mrs_user_meta;
}

if ($mrs_user_data)
{
  eval GRANT 'mysql_rest_service_data_provider' TO $mrs_user_data;
}

--enable_query_log
--enable_result_log

echo # MySQL Rest Service, server accounts configured.;
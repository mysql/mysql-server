
if (!$mrs_sql_id_variable)
{
  --let $mrs_sql_id_variable=@auth_app_id
}

if (!$mrs_add_auth_app)
{
  --die 'mrs_add_auth_app' variable is required.
}

if (!$mrs_add_auth_vendor)
{
  --die 'mrs_add_auth_vendor' variable is required.
}

if (!$mrs_add_auth_service)
{
  --die 'mrs_add_auth_vendor' variable is required.
}

if (!$mrs_add_auth_registered_users_only)
{
  --let $mrs_add_auth_registered_users_only=0
}

if (!$mrs_add_auth_client_id)
{
  --let $mrs_add_auth_client_id=NULL
}

if (!$mrs_add_auth_client_secret)
{
  --let $mrs_add_auth_client_secret=NULL
}

if (!$mrs_add_auth_role_name)
{
  --let $mrs_add_auth_role_name=Full Access
}


--disable_query_log
--disable_result_log
eval  SET $mrs_sql_id_variable=mysql_rest_service_metadata.get_sequence_id();
 
eval INSERT INTO mysql_rest_service_metadata.auth_app
    (id, auth_vendor_id, service_id, name, enabled, limit_to_registered_users,
    default_role_id, app_id, access_token)
    VALUES($mrs_sql_id_variable,
    (SELECT id FROM mysql_rest_service_metadata.auth_vendor WHERE name like "$mrs_add_auth_vendor"),
    (SELECT id FROM mysql_rest_service_metadata.service WHERE url_context_root like "$mrs_add_auth_service"),
    "$mrs_add_auth_app",
    1,
    $mrs_add_auth_registered_users_only,
    (SELECT id FROM mysql_rest_service_metadata.mrs_role WHERE caption like "$mrs_add_auth_role_name"),
    $mrs_add_auth_client_id,
    $mrs_add_auth_client_secret
    );
  
--echo # Registred AUTH APP at path: $mrs_add_auth_service
  
--enable_query_log
--enable_result_log


--let $mrs_sql_id_variable=
--let $mrs_add_auth_registered_users_only=
--let $mrs_add_auth_service=
--let $mrs_add_auth_vendor=
--let $mrs_add_auth_app=
--let $mrs_add_auth_client_id=
--let $mrs_add_auth_client_secret=
--let $mrs_add_auth_role_name=

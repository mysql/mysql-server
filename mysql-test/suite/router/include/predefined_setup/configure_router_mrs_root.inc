# ==== Purpose ====
#
# Reusable MRS configuration for tests that normally use Router MRS with root
# access to both meta and user data. If MTR_ROUTER_MRS_USE_COMPONENT is set,
# install and target the MRS component instead.
#
# ==== Usage ====
#
# --source ../include/predefined_setup/configure_router_mrs_root.inc
#

if ($MTR_ROUTER_MRS_USE_COMPONENT)
{
  if (!$MYSQL_REST_SERVICE_COMPONENT)
  {
    --skip MySQL REST Service component is not available.
  }

  if (!$MTR_COMPONENT_MRS_PORT_OFFSET)
  {
    --die MTR must reserve a Component MRS port when MTR_ROUTER_MRS_USE_COMPONENT is set.
  }

  if ($mrs_user_meta)
  {
    --skip Component MRS mode uses fixed internal accounts.
  }

  if ($mrs_user_data)
  {
    --skip Component MRS mode uses fixed internal accounts.
  }

  --let $mrs_user_meta=mysql_mrs_metadata@'localhost'
  --let $mrs_user_data=mysql_mrs_data@'localhost'

  --disable_query_log
  --disable_result_log
  DROP USER IF EXISTS 'mysql_mrs_metadata'@'localhost',
    'mysql_mrs_data'@'localhost';
  --enable_result_log
  --enable_query_log

  --source ../mrs/initialize_schema.inc

  --disable_query_log
  --disable_result_log
  --let $HTTP_SERVER_SSL=1
  --let $HTTP_SERVER_PORT=$MTR_COMPONENT_MRS_PORT_OFFSET

  if ($install_component_extras)
  {
    --let $INSTALL_EXTRAS=$install_component_extras
  }

  --let $install_component_extras=GLOBAL mysql_rest_service.http_port = $HTTP_SERVER_PORT

  if ($INSTALL_EXTRAS)
  {
    --let $install_component_extras=$install_component_extras, $INSTALL_EXTRAS
  }

  --eval INSTALL COMPONENT "file://component_mysql_rest_service" SET $install_component_extras
  --let $install_component_extras=
  --let $INSTALL_EXTRAS=
  --let $_mrs_component_installed=1
  --let $extra_mrs_router_id= `SELECT @@server_id`
  --enable_result_log
  --enable_query_log

  --let $mrs_host_and_port=127.0.0.1:$HTTP_SERVER_PORT
  --let MRS_URL=https://$mrs_host_and_port
  --let MRS_CLIENT_ARGS=$MRS_CLIENT --url https://$mrs_host_and_port

  --let $wait_timeout=10
  --let $wait_condition=SELECT VARIABLE_VALUE = 'READY' FROM performance_schema.global_status WHERE VARIABLE_NAME = 'mysql_rest_service.state'
  --source include/wait_condition_or_abort.inc
  --echo # MySQL Rest Service, server accounts configured.
}

if (!$MTR_ROUTER_MRS_USE_COMPONENT)
{
  --source ../mrs/initialize_schema.inc

  if (!$mrs_user_meta)
  {
    --let $mrs_user_meta=root@localhost
  }
  if (!$mrs_user_data) {
    --let $mrs_user_data=root@localhost
  }
  --source ../mrs/user_configure.inc

  --let $mrs_host_and_port=127.0.0.1:$HTTP_SERVER_PORT
  if ($HTTP_SERVER_SSL==1)
  {
    --let MRS_URL=https://$mrs_host_and_port
    --let MRS_CLIENT_ARGS=$MRS_CLIENT --url https://$mrs_host_and_port
  }

  if (!$MRS_CLIENT_ARGS)
  {
    --let MRS_URL=http://$mrs_host_and_port
    --let MRS_CLIENT_ARGS=$MRS_CLIENT --url http://$mrs_host_and_port
  }
}

--source ../test_dir.inc
--let $SCHEMA_DIR=$TEST_DIR/../include/json_schema/

if ($_MRS_CONTROL_START)
{
  if (!$MTR_ROUTER_MRS_USE_COMPONENT)
  {
    --exec $MRS_CLIENT_ARGS --path /debug?do=start --wait-until-status=60 --display none
    --let $_mrs_started=1
  }
}

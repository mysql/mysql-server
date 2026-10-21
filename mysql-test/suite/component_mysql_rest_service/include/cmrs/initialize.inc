# Do the full setup of the MySQL Server and MRS component to start handling
# REST request.

#
#--let $init_setup_variables=1|0    (default:0)
#--let $skip_schema_init=1|0    (default:0)
#--let $init_does_mrs_bootstrap=1|0   (default:1)
#--source ../include/cmrs/initialize.inc
#
# The script sets following variables:
#
# * $extra_mrs_router_id
# * $router_tests
# * $MRS_CLIENT_ARGS
#

--let $check_ipv6_just_check=1
--source include/check_ipv6.inc
--let $check_ipv6_just_check=0

# Force MTR to reserve and define MTR_COMPONENT_MRS_PORT_OFFSET for MRS component.
--source have_component_mrs.inc

# Create MRS Metadata Schema
if (!$init_setup_variables)
{
  if (!$skip_schema_init) {
    --source ../../../router/include/mrs/initialize_schema.inc
  }
}

if (!$init_does_mrs_bootstrap)
{
  if (`SELECT LENGTH("$init_does_mrs_bootstrap")=0`)
  {
    --let $init_does_mrs_bootstrap=1;
  }
}

#TODO: for now run only SSL tests.
--let $HTTP_SERVER_SSL=1
--let $HTTP_SERVER_PORT=$MTR_COMPONENT_MRS_PORT_OFFSET

--let $router_tests=../../router/include/

--let $install_component_extras=GLOBAL mysql_rest_service.http_port = $HTTP_SERVER_PORT

if ($init_does_mrs_bootstrap)
{
  --let $MYSQLD_DATADIR= `SELECT @@datadir`

  if (!$init_setup_variables)
  {
    --let $mrs_user_meta=mysql_mrs_metadata@'localhost'
    --let $mrs_user_data=mysql_mrs_data@'localhost'
  }

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

if (!$init_setup_variables)
{
  --let $install_component_echo=0
  --disable_query_log
  --disable_result_log
  --source install_component.inc
  --enable_query_log
  --enable_result_log

  --echo # MySQL REST Service component installed.
}

# MRS Component uses "server-id" as "router-id" reported in MRS metadata schema.
--let $extra_mrs_router_id= `SELECT @@server_id`

--source ../../../router/include/test_dir.inc

--let $init_setup_variables=
--let $skip_schema_init=
--let $init_does_mrs_bootstrap=

## This script is a part of a test-case that expects
#  a failure when too old version of mrs schema is used
#
## Example
#  =======
# --let $mrs_schema_version=1,0,12
# --source ../include/test/bootstrap_mrs_schema_version.inc
#

if (!$mrs_schema_version)
{
  --die The 'mrs_schema_version' veriable is required by the script.
}

echo # Checking that version=$mrs_schema_version is incompatible for bootstrap;
eval CREATE OR REPLACE SQL SECURITY INVOKER VIEW
    mysql_rest_service_metadata.schema_version (major, minor, patch)
    AS SELECT $mrs_schema_version;

let $_bootstrap_paths_and_ports_replace = / Bootstrapping MySQL Router.* instance at \'.*/ Bootstrapping MySQL Router instance at 'DIRECTORY'/
   /Creating configuration .*mysqlrouter.conf/Creating configuration ...\/mysqlrouter.conf/
   /\\\$ .*mysqlrouter -c .*mysqlrouter.conf/\$ mysqlrouter -c ...\/mysqlrouter.conf/
   /Adjusting configuration file .*mysqlrouter.conf/Adjusting configuration file ...\/mysqlrouter.conf/
   /\(mysql_router1_[^)]+\)/(mysql_router1_RANDOM)/
   / Using existing certificates from the '.*' directory/Using existing certificates from the 'TEMP_DIRECTORY' directory/
   /Reconfiguring MySQL Router.* instance at \'.*\'/Reconfiguring MySQL Router instance at 'DIRECTORY'/;
--replace_regex $_bootstrap_paths_and_ports_replace;
--error 1,2,256
exec $MYSQLROUTER_BOOTSTRAP 
   -B admin:admin_pass@127.0.0.1:$port
   --directory $MYSQL_TMP_DIR/bootstrap_folder/
   --conf-set-option=DEFAULT.plugin_folder=$ROUTER_PLUGIN_DIRECTORY
   --mrs --mrs-global-secret something_important 2>&1;

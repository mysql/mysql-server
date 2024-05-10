## Script xplugin_preamble.inc
#
#  $do_not_wait_for_x_interface -
#    set for which interfaces the script shouldn't wait
#    In default the script wait for both socket and tcp
#    on *NIX OS-es and only for TCP on windows
#  $dont_load_mysqlx_cleaner_plugin -
#    define if the script should install the mysqlx_cleaner plugin
#    or do only pre-checks
#  $dont_load_mysqlx_plugin -
#    define if the script should install mysqlx plugin
#    or do only pre-checks
#  $xplugin_disable_ssl_for_x_at_client_side -
#    old tests set this flag, to mark that all X connection
#    should send unencrpted data (user/test that doesn't set this flag
#    must be able to use X Plugin without first caching mysql accounts).
#
## Usage
#
# --let $wait_for_tcpsocket_status = valid_value|undefined_value|no_wait
# --let $wait_for_unixsocket_status = valid_value|undefined_value|no_wait
# --source include/xplugin_preamble.inc
#

source have_mysqlx_plugin.inc;
source xplugin_reset_and_wait.inc;
source mysqlxtest_macro_dir.inc;

--disable_query_log
--disable_result_log
TRUNCATE TABLE performance_schema.events_waits_history_long;
--enable_result_log
--enable_query_log

if (!$xplugin_disable_ssl_for_x_at_client_side)
{
--let MYSQLXTEST=$MYSQLXTEST --ssl-mode=REQUIRED
}

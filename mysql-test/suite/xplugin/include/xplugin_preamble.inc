## Preamble

source include/have_mysqlx_plugin.inc;
source include/not_embedded.inc;

if ( $dont_load_plugin != 1)
{
    --replace_regex /\.dll/.so/
    --eval install plugin mysqlx soname "$MYSQLXPLUGIN"
}

call mtr.add_suppression("Plugin mysqlx reported: .Failed at SSL configuration: .SSL context is not usable without certificate and private key..");
call mtr.add_suppression("Plugin mysqlx reported: .SSL_CTX_load_verify_locations failed.");
--disable_query_log
--disable_result_log
TRUNCATE TABLE performance_schema.events_waits_history_long;
--enable_result_log
--enable_query_log

## Script xplugin_ssl_warnings_suppression.inc

--disable_query_log
--disable_result_log
call mtr.add_suppression("Plugin mysqlx reported: .Failed at SSL configuration: .SSL context is not usable without certificate and private key..");
call mtr.add_suppression("Plugin mysqlx reported: .SSL_CTX_load_verify_locations failed.");
--enable_result_log
--enable_query_log

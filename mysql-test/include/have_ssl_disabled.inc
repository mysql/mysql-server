-- require r/have_ssl_disabled.require
disable_query_log;
show variables like 'have_ssl';
enable_query_log;

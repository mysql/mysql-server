-- require r/have_ssl.require
disable_query_log;
show variables like 'have_ssl';
enable_query_log;

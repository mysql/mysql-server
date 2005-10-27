-- require r/have_innodb.require
disable_query_log;
show variables like "have_innodb";
enable_query_log;

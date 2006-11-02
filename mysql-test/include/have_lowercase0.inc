--require r/lowercase0.require
--disable_query_log
show variables like "lower_case_%";
--enable_query_log

-- require r/have_response_time_distribution.require
disable_query_log;
show variables like 'have_response_time_distribution';
enable_query_log;

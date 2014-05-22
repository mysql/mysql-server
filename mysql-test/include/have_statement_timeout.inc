-- require r/have_statement_timeout.require
disable_query_log;
show variables like 'have_statement_timeout';
enable_query_log;

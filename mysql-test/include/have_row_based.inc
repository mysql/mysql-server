-- require r/have_row_based.require
disable_query_log;
show variables like "have_row_based_replication";
enable_query_log;

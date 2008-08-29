-- require r/have_partition.require
disable_query_log;
show variables like 'have_partitioning';
enable_query_log;

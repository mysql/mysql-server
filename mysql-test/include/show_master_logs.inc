# show master logs

# mask out the binlog position
-- replace_column 2 #
query_vertical show master logs;

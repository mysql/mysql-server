# show binary logs

# mask out the binlog position and the encrypted columns
-- replace_column 2 # 3 #
query_vertical show binary logs;

# show binary logs

# mask out the binlog position
-- replace_column 2 # 3 #
show binary logs;

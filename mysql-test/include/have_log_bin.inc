# ==== Purpose ====
# 
# Ensure that the server is running with binlogging on and reset the
# binlog.
#
# ==== Usage ====
#
# source include/have_log_bin.inc;

-- require r/have_log_bin.require
disable_query_log;
show variables like "log_bin";
RESET MASTER;
enable_query_log;

# ==== Purpose ====
# 
# Ensure that the server is running with binlogging on
#
# ==== Usage ====
#
# source include/have_log_bin.inc;

-- require r/have_log_bin.require
disable_query_log;
show variables like 'log_bin';
enable_query_log;

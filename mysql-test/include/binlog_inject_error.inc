#
# === Name
#
# binlog_inject_error.inc
#
# === Description
#
# Inject binlog write error when running the query, verifies that the
# query is ended with the proper error (ER_ERROR_ON_WRITE).
#
# === Usage
#
# let query= 'CREATE TABLE t1 (a INT)';
# source include/binlog_inject_error.inc;
#

SET GLOBAL debug='d,injecting_fault_writing';
--echo $query;
--replace_regex /(errno: .*)/(errno: #)/
--error ER_ERROR_ON_WRITE
--eval $query
SET GLOBAL debug='';

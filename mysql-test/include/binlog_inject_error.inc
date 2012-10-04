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

--disable_query_log
SET @old_debug = @@global.debug;
SET GLOBAL debug=CONCAT(IF(LENGTH(@old_debug) > 9, CONCAT(@old_debug, ':'), ''), 'd,injecting_fault_writing');
--enable_query_log
--echo $query;
--replace_regex /(errno: .*)/(errno: #)/
--error ER_ERROR_ON_WRITE
--eval $query
--disable_query_log
SET GLOBAL debug=@old_debug;
--enable_query_log

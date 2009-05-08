DROP TABLE IF EXISTS t1;
'--- check if log file is rotated after 4096 bytes ----'
SET @saved_max_binlog_size= @@global.max_binlog_size;
SET @@global.max_binlog_size = 4096;
CREATE TABLE t1(a CHAR(5));
SELECT COUNT(*) FROM t1;
COUNT(*)
50
'mylog.000002 exists'
SET @@global.max_binlog_size= @saved_max_binlog_size;
DROP TABLE t1;

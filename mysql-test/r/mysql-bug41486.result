DROP TABLE IF EXISTS t1;
SET @old_max_allowed_packet= @@global.max_allowed_packet;
SET @@global.max_allowed_packet = 2 * 1024 * 1024 + 1024;
CREATE TABLE t1(data LONGBLOB);
INSERT INTO t1 SELECT REPEAT('1', 2*1024*1024);
SET @old_general_log = @@global.general_log;
SET @@global.general_log = 0;
SET @@global.general_log = @old_general_log;
SELECT LENGTH(data) FROM t1;
LENGTH(data)
2097152
DROP TABLE t1;
SET @@global.max_allowed_packet = @old_max_allowed_packet;

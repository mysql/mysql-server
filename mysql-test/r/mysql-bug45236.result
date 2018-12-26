DROP TABLE IF EXISTS t1;
SET @old_max_allowed_packet= @@global.max_allowed_packet;
SET @@global.max_allowed_packet = 1024 * 1024 + 1024;
CREATE TABLE t1(data LONGBLOB);
INSERT INTO t1 SELECT CONCAT(REPEAT('1', 1024*1024 - 27), 
"\'\r dummydb dummyhost");
DROP TABLE t1;
SET @@global.max_allowed_packet = @old_max_allowed_packet;

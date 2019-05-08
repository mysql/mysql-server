--source include/have_debug.inc
--source include/have_ndb.inc
--source include/have_binlog_format_mixed_or_row.inc
--source suite/ndb_rpl/ndb_master-slave.inc

SET SESSION DEBUG="+d,ndb_crash_on_drop_SUB_TABLE_DATA";

error 0,1005;
eval CREATE TABLE t (
   pk01 BINARY(192) NOT NULL,
   pk02 BINARY(192) NOT NULL DEFAULT 0,
   pk03 BINARY(192) NOT NULL DEFAULT 0,
   pk04 BINARY(192) NOT NULL DEFAULT 0,
   pk05 BINARY(192) NOT NULL DEFAULT 0,
   pk06 BINARY(192) NOT NULL DEFAULT 0,
   pk07 BINARY(192) NOT NULL DEFAULT 0,
   pk08 BINARY(192) NOT NULL DEFAULT 0,
   pk09 BINARY(192) NOT NULL DEFAULT 0,
   pk10 BINARY(192) NOT NULL DEFAULT 0,
   pk11 BINARY(192) NOT NULL DEFAULT 0,
   pk12 BINARY(192) NOT NULL DEFAULT 0,
   pk13 BINARY(192) NOT NULL DEFAULT 0,
   pk14 BINARY(192) NOT NULL DEFAULT 0,
   pk15 BINARY(192) NOT NULL DEFAULT 0,
   pk16 BINARY(192) NOT NULL DEFAULT 0,
   data VARBINARY($varsize) NOT NULL,
   PRIMARY KEY(pk01,pk02,pk03,pk04,pk05,pk06,pk07,pk08,pk09,pk10,pk11,pk12,pk13,pk14,pk15,pk16))
   ENGINE ndb;
SHOW WARNINGS;

eval INSERT INTO t (pk01, data) VALUES (REPEAT(' ', 192), REPEAT('.', $varsize));
SELECT LENGTH(pk01), CRC32(pk01), LENGTH(data), CRC32(data) FROM t;

eval UPDATE t SET data = repeat(':', $varsize) where pk01 = REPEAT(' ', 192);
SELECT LENGTH(pk01), CRC32(pk01), LENGTH(data), CRC32(data) FROM t;

sync_slave_with_master;
--echo ******* Select from Slave *************
--echo
SELECT LENGTH(pk01), CRC32(pk01), LENGTH(data), CRC32(data) FROM t;

connection default;

DELETE FROM t where pk01 = REPEAT(' ', 192);
SELECT LENGTH(pk01), CRC32(pk01), LENGTH(data), CRC32(data) FROM t;

sync_slave_with_master;
--echo ******* Select from Slave *************
--echo
SELECT LENGTH(pk01), CRC32(pk01), LENGTH(data), CRC32(data) FROM t;

connection default;
SET SESSION DEBUG="-d,ndb_crash_on_drop_SUB_TABLE_DATA";

connection master;
DROP TABLE t;

sync_slave_with_master;
--source include/rpl_end.inc

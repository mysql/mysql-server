include/master-slave.inc
Warnings:
Note	####	Sending passwords in plain text without SSL/TLS is extremely insecure.
Note	####	Storing MySQL user name or password information in the master info repository is not secure and is therefore not recommended. Please consider using the USER and PASSWORD connection options for START SLAVE; see the 'START SLAVE Syntax' in the MySQL Manual for more information.
[connection master]
#### Initialize ####
SET sql_log_bin = 0;
CREATE TABLE algorithms (
id INT AUTO_INCREMENT PRIMARY KEY,
name TEXT NOT NULL);
INSERT INTO algorithms(name) VALUES
('default'),
('zlib'),
('zstd'),
('uncompressed'),
('zlib,zstd'),
('zlib,uncompressed'),
('zstd,uncompressed'),
('zlib,zstd,uncompressed');
CREATE TABLE levels (
id INT AUTO_INCREMENT PRIMARY KEY COMMENT 'order in which to set option',
level TEXT NOT NULL COMMENT 'compression level',
position INT NOT NULL COMMENT 'expected ordinal position of achieved compression'
);
INSERT INTO levels(level, position) VALUES
#  ('1', 1),
#('2', 2),
('default', 3),
#('3', 3),
#('13', 4),
('22', 5);
CREATE FUNCTION set_intersection(set1 TEXT, set2 TEXT) RETURNS TEXT BEGIN
DECLARE ret TEXT DEFAULT '';
DECLARE elem TEXT;
WHILE set1 != '' DO
SET elem = SUBSTRING_INDEX(set1, ',', 1);
IF FIND_IN_SET(elem, set2) THEN
SET ret = sys.list_add(ret, elem);
SET set2 = sys.list_drop(set2, elem);
END IF;
SET set1 = sys.list_drop(set1, elem);
END WHILE;
RETURN ret;
END|
SET sql_log_bin = 1;
CREATE TABLE t (a LONGTEXT);
INSERT INTO t VALUES (REPEAT('a', 1000000));
#### Test ####
[connection master]
==== master:default ====
[connection master]
SET @@global.protocol_compression_algorithms = DEFAULT;
SET @@global.protocol_compression_algorithms = 'zlib,zstd,uncompressed';
SELECT @@global.protocol_compression_algorithms;
@@global.protocol_compression_algorithms
zlib,zstd,uncompressed
[connection master]
[connection slave]
[connection master]
---- master:zlib,zstd,uncompressed slave:default slave_option:0 expected:uncompressed ----
SET @@global.replica_compressed_protocol = 0;
START SLAVE IO_THREAD
include/assert.inc [More than 1000000 bytes should be sent uncompressed]
Warnings:
Warning	1287	'RESET SLAVE' is deprecated and will be removed in a future release. Please use RESET REPLICA instead
---- master:zlib,zstd,uncompressed slave:default slave_option:1 expected:zlib ----
SET @@global.replica_compressed_protocol = 1;
START SLAVE IO_THREAD
include/assert.inc [Less than 1000000 bytes should be sent compressed]
Warnings:
Warning	1287	'RESET SLAVE' is deprecated and will be removed in a future release. Please use RESET REPLICA instead
---- master:zlib,zstd,uncompressed slave:zlib slave_option:0 expected:zlib ----
CHANGE REPLICATION SOURCE TO SOURCE_COMPRESSION_ALGORITHMS = 'zlib';
SET @@global.replica_compressed_protocol = 0;
START SLAVE IO_THREAD
include/assert.inc [Less than 1000000 bytes should be sent compressed]
Warnings:
Warning	1287	'RESET SLAVE' is deprecated and will be removed in a future release. Please use RESET REPLICA instead
---- master:zlib,zstd,uncompressed slave:zstd slave_option:0 expect:zstd level:default ----
CHANGE REPLICATION SOURCE TO SOURCE_COMPRESSION_ALGORITHMS = 'zstd';
SET @@global.replica_compressed_protocol = 0;
START SLAVE IO_THREAD
include/assert.inc [Less than 1000000 bytes should be sent compressed]
Warnings:
Warning	1287	'RESET SLAVE' is deprecated and will be removed in a future release. Please use RESET REPLICA instead
---- master:zlib,zstd,uncompressed slave:zstd slave_option:0 expect:zstd level:22 ----
CHANGE REPLICATION SOURCE TO SOURCE_COMPRESSION_ALGORITHMS = 'zstd';
CHANGE REPLICATION SOURCE TO SOURCE_ZSTD_COMPRESSION_LEVEL = 22;
SET @@global.replica_compressed_protocol = 0;
START SLAVE IO_THREAD
include/assert.inc [Less than 1000000 bytes should be sent compressed]
Warnings:
Warning	1287	'RESET SLAVE' is deprecated and will be removed in a future release. Please use RESET REPLICA instead
include/assert.inc [Compression ratio should be better than previous algorithm]
---- master:zlib,zstd,uncompressed slave:uncompressed slave_option:0 expected:uncompressed ----
CHANGE REPLICATION SOURCE TO SOURCE_COMPRESSION_ALGORITHMS = 'uncompressed';
CHANGE REPLICATION SOURCE TO SOURCE_ZSTD_COMPRESSION_LEVEL = 22;
SET @@global.replica_compressed_protocol = 0;
START SLAVE IO_THREAD
include/assert.inc [More than 1000000 bytes should be sent uncompressed]
Warnings:
Warning	1287	'RESET SLAVE' is deprecated and will be removed in a future release. Please use RESET REPLICA instead
---- master:zlib,zstd,uncompressed slave:uncompressed slave_option:1 expected:zlib ----
CHANGE REPLICATION SOURCE TO SOURCE_COMPRESSION_ALGORITHMS = 'uncompressed';
CHANGE REPLICATION SOURCE TO SOURCE_ZSTD_COMPRESSION_LEVEL = 22;
SET @@global.replica_compressed_protocol = 1;
START SLAVE IO_THREAD
include/assert.inc [Less than 1000000 bytes should be sent compressed]
Warnings:
Warning	1287	'RESET SLAVE' is deprecated and will be removed in a future release. Please use RESET REPLICA instead
---- master:zlib,zstd,uncompressed slave:zlib,zstd slave_option:0 expected:zlib ----
CHANGE REPLICATION SOURCE TO SOURCE_COMPRESSION_ALGORITHMS = 'zlib,zstd';
CHANGE REPLICATION SOURCE TO SOURCE_ZSTD_COMPRESSION_LEVEL = 22;
SET @@global.replica_compressed_protocol = 0;
START SLAVE IO_THREAD
include/assert.inc [Less than 1000000 bytes should be sent compressed]
Warnings:
Warning	1287	'RESET SLAVE' is deprecated and will be removed in a future release. Please use RESET REPLICA instead
---- master:zlib,zstd,uncompressed slave:zlib,uncompressed slave_option:0 expected:zlib ----
CHANGE REPLICATION SOURCE TO SOURCE_COMPRESSION_ALGORITHMS = 'zlib,uncompressed';
CHANGE REPLICATION SOURCE TO SOURCE_ZSTD_COMPRESSION_LEVEL = 22;
SET @@global.replica_compressed_protocol = 0;
START SLAVE IO_THREAD
include/assert.inc [Less than 1000000 bytes should be sent compressed]
Warnings:
Warning	1287	'RESET SLAVE' is deprecated and will be removed in a future release. Please use RESET REPLICA instead
---- master:zlib,zstd,uncompressed slave:zstd,uncompressed slave_option:0 expect:zstd level:default ----
CHANGE REPLICATION SOURCE TO SOURCE_COMPRESSION_ALGORITHMS = 'zstd,uncompressed';
SET @@global.replica_compressed_protocol = 0;
START SLAVE IO_THREAD
include/assert.inc [Less than 1000000 bytes should be sent compressed]
Warnings:
Warning	1287	'RESET SLAVE' is deprecated and will be removed in a future release. Please use RESET REPLICA instead
---- master:zlib,zstd,uncompressed slave:zstd,uncompressed slave_option:0 expect:zstd level:22 ----
CHANGE REPLICATION SOURCE TO SOURCE_COMPRESSION_ALGORITHMS = 'zstd,uncompressed';
CHANGE REPLICATION SOURCE TO SOURCE_ZSTD_COMPRESSION_LEVEL = 22;
SET @@global.replica_compressed_protocol = 0;
START SLAVE IO_THREAD
include/assert.inc [Less than 1000000 bytes should be sent compressed]
Warnings:
Warning	1287	'RESET SLAVE' is deprecated and will be removed in a future release. Please use RESET REPLICA instead
include/assert.inc [Compression ratio should be better than previous algorithm]
---- master:zlib,zstd,uncompressed slave:zlib,zstd,uncompressed slave_option:0 expected:zlib ----
CHANGE REPLICATION SOURCE TO SOURCE_COMPRESSION_ALGORITHMS = 'zlib,zstd,uncompressed';
CHANGE REPLICATION SOURCE TO SOURCE_ZSTD_COMPRESSION_LEVEL = 22;
SET @@global.replica_compressed_protocol = 0;
START SLAVE IO_THREAD
include/assert.inc [Less than 1000000 bytes should be sent compressed]
Warnings:
Warning	1287	'RESET SLAVE' is deprecated and will be removed in a future release. Please use RESET REPLICA instead
==== master:zlib ====
SET @@global.protocol_compression_algorithms = 'zlib';
SELECT @@global.protocol_compression_algorithms;
@@global.protocol_compression_algorithms
zlib
---- master:zlib slave:default slave_option:0 expect:error ----
CHANGE REPLICATION SOURCE TO SOURCE_ZSTD_COMPRESSION_LEVEL = 22;
SET @@global.replica_compressed_protocol = 0;
START SLAVE IO_THREAD;
Warnings:
Warning	1287	'START SLAVE' is deprecated and will be removed in a future release. Please use START REPLICA instead
got error
RESET SLAVE ALL;
Warnings:
Warning	1287	'RESET SLAVE' is deprecated and will be removed in a future release. Please use RESET REPLICA instead
---- master:zlib slave:default slave_option:1 expected:zlib ----
CHANGE REPLICATION SOURCE TO SOURCE_ZSTD_COMPRESSION_LEVEL = 22;
SET @@global.replica_compressed_protocol = 1;
START SLAVE IO_THREAD
include/assert.inc [Less than 1000000 bytes should be sent compressed]
Warnings:
Warning	1287	'RESET SLAVE' is deprecated and will be removed in a future release. Please use RESET REPLICA instead
---- master:zlib slave:zlib slave_option:0 expected:zlib ----
CHANGE REPLICATION SOURCE TO SOURCE_COMPRESSION_ALGORITHMS = 'zlib';
CHANGE REPLICATION SOURCE TO SOURCE_ZSTD_COMPRESSION_LEVEL = 22;
SET @@global.replica_compressed_protocol = 0;
START SLAVE IO_THREAD
include/assert.inc [Less than 1000000 bytes should be sent compressed]
Warnings:
Warning	1287	'RESET SLAVE' is deprecated and will be removed in a future release. Please use RESET REPLICA instead
---- master:zlib slave:zstd slave_option:0 expect:error ----
CHANGE REPLICATION SOURCE TO SOURCE_COMPRESSION_ALGORITHMS = 'zstd';
CHANGE REPLICATION SOURCE TO SOURCE_ZSTD_COMPRESSION_LEVEL = 22;
SET @@global.replica_compressed_protocol = 0;
START SLAVE IO_THREAD;
Warnings:
Warning	1287	'START SLAVE' is deprecated and will be removed in a future release. Please use START REPLICA instead
got error
RESET SLAVE ALL;
Warnings:
Warning	1287	'RESET SLAVE' is deprecated and will be removed in a future release. Please use RESET REPLICA instead
---- master:zlib slave:uncompressed slave_option:0 expect:error ----
CHANGE REPLICATION SOURCE TO SOURCE_COMPRESSION_ALGORITHMS = 'uncompressed';
CHANGE REPLICATION SOURCE TO SOURCE_ZSTD_COMPRESSION_LEVEL = 22;
SET @@global.replica_compressed_protocol = 0;
START SLAVE IO_THREAD;
Warnings:
Warning	1287	'START SLAVE' is deprecated and will be removed in a future release. Please use START REPLICA instead
got error
RESET SLAVE ALL;
Warnings:
Warning	1287	'RESET SLAVE' is deprecated and will be removed in a future release. Please use RESET REPLICA instead
---- master:zlib slave:uncompressed slave_option:1 expected:zlib ----
CHANGE REPLICATION SOURCE TO SOURCE_COMPRESSION_ALGORITHMS = 'uncompressed';
CHANGE REPLICATION SOURCE TO SOURCE_ZSTD_COMPRESSION_LEVEL = 22;
SET @@global.replica_compressed_protocol = 1;
START SLAVE IO_THREAD
include/assert.inc [Less than 1000000 bytes should be sent compressed]
Warnings:
Warning	1287	'RESET SLAVE' is deprecated and will be removed in a future release. Please use RESET REPLICA instead
---- master:zlib slave:zlib,zstd slave_option:0 expected:zlib ----
CHANGE REPLICATION SOURCE TO SOURCE_COMPRESSION_ALGORITHMS = 'zlib,zstd';
CHANGE REPLICATION SOURCE TO SOURCE_ZSTD_COMPRESSION_LEVEL = 22;
SET @@global.replica_compressed_protocol = 0;
START SLAVE IO_THREAD
include/assert.inc [Less than 1000000 bytes should be sent compressed]
Warnings:
Warning	1287	'RESET SLAVE' is deprecated and will be removed in a future release. Please use RESET REPLICA instead
---- master:zlib slave:zlib,uncompressed slave_option:0 expected:zlib ----
CHANGE REPLICATION SOURCE TO SOURCE_COMPRESSION_ALGORITHMS = 'zlib,uncompressed';
CHANGE REPLICATION SOURCE TO SOURCE_ZSTD_COMPRESSION_LEVEL = 22;
SET @@global.replica_compressed_protocol = 0;
START SLAVE IO_THREAD
include/assert.inc [Less than 1000000 bytes should be sent compressed]
Warnings:
Warning	1287	'RESET SLAVE' is deprecated and will be removed in a future release. Please use RESET REPLICA instead
---- master:zlib slave:zstd,uncompressed slave_option:0 expect:error ----
CHANGE REPLICATION SOURCE TO SOURCE_COMPRESSION_ALGORITHMS = 'zstd,uncompressed';
CHANGE REPLICATION SOURCE TO SOURCE_ZSTD_COMPRESSION_LEVEL = 22;
SET @@global.replica_compressed_protocol = 0;
START SLAVE IO_THREAD;
Warnings:
Warning	1287	'START SLAVE' is deprecated and will be removed in a future release. Please use START REPLICA instead
got error
RESET SLAVE ALL;
Warnings:
Warning	1287	'RESET SLAVE' is deprecated and will be removed in a future release. Please use RESET REPLICA instead
---- master:zlib slave:zlib,zstd,uncompressed slave_option:0 expected:zlib ----
CHANGE REPLICATION SOURCE TO SOURCE_COMPRESSION_ALGORITHMS = 'zlib,zstd,uncompressed';
CHANGE REPLICATION SOURCE TO SOURCE_ZSTD_COMPRESSION_LEVEL = 22;
SET @@global.replica_compressed_protocol = 0;
START SLAVE IO_THREAD
include/assert.inc [Less than 1000000 bytes should be sent compressed]
Warnings:
Warning	1287	'RESET SLAVE' is deprecated and will be removed in a future release. Please use RESET REPLICA instead
==== master:zstd ====
SET @@global.protocol_compression_algorithms = 'zstd';
SELECT @@global.protocol_compression_algorithms;
@@global.protocol_compression_algorithms
zstd
---- master:zstd slave:default slave_option:0 expect:error ----
CHANGE REPLICATION SOURCE TO SOURCE_ZSTD_COMPRESSION_LEVEL = 22;
SET @@global.replica_compressed_protocol = 0;
START SLAVE IO_THREAD;
Warnings:
Warning	1287	'START SLAVE' is deprecated and will be removed in a future release. Please use START REPLICA instead
got error
RESET SLAVE ALL;
Warnings:
Warning	1287	'RESET SLAVE' is deprecated and will be removed in a future release. Please use RESET REPLICA instead
---- master:zstd slave:default slave_option:1 expect:error ----
CHANGE REPLICATION SOURCE TO SOURCE_ZSTD_COMPRESSION_LEVEL = 22;
SET @@global.replica_compressed_protocol = 1;
START SLAVE IO_THREAD;
Warnings:
Warning	1287	'START SLAVE' is deprecated and will be removed in a future release. Please use START REPLICA instead
got error
RESET SLAVE ALL;
Warnings:
Warning	1287	'RESET SLAVE' is deprecated and will be removed in a future release. Please use RESET REPLICA instead
---- master:zstd slave:zlib slave_option:0 expect:error ----
CHANGE REPLICATION SOURCE TO SOURCE_COMPRESSION_ALGORITHMS = 'zlib';
CHANGE REPLICATION SOURCE TO SOURCE_ZSTD_COMPRESSION_LEVEL = 22;
SET @@global.replica_compressed_protocol = 0;
START SLAVE IO_THREAD;
Warnings:
Warning	1287	'START SLAVE' is deprecated and will be removed in a future release. Please use START REPLICA instead
got error
RESET SLAVE ALL;
Warnings:
Warning	1287	'RESET SLAVE' is deprecated and will be removed in a future release. Please use RESET REPLICA instead
---- master:zstd slave:zstd slave_option:0 expect:zstd level:default ----
CHANGE REPLICATION SOURCE TO SOURCE_COMPRESSION_ALGORITHMS = 'zstd';
SET @@global.replica_compressed_protocol = 0;
START SLAVE IO_THREAD
include/assert.inc [Less than 1000000 bytes should be sent compressed]
Warnings:
Warning	1287	'RESET SLAVE' is deprecated and will be removed in a future release. Please use RESET REPLICA instead
---- master:zstd slave:zstd slave_option:0 expect:zstd level:22 ----
CHANGE REPLICATION SOURCE TO SOURCE_COMPRESSION_ALGORITHMS = 'zstd';
CHANGE REPLICATION SOURCE TO SOURCE_ZSTD_COMPRESSION_LEVEL = 22;
SET @@global.replica_compressed_protocol = 0;
START SLAVE IO_THREAD
include/assert.inc [Less than 1000000 bytes should be sent compressed]
Warnings:
Warning	1287	'RESET SLAVE' is deprecated and will be removed in a future release. Please use RESET REPLICA instead
include/assert.inc [Compression ratio should be better than previous algorithm]
---- master:zstd slave:uncompressed slave_option:0 expect:error ----
CHANGE REPLICATION SOURCE TO SOURCE_COMPRESSION_ALGORITHMS = 'uncompressed';
CHANGE REPLICATION SOURCE TO SOURCE_ZSTD_COMPRESSION_LEVEL = 22;
SET @@global.replica_compressed_protocol = 0;
START SLAVE IO_THREAD;
Warnings:
Warning	1287	'START SLAVE' is deprecated and will be removed in a future release. Please use START REPLICA instead
got error
RESET SLAVE ALL;
Warnings:
Warning	1287	'RESET SLAVE' is deprecated and will be removed in a future release. Please use RESET REPLICA instead
---- master:zstd slave:uncompressed slave_option:1 expect:error ----
CHANGE REPLICATION SOURCE TO SOURCE_COMPRESSION_ALGORITHMS = 'uncompressed';
CHANGE REPLICATION SOURCE TO SOURCE_ZSTD_COMPRESSION_LEVEL = 22;
SET @@global.replica_compressed_protocol = 1;
START SLAVE IO_THREAD;
Warnings:
Warning	1287	'START SLAVE' is deprecated and will be removed in a future release. Please use START REPLICA instead
got error
RESET SLAVE ALL;
Warnings:
Warning	1287	'RESET SLAVE' is deprecated and will be removed in a future release. Please use RESET REPLICA instead
---- master:zstd slave:zlib,zstd slave_option:0 expect:zstd level:default ----
CHANGE REPLICATION SOURCE TO SOURCE_COMPRESSION_ALGORITHMS = 'zlib,zstd';
SET @@global.replica_compressed_protocol = 0;
START SLAVE IO_THREAD
include/assert.inc [Less than 1000000 bytes should be sent compressed]
Warnings:
Warning	1287	'RESET SLAVE' is deprecated and will be removed in a future release. Please use RESET REPLICA instead
---- master:zstd slave:zlib,zstd slave_option:0 expect:zstd level:22 ----
CHANGE REPLICATION SOURCE TO SOURCE_COMPRESSION_ALGORITHMS = 'zlib,zstd';
CHANGE REPLICATION SOURCE TO SOURCE_ZSTD_COMPRESSION_LEVEL = 22;
SET @@global.replica_compressed_protocol = 0;
START SLAVE IO_THREAD
include/assert.inc [Less than 1000000 bytes should be sent compressed]
Warnings:
Warning	1287	'RESET SLAVE' is deprecated and will be removed in a future release. Please use RESET REPLICA instead
include/assert.inc [Compression ratio should be better than previous algorithm]
---- master:zstd slave:zlib,uncompressed slave_option:0 expect:error ----
CHANGE REPLICATION SOURCE TO SOURCE_COMPRESSION_ALGORITHMS = 'zlib,uncompressed';
CHANGE REPLICATION SOURCE TO SOURCE_ZSTD_COMPRESSION_LEVEL = 22;
SET @@global.replica_compressed_protocol = 0;
START SLAVE IO_THREAD;
Warnings:
Warning	1287	'START SLAVE' is deprecated and will be removed in a future release. Please use START REPLICA instead
got error
RESET SLAVE ALL;
Warnings:
Warning	1287	'RESET SLAVE' is deprecated and will be removed in a future release. Please use RESET REPLICA instead
---- master:zstd slave:zstd,uncompressed slave_option:0 expect:zstd level:default ----
CHANGE REPLICATION SOURCE TO SOURCE_COMPRESSION_ALGORITHMS = 'zstd,uncompressed';
SET @@global.replica_compressed_protocol = 0;
START SLAVE IO_THREAD
include/assert.inc [Less than 1000000 bytes should be sent compressed]
Warnings:
Warning	1287	'RESET SLAVE' is deprecated and will be removed in a future release. Please use RESET REPLICA instead
---- master:zstd slave:zstd,uncompressed slave_option:0 expect:zstd level:22 ----
CHANGE REPLICATION SOURCE TO SOURCE_COMPRESSION_ALGORITHMS = 'zstd,uncompressed';
CHANGE REPLICATION SOURCE TO SOURCE_ZSTD_COMPRESSION_LEVEL = 22;
SET @@global.replica_compressed_protocol = 0;
START SLAVE IO_THREAD
include/assert.inc [Less than 1000000 bytes should be sent compressed]
Warnings:
Warning	1287	'RESET SLAVE' is deprecated and will be removed in a future release. Please use RESET REPLICA instead
include/assert.inc [Compression ratio should be better than previous algorithm]
---- master:zstd slave:zlib,zstd,uncompressed slave_option:0 expect:zstd level:default ----
CHANGE REPLICATION SOURCE TO SOURCE_COMPRESSION_ALGORITHMS = 'zlib,zstd,uncompressed';
SET @@global.replica_compressed_protocol = 0;
START SLAVE IO_THREAD
include/assert.inc [Less than 1000000 bytes should be sent compressed]
Warnings:
Warning	1287	'RESET SLAVE' is deprecated and will be removed in a future release. Please use RESET REPLICA instead
---- master:zstd slave:zlib,zstd,uncompressed slave_option:0 expect:zstd level:22 ----
CHANGE REPLICATION SOURCE TO SOURCE_COMPRESSION_ALGORITHMS = 'zlib,zstd,uncompressed';
CHANGE REPLICATION SOURCE TO SOURCE_ZSTD_COMPRESSION_LEVEL = 22;
SET @@global.replica_compressed_protocol = 0;
START SLAVE IO_THREAD
include/assert.inc [Less than 1000000 bytes should be sent compressed]
Warnings:
Warning	1287	'RESET SLAVE' is deprecated and will be removed in a future release. Please use RESET REPLICA instead
include/assert.inc [Compression ratio should be better than previous algorithm]
==== master:uncompressed ====
SET @@global.protocol_compression_algorithms = 'uncompressed';
SELECT @@global.protocol_compression_algorithms;
@@global.protocol_compression_algorithms
uncompressed
---- master:uncompressed slave:default slave_option:0 expected:uncompressed ----
CHANGE REPLICATION SOURCE TO SOURCE_ZSTD_COMPRESSION_LEVEL = 22;
SET @@global.replica_compressed_protocol = 0;
START SLAVE IO_THREAD
include/assert.inc [More than 1000000 bytes should be sent uncompressed]
Warnings:
Warning	1287	'RESET SLAVE' is deprecated and will be removed in a future release. Please use RESET REPLICA instead
---- master:uncompressed slave:default slave_option:1 expect:error ----
CHANGE REPLICATION SOURCE TO SOURCE_ZSTD_COMPRESSION_LEVEL = 22;
SET @@global.replica_compressed_protocol = 1;
START SLAVE IO_THREAD;
Warnings:
Warning	1287	'START SLAVE' is deprecated and will be removed in a future release. Please use START REPLICA instead
got error
RESET SLAVE ALL;
Warnings:
Warning	1287	'RESET SLAVE' is deprecated and will be removed in a future release. Please use RESET REPLICA instead
---- master:uncompressed slave:zlib slave_option:0 expect:error ----
CHANGE REPLICATION SOURCE TO SOURCE_COMPRESSION_ALGORITHMS = 'zlib';
CHANGE REPLICATION SOURCE TO SOURCE_ZSTD_COMPRESSION_LEVEL = 22;
SET @@global.replica_compressed_protocol = 0;
START SLAVE IO_THREAD;
Warnings:
Warning	1287	'START SLAVE' is deprecated and will be removed in a future release. Please use START REPLICA instead
got error
RESET SLAVE ALL;
Warnings:
Warning	1287	'RESET SLAVE' is deprecated and will be removed in a future release. Please use RESET REPLICA instead
---- master:uncompressed slave:zstd slave_option:0 expect:error ----
CHANGE REPLICATION SOURCE TO SOURCE_COMPRESSION_ALGORITHMS = 'zstd';
CHANGE REPLICATION SOURCE TO SOURCE_ZSTD_COMPRESSION_LEVEL = 22;
SET @@global.replica_compressed_protocol = 0;
START SLAVE IO_THREAD;
Warnings:
Warning	1287	'START SLAVE' is deprecated and will be removed in a future release. Please use START REPLICA instead
got error
RESET SLAVE ALL;
Warnings:
Warning	1287	'RESET SLAVE' is deprecated and will be removed in a future release. Please use RESET REPLICA instead
---- master:uncompressed slave:uncompressed slave_option:0 expected:uncompressed ----
CHANGE REPLICATION SOURCE TO SOURCE_COMPRESSION_ALGORITHMS = 'uncompressed';
CHANGE REPLICATION SOURCE TO SOURCE_ZSTD_COMPRESSION_LEVEL = 22;
SET @@global.replica_compressed_protocol = 0;
START SLAVE IO_THREAD
include/assert.inc [More than 1000000 bytes should be sent uncompressed]
Warnings:
Warning	1287	'RESET SLAVE' is deprecated and will be removed in a future release. Please use RESET REPLICA instead
---- master:uncompressed slave:uncompressed slave_option:1 expect:error ----
CHANGE REPLICATION SOURCE TO SOURCE_COMPRESSION_ALGORITHMS = 'uncompressed';
CHANGE REPLICATION SOURCE TO SOURCE_ZSTD_COMPRESSION_LEVEL = 22;
SET @@global.replica_compressed_protocol = 1;
START SLAVE IO_THREAD;
Warnings:
Warning	1287	'START SLAVE' is deprecated and will be removed in a future release. Please use START REPLICA instead
got error
RESET SLAVE ALL;
Warnings:
Warning	1287	'RESET SLAVE' is deprecated and will be removed in a future release. Please use RESET REPLICA instead
---- master:uncompressed slave:zlib,zstd slave_option:0 expect:error ----
CHANGE REPLICATION SOURCE TO SOURCE_COMPRESSION_ALGORITHMS = 'zlib,zstd';
CHANGE REPLICATION SOURCE TO SOURCE_ZSTD_COMPRESSION_LEVEL = 22;
SET @@global.replica_compressed_protocol = 0;
START SLAVE IO_THREAD;
Warnings:
Warning	1287	'START SLAVE' is deprecated and will be removed in a future release. Please use START REPLICA instead
got error
RESET SLAVE ALL;
Warnings:
Warning	1287	'RESET SLAVE' is deprecated and will be removed in a future release. Please use RESET REPLICA instead
---- master:uncompressed slave:zlib,uncompressed slave_option:0 expected:uncompressed ----
CHANGE REPLICATION SOURCE TO SOURCE_COMPRESSION_ALGORITHMS = 'zlib,uncompressed';
CHANGE REPLICATION SOURCE TO SOURCE_ZSTD_COMPRESSION_LEVEL = 22;
SET @@global.replica_compressed_protocol = 0;
START SLAVE IO_THREAD
include/assert.inc [More than 1000000 bytes should be sent uncompressed]
Warnings:
Warning	1287	'RESET SLAVE' is deprecated and will be removed in a future release. Please use RESET REPLICA instead
---- master:uncompressed slave:zstd,uncompressed slave_option:0 expected:uncompressed ----
CHANGE REPLICATION SOURCE TO SOURCE_COMPRESSION_ALGORITHMS = 'zstd,uncompressed';
CHANGE REPLICATION SOURCE TO SOURCE_ZSTD_COMPRESSION_LEVEL = 22;
SET @@global.replica_compressed_protocol = 0;
START SLAVE IO_THREAD
include/assert.inc [More than 1000000 bytes should be sent uncompressed]
Warnings:
Warning	1287	'RESET SLAVE' is deprecated and will be removed in a future release. Please use RESET REPLICA instead
---- master:uncompressed slave:zlib,zstd,uncompressed slave_option:0 expected:uncompressed ----
CHANGE REPLICATION SOURCE TO SOURCE_COMPRESSION_ALGORITHMS = 'zlib,zstd,uncompressed';
CHANGE REPLICATION SOURCE TO SOURCE_ZSTD_COMPRESSION_LEVEL = 22;
SET @@global.replica_compressed_protocol = 0;
START SLAVE IO_THREAD
include/assert.inc [More than 1000000 bytes should be sent uncompressed]
Warnings:
Warning	1287	'RESET SLAVE' is deprecated and will be removed in a future release. Please use RESET REPLICA instead
==== master:zlib,zstd ====
SET @@global.protocol_compression_algorithms = 'zlib,zstd';
SELECT @@global.protocol_compression_algorithms;
@@global.protocol_compression_algorithms
zlib,zstd
---- master:zlib,zstd slave:default slave_option:0 expect:error ----
CHANGE REPLICATION SOURCE TO SOURCE_ZSTD_COMPRESSION_LEVEL = 22;
SET @@global.replica_compressed_protocol = 0;
START SLAVE IO_THREAD;
Warnings:
Warning	1287	'START SLAVE' is deprecated and will be removed in a future release. Please use START REPLICA instead
got error
RESET SLAVE ALL;
Warnings:
Warning	1287	'RESET SLAVE' is deprecated and will be removed in a future release. Please use RESET REPLICA instead
---- master:zlib,zstd slave:default slave_option:1 expected:zlib ----
CHANGE REPLICATION SOURCE TO SOURCE_ZSTD_COMPRESSION_LEVEL = 22;
SET @@global.replica_compressed_protocol = 1;
START SLAVE IO_THREAD
include/assert.inc [Less than 1000000 bytes should be sent compressed]
Warnings:
Warning	1287	'RESET SLAVE' is deprecated and will be removed in a future release. Please use RESET REPLICA instead
---- master:zlib,zstd slave:zlib slave_option:0 expected:zlib ----
CHANGE REPLICATION SOURCE TO SOURCE_COMPRESSION_ALGORITHMS = 'zlib';
CHANGE REPLICATION SOURCE TO SOURCE_ZSTD_COMPRESSION_LEVEL = 22;
SET @@global.replica_compressed_protocol = 0;
START SLAVE IO_THREAD
include/assert.inc [Less than 1000000 bytes should be sent compressed]
Warnings:
Warning	1287	'RESET SLAVE' is deprecated and will be removed in a future release. Please use RESET REPLICA instead
---- master:zlib,zstd slave:zstd slave_option:0 expect:zstd level:default ----
CHANGE REPLICATION SOURCE TO SOURCE_COMPRESSION_ALGORITHMS = 'zstd';
SET @@global.replica_compressed_protocol = 0;
START SLAVE IO_THREAD
include/assert.inc [Less than 1000000 bytes should be sent compressed]
Warnings:
Warning	1287	'RESET SLAVE' is deprecated and will be removed in a future release. Please use RESET REPLICA instead
---- master:zlib,zstd slave:zstd slave_option:0 expect:zstd level:22 ----
CHANGE REPLICATION SOURCE TO SOURCE_COMPRESSION_ALGORITHMS = 'zstd';
CHANGE REPLICATION SOURCE TO SOURCE_ZSTD_COMPRESSION_LEVEL = 22;
SET @@global.replica_compressed_protocol = 0;
START SLAVE IO_THREAD
include/assert.inc [Less than 1000000 bytes should be sent compressed]
Warnings:
Warning	1287	'RESET SLAVE' is deprecated and will be removed in a future release. Please use RESET REPLICA instead
include/assert.inc [Compression ratio should be better than previous algorithm]
---- master:zlib,zstd slave:uncompressed slave_option:0 expect:error ----
CHANGE REPLICATION SOURCE TO SOURCE_COMPRESSION_ALGORITHMS = 'uncompressed';
CHANGE REPLICATION SOURCE TO SOURCE_ZSTD_COMPRESSION_LEVEL = 22;
SET @@global.replica_compressed_protocol = 0;
START SLAVE IO_THREAD;
Warnings:
Warning	1287	'START SLAVE' is deprecated and will be removed in a future release. Please use START REPLICA instead
got error
RESET SLAVE ALL;
Warnings:
Warning	1287	'RESET SLAVE' is deprecated and will be removed in a future release. Please use RESET REPLICA instead
---- master:zlib,zstd slave:uncompressed slave_option:1 expected:zlib ----
CHANGE REPLICATION SOURCE TO SOURCE_COMPRESSION_ALGORITHMS = 'uncompressed';
CHANGE REPLICATION SOURCE TO SOURCE_ZSTD_COMPRESSION_LEVEL = 22;
SET @@global.replica_compressed_protocol = 1;
START SLAVE IO_THREAD
include/assert.inc [Less than 1000000 bytes should be sent compressed]
Warnings:
Warning	1287	'RESET SLAVE' is deprecated and will be removed in a future release. Please use RESET REPLICA instead
---- master:zlib,zstd slave:zlib,zstd slave_option:0 expected:zlib ----
CHANGE REPLICATION SOURCE TO SOURCE_COMPRESSION_ALGORITHMS = 'zlib,zstd';
CHANGE REPLICATION SOURCE TO SOURCE_ZSTD_COMPRESSION_LEVEL = 22;
SET @@global.replica_compressed_protocol = 0;
START SLAVE IO_THREAD
include/assert.inc [Less than 1000000 bytes should be sent compressed]
Warnings:
Warning	1287	'RESET SLAVE' is deprecated and will be removed in a future release. Please use RESET REPLICA instead
---- master:zlib,zstd slave:zlib,uncompressed slave_option:0 expected:zlib ----
CHANGE REPLICATION SOURCE TO SOURCE_COMPRESSION_ALGORITHMS = 'zlib,uncompressed';
CHANGE REPLICATION SOURCE TO SOURCE_ZSTD_COMPRESSION_LEVEL = 22;
SET @@global.replica_compressed_protocol = 0;
START SLAVE IO_THREAD
include/assert.inc [Less than 1000000 bytes should be sent compressed]
Warnings:
Warning	1287	'RESET SLAVE' is deprecated and will be removed in a future release. Please use RESET REPLICA instead
---- master:zlib,zstd slave:zstd,uncompressed slave_option:0 expect:zstd level:default ----
CHANGE REPLICATION SOURCE TO SOURCE_COMPRESSION_ALGORITHMS = 'zstd,uncompressed';
SET @@global.replica_compressed_protocol = 0;
START SLAVE IO_THREAD
include/assert.inc [Less than 1000000 bytes should be sent compressed]
Warnings:
Warning	1287	'RESET SLAVE' is deprecated and will be removed in a future release. Please use RESET REPLICA instead
---- master:zlib,zstd slave:zstd,uncompressed slave_option:0 expect:zstd level:22 ----
CHANGE REPLICATION SOURCE TO SOURCE_COMPRESSION_ALGORITHMS = 'zstd,uncompressed';
CHANGE REPLICATION SOURCE TO SOURCE_ZSTD_COMPRESSION_LEVEL = 22;
SET @@global.replica_compressed_protocol = 0;
START SLAVE IO_THREAD
include/assert.inc [Less than 1000000 bytes should be sent compressed]
Warnings:
Warning	1287	'RESET SLAVE' is deprecated and will be removed in a future release. Please use RESET REPLICA instead
include/assert.inc [Compression ratio should be better than previous algorithm]
---- master:zlib,zstd slave:zlib,zstd,uncompressed slave_option:0 expected:zlib ----
CHANGE REPLICATION SOURCE TO SOURCE_COMPRESSION_ALGORITHMS = 'zlib,zstd,uncompressed';
CHANGE REPLICATION SOURCE TO SOURCE_ZSTD_COMPRESSION_LEVEL = 22;
SET @@global.replica_compressed_protocol = 0;
START SLAVE IO_THREAD
include/assert.inc [Less than 1000000 bytes should be sent compressed]
Warnings:
Warning	1287	'RESET SLAVE' is deprecated and will be removed in a future release. Please use RESET REPLICA instead
==== master:zlib,uncompressed ====
SET @@global.protocol_compression_algorithms = 'zlib,uncompressed';
SELECT @@global.protocol_compression_algorithms;
@@global.protocol_compression_algorithms
zlib,uncompressed
---- master:zlib,uncompressed slave:default slave_option:0 expected:uncompressed ----
CHANGE REPLICATION SOURCE TO SOURCE_ZSTD_COMPRESSION_LEVEL = 22;
SET @@global.replica_compressed_protocol = 0;
START SLAVE IO_THREAD
include/assert.inc [More than 1000000 bytes should be sent uncompressed]
Warnings:
Warning	1287	'RESET SLAVE' is deprecated and will be removed in a future release. Please use RESET REPLICA instead
---- master:zlib,uncompressed slave:default slave_option:1 expected:zlib ----
CHANGE REPLICATION SOURCE TO SOURCE_ZSTD_COMPRESSION_LEVEL = 22;
SET @@global.replica_compressed_protocol = 1;
START SLAVE IO_THREAD
include/assert.inc [Less than 1000000 bytes should be sent compressed]
Warnings:
Warning	1287	'RESET SLAVE' is deprecated and will be removed in a future release. Please use RESET REPLICA instead
---- master:zlib,uncompressed slave:zlib slave_option:0 expected:zlib ----
CHANGE REPLICATION SOURCE TO SOURCE_COMPRESSION_ALGORITHMS = 'zlib';
CHANGE REPLICATION SOURCE TO SOURCE_ZSTD_COMPRESSION_LEVEL = 22;
SET @@global.replica_compressed_protocol = 0;
START SLAVE IO_THREAD
include/assert.inc [Less than 1000000 bytes should be sent compressed]
Warnings:
Warning	1287	'RESET SLAVE' is deprecated and will be removed in a future release. Please use RESET REPLICA instead
---- master:zlib,uncompressed slave:zstd slave_option:0 expect:error ----
CHANGE REPLICATION SOURCE TO SOURCE_COMPRESSION_ALGORITHMS = 'zstd';
CHANGE REPLICATION SOURCE TO SOURCE_ZSTD_COMPRESSION_LEVEL = 22;
SET @@global.replica_compressed_protocol = 0;
START SLAVE IO_THREAD;
Warnings:
Warning	1287	'START SLAVE' is deprecated and will be removed in a future release. Please use START REPLICA instead
got error
RESET SLAVE ALL;
Warnings:
Warning	1287	'RESET SLAVE' is deprecated and will be removed in a future release. Please use RESET REPLICA instead
---- master:zlib,uncompressed slave:uncompressed slave_option:0 expected:uncompressed ----
CHANGE REPLICATION SOURCE TO SOURCE_COMPRESSION_ALGORITHMS = 'uncompressed';
CHANGE REPLICATION SOURCE TO SOURCE_ZSTD_COMPRESSION_LEVEL = 22;
SET @@global.replica_compressed_protocol = 0;
START SLAVE IO_THREAD
include/assert.inc [More than 1000000 bytes should be sent uncompressed]
Warnings:
Warning	1287	'RESET SLAVE' is deprecated and will be removed in a future release. Please use RESET REPLICA instead
---- master:zlib,uncompressed slave:uncompressed slave_option:1 expected:zlib ----
CHANGE REPLICATION SOURCE TO SOURCE_COMPRESSION_ALGORITHMS = 'uncompressed';
CHANGE REPLICATION SOURCE TO SOURCE_ZSTD_COMPRESSION_LEVEL = 22;
SET @@global.replica_compressed_protocol = 1;
START SLAVE IO_THREAD
include/assert.inc [Less than 1000000 bytes should be sent compressed]
Warnings:
Warning	1287	'RESET SLAVE' is deprecated and will be removed in a future release. Please use RESET REPLICA instead
---- master:zlib,uncompressed slave:zlib,zstd slave_option:0 expected:zlib ----
CHANGE REPLICATION SOURCE TO SOURCE_COMPRESSION_ALGORITHMS = 'zlib,zstd';
CHANGE REPLICATION SOURCE TO SOURCE_ZSTD_COMPRESSION_LEVEL = 22;
SET @@global.replica_compressed_protocol = 0;
START SLAVE IO_THREAD
include/assert.inc [Less than 1000000 bytes should be sent compressed]
Warnings:
Warning	1287	'RESET SLAVE' is deprecated and will be removed in a future release. Please use RESET REPLICA instead
---- master:zlib,uncompressed slave:zlib,uncompressed slave_option:0 expected:zlib ----
CHANGE REPLICATION SOURCE TO SOURCE_COMPRESSION_ALGORITHMS = 'zlib,uncompressed';
CHANGE REPLICATION SOURCE TO SOURCE_ZSTD_COMPRESSION_LEVEL = 22;
SET @@global.replica_compressed_protocol = 0;
START SLAVE IO_THREAD
include/assert.inc [Less than 1000000 bytes should be sent compressed]
Warnings:
Warning	1287	'RESET SLAVE' is deprecated and will be removed in a future release. Please use RESET REPLICA instead
---- master:zlib,uncompressed slave:zstd,uncompressed slave_option:0 expected:uncompressed ----
CHANGE REPLICATION SOURCE TO SOURCE_COMPRESSION_ALGORITHMS = 'zstd,uncompressed';
CHANGE REPLICATION SOURCE TO SOURCE_ZSTD_COMPRESSION_LEVEL = 22;
SET @@global.replica_compressed_protocol = 0;
START SLAVE IO_THREAD
include/assert.inc [More than 1000000 bytes should be sent uncompressed]
Warnings:
Warning	1287	'RESET SLAVE' is deprecated and will be removed in a future release. Please use RESET REPLICA instead
---- master:zlib,uncompressed slave:zlib,zstd,uncompressed slave_option:0 expected:zlib ----
CHANGE REPLICATION SOURCE TO SOURCE_COMPRESSION_ALGORITHMS = 'zlib,zstd,uncompressed';
CHANGE REPLICATION SOURCE TO SOURCE_ZSTD_COMPRESSION_LEVEL = 22;
SET @@global.replica_compressed_protocol = 0;
START SLAVE IO_THREAD
include/assert.inc [Less than 1000000 bytes should be sent compressed]
Warnings:
Warning	1287	'RESET SLAVE' is deprecated and will be removed in a future release. Please use RESET REPLICA instead
==== master:zstd,uncompressed ====
SET @@global.protocol_compression_algorithms = 'zstd,uncompressed';
SELECT @@global.protocol_compression_algorithms;
@@global.protocol_compression_algorithms
zstd,uncompressed
---- master:zstd,uncompressed slave:default slave_option:0 expected:uncompressed ----
CHANGE REPLICATION SOURCE TO SOURCE_ZSTD_COMPRESSION_LEVEL = 22;
SET @@global.replica_compressed_protocol = 0;
START SLAVE IO_THREAD
include/assert.inc [More than 1000000 bytes should be sent uncompressed]
Warnings:
Warning	1287	'RESET SLAVE' is deprecated and will be removed in a future release. Please use RESET REPLICA instead
---- master:zstd,uncompressed slave:default slave_option:1 expect:error ----
CHANGE REPLICATION SOURCE TO SOURCE_ZSTD_COMPRESSION_LEVEL = 22;
SET @@global.replica_compressed_protocol = 1;
START SLAVE IO_THREAD;
Warnings:
Warning	1287	'START SLAVE' is deprecated and will be removed in a future release. Please use START REPLICA instead
got error
RESET SLAVE ALL;
Warnings:
Warning	1287	'RESET SLAVE' is deprecated and will be removed in a future release. Please use RESET REPLICA instead
---- master:zstd,uncompressed slave:zlib slave_option:0 expect:error ----
CHANGE REPLICATION SOURCE TO SOURCE_COMPRESSION_ALGORITHMS = 'zlib';
CHANGE REPLICATION SOURCE TO SOURCE_ZSTD_COMPRESSION_LEVEL = 22;
SET @@global.replica_compressed_protocol = 0;
START SLAVE IO_THREAD;
Warnings:
Warning	1287	'START SLAVE' is deprecated and will be removed in a future release. Please use START REPLICA instead
got error
RESET SLAVE ALL;
Warnings:
Warning	1287	'RESET SLAVE' is deprecated and will be removed in a future release. Please use RESET REPLICA instead
---- master:zstd,uncompressed slave:zstd slave_option:0 expect:zstd level:default ----
CHANGE REPLICATION SOURCE TO SOURCE_COMPRESSION_ALGORITHMS = 'zstd';
SET @@global.replica_compressed_protocol = 0;
START SLAVE IO_THREAD
include/assert.inc [Less than 1000000 bytes should be sent compressed]
Warnings:
Warning	1287	'RESET SLAVE' is deprecated and will be removed in a future release. Please use RESET REPLICA instead
---- master:zstd,uncompressed slave:zstd slave_option:0 expect:zstd level:22 ----
CHANGE REPLICATION SOURCE TO SOURCE_COMPRESSION_ALGORITHMS = 'zstd';
CHANGE REPLICATION SOURCE TO SOURCE_ZSTD_COMPRESSION_LEVEL = 22;
SET @@global.replica_compressed_protocol = 0;
START SLAVE IO_THREAD
include/assert.inc [Less than 1000000 bytes should be sent compressed]
Warnings:
Warning	1287	'RESET SLAVE' is deprecated and will be removed in a future release. Please use RESET REPLICA instead
include/assert.inc [Compression ratio should be better than previous algorithm]
---- master:zstd,uncompressed slave:uncompressed slave_option:0 expected:uncompressed ----
CHANGE REPLICATION SOURCE TO SOURCE_COMPRESSION_ALGORITHMS = 'uncompressed';
CHANGE REPLICATION SOURCE TO SOURCE_ZSTD_COMPRESSION_LEVEL = 22;
SET @@global.replica_compressed_protocol = 0;
START SLAVE IO_THREAD
include/assert.inc [More than 1000000 bytes should be sent uncompressed]
Warnings:
Warning	1287	'RESET SLAVE' is deprecated and will be removed in a future release. Please use RESET REPLICA instead
---- master:zstd,uncompressed slave:uncompressed slave_option:1 expect:error ----
CHANGE REPLICATION SOURCE TO SOURCE_COMPRESSION_ALGORITHMS = 'uncompressed';
CHANGE REPLICATION SOURCE TO SOURCE_ZSTD_COMPRESSION_LEVEL = 22;
SET @@global.replica_compressed_protocol = 1;
START SLAVE IO_THREAD;
Warnings:
Warning	1287	'START SLAVE' is deprecated and will be removed in a future release. Please use START REPLICA instead
got error
RESET SLAVE ALL;
Warnings:
Warning	1287	'RESET SLAVE' is deprecated and will be removed in a future release. Please use RESET REPLICA instead
---- master:zstd,uncompressed slave:zlib,zstd slave_option:0 expect:zstd level:default ----
CHANGE REPLICATION SOURCE TO SOURCE_COMPRESSION_ALGORITHMS = 'zlib,zstd';
SET @@global.replica_compressed_protocol = 0;
START SLAVE IO_THREAD
include/assert.inc [Less than 1000000 bytes should be sent compressed]
Warnings:
Warning	1287	'RESET SLAVE' is deprecated and will be removed in a future release. Please use RESET REPLICA instead
---- master:zstd,uncompressed slave:zlib,zstd slave_option:0 expect:zstd level:22 ----
CHANGE REPLICATION SOURCE TO SOURCE_COMPRESSION_ALGORITHMS = 'zlib,zstd';
CHANGE REPLICATION SOURCE TO SOURCE_ZSTD_COMPRESSION_LEVEL = 22;
SET @@global.replica_compressed_protocol = 0;
START SLAVE IO_THREAD
include/assert.inc [Less than 1000000 bytes should be sent compressed]
Warnings:
Warning	1287	'RESET SLAVE' is deprecated and will be removed in a future release. Please use RESET REPLICA instead
include/assert.inc [Compression ratio should be better than previous algorithm]
---- master:zstd,uncompressed slave:zlib,uncompressed slave_option:0 expected:uncompressed ----
CHANGE REPLICATION SOURCE TO SOURCE_COMPRESSION_ALGORITHMS = 'zlib,uncompressed';
CHANGE REPLICATION SOURCE TO SOURCE_ZSTD_COMPRESSION_LEVEL = 22;
SET @@global.replica_compressed_protocol = 0;
START SLAVE IO_THREAD
include/assert.inc [More than 1000000 bytes should be sent uncompressed]
Warnings:
Warning	1287	'RESET SLAVE' is deprecated and will be removed in a future release. Please use RESET REPLICA instead
---- master:zstd,uncompressed slave:zstd,uncompressed slave_option:0 expect:zstd level:default ----
CHANGE REPLICATION SOURCE TO SOURCE_COMPRESSION_ALGORITHMS = 'zstd,uncompressed';
SET @@global.replica_compressed_protocol = 0;
START SLAVE IO_THREAD
include/assert.inc [Less than 1000000 bytes should be sent compressed]
Warnings:
Warning	1287	'RESET SLAVE' is deprecated and will be removed in a future release. Please use RESET REPLICA instead
---- master:zstd,uncompressed slave:zstd,uncompressed slave_option:0 expect:zstd level:22 ----
CHANGE REPLICATION SOURCE TO SOURCE_COMPRESSION_ALGORITHMS = 'zstd,uncompressed';
CHANGE REPLICATION SOURCE TO SOURCE_ZSTD_COMPRESSION_LEVEL = 22;
SET @@global.replica_compressed_protocol = 0;
START SLAVE IO_THREAD
include/assert.inc [Less than 1000000 bytes should be sent compressed]
Warnings:
Warning	1287	'RESET SLAVE' is deprecated and will be removed in a future release. Please use RESET REPLICA instead
include/assert.inc [Compression ratio should be better than previous algorithm]
---- master:zstd,uncompressed slave:zlib,zstd,uncompressed slave_option:0 expect:zstd level:default ----
CHANGE REPLICATION SOURCE TO SOURCE_COMPRESSION_ALGORITHMS = 'zlib,zstd,uncompressed';
SET @@global.replica_compressed_protocol = 0;
START SLAVE IO_THREAD
include/assert.inc [Less than 1000000 bytes should be sent compressed]
Warnings:
Warning	1287	'RESET SLAVE' is deprecated and will be removed in a future release. Please use RESET REPLICA instead
---- master:zstd,uncompressed slave:zlib,zstd,uncompressed slave_option:0 expect:zstd level:22 ----
CHANGE REPLICATION SOURCE TO SOURCE_COMPRESSION_ALGORITHMS = 'zlib,zstd,uncompressed';
CHANGE REPLICATION SOURCE TO SOURCE_ZSTD_COMPRESSION_LEVEL = 22;
SET @@global.replica_compressed_protocol = 0;
START SLAVE IO_THREAD
include/assert.inc [Less than 1000000 bytes should be sent compressed]
Warnings:
Warning	1287	'RESET SLAVE' is deprecated and will be removed in a future release. Please use RESET REPLICA instead
include/assert.inc [Compression ratio should be better than previous algorithm]
==== master:zlib,zstd,uncompressed ====
SET @@global.protocol_compression_algorithms = 'zlib,zstd,uncompressed';
SELECT @@global.protocol_compression_algorithms;
@@global.protocol_compression_algorithms
zlib,zstd,uncompressed
---- master:zlib,zstd,uncompressed slave:default slave_option:0 expected:uncompressed ----
CHANGE REPLICATION SOURCE TO SOURCE_ZSTD_COMPRESSION_LEVEL = 22;
SET @@global.replica_compressed_protocol = 0;
START SLAVE IO_THREAD
include/assert.inc [More than 1000000 bytes should be sent uncompressed]
Warnings:
Warning	1287	'RESET SLAVE' is deprecated and will be removed in a future release. Please use RESET REPLICA instead
---- master:zlib,zstd,uncompressed slave:default slave_option:1 expected:zlib ----
CHANGE REPLICATION SOURCE TO SOURCE_ZSTD_COMPRESSION_LEVEL = 22;
SET @@global.replica_compressed_protocol = 1;
START SLAVE IO_THREAD
include/assert.inc [Less than 1000000 bytes should be sent compressed]
Warnings:
Warning	1287	'RESET SLAVE' is deprecated and will be removed in a future release. Please use RESET REPLICA instead
---- master:zlib,zstd,uncompressed slave:zlib slave_option:0 expected:zlib ----
CHANGE REPLICATION SOURCE TO SOURCE_COMPRESSION_ALGORITHMS = 'zlib';
CHANGE REPLICATION SOURCE TO SOURCE_ZSTD_COMPRESSION_LEVEL = 22;
SET @@global.replica_compressed_protocol = 0;
START SLAVE IO_THREAD
include/assert.inc [Less than 1000000 bytes should be sent compressed]
Warnings:
Warning	1287	'RESET SLAVE' is deprecated and will be removed in a future release. Please use RESET REPLICA instead
---- master:zlib,zstd,uncompressed slave:zstd slave_option:0 expect:zstd level:default ----
CHANGE REPLICATION SOURCE TO SOURCE_COMPRESSION_ALGORITHMS = 'zstd';
SET @@global.replica_compressed_protocol = 0;
START SLAVE IO_THREAD
include/assert.inc [Less than 1000000 bytes should be sent compressed]
Warnings:
Warning	1287	'RESET SLAVE' is deprecated and will be removed in a future release. Please use RESET REPLICA instead
---- master:zlib,zstd,uncompressed slave:zstd slave_option:0 expect:zstd level:22 ----
CHANGE REPLICATION SOURCE TO SOURCE_COMPRESSION_ALGORITHMS = 'zstd';
CHANGE REPLICATION SOURCE TO SOURCE_ZSTD_COMPRESSION_LEVEL = 22;
SET @@global.replica_compressed_protocol = 0;
START SLAVE IO_THREAD
include/assert.inc [Less than 1000000 bytes should be sent compressed]
Warnings:
Warning	1287	'RESET SLAVE' is deprecated and will be removed in a future release. Please use RESET REPLICA instead
include/assert.inc [Compression ratio should be better than previous algorithm]
---- master:zlib,zstd,uncompressed slave:uncompressed slave_option:0 expected:uncompressed ----
CHANGE REPLICATION SOURCE TO SOURCE_COMPRESSION_ALGORITHMS = 'uncompressed';
CHANGE REPLICATION SOURCE TO SOURCE_ZSTD_COMPRESSION_LEVEL = 22;
SET @@global.replica_compressed_protocol = 0;
START SLAVE IO_THREAD
include/assert.inc [More than 1000000 bytes should be sent uncompressed]
Warnings:
Warning	1287	'RESET SLAVE' is deprecated and will be removed in a future release. Please use RESET REPLICA instead
---- master:zlib,zstd,uncompressed slave:uncompressed slave_option:1 expected:zlib ----
CHANGE REPLICATION SOURCE TO SOURCE_COMPRESSION_ALGORITHMS = 'uncompressed';
CHANGE REPLICATION SOURCE TO SOURCE_ZSTD_COMPRESSION_LEVEL = 22;
SET @@global.replica_compressed_protocol = 1;
START SLAVE IO_THREAD
include/assert.inc [Less than 1000000 bytes should be sent compressed]
Warnings:
Warning	1287	'RESET SLAVE' is deprecated and will be removed in a future release. Please use RESET REPLICA instead
---- master:zlib,zstd,uncompressed slave:zlib,zstd slave_option:0 expected:zlib ----
CHANGE REPLICATION SOURCE TO SOURCE_COMPRESSION_ALGORITHMS = 'zlib,zstd';
CHANGE REPLICATION SOURCE TO SOURCE_ZSTD_COMPRESSION_LEVEL = 22;
SET @@global.replica_compressed_protocol = 0;
START SLAVE IO_THREAD
include/assert.inc [Less than 1000000 bytes should be sent compressed]
Warnings:
Warning	1287	'RESET SLAVE' is deprecated and will be removed in a future release. Please use RESET REPLICA instead
---- master:zlib,zstd,uncompressed slave:zlib,uncompressed slave_option:0 expected:zlib ----
CHANGE REPLICATION SOURCE TO SOURCE_COMPRESSION_ALGORITHMS = 'zlib,uncompressed';
CHANGE REPLICATION SOURCE TO SOURCE_ZSTD_COMPRESSION_LEVEL = 22;
SET @@global.replica_compressed_protocol = 0;
START SLAVE IO_THREAD
include/assert.inc [Less than 1000000 bytes should be sent compressed]
Warnings:
Warning	1287	'RESET SLAVE' is deprecated and will be removed in a future release. Please use RESET REPLICA instead
---- master:zlib,zstd,uncompressed slave:zstd,uncompressed slave_option:0 expect:zstd level:default ----
CHANGE REPLICATION SOURCE TO SOURCE_COMPRESSION_ALGORITHMS = 'zstd,uncompressed';
SET @@global.replica_compressed_protocol = 0;
START SLAVE IO_THREAD
include/assert.inc [Less than 1000000 bytes should be sent compressed]
Warnings:
Warning	1287	'RESET SLAVE' is deprecated and will be removed in a future release. Please use RESET REPLICA instead
---- master:zlib,zstd,uncompressed slave:zstd,uncompressed slave_option:0 expect:zstd level:22 ----
CHANGE REPLICATION SOURCE TO SOURCE_COMPRESSION_ALGORITHMS = 'zstd,uncompressed';
CHANGE REPLICATION SOURCE TO SOURCE_ZSTD_COMPRESSION_LEVEL = 22;
SET @@global.replica_compressed_protocol = 0;
START SLAVE IO_THREAD
include/assert.inc [Less than 1000000 bytes should be sent compressed]
Warnings:
Warning	1287	'RESET SLAVE' is deprecated and will be removed in a future release. Please use RESET REPLICA instead
include/assert.inc [Compression ratio should be better than previous algorithm]
---- master:zlib,zstd,uncompressed slave:zlib,zstd,uncompressed slave_option:0 expected:zlib ----
CHANGE REPLICATION SOURCE TO SOURCE_COMPRESSION_ALGORITHMS = 'zlib,zstd,uncompressed';
CHANGE REPLICATION SOURCE TO SOURCE_ZSTD_COMPRESSION_LEVEL = 22;
SET @@global.replica_compressed_protocol = 0;
START SLAVE IO_THREAD
include/assert.inc [Less than 1000000 bytes should be sent compressed]
Warnings:
Warning	1287	'RESET SLAVE' is deprecated and will be removed in a future release. Please use RESET REPLICA instead
#### Clean up ####
SET @@global.protocol_compression_algorithms = "zlib,zstd,uncompressed";;
SET sql_log_bin = 0;
DROP TABLE algorithms;
DROP TABLE levels;
DROP TABLE t;
DROP FUNCTION set_intersection;
SET sql_log_bin = 1;
Warnings:
Note	1759	Sending passwords in plain text without SSL/TLS is extremely insecure.
Note	1760	Storing MySQL user name or password information in the master info repository is not secure and is therefore not recommended. Please consider using the USER and PASSWORD connection options for START SLAVE; see the 'START SLAVE Syntax' in the MySQL Manual for more information.
include/rpl_end.inc
Warnings:
Note	3084	Replication thread(s) for channel '' are already stopped.

set global innodb_compression_level = 0;
CREATE DATABASE d1;
CREATE TABLE d1.t1 (f1 longblob) row_format=compressed;
INSERT INTO d1.t1 VALUES (repeat('x', 11000000));
FLUSH TABLES d1.t1 FOR EXPORT;
backup: t1
UNLOCK TABLES;
drop table d1.t1;
CREATE TABLE d1.t1 (f1 longblob) row_format=compressed;
ALTER TABLE d1.t1 DISCARD TABLESPACE;
restore: t1 .ibd and .cfg files
ALTER TABLE d1.t1 IMPORT TABLESPACE;
drop table d1.t1;
DROP DATABASE d1;
unlink: t1.ibd
unlink: t1.cfg
set global innodb_compression_level = default;

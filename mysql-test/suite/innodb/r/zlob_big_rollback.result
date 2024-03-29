SET GLOBAL innodb_compression_level = 0;
CREATE TABLE t1 (id INT PRIMARY KEY NOT NULL, val LONGTEXT)
ENGINE=InnoDB ROW_FORMAT=compressed;
BEGIN;
INSERT INTO t1 VALUES (1, repeat('a',6000000));
ROLLBACK;
DROP TABLE t1;
SET GLOBAL innodb_compression_level = default;

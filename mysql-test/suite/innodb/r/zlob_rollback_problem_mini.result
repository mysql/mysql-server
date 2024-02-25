SET GLOBAL innodb_compression_level = 0;
CREATE TABLE t1 (
id INT PRIMARY KEY,
val LONGBLOB
) ENGINE=InnoDB ROW_FORMAT=compressed;
INSERT INTO t1 (id,val) VALUES (1,REPEAT('a',13));
BEGIN;
UPDATE t1 SET val = REPEAT('b',1000000) WHERE id=1;
ROLLBACK;
DROP TABLE t1;
SET GLOBAL innodb_compression_level = default;

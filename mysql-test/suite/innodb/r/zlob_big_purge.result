SET GLOBAL innodb_compression_level = 0;
SET debug = '+d,zlob_reduce_chunk_size';
CREATE TABLE t1 (id INT PRIMARY KEY NOT NULL, val LONGTEXT)
ENGINE=InnoDB ROW_FORMAT=COMPRESSED;
INSERT INTO t1 VALUES (1, repeat('a',7000000));
UPDATE t1 SET val = repeat('b',7000000);
DROP TABLE t1;
SET GLOBAL innodb_compression_level = default;
SET debug = '-d,zlob_reduce_chunk_size';

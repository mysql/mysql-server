set global innodb_compression_level = 0;
CREATE TABLE `t1` (
`a` int NOT NULL,
`b` longblob,
`c` longblob,
PRIMARY KEY (`a`)
) ENGINE=InnoDB row_format=compressed;
SET global innodb_purge_stop_now=ON;
DELETE FROM t1 WHERE a = 24;
BEGIN;
INSERT INTO `t1` VALUES(24, repeat("c",89152),repeat("d",89152));
SET global innodb_purge_run_now=ON;
ROLLBACK;
DROP TABLE t1;
set global innodb_compression_level = default;

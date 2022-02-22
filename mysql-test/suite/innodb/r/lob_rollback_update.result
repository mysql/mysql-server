CREATE TABLE `t1` (
`a` int NOT NULL,
`b` text,
`c` text,
PRIMARY KEY (`a`)
) ENGINE=InnoDB;
SET global innodb_purge_stop_now=ON;
DELETE FROM t1 WHERE a = 24;
BEGIN;
INSERT INTO `t1` VALUES(24, repeat("c",49152),repeat("d",49152));
SET global innodb_purge_run_now=ON;
ROLLBACK;
DROP TABLE t1;

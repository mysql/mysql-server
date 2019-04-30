#
# Bug#26938297 - IMPORT OF COMPRESSED TABLE FAILS AND CRASHES THE SERVER
#
CREATE DATABASE testdb;
USE testdb;
CREATE TABLE t1 (
`col_1` char(255) DEFAULT NULL,
`col_2` varchar(255) DEFAULT NULL,
`col_3` longtext,
`col_4` longtext,
`col_5` longtext,
`col_6` longtext,
`col_7` longtext,
`col_8` longtext,
`col_9` longtext,
`col_10` longtext,
`col_11` int(11),
PRIMARY KEY(col_11),
KEY k1(col_1(10))
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 KEY_BLOCK_SIZE=1 STATS_PERSISTENT=0;
Warnings:
Warning	1681	Integer display width is deprecated and will be removed in a future release.
FLUSH TABLES t1  FOR EXPORT;
# Copying exported .ibd
UNLOCK TABLES;
ALTER TABLE t1 DISCARD TABLESPACE;
# Copying exported .ibd to import
ALTER TABLE t1 IMPORT TABLESPACE;
ALTER TABLE t1 DROP INDEX k1;
DROP TABLE t1;
DROP DATABASE testdb;

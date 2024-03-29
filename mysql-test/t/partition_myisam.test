--source include/force_myisam_default.inc
--source include/have_myisam.inc

--echo #
--echo # Bug #27816: Log tables ran with partitions crashes the server when logging
--echo #             is enabled.
--echo #

USE mysql;
TRUNCATE TABLE general_log;
SET @old_general_log_state = @@global.general_log;
SET GLOBAL general_log = 0;
ALTER TABLE general_log ENGINE = MyISAM;
--error ER_WRONG_USAGE
ALTER TABLE general_log PARTITION BY RANGE (TO_DAYS(event_time))
  (PARTITION p0 VALUES LESS THAN (733144), PARTITION p1 VALUES LESS THAN (3000000));
ALTER TABLE general_log ENGINE = CSV;
SET GLOBAL general_log = @old_general_log_state;
USE test;

--echo #
--echo # Bug#31931: Mix of handlers error message
--echo #

--error ER_MIX_HANDLER_ERROR
CREATE TABLE t1 (a INT)
PARTITION BY HASH (a)
( PARTITION p0 ENGINE=MyISAM,
  PARTITION p1);

--error ER_MIX_HANDLER_ERROR
CREATE TABLE t1 (a INT)
PARTITION BY LIST (a)
SUBPARTITION BY HASH (a)
( PARTITION p0 VALUES IN (0)
( SUBPARTITION s0, SUBPARTITION s1 ENGINE=MyISAM, SUBPARTITION s2),
  PARTITION p1 VALUES IN (1)
( SUBPARTITION s3 ENGINE=MyISAM, SUBPARTITION s4, SUBPARTITION s5 ENGINE=MyISAM));

--echo #
--echo # bug#11760213-52599: ALTER TABLE REMOVE PARTITIONING ON NON-PARTITIONED
--echo #                                 TABLE CORRUPTS MYISAM

CREATE TABLE  `t1`(`a` INT)ENGINE=myisam;
ALTER TABLE `t1` ADD COLUMN `b` INT;
CREATE UNIQUE INDEX `i1` ON `t1`(`b`);
CREATE UNIQUE INDEX `i2` ON `t1`(`a`);
ALTER TABLE `t1` ADD PRIMARY KEY  (`a`);

--error ER_PARTITION_MGMT_ON_NONPARTITIONED
ALTER TABLE `t1` REMOVE PARTITIONING;
CHECK TABLE `t1` EXTENDED;
DROP TABLE t1;

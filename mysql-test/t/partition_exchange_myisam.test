--source include/have_myisam.inc
--echo # Tests for WL#4445
CREATE TABLE t (a INT,
  b VARCHAR(55),
  PRIMARY KEY (a)) engine=InnoDB;

CREATE TABLE tp (a INT,
  b VARCHAR(55),
  PRIMARY KEY (a)) engine=InnoDB
PARTITION BY RANGE (a)
(PARTITION p0 VALUES LESS THAN (100),
 PARTITION p1 VALUES LESS THAN MAXVALUE);

INSERT INTO t VALUES (1, "First value"), (3, "Three"), (5, "Five"), (99, "End of values");

INSERT INTO tp VALUES (2, "First value"), (10, "Ten"), (50, "Fifty"), (200, "Two hundred, end of values"), (61, "Sixty one"), (62, "Sixty two"), (63, "Sixty three"), (64, "Sixty four"), (161, "161"), (162, "162"), (163, "163"), (164, "164");

--echo # test different engines
ALTER TABLE t ENGINE = MyISAM;
ALTER TABLE tp ENGINE = InnoDB;
SHOW CREATE TABLE t;
--error ER_MIX_HANDLER_ERROR
ALTER TABLE tp EXCHANGE PARTITION p0 WITH TABLE t;
SHOW CREATE TABLE t;
ALTER TABLE t ENGINE = InnoDB;
DROP TABLE t,tp;

--echo # Test with general_log
use mysql;
SET @old_general_log_state = @@global.general_log;
SET GLOBAL general_log = 0;
ALTER TABLE general_log ENGINE = MyISAM;
CREATE TABLE t LIKE general_log;
ALTER TABLE t ENGINE InnoDB PARTITION BY RANGE (thread_id)
(PARTITION p0 VALUES LESS THAN (123456789),
 PARTITION pMAX VALUES LESS THAN MAXVALUE);
--error ER_WRONG_USAGE
ALTER TABLE t EXCHANGE PARTITION p0 WITH TABLE general_log;
ALTER TABLE general_log ENGINE = CSV;
SET @@global.general_log = @old_general_log_state;
DROP TABLE t;
use test;


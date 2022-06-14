# Tests for WL#4445
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
# test different engines
ALTER TABLE t ENGINE = MyISAM;
ALTER TABLE tp ENGINE = InnoDB;
SHOW CREATE TABLE t;
Table	Create Table
t	CREATE TABLE `t` (
  `a` int NOT NULL,
  `b` varchar(55) DEFAULT NULL,
  PRIMARY KEY (`a`)
) ENGINE=MyISAM DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
ALTER TABLE tp EXCHANGE PARTITION p0 WITH TABLE t;
ERROR HY000: The mix of handlers in the partitions is not allowed in this version of MySQL
SHOW CREATE TABLE t;
Table	Create Table
t	CREATE TABLE `t` (
  `a` int NOT NULL,
  `b` varchar(55) DEFAULT NULL,
  PRIMARY KEY (`a`)
) ENGINE=MyISAM DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
ALTER TABLE t ENGINE = InnoDB;
DROP TABLE t,tp;
# Test with general_log
use mysql;
SET @old_general_log_state = @@global.general_log;
SET GLOBAL general_log = 0;
ALTER TABLE general_log ENGINE = MyISAM;
CREATE TABLE t LIKE general_log;
ALTER TABLE t ENGINE InnoDB PARTITION BY RANGE (thread_id)
(PARTITION p0 VALUES LESS THAN (123456789),
PARTITION pMAX VALUES LESS THAN MAXVALUE);
ALTER TABLE t EXCHANGE PARTITION p0 WITH TABLE general_log;
ERROR HY000: Incorrect usage of PARTITION and log table
ALTER TABLE general_log ENGINE = CSV;
SET @@global.general_log = @old_general_log_state;
DROP TABLE t;
use test;

#        This test file contains test cases divided in 6 different parts.
#        Part 1 : Statments WITH IGNORE (Error to Warning )
#        Part 2 : Statements WITH IGNORE in STRICT Mode ( Error to Warning )
#
#        Part 3:  This contains all the missing test cases for the handler errors
#                 converted to warning message by the IGNORE keyword.
#
#
#        Part 4 : Error Codes affected by STRICT mode
#                 Default behavior is Warning.
#                 In STRICT mode : Error
#                 WITH IGNORE in STRICT Mode: Warning
#                 Using IGNORE + STRICT mode keeps the default bevavior.
#
#        Part 5 : Test cases for the non transactional (MyIsam) engine.
#
#        Part 6 : Miscellaneous/ Other Test cases
#



--echo
--echo #
--echo # Part 1 : Statements with IGNORE to convert Errors to Warnings.
--echo #
--echo

#Store the original SQL_Mode in org_mode variable.
SET @org_mode=@@sql_mode;

--echo #
--echo #For Error Code : ER_ROW_DOES_NOT_MATCH_GIVEN_PARTITION_SET
--echo #

CREATE TABLE t1 (a int) ENGINE = InnoDB PARTITION BY HASH (a) PARTITIONS 2;
INSERT INTO t1 VALUES (0), (1), (2), (3);
CREATE VIEW v1 AS SELECT a FROM t1 PARTITION (p0);
SELECT * FROM t1;
SELECT * FROM v1;
--error ER_ROW_DOES_NOT_MATCH_GIVEN_PARTITION_SET
INSERT INTO v1 VALUES (10),(11);
INSERT IGNORE INTO v1 VALUES (10),(11);
UPDATE IGNORE v1 SET a=11 WHERE a=2;
SELECT * FROM v1;
SELECT * from t1;
DROP TABLE t1;
DROP VIEW v1;

--echo #
--echo #For Error Code : ER_NO_PARTITION_FOR_GIVEN_VALUE
--echo #                 ER_NO_PARTITION_FOR_GIVEN_VALUE_SILENT
--echo #
CREATE TABLE t1 (a int) ENGINE = InnoDB PARTITION BY LIST (a)
(PARTITION x1 VALUES IN (2,5), PARTITION x2 VALUES IN (3));
INSERT INTO t1 VALUES (2), (3), (5);
--error ER_NO_PARTITION_FOR_GIVEN_VALUE
INSERT INTO t1 VALUES (2),(4);
INSERT IGNORE INTO t1 VALUES (2),(4);
SELECT * FROM t1;
--error ER_NO_PARTITION_FOR_GIVEN_VALUE
UPDATE t1 SET a=a+1;
UPDATE IGNORE t1 SET a=a+1;
SELECT * FROM t1;
DROP TABLE t1;

#ER_ROW_IS_REFERENCED_2
#ER_NO_REFERENCED_ROW_2
SET restrict_fk_on_non_standard_key=OFF;
CREATE TABLE t1 (a INT PRIMARY KEY) ENGINE = InnoDB;
CREATE TABLE t2 (b INT, FOREIGN KEY(b) REFERENCES t1(a)) ENGINE = InnoDB;
CREATE TABLE t3 (c INT PRIMARY KEY, FOREIGN KEY(c) REFERENCES t2(b)) ENGINE = InnoDB;
INSERT INTO t1 VALUES (1), (2), (5);
INSERT INTO t2 VALUES (1);
--error ER_NO_REFERENCED_ROW_2
INSERT INTO t3 VALUES (1), (2);
INSERT IGNORE INTO t3 VALUES (1), (2);
--error ER_NO_REFERENCED_ROW_2
UPDATE t3 SET c=2 where c=1;
UPDATE IGNORE t3 SET c=2 where c=1;

--error ER_ROW_IS_REFERENCED_2
DELETE FROM t1;
DELETE IGNORE FROM t1;
SELECT * FROM t1;
--error ER_ROW_IS_REFERENCED_2
UPDATE t2 SET b=b+5;
UPDATE IGNORE t2 SET b=b+5;
SELECT * FROM t2;

DROP TABLE t3,t2,t1;
SET restrict_fk_on_non_standard_key=ON;

#ER_BAD_NULL_ERROR
CREATE TABLE t1 (a INT) ENGINE = InnoDB;
INSERT INTO t1 VALUES (1),(NULL),(2);
SET sql_mode = 'NO_ENGINE_SUBSTITUTION';
ALTER TABLE t1 CHANGE a a INT NOT NULL;
SET sql_mode = default;
SELECT * FROM t1;
UPDATE IGNORE t1 SET a=NULL WHERE a=0;
SELECT * FROM t1;
DELETE FROM t1;
--error ER_BAD_NULL_ERROR
INSERT INTO t1 VALUES (NULL);
INSERT IGNORE INTO t1 VALUES (NULL);
SELECT * FROM t1;
INSERT IGNORE INTO t1 VALUES (NULL),(3);
SELECT * FROM t1;
DROP TABLE t1;
CREATE TABLE t1(a INT, b INT) ENGINE = InnoDB;
CREATE TABLE t2(a INT, b INT NOT NULL) ENGINE = InnoDB;
INSERT INTO t1 VALUES(1, NULL),(2,NULL);
INSERT INTO t2 VALUES (1,3), (2,4);
# Different number of warnings with hypergraph.
--disable_warnings
UPDATE IGNORE t1,t2 SET t2.b=NULL;
--enable_warnings
DROP TABLE t1,t2;

--echo #
--echo # IGNORE keyword in the statement should not affect the errors in the
--echo # trigger execution if trigger statement does not have IGNORE keyword.
--echo # and vice versa.
--echo #

CREATE TABLE t1 (a INT PRIMARY KEY) ENGINE = InnoDB;
CREATE TABLE t2 (b INT PRIMARY KEY, FOREIGN KEY(b) REFERENCES t1(a)) ENGINE = InnoDB;
CREATE TABLE t3 (c INT PRIMARY KEY) ENGINE = InnoDB;
INSERT INTO t1 VALUES (1), (2);
INSERT INTO t2 VALUES (1);
INSERT INTO t3 VALUES (1),(2),(3),(4),(5);
DELIMITER |;
CREATE TRIGGER post_insert_t1 AFTER INSERT ON t1
FOR EACH ROW BEGIN
  INSERT INTO t3 VALUES(5);
END|
CREATE TRIGGER post_update_t1 AFTER UPDATE ON t1
FOR EACH ROW BEGIN
  INSERT INTO t3 VALUES(4);
END|
DELIMITER ;|
--error ER_DUP_ENTRY
INSERT IGNORE INTO t1 VALUES(3);
SELECT * FROM t1;
--error ER_DUP_ENTRY
UPDATE IGNORE t1 SET a=3 WHERE a=2;
SELECT * FROM t1;

DROP TRIGGER post_insert_t1;
DROP TRIGGER post_update_t1;

DELIMITER |;
CREATE TRIGGER post_insert_t1 AFTER INSERT ON t1
FOR EACH ROW BEGIN
  INSERT IGNORE INTO t3 VALUES (5),(6);
END|
CREATE TRIGGER post_update_t1 AFTER UPDATE ON t1
FOR EACH ROW BEGIN
  INSERT IGNORE INTO t3 VALUES(4);
END|
DELIMITER ;|
--error ER_DUP_ENTRY
INSERT INTO t1 VALUES(2);
SELECT * FROM t3;
INSERT IGNORE INTO t1 VALUES(2),(3);
SELECT * FROM t1;
SELECT * FROM t3;
UPDATE IGNORE t1 SET a=3 WHERE a=2;

DROP TRIGGER post_insert_t1;
DROP TRIGGER post_update_t1;

DROP TABLE t2,t1,t3;

--echo
--echo #
--echo # Part 2 : Statements with IGNORE + STRICT
--echo #          These statements gives error by default
--echo #           without IGNORE and STRICT mode.
--echo #
--echo

SET sql_mode=default;
#ER_BAD_NULL_ERROR
CREATE TABLE t1 (a INT) ENGINE = InnoDB;
INSERT INTO t1 VALUES (1),(NULL),(2);
DROP TABLE t1;

CREATE TABLE t1(a INT NOT NULL) ENGINE = InnoDB;
--error ER_BAD_NULL_ERROR
INSERT INTO t1 VALUES (NULL);
INSERT IGNORE INTO t1 VALUES(NULL);
SELECT * FROM t1;
--error ER_BAD_NULL_ERROR
INSERT INTO t1 VALUES (NULL),(3);
INSERT IGNORE INTO t1 VALUES (NULL),(1);
SELECT * FROM t1;
--error ER_BAD_NULL_ERROR
UPDATE t1 SET a=NULL WHERE a=0;
UPDATE IGNORE t1 SET a=NULL WHERE a=0;
SELECT * FROM t1;
DROP TABLE t1;
CREATE TABLE t1(a INT, b INT) ENGINE = InnoDB;
CREATE TABLE t2(a INT, b INT NOT NULL) ENGINE = InnoDB;
INSERT INTO t1 VALUES(1, NULL),(2,NULL);
INSERT INTO t2 VALUES (1,3), (2,4);
--error ER_BAD_NULL_ERROR
UPDATE t1,t2 SET t2.b=NULL;
# Different number of warnings with hypergraph.
--disable_warnings
UPDATE IGNORE t1,t2 SET t2.b=NULL;
--enable_warnings
DROP TABLE t1, t2;
#ER_ROW_IS_REFERENCED_2
#ER_NO_REFERENCED_ROW_2
SET restrict_fk_on_non_standard_key=OFF;
CREATE TABLE t1 (a INT PRIMARY KEY) ENGINE = InnoDB;
CREATE TABLE t2 (b INT, FOREIGN KEY(b) REFERENCES t1(a)) ENGINE = InnoDB;
CREATE TABLE t3 (c INT PRIMARY KEY, FOREIGN KEY(c) REFERENCES t2(b)) ENGINE = InnoDB;
INSERT INTO t1 VALUES (1), (2), (5);
INSERT INTO t2 VALUES (1);
--error ER_NO_REFERENCED_ROW_2
INSERT INTO t3 VALUES (1), (2);
INSERT IGNORE INTO t3 VALUES (1), (2);
--error ER_NO_REFERENCED_ROW_2
UPDATE t3 SET c=2 where c=1;
UPDATE IGNORE t3 SET c=2 where c=1;

--error ER_ROW_IS_REFERENCED_2
DELETE FROM t1;
DELETE IGNORE FROM t1;
SELECT * FROM t1;
--error ER_ROW_IS_REFERENCED_2
UPDATE t2 SET b=b+5;
UPDATE IGNORE t2 SET b=b+5;
SELECT * FROM t2;

DROP TABLE t3,t2,t1;
SET restrict_fk_on_non_standard_key=ON;

#ER_ROW_DOES_NOT_MATCH_GIVEN_PARTITION_SET
CREATE TABLE t1 (a int) ENGINE = InnoDB PARTITION BY HASH (a) PARTITIONS 2;
INSERT INTO t1 VALUES (0), (1), (2), (3);
CREATE VIEW v1 AS SELECT a FROM t1 PARTITION (p0);
SELECT * FROM t1;
SELECT * FROM v1;
--error ER_ROW_DOES_NOT_MATCH_GIVEN_PARTITION_SET
INSERT INTO v1 VALUES (10),(11);
INSERT IGNORE INTO v1 VALUES (10),(11);
--error ER_ROW_DOES_NOT_MATCH_GIVEN_PARTITION_SET
UPDATE v1 SET a=11 WHERE a=2;
UPDATE IGNORE v1 SET a=11 WHERE a=2;
DROP VIEW v1;
DROP TABLE t1;

#ER_NO_PARTITION_FOR_GIVEN_VALUE
#ER_NO_PARTITION_FOR_GIVEN_VALUE_SILENT
CREATE TABLE t1 (a int) ENGINE = InnoDB PARTITION BY LIST (a)
(PARTITION x1 VALUES IN (2,5), PARTITION x2 VALUES IN (3));
INSERT INTO t1 VALUES (2), (3),(5);
--error ER_NO_PARTITION_FOR_GIVEN_VALUE
INSERT INTO t1 VALUES (2),(4);
INSERT IGNORE INTO t1 VALUES (2),(4);
--error ER_NO_PARTITION_FOR_GIVEN_VALUE
UPDATE t1 SET a=4 WHERE a=5;
UPDATE IGNORE t1 SET a=4 WHERE a=5;
DROP TABLE t1;

#ER_SUBQUERY_NO_1_ROW (no longer ignored)
CREATE TABLE t11 (a INT NOT NULL, b INT, PRIMARY KEY (a)) ENGINE = InnoDB;
CREATE TABLE t2 (a INT NOT NULL, b INT, PRIMARY KEY (a)) ENGINE = InnoDB;
INSERT INTO t11 VALUES (0, 10),(1, 11),(2, 12);
INSERT INTO t2 VALUES (1, 21),(2, 12),(3, 23);
--error ER_SUBQUERY_NO_1_ROW
DELETE FROM t11 WHERE t11.b = (SELECT b FROM t2 WHERE t11.a < t2.a);
# ER_SUBQUERY_NO_1_ROW is no longer in the ignored set
--error ER_SUBQUERY_NO_1_ROW
DELETE IGNORE FROM t11 WHERE t11.b = (SELECT b FROM t2 WHERE t11.a < t2.a);
DROP TABLE t11, t2;

#ER_VIEW_CHECK_FAILED
CREATE TABLE t1 (a INT) ENGINE = InnoDB;
CREATE VIEW v1 AS SELECT * FROM t1 WHERE a < 2 WITH CHECK OPTION;
--error ER_VIEW_CHECK_FAILED
INSERT INTO v1 VALUES (1), (3);
INSERT IGNORE INTO v1 VALUES (1), (3);
--error ER_VIEW_CHECK_FAILED
UPDATE v1 SET a=5 WHERE a=1;
SELECT * FROM t1;
--echo # Verify multi-table UPDATE too (Bug#30922503)
--error ER_VIEW_CHECK_FAILED
UPDATE v1 STRAIGHT_JOIN (SELECT 1) AS dt SET a=5 WHERE a=1;
SELECT * FROM t1;
UPDATE IGNORE v1 SET a=5 WHERE a=1;
SELECT * FROM t1;
INSERT INTO t1 VALUES (1);
UPDATE IGNORE v1 STRAIGHT_JOIN (SELECT 1) AS dt SET a=5 WHERE a=1;
SELECT * FROM t1;
DROP VIEW v1;
DROP TABLE t1;


--echo
--echo #
--echo # Part 3 : For the handler errors converted to warning
--echo #             by IGNORE keyword.
--echo #
--echo

--echo #
--echo # Test For warning message for unique key constraint violation.
--echo #

# Part 2 covers most of the test case for this section.

CREATE TABLE t1( a INT PRIMARY KEY) ENGINE = InnoDB;
CREATE TABLE t2( a INT PRIMARY KEY) ENGINE = InnoDB;
INSERT INTO t1 VALUES (1), (2), (3), (4);
INSERT INTO t2 VALUES (2), (4);
--let $fields=*
--replace_result $MYSQLTEST_VARDIR MYSQLTEST_VARDIR
--eval SELECT $fields INTO OUTFILE '$MYSQLTEST_VARDIR/tmp/wl6614.txt' FROM t1
--replace_result $MYSQLTEST_VARDIR MYSQLTEST_VARDIR
--eval SELECT LOAD_FILE('$MYSQLTEST_VARDIR/tmp/wl6614.txt')
--replace_result $MYSQLTEST_VARDIR MYSQLTEST_VARDIR
--error ER_DUP_ENTRY
--eval LOAD DATA INFILE '$MYSQLTEST_VARDIR/tmp/wl6614.txt' INTO TABLE t2
--replace_result $MYSQLTEST_VARDIR MYSQLTEST_VARDIR
--eval LOAD DATA INFILE '$MYSQLTEST_VARDIR/tmp/wl6614.txt' IGNORE INTO TABLE t2
--eval SELECT $fields FROM t2
DELETE FROM t2;
INSERT INTO t2 VALUES (2),(4);
--replace_result $MYSQLTEST_VARDIR MYSQLTEST_VARDIR
--eval LOAD DATA INFILE '$MYSQLTEST_VARDIR/tmp/wl6614.txt' IGNORE INTO TABLE t2
SELECT * from t2;
--remove_file $MYSQLTEST_VARDIR/tmp/wl6614.txt
DROP TABLE t1,t2;

--echo
--echo #
--echo # Part 4 : For the error codes affected by STRICT Mode,
--echo #          Using IGNORE + STRICT should give the behavior
--echo #          Warning --->  Warning ( IGNORE+STRICT mode)
--echo #
--echo
--echo #
--echo #For Error Code : ER_NO_DEFAULT_FOR_FIELD
--echo #                 ER_NO_DEFAULT_FOR_VIEW_FIELD
--echo #
DROP TABLE IF EXISTS t1;
CREATE TABLE t1 (col1 INT NOT NULL, col2 INT NOT NULL) ENGINE = InnoDB;
CREATE VIEW v1 (vcol1) AS SELECT col1 FROM t1;
--error ER_NO_DEFAULT_FOR_FIELD
INSERT INTO t1 (col1) VALUES(12);
INSERT IGNORE INTO t1 (col1) VALUES(12);
--error ER_NO_DEFAULT_FOR_VIEW_FIELD
INSERT INTO v1 (vcol1) VALUES(12);
INSERT IGNORE INTO v1 (vcol1) VALUES(12);
DROP TABLE t1;
DROP VIEW v1;

--echo #
--echo #For Error Code : ER_WARN_DATA_OUT_OF_RANGE
--echo #
SET sql_mode=default;
CREATE TABLE t1(a INT) ENGINE = InnoDB;
--error ER_WARN_DATA_OUT_OF_RANGE
INSERT INTO t1 VALUES(2147483648);
INSERT IGNORE INTO t1 VALUES(2147483648);
UPDATE IGNORE t1 SET a=2147483648 WHERE a=0;
DROP TABLE t1;

--echo #
--echo #For Error Code : ER_TRUNCATED_WRONG_VALUE_FOR_FIELD
--echo #
DROP TABLE IF EXISTS t1;
CREATE TABLE t1(a INT) ENGINE = InnoDB;
--error ER_TRUNCATED_WRONG_VALUE_FOR_FIELD
INSERT INTO t1 values('a');
INSERT IGNORE INTO t1 values('a');
UPDATE IGNORE t1 SET a='a' WHERE a=0;
DROP TABLE t1;

--echo #
--echo #For Error Code : ER_DATA_TOO_LONG
--echo #
DROP TABLE IF EXISTS t3;
CREATE TABLE t3(c1 CHAR(10) NOT NULL) ENGINE = InnoDB;
INSERT INTO t3 VALUES('a');
--error ER_DATA_TOO_LONG
INSERT INTO t3 (c1) VALUES('12345678901.x');
INSERT IGNORE INTO t3 (c1) VALUES('12345678901.x');
UPDATE IGNORE t3 SET c1='12345678901.x' WHERE c1='a';
DROP TABLE t3;

--echo #
--echo #For Error Code : ER_WRONG_VALUE_FOR_TYPE
--echo #
CREATE TABLE t1 (col1 DATETIME) ENGINE = InnoDB;
INSERT INTO t1 VALUES('1000-01-01 00:00:00');
--error ER_WRONG_VALUE_FOR_TYPE
INSERT INTO t1 VALUES(STR_TO_DATE('32.10.2004 15.30','%D.%D.%Y %H.%I'));
INSERT IGNORE INTO t1 VALUES(STR_TO_DATE('32.10.2004 15.30','%D.%D.%Y %H.%I'));
UPDATE IGNORE t1 SET col1=STR_TO_DATE('32.10.2004 15.30','%D.%D.%Y %H.%I') WHERE col1='1000-01-01 00:00:00';
DROP TABLE t1;

--echo #
--echo #For Error Code : ER_DATETIME_FUNCTION_OVERFLOW
--echo #
CREATE TABLE t1 (d DATE) ENGINE = InnoDB;
--error ER_DATETIME_FUNCTION_OVERFLOW
INSERT INTO t1 (d) SELECT DATE_SUB('2000-01-01', INTERVAL 2001 YEAR);
INSERT IGNORE INTO t1 (d) SELECT DATE_SUB('2000-01-01',INTERVAL 2001 YEAR);
DROP TABLE t1;


--echo #
--echo #For Error Code : ER_TRUNCATED_WRONG_VALUE
--echo #

CREATE TABLE t1 (col1 CHAR(3), col2 INT) ENGINE = InnoDB;
--error ER_TRUNCATED_WRONG_VALUE
INSERT INTO t1 (col1) VALUES (CAST(1000 as CHAR(3)));
SELECT * FROM t1;
INSERT IGNORE into t1 (col1) VALUES (CAST(1000 as CHAR(3)));
SELECT * FROM t1;
--error ER_TRUNCATED_WRONG_VALUE
INSERT INTO t1 (col1) VALUES (CAST(1000 as CHAR(3)));
SELECT * FROM t1;
INSERT IGNORE into t1 (col1) VALUES (CAST(1000 as CHAR(3)));
SELECT * FROM t1;
DROP TABLE t1;

--echo #
--echo #For Error Code : ER_DIVISION_BY_ZERO
--echo #For Error Code : WARN_DATA_TRUNCATED
--echo #

SET sql_mode='ERROR_FOR_DIVISION_BY_ZERO,STRICT_ALL_TABLES';
CREATE TABLE t1(a TINYINT);
--error ER_DIVISION_BY_ZERO
INSERT INTO t1 VALUES(2/0);
INSERT IGNORE INTO t1 VALUES(2/0);
SELECT * FROM t1;
DELETE FROM t1;
--error ER_TRUNCATED_WRONG_VALUE_FOR_FIELD
INSERT INTO t1 VALUES ('a59b');
INSERT IGNORE INTO t1 VALUES ('a59b');
--error 1265
INSERT INTO t1 VALUES ('1a');
INSERT IGNORE INTO t1 VALUES ('1a');
DROP TABLE t1;

--echo #
--echo #For Error Code : ER_WRONG_ARGUMENTS
--echo #

CREATE TABLE t1(a INT) ENGINE = InnoDB;
--error ER_WRONG_ARGUMENTS
INSERT INTO t1(SELECT SLEEP(NULL));
INSERT IGNORE into t1(SELECT SLEEP(NULL));
SELECT * FROM t1;
DROP TABLE t1;

--echo #
--echo #For Error Code : ER_WARN_NULL_TO_NOTNULL
--echo #

CREATE TABLE t1(a INT, b INT NOT NULL, c INT NOT NULL, d INT NOT NULL) ENGINE = InnoDB;
--error ER_WARN_NULL_TO_NOTNULL
LOAD DATA INFILE '../../std_data/wl6030_2.dat' INTO TABLE t1 FIELDS TERMINATED BY ',' ENCLOSED BY '"';
SELECT * FROM t1;
LOAD DATA INFILE '../../std_data/wl6030_2.dat' IGNORE INTO TABLE t1 FIELDS TERMINATED BY ',' ENCLOSED BY '"';
SELECT * FROM t1;
DROP TABLE t1;


#Restore the Orignal SQL_Mode
SET sql_mode=@org_mode;

--echo
--echo #
--echo # Part 5 : Test cases for the non transactional (MyIsam) engine.
--echo #

#ER_BAD_NULL_ERROR
CREATE TABLE t1 (a INT) ENGINE = InnoDB;
INSERT INTO t1 VALUES (1),(NULL),(2);
SET sql_mode = 'NO_ENGINE_SUBSTITUTION';
ALTER TABLE t1 CHANGE a a INT NOT NULL;
SET sql_mode = default;
SELECT * FROM t1;
UPDATE IGNORE t1 SET a=NULL WHERE a=0;
SELECT * FROM t1;
DELETE FROM t1;
--error ER_BAD_NULL_ERROR
INSERT INTO t1 VALUES (NULL);
INSERT IGNORE INTO t1 VALUES (NULL);
SELECT * FROM t1;
INSERT IGNORE INTO t1 VALUES (NULL),(3);
--sorted_result
SELECT * FROM t1;
DROP TABLE t1;
CREATE TABLE t1(a INT, b INT) ENGINE = InnoDB;
CREATE TABLE t2(a INT, b INT NOT NULL) ENGINE = InnoDB;
INSERT INTO t1 VALUES(1, NULL),(2,NULL);
INSERT INTO t2 VALUES (1,3), (2,4);
# Different number of warnings with hypergraph.
--disable_warnings
UPDATE IGNORE t1,t2 SET t2.b=NULL;
UPDATE IGNORE t2,t1 SET t2.b=NULL;
--enable_warnings
DROP TABLE t1,t2;

--echo
--echo #
--echo # Part 6 : Miscellaneous Test cases
--echo #
--echo

--echo # This case checks foreign key relationships.
#ER_DUP_ENTRY
#ER_NO_REFERENCED_ROW_2
SET restrict_fk_on_non_standard_key=OFF;
CREATE TABLE t1 (a INT PRIMARY KEY) ENGINE = InnoDB;
CREATE TABLE t2 (b INT, FOREIGN KEY(b) REFERENCES t1(a)) ENGINE = InnoDB;
CREATE TABLE t3 (c INT PRIMARY KEY, FOREIGN KEY(c) REFERENCES t2(b)) ENGINE = InnoDB;
INSERT INTO t1 VALUES (1), (2), (5);
INSERT INTO t2 VALUES (1);
--error ER_NO_REFERENCED_ROW_2
INSERT INTO t3 VALUES (1), (2);
INSERT IGNORE INTO t3 VALUES (1), (2);
--error ER_NO_REFERENCED_ROW_2
UPDATE t3 SET c=2 where c=1;
UPDATE IGNORE t3 SET c=2 where c=1;

--error ER_ROW_IS_REFERENCED_2
DELETE FROM t1;
DELETE IGNORE FROM t1;
SELECT * FROM t1;
--error ER_ROW_IS_REFERENCED_2
UPDATE t2 SET b=b+5;
UPDATE IGNORE t2 SET b=b+5;
SELECT * FROM t2;

DROP TABLE t3,t2,t1;
SET restrict_fk_on_non_standard_key=ON;

--echo # This case checks that the number of warnings returned
--echo # after multiupdate is correct.
CREATE TABLE t1 (a INT PRIMARY KEY) ENGINE = InnoDB;
CREATE TABLE t2 (b INT PRIMARY KEY, FOREIGN KEY(b) REFERENCES t1(a)) ENGINE = InnoDB;
CREATE TABLE t3 (c INT PRIMARY KEY) ENGINE = InnoDB;
INSERT INTO t1 VALUES (1);
INSERT INTO t2 VALUES (1);
INSERT INTO t3 VALUES (1);
--enable_info
UPDATE IGNORE t1,t3 SET t1.a=5 where t1.a=t3.c;
UPDATE IGNORE t2,t3 SET t2.b=5 where t2.b=t3.c;
--disable_info
DROP TABLE t3,t2,t1;

--echo # Test cases to increase Code Coverage
CREATE TABLE t1 (a INT PRIMARY KEY) ENGINE = InnoDB;
CREATE TABLE t2 (b INT PRIMARY KEY) ENGINE = InnoDB;
CREATE TABLE t3 (c INT PRIMARY KEY, FOREIGN KEY(c) REFERENCES t2(b)) ENGINE = InnoDB;
INSERT INTO t1 VALUES(1),(2);
INSERT INTO t2 VALUES (3),(4);
INSERT INTO t3 VALUES(3),(4);
--error ER_ROW_IS_REFERENCED_2
DELETE t1.*,t2.* FROM t1,t2;
DELETE IGNORE t1.*,t2.* FROM t1,t2;
SELECT * FROM t1;
SELECT * FROM t2;
DROP TABLE t3,t2,t1;

CREATE TABLE t1 (a INT PRIMARY KEY) ENGINE=InnoDB;
CREATE TABLE t2 (b INT PRIMARY KEY, FOREIGN KEY(b) REFERENCES t1(a)) ENGINE=InnoDB;
INSERT INTO t1 VALUES(1);
--error ER_NO_REFERENCED_ROW_2
INSERT INTO t2 VALUES(2) ON DUPLICATE KEY UPDATE b=b-1;
INSERT IGNORE INTO t2 VALUES(2) ON DUPLICATE KEY UPDATE b=b-1;
DROP table t2,t1;

--echo #
--echo # Bug#14786621 ASSERTION FAILED: THD->IS_ERROR() || KILL:
--echo #              FILESORT + DISCARDED TABLESPACE
--echo #
CREATE TABLE t(a int) engine=innodb;
ALTER TABLE t DISCARD TABLESPACE;

--error ER_TABLESPACE_DISCARDED
DELETE IGNORE FROM t ;
--error ER_TABLESPACE_DISCARDED
DELETE IGNORE FROM t ORDER BY 0 + 1 ;

DROP TABLE t;

--echo #
--echo # BUG#18526888 - STRICT MODE DOES NOT APPLY TO MULTI DELETE STATEMENT
--echo #
CREATE TABLE t1(a INT);
INSERT INTO t1 VALUES(5);
CREATE TABLE t2(b int);
INSERT INTO t2 VALUES(7);

--echo #
--echo # STRICT MODE and IGNORE test case for DELETE
--echo #
--error ER_INVALID_ARGUMENT_FOR_LOGARITHM
DELETE FROM t1 where a <=> ln(0);
--disable_warnings
DELETE IGNORE FROM t1 where a <=> ln(0);
--enable_warnings
--skip_if_hypergraph  # Different number of warnings.
SHOW WARNINGS;

--echo #
--echo # STRICT MODE and IGNORE test case for MULTI DELETE
--echo # An error ER_INVALID_ARGUMENT_FOR_LOGARITHM is expected here.
--echo #
--error ER_INVALID_ARGUMENT_FOR_LOGARITHM
DELETE t1, t2 FROM t1 INNER JOIN t2 WHERE t1.a <=> ln(0) AND t2.b <=> ln(0);
--echo # Warnings are expected here and works fine.
DELETE IGNORE t1, t2 FROM t1 INNER JOIN t2 WHERE t1.a <=> ln(0) AND t2.b <=> ln(0);
SHOW WARNINGS;

--echo # Clean-up
DROP TABLE t1,t2;
--echo # Restore the orginal sql_mode
SET sql_mode= @org_mode;

--echo #
--echo # BUG 18662121 - ASSERT IN PROTOCOL::END_STATEMENT() IN LOAD DATA STATEMENT
--echo #
SET sql_mode='STRICT_TRANS_TABLES';

CREATE TABLE t2 (a int default 0, b int primary key) engine=innodb;
INSERT INTO t2 VALUES (0, 17);

# This statement should not assert with the fix.
--error ER_DUP_ENTRY
LOAD DATA INFILE '../../std_data/rpl_loaddata.dat' INTO TABLE t2 (a, @b) SET b= @b + 2;

#Clean-up
DROP TABLE t2;
# Restore the orginal sql_mode
SET sql_mode= @org_mode;

--echo #
--echo # BUG#18662043 - ASSERT IN MY_OK() IN SQL_CMD_SIGNAL::EXECUTE
--echo #
SET sql_mode='STRICT_TRANS_TABLES';

DELIMITER |;

CREATE PROCEDURE p1()
BEGIN
  # warning
  DECLARE too_few_records CONDITION FOR SQLSTATE '01000';
  SIGNAL too_few_records SET MYSQL_ERRNO = 1261;
END |

DELIMITER ;|

--echo # This statement should pass with the fix.
CALL p1();

--echo # Clean-up
DROP PROCEDURE p1;
--echo # Restore the orginal sql_mode
SET sql_mode= @org_mode;

--echo #
--echo # Bug #19873291 : MYSQL_EXECUTE_COMMAND(THD*): ASSERTION
--echo #                 `THD->IS_ERROR() || THD->KILLED' FAILED
--echo #
CREATE TABLE t1(a INT);
SET @org_safe_updates= @@sql_safe_updates;
SET SESSION sql_safe_updates=ON;
#This statement used to trigger an assert in DEBUG build before WL#6614
--error ER_UPDATE_WITHOUT_KEY_IN_SAFE_MODE
DELETE IGNORE FROM t1 WHERE a=1;
DROP TABLE t1;
SET sql_safe_updates= @org_safe_updates;

--echo #
--echo # Bug#35373406 Subquery to derived optimizer assertion error
--echo #
CREATE TABLE t1(c0 INT);
INSERT INTO t1 VALUES (1), (2);
SET @@SESSION.OPTIMIZER_SWITCH = 'subquery_to_derived=on';
--error ER_SUBQUERY_NO_1_ROW
INSERT IGNORE INTO t1 (SELECT (SELECT t1.c0 FROM t1) FROM t1);
SET @@SESSION.OPTIMIZER_SWITCH = 'subquery_to_derived=default';
DROP TABLE t1;

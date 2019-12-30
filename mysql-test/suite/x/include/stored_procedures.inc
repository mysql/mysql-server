--disable_result_log
--disable_query_log

--echo #
--echo ## Setup tests: tables and stored procedures
## CURRENT CONTENT OF stored_procedure.inc##

eval CREATE SCHEMA IF NOT EXISTS $STORED_PROC_DB_NAME;
eval USE $STORED_PROC_DB_NAME;

DELIMITER //;

CREATE PROCEDURE `recreate_tables_for_sp`()
BEGIN
  CREATE TABLE IF NOT EXISTS tab_1 (c1t1 INT UNSIGNED, c2t1 VARCHAR(10));
  TRUNCATE TABLE tab_1;
  INSERT INTO tab_1 VALUES (1, "ONE"), (2, "TWO"), (3, "THREE");

  CREATE TABLE IF NOT EXISTS tab_2 (c1t2 DATETIME);
  TRUNCATE TABLE tab_2;
  INSERT INTO tab_2 VALUES ('2006-07-03'), ('2013-06-15'), ('2012-11-24'), ('1979-11-24');
END //

CALL `recreate_tables_for_sp`() //

CREATE PROCEDURE `empty`()
BEGIN
END //

CREATE PROCEDURE one_results_set()
BEGIN
  SELECT * FROM tab_1;
END //

CREATE PROCEDURE two_resultsets()
BEGIN
  SELECT c1t1 FROM tab_1 LIMIT 1;
  SELECT c1t2 FROM tab_2 LIMIT 1;
END //


CREATE PROCEDURE three_results_sets()
BEGIN
  SELECT * FROM tab_1;
  SELECT * FROM tab_2;

  CALL one_results_set();
END //

CREATE PROCEDURE out_param_no_select(IN val INT, OUT res INT)
BEGIN
  SET res = 2 * val;
END //

CREATE PROCEDURE out_params_with_select(OUT min_val DATETIME, OUT max_val DATETIME)
BEGIN
  SELECT MIN(c1t2) INTO min_val FROM tab_2;
  SELECT MAX(c1t2) INTO max_val FROM tab_2;

  SELECT * FROM tab_1;
END //

CREATE PROCEDURE update_no_select()
BEGIN

  UPDATE tab_1 SET c1t1 = c1t1 + 1 WHERE c1t1 > 1;
END //

CREATE PROCEDURE update_and_select()
BEGIN

  UPDATE tab_1 SET c1t1 = c1t1 + 2 WHERE c1t1 > 2;
  SELECT * FROM tab_1;
END //

CREATE PROCEDURE update_and_out_param(OUT max_val INT)
BEGIN

  UPDATE tab_1 SET c1t1 = c1t1 + 2 ORDER BY c1t1 LIMIT 1;
  SELECT MAX(c1t1) into max_val FROM tab_1;
END //

CREATE PROCEDURE update_and_out_param_and_select(OUT max_val INT)
BEGIN
  UPDATE tab_1 SET c1t1 = c1t1 * 2 ORDER BY c1t1 DESC LIMIT 1;
  SELECT MAX(c1t1) into max_val FROM tab_1;
  SELECT AVG(c1t1) FROM tab_1;
END //

CREATE PROCEDURE insert_no_select()
BEGIN
  INSERT INTO tab_1 VALUES (11, "s11"), (12, "s12"), (13, "s13");
END //

CREATE PROCEDURE insert_and_select()
BEGIN
  INSERT INTO tab_1 VALUES (111, "s111"), (112, "s112"), (13, "s113");
  SELECT * FROM tab_1;
END //

CREATE PROCEDURE insert_and_out_param(OUT max_val INT)
BEGIN

  INSERT INTO tab_1 VALUES(9999, "BIG ONE");
  SELECT MAX(c1t1) into max_val FROM tab_1;
END //

CREATE PROCEDURE insert_and_out_param_and_select(OUT max_val INT)
BEGIN

  INSERT INTO tab_1 VALUES(19999, "BIGER ONE");
  SELECT MAX(c1t1) into max_val FROM tab_1;
  SELECT MIN(c1t1) FROM tab_1;
END //

CREATE PROCEDURE error()
BEGIN
  SELECT * FROM tab_not_existing;
END //

CREATE PROCEDURE select_and_error()
BEGIN
  SELECT * FROM tab_1;
  SELECT * FROM tab_not_existing;
END //

CREATE PROCEDURE out_param_and_error(OUT min_val INT)
BEGIN
  SELECT MIN(c1t1) INTO min_val FROM tab_1;
  SELECT * FROM tab_not_existing;
END //

CREATE PROCEDURE select_out_param_and_error(OUT min_val INT)
BEGIN
  SELECT MIN(c1t1) INTO min_val FROM tab_1;
  SELECT COUNT(c1t1) FROM tab_1;
  SELECT * FROM tab_not_existing;
END //

CREATE PROCEDURE with_warning()
BEGIN
  DECLARE tiny TINYINT;

  SET tiny = 9000;
END //

CREATE PROCEDURE select_and_warning()
BEGIN
  DECLARE tiny TINYINT;
  SELECT * FROM tab_2;
  SET tiny = 9000;
END //

CREATE PROCEDURE out_param_and_warning(OUT vavg FLOAT)
BEGIN
  DECLARE tiny TINYINT;
  SELECT AVG(c1t1) INTO vavg FROM tab_1;
  SET tiny = 9000;
END //

CREATE PROCEDURE select_out_param_and_warning(OUT min_val INT)
BEGIN
  DECLARE c CHAR(2);
  SELECT MIN(c1t1) INTO min_val FROM tab_1;
  SELECT COUNT(c1t1) FROM tab_1;
  SET c = "TOO LONG";
END //

DELIMITER ;//

--enable_result_log
--enable_query_log

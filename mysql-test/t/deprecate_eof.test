
--echo #
--echo # WL#7766 Deprecate the EOF packet
--echo # This WL can be tested by checking if session state which is sent
--echo # as part of OK packet is displayed as part of SELECT sql statement.
--echo #

--source include/no_ps_protocol.inc

# When running mtr with --hypergraph, statement_id is different.
--source include/not_hypergraph.inc

--echo # Restart the mysqld server with default options so, statement_id will work. 
--source include/restart_mysqld.inc

CREATE DATABASE wl7766;
USE wl7766;
CREATE TABLE t1 (a int);
INSERT INTO t1 VALUES (1), (2);
CREATE TABLE t2 (a int);
INSERT INTO t2 VALUES (1), (2);

--echo Turn on all trackers
SET @@session.session_track_schema=ON;
SET @@session.session_track_system_variables='*';
SET @@session.session_track_state_change=ON;

--enable_session_track_info

--echo
--echo #CASE1: SELECT inside PROCEDURE
--echo

DELIMITER |;
CREATE PROCEDURE t1_sel()
BEGIN
SET @var1=20;
SELECT * FROM t1 ORDER BY 1;
END |
DELIMITER ;|

CALL t1_sel();

DELIMITER |;
CREATE PROCEDURE t1_inssel()
BEGIN
SET @a=1;
INSERT INTO t1 VALUES (3),(4);
SELECT * FROM t1 ORDER BY 1;
SELECT "session state sent as part of above SELECT" AS col_heading;
END |
DELIMITER ;|

CALL t1_inssel();

DELIMITER |;
CREATE PROCEDURE t1_selins()
BEGIN
SELECT * FROM t1 ORDER BY 1;
INSERT INTO t1 VALUES (5),(6);
SELECT "no session state exists" AS col_heading;
END |
DELIMITER ;|

CALL t1_selins();

DELIMITER |;
CREATE PROCEDURE t2t1_sel()
BEGIN
SET @a=20;
SELECT MIN(a) FROM t2;
SET @@session.sql_mode='traditional';
SELECT MAX(2) FROM t1;
END |
DELIMITER ;|

--echo session state sent for both SELECT statement
CALL t2t1_sel();

DELIMITER |;
CREATE PROCEDURE t1_call()
BEGIN
SET @a=20;
CALL t1_sel();
SELECT "session state sent for SELECT inside t1_sel()" AS col_heading;
END |
DELIMITER ;|

CALL t1_call();

DELIMITER |;
CREATE PROCEDURE t1_inout(
  IN v0 INT,
  OUT v_str_1 CHAR(32),
  OUT v_dbl_1 DOUBLE(4, 2),
  OUT v_dec_1 DECIMAL(6, 3),
  OUT v_int_1 INT,
  IN v1 INT,
  INOUT v_str_2 CHAR(64),
  INOUT v_dbl_2 DOUBLE(5, 3),
  INOUT v_dec_2 DECIMAL(7, 4),
  INOUT v_int_2 INT)
 BEGIN
  SET v0 = -1;
  SET v1 = -1;
  SET v_str_1 = 'test_1';
  SET v_dbl_1 = 12.34;
  SET v_dec_1 = 567.891;
  SET v_int_1 = 2345;
  SET v_str_2 = 'test_2';
  SET v_dbl_2 = 67.891;
  SET v_dec_2 = 234.6789;
  SET v_int_2 = 6789;
  SET @@session.time_zone='Europe/Moscow';
  SELECT * FROM t1;
  SET @@session.TIMESTAMP=200;
  SELECT * FROM t2;
 END |
DELIMITER ;|

CALL t1_inout(@a,@b,@c,@d,@e,@f,@g,@h,@i,@j);
SELECT @a,@b,@c,@d,@e,@f,@g,@h,@i,@j;

--echo
--echo #CASE2: SELECT FUNCTIONs
--echo

DELIMITER |;
CREATE FUNCTION f1 () RETURNS int
BEGIN
SET NAMES 'big5';
RETURN (SELECT COUNT(*) FROM t1);
END |
DELIMITER ;|

SELECT f1();
CREATE VIEW v1 AS SELECT f1();
SELECT * FROM v1;

DELIMITER |;
CREATE PROCEDURE sp1(OUT x INT)
BEGIN
SELECT MIN(a) INTO x FROM t1;
END |
CREATE FUNCTION f2() RETURNS int
BEGIN
DECLARE a int;
SET @a=20;
CALL sp1(a);
RETURN a;
END |
DELIMITER ;|

SELECT f2();

DELIMITER |;
CREATE FUNCTION f3() RETURNS int
 BEGIN
   DECLARE a, b int;
   DROP TEMPORARY TABLE IF EXISTS t3;
   CREATE TEMPORARY TABLE t3 (id INT);
   INSERT INTO t3 VALUES (1), (2), (3);
   SET a:= (SELECT COUNT(*) FROM t3);
   SET b:= (SELECT COUNT(*) FROM t3 t3_alias);
   RETURN a + b;
 END |
DELIMITER ;|

SELECT f3();

DELIMITER |;
CREATE FUNCTION f4() RETURNS int
 BEGIN
   DECLARE x int;
   DECLARE c CURSOR FOR SELECT * FROM t1 limit 1;
   SET NAMES 'utf8mb3';
   SET @var1=20;
   OPEN c;
   FETCH c INTO x;
   CLOSE c;
   RETURN x;
 END |
DELIMITER ;|

SELECT f4();

--echo
--echo #CASE3: SELECT with CURSORS
--echo

DELIMITER |;
CREATE PROCEDURE cursor1()
BEGIN
  DECLARE v1 int;
  DECLARE done INT DEFAULT FALSE;
  DECLARE cur1 CURSOR FOR SELECT * FROM t1;
  DECLARE CONTINUE HANDLER FOR NOT FOUND SET done = TRUE;
  SET @@session.transaction_isolation='READ-COMMITTED';
  OPEN cur1;
  read_loop: LOOP
    FETCH cur1 INTO v1;
    IF done THEN
      LEAVE read_loop;
    END IF;
  END LOOP;
  SELECT v1;
  CLOSE cur1;
END |
DELIMITER ;|

CALL cursor1();

DELIMITER |;
CREATE PROCEDURE cursor2()
BEGIN
  DECLARE x int;
  DECLARE y int;
  DECLARE c1 CURSOR FOR SELECT * FROM t1 limit 1;
  DECLARE c2 CURSOR FOR SELECT * FROM t2 limit 1;
  SET @@session.transaction_isolation='READ-COMMITTED';
  OPEN c1;
  OPEN c2;

  FETCH c1 INTO x;
  FETCH c2 INTO y;
  SELECT (x+y);
  SELECT "session state sent as part of above SELECT" AS col_heading;
  CLOSE c1;
  CLOSE c2;
END |
DELIMITER ;|

CALL cursor2();

DROP DATABASE wl7766;

--echo #
--echo # Bug#19550875: SESSION STATE NOT SENT AS PART OF RESULT SETS WHEN
--echo #               QUERY CACHE IS ON
--echo #

SET @@session.session_track_state_change=ON;

CREATE DATABASE bug19550875;
USE bug19550875;

CREATE TABLE t1 (a int);
INSERT INTO t1 VALUES (1), (2);

DELIMITER |;
CREATE PROCEDURE t_cache()
BEGIN
  SET @A= 20;
  SELECT * FROM t1;
  SELECT * FROM t1;
  PREPARE x FROM 'SELECT 1';
  SELECT * FROM t1;
  SELECT * FROM t1;
END |
DELIMITER ;|

CALL t_cache();

DELIMITER |;
CREATE PROCEDURE sel_with_session()
BEGIN
SET @var1=20;
SELECT * FROM t1 ORDER BY 1;
END |

CREATE PROCEDURE sel_with_no_session()
BEGIN
SELECT * FROM t1 ORDER BY 1;
END |
DELIMITER ;|

--echo #cleanup
DROP DATABASE bug19550875;

--echo
--echo End of tests

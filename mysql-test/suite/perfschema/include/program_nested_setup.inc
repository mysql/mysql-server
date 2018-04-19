#
# SET-UP - Creation of various nested stored programs
#

# Test requires: sp-protocol/ps-protocol/view-protocol/cursor-protocol disabled
--source include/no_protocol.inc

--echo # SET-UP
CREATE DATABASE nested_sp;
USE nested_sp;

CREATE TABLE t1(
        id   CHAR(16) NOT NULL DEFAULT '',
        data INT NOT NULL
);

CREATE TABLE t2(
  n INT UNSIGNED NOT NULL, 
  f BIGINT UNSIGNED
);

--echo ############################
--echo # Creating Stored Programs #
--echo ############################
DELIMITER |;

# Nested Stored Procedure - 1
CREATE PROCEDURE c1(x INT)
  CALL c2("c", x)|
CREATE PROCEDURE c2(s CHAR(16), x INT)
  CALL c3(x, s)|
CREATE PROCEDURE c3(x INT, s CHAR(16))
  CALL c4("level", x, s)|
CREATE PROCEDURE c4(l CHAR(8), x INT, s CHAR(16))
  INSERT INTO t1 VALUES (concat(l,s), x)|

# Nested Stored Procedure - 2
CREATE PROCEDURE iotest(x1 CHAR(16), x2 CHAR(16), y INT)
BEGIN
  CALL inc2(x2, y);
  INSERT INTO t1 VALUES (x1, y);
END|
CREATE PROCEDURE inc2(x CHAR(16), y INT)
BEGIN
  CALL inc(y);
  INSERT INTO t1 VALUES (x, y);
END|
CREATE PROCEDURE inc(inout io INT)
  SET io = io + 1|

# Nested Stored Function 
CREATE FUNCTION mul(x INT, y INT) RETURNS INT
  RETURN x*y|
CREATE FUNCTION inc(i INT) RETURNS INT
  RETURN i+1|
CREATE FUNCTION fac(n INT UNSIGNED) RETURNS BIGINT UNSIGNED
BEGIN
  DECLARE f BIGINT UNSIGNED DEFAULT 1;
  WHILE n > 1 DO
    SET f = f * n;
    SET n = n - 1;
  END WHILE;
  RETURN f;
END|
CREATE FUNCTION fun(i INT, u INT UNSIGNED) RETURNS DOUBLE
  RETURN mul(inc(i), fac(u))|
#
# A nested stored program 
#
CREATE PROCEDURE ifac(n INT UNSIGNED)
BEGIN
  DECLARE i BIGINT UNSIGNED DEFAULT 1;

  IF n > 20 THEN
    SET n = 20;         # bigint overflow otherwise
  END IF;
  WHILE i <= n DO
    BEGIN
      INSERT INTO t2 VALUES (i, fac(i));
      SET i = i + 1;
    END;
  END WHILE;
END|
CREATE TRIGGER trg AFTER INSERT ON t1 FOR EACH ROW
  CALL ifac(10)|

DELIMITER ;|


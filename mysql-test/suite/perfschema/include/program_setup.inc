#
# SET-UP - Creation of various non-nested stored programs 
#

# Test requires: sp-protocol/ps-protocol/view-protocol/cursor-protocol disabled
--source include/no_protocol.inc

--echo # SET-UP 

CREATE DATABASE stored_programs;
USE stored_programs;

CREATE TABLE t1(
   i INT NOT NULL,
   j INT
);

CREATE TABLE t2(
   name   CHAR(16) NOT NULL DEFAULT '',
   id INT NOT NULL
);

CREATE TABLE t3(
   d DATE,
   n INT,
   f DOUBLE,
   s VARCHAR(32)
);

CREATE TABLE t4(
   `k` int(10) unsigned NOT NULL AUTO_INCREMENT,
   `word` varchar(100) NOT NULL,
   `mean` varchar(300) NOT NULL,
   PRIMARY KEY (`k`)
);


--echo ############################
--echo # Creating Stored Programs #
--echo ############################

--echo # Stored Routine ( Procedure & Function )

DELIMITER |;
CREATE PROCEDURE SampleProc1(x1 INT, x2 INT, y INT)
BEGIN
  INSERT INTO t1 VALUES (x1, y);
  INSERT INTO t1 VALUES (x2, y);
END|

CREATE PROCEDURE SampleProc2(x CHAR(16), y INT)
BEGIN
  DECLARE z1, z2 INT;
  SET z1 = y;
  SET z2 = z1+2;
  INSERT INTO t2 VALUES (x, z2);
END|

CREATE PROCEDURE SampleProc3()
BEGIN
  DECLARE ld DATE;
  DECLARE li INT;
  DECLARE lf DOUBLE;
  DECLARE ls VARCHAR(32);

  SET ld = NULL, li = NULL, lf = NULL, ls = NULL;
  INSERT INTO t3 VALUES (ld, li, lf, ls);

  INSERT INTO t3 (n, f, s) VALUES ((ld IS NULL), 1,    "ld is null"),
                                  ((li IS NULL), 1,    "li is null"),
                                  ((li = 0),     NULL, "li = 0"),
                                  ((lf IS NULL), 1,    "lf is null"),
                                  ((lf = 0),     NULL, "lf = 0"),
                                  ((ls IS NULL), 1,    "ls is null");
END|

 
CREATE PROCEDURE SampleProc4()
BEGIN
               DECLARE x  INT;
               SET x = 1;
               WHILE x  <= 2 DO
                   INSERT INTO t4(word, mean) VALUES('a','a mean');
                   SET  x = x + 1;
               END WHILE;
END|

CREATE FUNCTION append(s1 CHAR(8), s2 CHAR(8)) RETURNS CHAR(16)
  RETURN concat(s1, s2)|

CREATE FUNCTION wt_avg(n1 INT, n2 INT, n3 INT, n4 INT)
  RETURNS INT
   DETERMINISTIC
    BEGIN
     DECLARE avg INT;
     SET avg = (n1+n2+n3*2+n4*4)/8;
     RETURN avg;
    END|

CREATE FUNCTION fac(n INT UNSIGNED) RETURNS BIGINT UNSIGNED
BEGIN
  DECLARE f BIGINT UNSIGNED DEFAULT 1;
  WHILE n > 1 DO
    SET f = f * n;
    SET n = n - 1;
  END WHILE;
  RETURN f;
END|

--echo # Triggers

# INSERT triggers
CREATE TRIGGER trg1 BEFORE INSERT ON t1 FOR EACH ROW
BEGIN
  if isnull(new.j) then
    SET new.j:= new.i * 10;
  END if;
END|

CREATE TRIGGER trg2 AFTER INSERT ON t2 FOR EACH ROW
BEGIN
  UPDATE t1 SET i=new.id+i ;
END|

# UPDATE trigger
CREATE TRIGGER trg3 AFTER UPDATE ON t2 FOR EACH ROW
  SET @change:= @change + new.id - old.id|

# DELETE triggers
CREATE TRIGGER trg4 BEFORE DELETE ON t1 FOR EACH ROW
  SET @del:= @del + 1|

CREATE TRIGGER trg5 AFTER DELETE ON t1 FOR EACH ROW 
  SET @del:= @del + 8 + old.j|

DELIMITER ;|

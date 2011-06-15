let $DATA_PATH=;
let $INDEX_PATH=;
--source include/have_partition.inc

--echo # Additional tests for WL#5217 by QA, testplan 1.1

let $TABLENAME= t1;
--source suite/parts/inc/partition-dml_cr_tab.inc
--source suite/parts/inc/partition-dml_ins_tab.inc
let $TABLENAME= t2;
--source suite/parts/inc/partition-dml_cr_tab.inc

delimiter //;
CREATE
PROCEDURE p1 ()
BEGIN
DECLARE c11,c21 int;
DECLARE c12,c22 varchar(64);
DECLARE cur1 CURSOR FOR SELECT * FROM t1 PARTITION (pneg,`p0-29`,`p30-299`,`p300-2999`,`p3000-299999`) ORDER BY a,b;
DECLARE cur2 CURSOR FOR SELECT * FROM t1 ORDER BY a,b;
DECLARE CONTINUE HANDLER FOR NOT FOUND SET @done = 1;
OPEN cur1;
OPEN cur2;
read_loop: LOOP
  FETCH cur1 INTO c11,c12;
  FETCH cur2 INTO c21,c22;
  IF !((c11=c21) AND (c12=c22)) THEN
  SELECT c11,c12,c21,c22;
  LEAVE read_loop;
  END IF;
  IF @done THEN
  LEAVE read_loop;
  END IF;
END LOOP;
CLOSE cur1;
CLOSE cur2;
END//

CREATE PROCEDURE p2 ()
BEGIN
UPDATE t1 PARTITION (`p0-29`) SET b='p0-29-upd' WHERE a BETWEEN 3 AND 10;
END//

CREATE PROCEDURE p3 ()
BEGIN
DELETE FROM t1 PARTITION (`p0-29`) WHERE b= 'p0-29-upd';
END//

CREATE TRIGGER tr1 AFTER INSERT ON t2
FOR EACH ROW BEGIN
    UPDATE t1 PARTITION (`p30-299`) SET b='p30-299-upd-trigger' WHERE a BETWEEN 30 AND 40;
END//

delimiter ;//

--disable_warnings
CALL p1;
--enable_warnings
CALL p2;
SELECT * FROM t1 PARTITION (`p0-29`) WHERE a BETWEEN 3 AND 10 ORDER BY a,b ;
SELECT * FROM t1 WHERE a BETWEEN 3 AND 10 ORDER BY a,b ;
CALL p3;
SELECT * FROM t1 PARTITION (`p0-29`) WHERE a BETWEEN 3 AND 10 ORDER BY a,b ;
CALL p3;
INSERT INTO t2 PARTITION (`p0-29`, subp3) VALUES (4, '(p0-29-)subp3');
SELECT * FROM t2 PARTITION (`p0-29`) WHERE a BETWEEN 3 AND 10 ORDER BY a,b ;
SELECT * FROM t1 PARTITION (`p30-299`) WHERE a BETWEEN 30 AND 40 ORDER BY a,b ;

DELETE FROM t1 PARTITION (pneg, `p30-299`);
SELECT * FROM t1 ORDER BY a,b;

--echo ########## Empty table t1 #########
DELETE FROM t1;
--disable_warnings
CALL p1;
--enable_warnings
CALL p2;
CALL p3;

DROP PROCEDURE p1;
DROP PROCEDURE p2;
DROP PROCEDURE p3;
DROP TRIGGER tr1;
DROP TABLE t1;
DROP TABLE t2;


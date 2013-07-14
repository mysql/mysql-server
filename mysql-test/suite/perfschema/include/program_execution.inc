#
# Execute the stored programs created in 
# suite/perfschema/include/program_setup.inc 
#

--echo #####################
--echo # Executing queries #
--echo #####################
INSERT INTO t1 VALUES (10,20);
CALL SampleProc1(30,40,50);
SET @a=1;
SELECT @a;
CALL SampleProc2("Jwalamukhi",34);
SELECT @a;
CALL SampleProc3();
CALL SampleProc4();
SET @change=1;
SELECT @change;
UPDATE t2 SET id=22 WHERE name="Jwalamukhi";
SELECT @change;
SET @del=1;
SELECT @del;
DELETE FROM t1 WHERE i=40;
SELECT @del;
SELECT wt_avg(1, 12, 1990, 1121990);
SELECT fac(5);
SELECT append("Bolly", "wood");

--echo # Events
DELIMITER |;
CREATE EVENT e1 ON SCHEDULE EVERY 2 SECOND DO
BEGIN
  CREATE TABLE table_t(a INT);
  INSERT INTO table_t VALUES(1);
END|
DELIMITER ;|

SET GLOBAL event_scheduler=ON;
--sleep 2
CREATE EVENT e2 ON SCHEDULE EVERY 1 SECOND DO
  DROP TABLE table_t;
let $wait_condition= SELECT count(*)= 1 FROM INFORMATION_SCHEMA.TABLES 
                     WHERE TABLE_NAME LIKE 'table_t';
--source include/wait_condition.inc
SET GLOBAL event_scheduler=OFF;

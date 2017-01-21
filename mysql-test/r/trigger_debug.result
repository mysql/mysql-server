CREATE TABLE th1(a TEXT,FULLTEXT INDEX(a)) ENGINE=MyISAM;
CREATE TABLE th2(a TEXT,FULLTEXT INDEX(a)) ENGINE=MyISAM;
CREATE TRIGGER trg_bi BEFORE INSERT ON th1
FOR EACH ROW
BEGIN
INSERT IGNORE INTO th2 SELECT 1 FROM th1
WHERE MATCH(a) AGAINST ( 'test'   IN BOOLEAN MODE);
END $
SET @@debug='+d,simulate_bug18831513';
INSERT INTO th1 VALUES(1);
ERROR 42000: You have an error in your SQL syntax; check the manual that corresponds to your MySQL server version for the right syntax to use near 'MODE)' at line 2
SET @@debug='-d,simulate_bug18831513';
INSERT INTO th1 VALUES(1);
DROP TRIGGER trg_bi;
DROP TABLE th1;
DROP TABLE th2;
#
# Bug#24506766: SIG 11 IN TRIGGER::ADD_TABLES_AND_ROUTINES
#               AT SQL/TRIGGER.CC:697
#
CREATE TABLE t1(a INT);
LOCK TABLES t1 WRITE;
SET @@debug= "+d,create_trigger_fail";
CREATE TRIGGER trig1 BEFORE INSERT ON t1 FOR EACH ROW BEGIN END;
ERROR 40001: Deadlock found when trying to get lock; try restarting transaction
INSERT INTO t1 VALUES(0);
SET @@debug= "-d,create_trigger_fail";
UNLOCK TABLES;
DROP TABLE t1;

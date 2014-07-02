use test;
CREATE PROCEDURE test_p1 ()
BEGIN
ALTER TABLE t_10 ADD UNIQUE INDEX USING BTREE (a);
ALTER TABLE tp   ADD UNIQUE INDEX USING BTREE (a);
ALTER TABLE tp EXCHANGE PARTITION p0 WITH TABLE t_10;
ALTER TABLE tp   DROP INDEX a;
ALTER TABLE t_10 DROP INDEX a;
END|
CALL test_p1;
SELECT * FROM t_10;
a	b
2	Two
4	Four
6	Six
8	Eight
SELECT * FROM tp WHERE a BETWEEN 0 AND 9;
a	b
1	One
3	Three
5	Five
9	Nine
DROP PROCEDURE test_p1;
SET @save_autocommit= @@autocommit;
SET @@autocommit= OFF;
SHOW VARIABLES LIKE '%autocommit%';
Variable_name	Value
autocommit	OFF
CREATE TRIGGER test_trg_1 BEFORE UPDATE ON tp FOR EACH ROW
BEGIN
ALTER TABLE tp EXCHANGE PARTITION p0 WITH TABLE t_10;
END|
ERROR HY000: Explicit or implicit commit is not allowed in stored function or trigger.
CREATE FUNCTION test_f_1() RETURNS int
BEGIN
ALTER TABLE tp EXCHANGE PARTITION p0 WITH TABLE t_10;
END|
ERROR HY000: Explicit or implicit commit is not allowed in stored function or trigger.
SET @@autocommit= @save_autocommit;
SET @save_event_scheduler= @@global.event_scheduler;
SET @@global.event_scheduler= ON;
CREATE EVENT test_ev_1
ON SCHEDULE AT CURRENT_TIMESTAMP
DO
ALTER TABLE tp EXCHANGE PARTITION p0 WITH TABLE t_10;
SELECT * FROM t_10;
a	b
1	One
3	Three
5	Five
9	Nine
SELECT * FROM tp WHERE a BETWEEN 0 AND 9;
a	b
2	Two
4	Four
6	Six
8	Eight
SET @@global.event_scheduler= @save_event_scheduler;
DROP TABLE IF EXISTS t_10;
DROP TABLE IF EXISTS t_100;
DROP TABLE IF EXISTS t_1000;
DROP TABLE IF EXISTS tp;
DROP TABLE IF EXISTS tsp;
DROP TABLE IF EXISTS tsp_00;
DROP TABLE IF EXISTS tsp_01;
DROP TABLE IF EXISTS tsp_02;
DROP TABLE IF EXISTS tsp_03;
DROP TABLE IF EXISTS tsp_04;
DROP TABLE IF EXISTS t_empty;
DROP TABLE IF EXISTS t_null;

SET @session_updatable_views_with_limit = @@Session.UPDATABLE_VIEWS_WITH_LIMIT;
DROP TABLE IF EXISTS t1;
CREATE TABLE t1 (a INT, b INT, c INT, PRIMARY KEY(a,b));
INSERT INTO t1 VALUES (10,2,-1), (20,3,-2),
(30,4,-3), (40,5,-4);
CREATE VIEW v1 (x,y) AS SELECT a, c FROM t1;
** Connecting test_con1 using username 'root' **
** Connection test_con1 **
SET @@Session.UPDATABLE_VIEWS_WITH_LIMIT=YES;
Warning expected, 'View does not contain complete key of the table'
UPDATE v1 SET x=x+6 LIMIT 1;
Warnings:
Note	1355	View being updated does not have complete key of underlying table in it
SELECT * FROM t1;
a	b	c
16	2	-1
20	3	-2
30	4	-3
40	5	-4
UPDATE v1 SET x=x+5;
SELECT * FROM t1;
a	b	c
21	2	-1
25	3	-2
35	4	-3
45	5	-4
** Connecting test_con2 using username 'root' **
** Connection test_con2 **
SET @@Session.UPDATABLE_VIEWS_WITH_LIMIT=NO;
SELECT @@SESSION.UPDATABLE_VIEWS_WITH_LIMIT;
@@SESSION.UPDATABLE_VIEWS_WITH_LIMIT
NO
UPDATE v1 SET x=x+10 LIMIT 1;
ERROR HY000: The target table v1 of the UPDATE is not updatable
Expected error 'Non updatable table'
SELECT * FROM t1;
a	b	c
21	2	-1
25	3	-2
35	4	-3
45	5	-4
'#---------------------FN_DYNVARS_039_01----------------------#'
SET UPDATABLE_VIEWS_WITH_LIMIT=NO;
UPDATE v1 SET x=x+1 LIMIT 1;
ERROR HY000: The target table v1 of the UPDATE is not updatable
Expected error 'Non updatable table'
SET UPDATABLE_VIEWS_WITH_LIMIT=0;
UPDATE v1 SET x=x+1 LIMIT 1;
ERROR HY000: The target table v1 of the UPDATE is not updatable
Expected error 'Non updatable table'
'#---------------------FN_DYNVARS_039_02----------------------#'
Warning expected, 'View does not contain complete key of the table'
SET UPDATABLE_VIEWS_WITH_LIMIT=DEFAULT;
UPDATE v1 SET x=x+1 LIMIT 1;
Warnings:
Note	1355	View being updated does not have complete key of underlying table in it
Warning expected, 'View does not contain complete key of the table'
SET UPDATABLE_VIEWS_WITH_LIMIT=YES;
UPDATE v1 SET x=x+2 LIMIT 1;
Warnings:
Note	1355	View being updated does not have complete key of underlying table in it
** Connection default **
** Disconnecting test_con1, test_con2 **
SET @@SESSION.updatable_views_with_limit = @session_updatable_views_with_limit;
DROP VIEW IF EXISTS v1;
DROP TABLE IF EXISTS t1;

use test;

call mtr.add_suppression("row in wrong partition.*from REBUILD/REORGANIZED");
#--disable_result_log
#--disable_query_log
--source suite/parts/inc/part_exch_ext_tabs.inc
#--enable_result_log
#--enable_query_log

--echo ######################################################################
--echo ### Key partitioned tables with/out validation

--error ER_ROW_DOES_NOT_MATCH_PARTITION
ALTER TABLE tp_k EXCHANGE PARTITION p0 WITH TABLE t_10 WITH VALIDATION;
--sorted_result
SELECT * FROM tp_k PARTITION (p0);
--sorted_result
SELECT * FROM tp_k PARTITION (p1);
--sorted_result
SELECT * FROM tp_k PARTITION (p2);

--error ER_ROW_DOES_NOT_MATCH_PARTITION
ALTER TABLE tp_k EXCHANGE PARTITION p0 WITH TABLE t_100 WITH VALIDATION;
ALTER TABLE tp_k EXCHANGE PARTITION p0 WITH TABLE t_100 WITHOUT VALIDATION;
--sorted_result
SELECT * FROM tp_k PARTITION (p0);
--sorted_result
SELECT * FROM tp_k PARTITION (p1);
--sorted_result
SELECT * FROM tp_k PARTITION (p2);

# Bug #19075764 	ALTER... REBUILD WITH MISPLACED ROWS IS CRASHING IN MYISAM.
#--error ER_ROW_IN_WRONG_PARTITION
#ALTER TABLE tp_k REBUILD PARTITION p0;
ALTER TABLE tp_k COALESCE PARTITION 1;
--sorted_result
SELECT * FROM tp_k PARTITION (p0);
--sorted_result
SELECT * FROM tp_k PARTITION (p1);
ALTER TABLE tp_k ADD PARTITION PARTITIONS 4;
ALTER TABLE tp_k EXCHANGE PARTITION p2 WITH TABLE t_100 WITHOUT VALIDATION;
--sorted_result
SELECT * FROM tp_k PARTITION (p2);
ALTER TABLE tp_k ANALYZE PARTITION p0;
ALTER TABLE tp_k OPTIMIZE PARTITION p0;

--error ER_ROW_DOES_NOT_MATCH_PARTITION
ALTER TABLE tp_k EXCHANGE PARTITION p0 WITH TABLE t_10 WITH VALIDATION;
--sorted_result
SELECT * FROM tp_k PARTITION (p0);
--sorted_result
SELECT * FROM tp_k PARTITION (p1);
--sorted_result
SELECT * FROM tp_k PARTITION (p2);
CREATE TABLE t_11 LIKE tp_k;
ALTER TABLE t_11 REMOVE PARTITIONING;
INSERT INTO t_11 SELECT * FROM t_10;
INSERT INTO t_11 SELECT * FROM t_100;
ALTER TABLE tp_k EXCHANGE PARTITION p0 WITH TABLE t_11 WITHOUT VALIDATION;
--sorted_result
SELECT * FROM tp_k PARTITION (p0);
DELETE FROM tp_k PARTITION (p1);
--sorted_result
SELECT * FROM tp_k PARTITION (p1);
ALTER TABLE tp_k CHECK PARTITION p0 ;
ALTER TABLE tp_k REPAIR PARTITION p0 ;
--sorted_result
SELECT * FROM tp_k PARTITION (p0);
--sorted_result
SELECT * FROM tp_k PARTITION (p1);
--sorted_result
SELECT * FROM tp_k PARTITION (p2);
--sorted_result
SELECT * FROM tp_k PARTITION (p3);
--sorted_result
SELECT * FROM tp_k PARTITION (p4);
--sorted_result
SELECT * FROM tp_k PARTITION (p5);
ALTER TABLE tp_k COALESCE PARTITION 2;
--sorted_result
SELECT * FROM tp_k PARTITION (p0);
--sorted_result
SELECT * FROM tp_k PARTITION (p1);
--sorted_result
SELECT * FROM tp_k PARTITION (p2);
--sorted_result
SELECT * FROM tp_k PARTITION (p3);
DROP TABLE IF EXISTS t_11;

# Values t_10 (not partitioned)
DELETE FROM t_10;
INSERT INTO t_10 VALUES (1, "One", '2001-01-01'), (3, "Three", '2003-01-01'), (5, "Five", '2005-01-01'), (9, "Nine", '2009-01-01');

# Values t_100 (not partitioned)
DELETE FROM t_100;
INSERT INTO t_100 VALUES (11, "Eleven", '2011-01-01'), (13, "Thirdteen", '2013-01-01'), (15, "Fifeteen", '2015-01-01'), (19, "Nineteen", '2019-01-01');
INSERT INTO t_100 VALUES (91, "Ninety-one", '2091-01-01'), (93, "Ninety-three", '2093-01-01'), (95, "Ninety-five", '2095-01-01'), (99, "Ninety-nine", '2099-01-01');

# Bug #19075764 	ALTER... REBUILD WITH MISPLACED ROWS IS CRASHING IN MYISAM.
#--error ER_ROW_IN_WRONG_PARTITION
#ALTER TABLE tp_kvar REBUILD PARTITION p0;
ALTER TABLE tp_kvar COALESCE PARTITION 1;
--sorted_result
SELECT * FROM tp_kvar PARTITION (p0);
--sorted_result
SELECT * FROM tp_kvar PARTITION (p1);
ALTER TABLE tp_kvar ADD PARTITION PARTITIONS 4;
--sorted_result
SELECT * FROM tp_kvar PARTITION (p2);
--sorted_result
SELECT * FROM tp_kvar PARTITION (p5);

CREATE TABLE t_11 LIKE tp_kvar;
ALTER TABLE t_11 REMOVE PARTITIONING;
INSERT INTO t_11 SELECT * FROM t_10;
INSERT INTO t_11 SELECT * FROM t_100;
ALTER TABLE tp_kvar EXCHANGE PARTITION p0 WITH TABLE t_11 WITHOUT VALIDATION;
--sorted_result
SELECT * FROM tp_kvar PARTITION (p0);
DELETE FROM tp_kvar PARTITION (p1);
--sorted_result
SELECT * FROM tp_kvar PARTITION (p1);
ALTER TABLE tp_kvar ANALYZE PARTITION p0;
ALTER TABLE tp_kvar OPTIMIZE PARTITION p0;
ALTER TABLE tp_kvar CHECK PARTITION p0 ;
ALTER TABLE tp_kvar REPAIR PARTITION p0 ;
--sorted_result
SELECT * FROM tp_kvar PARTITION (p0);
--sorted_result
SELECT * FROM tp_kvar PARTITION (p1);
--sorted_result
SELECT * FROM tp_kvar PARTITION (p2);
ALTER TABLE tp_kvar COALESCE PARTITION 2;
--sorted_result
SELECT * FROM tp_kvar PARTITION (p0);
--sorted_result
SELECT * FROM tp_kvar PARTITION (p1);
--sorted_result
SELECT * FROM tp_kvar PARTITION (p2);
--sorted_result
SELECT * FROM tp_kvar PARTITION (p3);
DROP TABLE IF EXISTS t_11;

# Values t_10 (not partitioned)
DELETE FROM t_10;
INSERT INTO t_10 VALUES (1, "One", '2001-01-01'), (3, "Three", '2003-01-01'), (5, "Five", '2005-01-01'), (9, "Nine", '2009-01-01');

# Values t_100 (not partitioned)
DELETE FROM t_100;
INSERT INTO t_100 VALUES (11, "Eleven", '2011-01-01'), (13, "Thirdteen", '2013-01-01'), (15, "Fifeteen", '2015-01-01'), (19, "Nineteen", '2019-01-01');
INSERT INTO t_100 VALUES (91, "Ninety-one", '2091-01-01'), (93, "Ninety-three", '2093-01-01'), (95, "Ninety-five", '2095-01-01'), (99, "Ninety-nine", '2099-01-01');

# Bug #19075764 	ALTER... REBUILD WITH MISPLACED ROWS IS CRASHING IN MYISAM.
#--error ER_ROW_IN_WRONG_PARTITION
#ALTER TABLE tp_kdate REBUILD PARTITION p0;
ALTER TABLE tp_kdate COALESCE PARTITION 1;
--sorted_result
SELECT * FROM tp_kdate PARTITION (p0);
--sorted_result
SELECT * FROM tp_kdate PARTITION (p1);
ALTER TABLE tp_kdate ADD PARTITION PARTITIONS 4;
CREATE TABLE t_11 LIKE tp_kdate;
ALTER TABLE t_11 REMOVE PARTITIONING;
INSERT INTO t_11 SELECT * FROM t_10;
INSERT INTO t_11 SELECT * FROM t_100;
ALTER TABLE tp_kdate EXCHANGE PARTITION p0 WITH TABLE t_11 WITHOUT VALIDATION;
--sorted_result
SELECT * FROM tp_kdate PARTITION (p0);
DELETE FROM tp_kdate PARTITION (p1);
--sorted_result
SELECT * FROM tp_kdate PARTITION (p1);
ALTER TABLE tp_kdate ANALYZE PARTITION p0;
ALTER TABLE tp_kdate OPTIMIZE PARTITION p0;
ALTER TABLE tp_kdate CHECK PARTITION p0 ;
ALTER TABLE tp_kdate REPAIR PARTITION p0 ;
--sorted_result
SELECT * FROM tp_kdate PARTITION (p0);
--sorted_result
SELECT * FROM tp_kdate PARTITION (p1);
--sorted_result
SELECT * FROM tp_kdate PARTITION (p2);
--sorted_result
SELECT * FROM tp_kdate PARTITION (p3);
--sorted_result
SELECT * FROM tp_kdate PARTITION (p4);
--sorted_result
SELECT * FROM tp_kdate PARTITION (p5);
ALTER TABLE tp_kdate COALESCE PARTITION 2;
--sorted_result
SELECT * FROM tp_kdate PARTITION (p0);
--sorted_result
SELECT * FROM tp_kdate PARTITION (p1);
--sorted_result
SELECT * FROM tp_kdate PARTITION (p2);
--sorted_result
SELECT * FROM tp_kdate PARTITION (p3);
DROP TABLE IF EXISTS t_11;

--source suite/parts/inc/part_exch_drop_ext_tabs.inc


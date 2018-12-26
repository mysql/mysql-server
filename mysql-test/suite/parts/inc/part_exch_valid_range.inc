use test;

call mtr.add_suppression("row in wrong partition.*from REBUILD/REORGANIZED");
#--disable_result_log
#--disable_query_log
--source suite/parts/inc/part_exch_ext_tabs.inc
#--enable_result_log
#--enable_query_log

--echo #############################################################################
--echo ### Range partitioned tables with/out validation

ALTER TABLE tp_r EXCHANGE PARTITION p0 WITH TABLE t_10 WITH VALIDATION;
--sorted_result
SELECT * FROM tp_r PARTITION (p0);
--sorted_result
SELECT * FROM tp_r PARTITION (p1);
--sorted_result
SELECT * FROM tp_r PARTITION (p2);
--sorted_result
SELECT * FROM t_10;

--error ER_ROW_DOES_NOT_MATCH_PARTITION
ALTER TABLE tp_r EXCHANGE PARTITION p0 WITH TABLE t_100 WITH VALIDATION;
ALTER TABLE tp_r EXCHANGE PARTITION p0 WITH TABLE t_100 WITHOUT VALIDATION;
--sorted_result
SELECT * FROM tp_r PARTITION (p0);
--sorted_result
SELECT * FROM tp_r PARTITION (p1);
--sorted_result
SELECT * FROM tp_r PARTITION (p2);
--sorted_result
SELECT * FROM t_100;

#--error ER_ROW_IN_WRONG_PARTITION
#ALTER TABLE tp_r REBUILD PARTITION p0;
#ALTER TABLE tp_r REORGANIZE PARTITION p2 INTO
# (PARTITION p2 VALUES LESS THAN (1500));
ALTER TABLE tp_r EXCHANGE PARTITION p2 WITH TABLE t_100 WITHOUT VALIDATION;
#--error ER_ROW_IN_WRONG_PARTITION
#ALTER TABLE tp_r REORGANIZE PARTITION p2 INTO
# (PARTITION p2 VALUES LESS THAN (1500));
ALTER TABLE tp_r EXCHANGE PARTITION p2 WITH TABLE t_100 WITHOUT VALIDATION;
--sorted_result
SELECT * FROM tp_r PARTITION (p2);
ALTER TABLE tp_r ANALYZE PARTITION p0;
ALTER TABLE tp_r OPTIMIZE PARTITION p0;


# Values t_10 (not partitioned)
DELETE FROM t_10;
INSERT INTO t_10 VALUES (1, "One", '2001-01-01'), (3, "Three", '2003-01-01'), (5, "Five", '2005-01-01'), (9, "Nine", '2009-01-01');

# Values t_100 (not partitioned)
DELETE FROM t_100;
INSERT INTO t_100 VALUES (11, "Eleven", '2011-01-01'), (13, "Thirdteen", '2013-01-01'), (15, "Fifeteen", '2015-01-01'), (19, "Nineteen", '2019-01-01');
INSERT INTO t_100 VALUES (91, "Ninety-one", '2091-01-01'), (93, "Ninety-three", '2093-01-01'), (95, "Ninety-five", '2095-01-01'), (99, "Ninety-nine", '2099-01-01');

CREATE TABLE t_11 LIKE tp_r;
ALTER TABLE t_11 REMOVE PARTITIONING;
INSERT INTO t_11 SELECT * FROM t_10;
INSERT INTO t_11 SELECT * FROM t_100;
ALTER TABLE tp_r EXCHANGE PARTITION p0 WITH TABLE t_11 WITHOUT VALIDATION;
--sorted_result
SELECT * FROM tp_r PARTITION (p0);
DELETE FROM tp_r PARTITION (p1);
--sorted_result
SELECT * FROM tp_r PARTITION (p1);
--sorted_result
SELECT * FROM t_11;
ALTER TABLE tp_r CHECK PARTITION p0 ;
ALTER TABLE tp_r REPAIR PARTITION p0 ;
--sorted_result
SELECT * FROM tp_r PARTITION (p0);
--sorted_result
SELECT * FROM tp_r PARTITION (p1);
--sorted_result
SELECT * FROM tp_r PARTITION (p2);
DROP TABLE IF EXISTS t_11;

# Values t_10 (not partitioned)
DELETE FROM t_10;
INSERT INTO t_10 VALUES (1, "One", '2001-01-01'), (3, "Three", '2003-01-01'), (5, "Five", '2005-01-01'), (9, "Nine", '2009-01-01');

# Values t_100 (not partitioned)
DELETE FROM t_100;
INSERT INTO t_100 VALUES (11, "Eleven", '2011-01-01'), (13, "Thirdteen", '2013-01-01'), (15, "Fifeteen", '2015-01-01'), (19, "Nineteen", '2019-01-01');
INSERT INTO t_100 VALUES (91, "Ninety-one", '2091-01-01'), (93, "Ninety-three", '2093-01-01'), (95, "Ninety-five", '2095-01-01'), (99, "Ninety-nine", '2099-01-01');

# Values tps (subpartitions)
DELETE FROM tsp_r;
INSERT INTO tsp_r VALUES (2, "Two", '2002-01-01'), (4, "Four", '2004-01-01'), (6, "Six", '2006-01-01'), (8, "Eight", '2008-01-01');
INSERT INTO tsp_r VALUES (12, "twelve", '2012-01-01'), (14, "Fourteen", '2014-01-01'), (16, "Sixteen", '2016-01-01'), (18, "Eightteen", '2018-01-01');
INSERT INTO tsp_r VALUES (112, "Hundred twelve", '2112-01-01'), (114, "Hundred fourteen", '2114-01-01'), (116, "Hundred sixteen", '2116-01-01'), (118, "Hundred eightteen", '2118-01-01');
INSERT INTO tsp_r VALUES (122, "Hundred twenty-two", '2122-01-01'), (124, "Hundred twenty-four", '2124-01-01'), (126, "Hundred twenty-six", '2126-01-01'), (128, "Hundred twenty-eight", '2128-01-01');
INSERT INTO tsp_r VALUES (162, "Hundred sixty-two", '2162-01-01'), (164, "Hundred sixty-four", '2164-01-01'), (166, "Hundred sixty-six", '2166-01-01'), (168, "Hundred sixty-eight", '2168-01-01');
INSERT INTO tsp_r VALUES (182, "Hundred eight-two", '2182-01-01'), (184, "Hundred eighty-four", '2184-01-01'), (186, "Hundred eighty-six", '2186-01-01'), (188, "Hundred eighty-eight", '2188-01-01');

--echo ### Range sub/partitioned tables with/out validation

--error ER_ROW_DOES_NOT_MATCH_PARTITION
ALTER TABLE tsp_r EXCHANGE PARTITION sp00 WITH TABLE t_10 WITH VALIDATION;
--sorted_result
--error ER_ROW_DOES_NOT_MATCH_PARTITION
ALTER TABLE tsp_r EXCHANGE PARTITION sp00 WITH TABLE t_100 WITH VALIDATION;
ALTER TABLE tsp_r EXCHANGE PARTITION sp00 WITH TABLE t_100 WITHOUT VALIDATION;
--sorted_result
SELECT * FROM t_100;
--sorted_result
SELECT * FROM tsp_r PARTITION (sp00);
# REBUILD is a noop on subpartitions.
#ALTER TABLE tsp_r REBUILD PARTITION sp00;
# Bug19075764 	ALTER... REBUILD WITH MISPLACED ROWS IS CRASHING IN MYISAM.
#ALTER TABLE tsp_r REORGANIZE PARTITION p2 INTO
# (PARTITION p2 VALUES LESS THAN (1500)
#   (SUBPARTITION sp20,
#    SUBPARTITION sp21,
#    SUBPARTITION sp22,
#    SUBPARTITION sp23,
#    SUBPARTITION sp24));
--sorted_result
SELECT * FROM tsp_r PARTITION (sp22);
ALTER TABLE tsp_r EXCHANGE PARTITION sp22 WITH TABLE t_100 WITHOUT VALIDATION;
--sorted_result
SELECT * FROM tsp_r PARTITION (sp22);
#--error ER_ROW_IN_WRONG_PARTITION
#ALTER TABLE tsp_r REORGANIZE PARTITION p2 INTO
# (PARTITION p2 VALUES LESS THAN (1500)
#   (SUBPARTITION sp20,
#    SUBPARTITION sp21,
#    SUBPARTITION sp22,
#    SUBPARTITION sp23,
#    SUBPARTITION sp24));
--sorted_result
SELECT * FROM tsp_r PARTITION (sp22);
ALTER TABLE tsp_r EXCHANGE PARTITION sp22 WITH TABLE t_100 WITHOUT VALIDATION;
--sorted_result
SELECT * FROM tsp_r PARTITION (sp22);
ALTER TABLE tsp_r ANALYZE PARTITION p0;
ALTER TABLE tsp_r OPTIMIZE PARTITION p0;
--error ER_ROW_DOES_NOT_MATCH_PARTITION
ALTER TABLE tsp_r EXCHANGE PARTITION sp00 WITH TABLE t_10 WITH VALIDATION;
CREATE TABLE t_11 LIKE t_10;
INSERT INTO t_11 SELECT * FROM t_10;
INSERT INTO t_11 SELECT * FROM t_100;
ALTER TABLE tsp_r EXCHANGE PARTITION sp00 WITH TABLE t_11 WITHOUT VALIDATION;
--sorted_result
SELECT * FROM tsp_r PARTITION (sp00);
--sorted_result
SELECT * FROM tsp_r PARTITION (sp01);
--sorted_result
SELECT * FROM tsp_r PARTITION (sp03);
--sorted_result
SELECT * FROM tsp_r PARTITION (sp04);
--sorted_result
SELECT * FROM tsp_r PARTITION (sp02);
ALTER TABLE tsp_r CHECK PARTITION sp00 ;
ALTER TABLE tsp_r REPAIR PARTITION sp00 ;
--sorted_result
SELECT * FROM tsp_r PARTITION (sp00);
--sorted_result
SELECT * FROM tsp_r PARTITION (sp01);
--sorted_result
SELECT * FROM tsp_r PARTITION (sp02);
--sorted_result
SELECT * FROM tsp_r PARTITION (sp03);
--sorted_result
SELECT * FROM tsp_r PARTITION (sp04);
DROP TABLE IF EXISTS t_11;

# Values t_10 (not partitioned)
DELETE FROM t_10;
INSERT INTO t_10 VALUES (1, "One", '2001-01-01'), (3, "Three", '2003-01-01'), (5, "Five", '2005-01-01'), (9, "Nine", '2009-01-01');

# Values t_100 (not partitioned)
DELETE FROM t_100;
INSERT INTO t_100 VALUES (11, "Eleven", '2011-01-01'), (13, "Thirdteen", '2013-01-01'), (15, "Fifeteen", '2015-01-01'), (19, "Nineteen", '2019-01-01');
INSERT INTO t_100 VALUES (91, "Ninety-one", '2091-01-01'), (93, "Ninety-three", '2093-01-01'), (95, "Ninety-five", '2095-01-01'), (99, "Ninety-nine", '2099-01-01');

CREATE TABLE t_11 LIKE tp_rintvar;
ALTER TABLE t_11 REMOVE PARTITIONING;
INSERT INTO t_11 SELECT * FROM t_10;
INSERT INTO t_11 SELECT * FROM t_100;
--error ER_ROW_DOES_NOT_MATCH_PARTITION
ALTER TABLE tp_rintvar EXCHANGE PARTITION p0 WITH TABLE t_11 WITH VALIDATION;
ALTER TABLE tp_rintvar EXCHANGE PARTITION p0 WITH TABLE t_11 WITHOUT VALIDATION;
--sorted_result
SELECT * FROM tp_rintvar PARTITION (p0);
DELETE FROM tp_rintvar PARTITION (p1);
--sorted_result
SELECT * FROM tp_rintvar PARTITION (p1);
--sorted_result
SELECT * FROM t_11;
ALTER TABLE tp_rintvar CHECK PARTITION p0 ;
ALTER TABLE tp_rintvar REPAIR PARTITION p0 ;
--sorted_result
SELECT * FROM tp_rintvar PARTITION (p0);
--sorted_result
SELECT * FROM tp_rintvar PARTITION (p1);
--sorted_result
SELECT * FROM tp_rintvar PARTITION (p2);
DROP TABLE IF EXISTS t_11;

# Values t_10 (not partitioned)
DELETE FROM t_10;
INSERT INTO t_10 VALUES (1, "One", '2001-01-01'), (3, "Three", '2003-01-01'), (5, "Five", '2005-01-01'), (9, "Nine", '2009-01-01');

# Values t_100 (not partitioned)
DELETE FROM t_100;
INSERT INTO t_100 VALUES (11, "Eleven", '2011-01-01'), (13, "Thirdteen", '2013-01-01'), (15, "Fifeteen", '2015-01-01'), (19, "Nineteen", '2019-01-01');
INSERT INTO t_100 VALUES (91, "Ninety-one", '2091-01-01'), (93, "Ninety-three", '2093-01-01'), (95, "Ninety-five", '2095-01-01'), (99, "Ninety-nine", '2099-01-01');

CREATE TABLE t_11 LIKE tp_rvar;
ALTER TABLE t_11 REMOVE PARTITIONING;
INSERT INTO t_11 SELECT * FROM t_10;
INSERT INTO t_11 SELECT * FROM t_100;
--error ER_ROW_DOES_NOT_MATCH_PARTITION
ALTER TABLE tp_rvar EXCHANGE PARTITION p0 WITH TABLE t_11 WITH VALIDATION;
ALTER TABLE tp_rvar EXCHANGE PARTITION p0 WITH TABLE t_11 WITHOUT VALIDATION;
--sorted_result
SELECT * FROM tp_rvar PARTITION (p0);
DELETE FROM tp_rvar PARTITION (p1);
--sorted_result
SELECT * FROM tp_rvar PARTITION (p1);
--sorted_result
SELECT * FROM t_11;
ALTER TABLE tp_rvar CHECK PARTITION p0 ;
ALTER TABLE tp_rvar REPAIR PARTITION p0 ;
--sorted_result
SELECT * FROM tp_rvar PARTITION (p0);
--sorted_result
SELECT * FROM tp_rvar PARTITION (p1);
--sorted_result
SELECT * FROM tp_rvar PARTITION (p2);
DROP TABLE IF EXISTS t_11;

# Values t_10 (not partitioned)
DELETE FROM t_10;
INSERT INTO t_10 VALUES (1, "One", '2001-01-01'), (3, "Three", '2003-01-01'), (5, "Five", '2005-01-01'), (9, "Nine", '2009-01-01');

# Values t_100 (not partitioned)
DELETE FROM t_100;
INSERT INTO t_100 VALUES (11, "Eleven", '2011-01-01'), (13, "Thirdteen", '2013-01-01'), (15, "Fifeteen", '2015-01-01'), (19, "Nineteen", '2019-01-01');
INSERT INTO t_100 VALUES (91, "Ninety-one", '2091-01-01'), (93, "Ninety-three", '2093-01-01'), (95, "Ninety-five", '2095-01-01'), (99, "Ninety-nine", '2099-01-01');

CREATE TABLE t_11 LIKE tp_rintdate;
ALTER TABLE t_11 REMOVE PARTITIONING;
INSERT INTO t_11 SELECT * FROM t_10;
INSERT INTO t_11 SELECT * FROM t_100;
--error ER_ROW_DOES_NOT_MATCH_PARTITION
ALTER TABLE tp_rintdate EXCHANGE PARTITION p0 WITH TABLE t_11 WITH VALIDATION;
ALTER TABLE tp_rintdate EXCHANGE PARTITION p0 WITH TABLE t_11 WITHOUT VALIDATION;
--sorted_result
SELECT * FROM tp_rintdate PARTITION (p0);
DELETE FROM tp_rintdate PARTITION (p1);
--sorted_result
SELECT * FROM tp_rintdate PARTITION (p1);
--sorted_result
SELECT * FROM t_11;
ALTER TABLE tp_rintdate CHECK PARTITION p0 ;
ALTER TABLE tp_rintdate REPAIR PARTITION p0 ;
--sorted_result
SELECT * FROM tp_rintdate PARTITION (p0);
--sorted_result
SELECT * FROM tp_rintdate PARTITION (p1);
--sorted_result
SELECT * FROM tp_rintdate PARTITION (p2);
DROP TABLE IF EXISTS t_11;

# Values t_10 (not partitioned)
DELETE FROM t_10;
INSERT INTO t_10 VALUES (1, "One", '2001-01-01'), (3, "Three", '2003-01-01'), (5, "Five", '2005-01-01'), (9, "Nine", '2009-01-01');

# Values t_100 (not partitioned)
DELETE FROM t_100;
INSERT INTO t_100 VALUES (11, "Eleven", '2011-01-01'), (13, "Thirdteen", '2013-01-01'), (15, "Fifeteen", '2015-01-01'), (19, "Nineteen", '2019-01-01');
INSERT INTO t_100 VALUES (91, "Ninety-one", '2091-01-01'), (93, "Ninety-three", '2093-01-01'), (95, "Ninety-five", '2095-01-01'), (99, "Ninety-nine", '2099-01-01');

CREATE TABLE t_11 LIKE tp_rdate;
ALTER TABLE t_11 REMOVE PARTITIONING;
INSERT INTO t_11 SELECT * FROM t_10;
INSERT INTO t_11 SELECT * FROM t_100;
--error ER_ROW_DOES_NOT_MATCH_PARTITION
ALTER TABLE tp_rdate EXCHANGE PARTITION p0 WITH TABLE t_11 WITH VALIDATION;
ALTER TABLE tp_rdate EXCHANGE PARTITION p0 WITH TABLE t_11 WITHOUT VALIDATION;
--sorted_result
SELECT * FROM tp_rdate PARTITION (p0);
DELETE FROM tp_rdate PARTITION (p1);
--sorted_result
SELECT * FROM tp_rdate PARTITION (p1);
--sorted_result
SELECT * FROM t_11;
ALTER TABLE tp_rdate CHECK PARTITION p0 ;
ALTER TABLE tp_rdate REPAIR PARTITION p0 ;
--sorted_result
SELECT * FROM tp_rdate PARTITION (p0);
--sorted_result
SELECT * FROM tp_rdate PARTITION (p1);
--sorted_result
SELECT * FROM tp_rdate PARTITION (p2);
DROP TABLE IF EXISTS t_11;

# Values t_10 (not partitioned)
DELETE FROM t_10;
INSERT INTO t_10 VALUES (1, "One", '2001-01-01'), (3, "Three", '2003-01-01'), (5, "Five", '2005-01-01'), (9, "Nine", '2009-01-01');

# Values t_100 (not partitioned)
DELETE FROM t_100;
INSERT INTO t_100 VALUES (11, "Eleven", '2011-01-01'), (13, "Thirdteen", '2013-01-01'), (15, "Fifeteen", '2015-01-01'), (19, "Nineteen", '2019-01-01');
INSERT INTO t_100 VALUES (91, "Ninety-one", '2091-01-01'), (93, "Ninety-three", '2093-01-01'), (95, "Ninety-five", '2095-01-01'), (99, "Ninety-nine", '2099-01-01');

CREATE TABLE t_11 LIKE tsp_rvar;
ALTER TABLE t_11 REMOVE PARTITIONING;
INSERT INTO t_11 SELECT * FROM t_10;
INSERT INTO t_11 SELECT * FROM t_100;
--error ER_ROW_DOES_NOT_MATCH_PARTITION
ALTER TABLE tsp_rvar EXCHANGE PARTITION sp00 WITH TABLE t_11 WITH VALIDATION;
ALTER TABLE tsp_rvar EXCHANGE PARTITION sp00 WITH TABLE t_11 WITHOUT VALIDATION;
--sorted_result
SELECT * FROM tsp_rvar PARTITION (sp00);
DELETE FROM tsp_rvar PARTITION (sp01);
--sorted_result
SELECT * FROM tsp_rvar PARTITION (sp01);
--sorted_result
SELECT * FROM t_11;
ALTER TABLE tsp_rvar CHECK PARTITION p0 ;
ALTER TABLE tsp_rvar REPAIR PARTITION p0 ;
--sorted_result
SELECT * FROM tsp_rvar PARTITION (sp00);
--sorted_result
SELECT * FROM tsp_rvar PARTITION (sp01);
--sorted_result
SELECT * FROM tsp_rvar PARTITION (sp02);
--sorted_result
SELECT * FROM tsp_rvar PARTITION (sp03);
--sorted_result
SELECT * FROM tsp_rvar PARTITION (sp04);
DROP TABLE IF EXISTS t_11;

# Values t_10 (not partitioned)
DELETE FROM t_10;
INSERT INTO t_10 VALUES (1, "One", '2001-01-01'), (3, "Three", '2003-01-01'), (5, "Five", '2005-01-01'), (9, "Nine", '2009-01-01');

# Values t_100 (not partitioned)
DELETE FROM t_100;
INSERT INTO t_100 VALUES (11, "Eleven", '2011-01-01'), (13, "Thirdteen", '2013-01-01'), (15, "Fifeteen", '2015-01-01'), (19, "Nineteen", '2019-01-01');
INSERT INTO t_100 VALUES (91, "Ninety-one", '2091-01-01'), (93, "Ninety-three", '2093-01-01'), (95, "Ninety-five", '2095-01-01'), (99, "Ninety-nine", '2099-01-01');

CREATE TABLE t_11 LIKE tsp_rvar1;
ALTER TABLE t_11 REMOVE PARTITIONING;
INSERT INTO t_11 SELECT * FROM t_10;
INSERT INTO t_11 SELECT * FROM t_100;
--error ER_ROW_DOES_NOT_MATCH_PARTITION
ALTER TABLE tsp_rvar1 EXCHANGE PARTITION p0sp0 WITH TABLE t_11 WITH VALIDATION;
ALTER TABLE tsp_rvar1 EXCHANGE PARTITION p0sp0 WITH TABLE t_11 WITHOUT VALIDATION;
--sorted_result
SELECT * FROM tsp_rvar1 PARTITION (p0sp0);
DELETE FROM tsp_rvar1 PARTITION (p0sp1);
--sorted_result
SELECT * FROM tsp_rvar1 PARTITION (p0sp1);
--sorted_result
SELECT * FROM t_11;
ALTER TABLE tsp_rvar1 CHECK PARTITION p0 ;
ALTER TABLE tsp_rvar1 REPAIR PARTITION p0 ;
--sorted_result
SELECT * FROM tsp_rvar1 PARTITION (p0sp0);
--sorted_result
SELECT * FROM tsp_rvar1 PARTITION (p0sp1);
--sorted_result
SELECT * FROM tsp_rvar1 PARTITION (p0sp2);
--sorted_result
SELECT * FROM tsp_rvar1 PARTITION (p0sp3);
--sorted_result
SELECT * FROM tsp_rvar1 PARTITION (p0sp4);
DROP TABLE IF EXISTS t_11;

--source suite/parts/inc/part_exch_drop_ext_tabs.inc


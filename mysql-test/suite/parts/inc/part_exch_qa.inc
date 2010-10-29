use test;

--disable_result_log
--disable_query_log
--source suite/parts/inc/part_exch_tabs.inc
--enable_result_log
--enable_query_log

--sorted_result
SELECT * FROM t_10;
--sorted_result
SELECT * FROM t_100;
--sorted_result
SELECT * FROM t_1000;
--sorted_result
SELECT * FROM tp;
--sorted_result
SELECT * FROM tsp;
--sorted_result
SELECT * FROM tsp_00;
--sorted_result
SELECT * FROM tsp_01;
--sorted_result
SELECT * FROM tsp_02;
--sorted_result
SELECT * FROM tsp_03;
--sorted_result
SELECT * FROM tsp_04;

# 1) Valid exchange with partitions.
# exchange of values < 10 of tp to t and complete contents of t to p0 and back.
ALTER TABLE tp EXCHANGE PARTITION p0 WITH TABLE t_10;
--sorted_result
SELECT * FROM t_10;
--sorted_result
SELECT * FROM tp WHERE a BETWEEN 0 AND 10;
# Back to the former contents.
ALTER TABLE tp EXCHANGE PARTITION p0 WITH TABLE t_10;
--sorted_result
SELECT * FROM t_10;
--sorted_result
SELECT * FROM tp WHERE a BETWEEN 0 AND 10;

# Exchange with empty table.
ALTER TABLE tp EXCHANGE PARTITION p0 WITH TABLE t_empty;
--sorted_result
SELECT * FROM t_empty;
--sorted_result
SELECT * FROM tp WHERE a BETWEEN 0 AND 10;
# Back to the former contents.
ALTER TABLE tp EXCHANGE PARTITION p0 WITH TABLE t_empty;
--sorted_result
SELECT * FROM t_empty;
--sorted_result
SELECT * FROM tp WHERE a BETWEEN 0 AND 10;

# Exchange with null table.
ALTER TABLE tp EXCHANGE PARTITION p0 WITH TABLE t_null;
--sorted_result
SELECT * FROM t_null;
--sorted_result
SELECT * FROM tp WHERE a BETWEEN 0 AND 10;
# Back to the former contents.
ALTER TABLE tp EXCHANGE PARTITION p0 WITH TABLE t_null;
--sorted_result
SELECT * FROM t_null;
--sorted_result
SELECT * FROM tp WHERE a BETWEEN 0 AND 10;

# exchange of values < 100 of tp to t and complete contents of t to p1 and back.
ALTER TABLE tp EXCHANGE PARTITION p1 WITH TABLE t_100;
--sorted_result
SELECT * FROM t_100;
--sorted_result
SELECT * FROM tp WHERE a BETWEEN 10 AND 100;
# Back to the former contents.
ALTER TABLE tp EXCHANGE PARTITION p1 WITH TABLE t_100;
--sorted_result
SELECT * FROM t_100;
--sorted_result
SELECT * FROM tp WHERE a BETWEEN 10 AND 100;

# exchange of values < 1000 of tp to t and complete contents of t to p2 and back.
ALTER TABLE tp EXCHANGE PARTITION p2 WITH TABLE t_1000;
--sorted_result
SELECT * FROM t_1000;
--sorted_result
SELECT * FROM tp WHERE a BETWEEN 100 AND 1000;
# Back to the former contents.
ALTER TABLE tp EXCHANGE PARTITION p2 WITH TABLE t_1000;
--sorted_result
SELECT * FROM t_1000;
--sorted_result
SELECT * FROM tp WHERE a BETWEEN 100 AND 1000;

# 2) Valid exchange of subpartitions.
# exchange of values < 10 of tsp to t and complete contents of t to p0 and back.
ALTER TABLE tsp EXCHANGE PARTITION sp00 WITH TABLE tsp_00;
ALTER TABLE tsp EXCHANGE PARTITION sp01 WITH TABLE tsp_01;
ALTER TABLE tsp EXCHANGE PARTITION sp02 WITH TABLE tsp_02;
ALTER TABLE tsp EXCHANGE PARTITION sp03 WITH TABLE tsp_03;
ALTER TABLE tsp EXCHANGE PARTITION sp04 WITH TABLE tsp_04;
--sorted_result
SELECT * FROM tsp_00;
--sorted_result
SELECT * FROM tsp_01;
--sorted_result
SELECT * FROM tsp_02;
--sorted_result
SELECT * FROM tsp_03;
--sorted_result
SELECT * FROM tsp_04;
--sorted_result
SELECT * FROM tsp WHERE a BETWEEN 0 AND 10;
# Back to the former contents.
ALTER TABLE tsp EXCHANGE PARTITION sp00 WITH TABLE tsp_00;
ALTER TABLE tsp EXCHANGE PARTITION sp01 WITH TABLE tsp_01;
ALTER TABLE tsp EXCHANGE PARTITION sp02 WITH TABLE tsp_02;
ALTER TABLE tsp EXCHANGE PARTITION sp03 WITH TABLE tsp_03;
ALTER TABLE tsp EXCHANGE PARTITION sp04 WITH TABLE tsp_04;
--sorted_result
SELECT * FROM tsp_00;
--sorted_result
SELECT * FROM tsp_01;
--sorted_result
SELECT * FROM tsp_02;
--sorted_result
SELECT * FROM tsp_03;
--sorted_result
SELECT * FROM tsp_04;
--sorted_result
SELECT * FROM tsp WHERE a BETWEEN 0 AND 10;

# Exchange with null table.
ALTER TABLE tsp EXCHANGE PARTITION sp01 WITH TABLE t_null;
--sorted_result
SELECT * FROM t_null;
--sorted_result
SELECT * FROM tsp WHERE a BETWEEN 0 AND 10;
# Back to the former contents.
ALTER TABLE tsp EXCHANGE PARTITION sp01 WITH TABLE t_null;
--sorted_result
SELECT * FROM t_null;
--sorted_result
SELECT * FROM tsp WHERE a BETWEEN 0 AND 10;

--source suite/parts/inc/part_exch_drop_tabs.inc


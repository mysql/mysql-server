CREATE DATABASE test_2;

USE test;
--disable_result_log
--disable_query_log
--source suite/parts/inc/part_exch_tabs.inc

USE test_2;
--source suite/parts/inc/part_exch_tabs.inc

--enable_result_log
--enable_query_log

USE test;
# 10) Exchanges with different databases.
ALTER TABLE tp EXCHANGE PARTITION p0 WITH TABLE test_2.t_10;
--sorted_result
SELECT * FROM test_2.t_10;
--sorted_result
SELECT * FROM tp WHERE a BETWEEN 0 AND 10;
# Back to former values.
ALTER TABLE tp EXCHANGE PARTITION p0 WITH TABLE test_2.t_10;
--sorted_result
SELECT * FROM test_2.t_10;
--sorted_result
SELECT * FROM tp WHERE a BETWEEN 0 AND 10;
ALTER TABLE tsp EXCHANGE PARTITION sp00 WITH TABLE test_2.tsp_00;
--sorted_result
SELECT * FROM test_2.tsp_00;
--sorted_result
SELECT * FROM tsp WHERE a BETWEEN 0 AND 10;
# Back to former values.
ALTER TABLE tsp EXCHANGE PARTITION sp00 WITH TABLE test_2.tsp_00;
--sorted_result
SELECT * FROM test_2.tsp_00;
--sorted_result
SELECT * FROM tsp WHERE a BETWEEN 0 AND 10;

--source suite/parts/inc/part_exch_drop_tabs.inc
USE test_2;
--source suite/parts/inc/part_exch_drop_tabs.inc
USE test;

DROP DATABASE test_2;


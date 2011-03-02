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

# 13) Exchanges with indexes.
# IGNORE was removed in bug#57708.
CREATE INDEX id_t_10_b USING BTREE ON t_10 (b);
--error ER_TABLES_DIFFERENT_METADATA 
ALTER TABLE tp EXCHANGE PARTITION p0 WITH TABLE t_10;
#--error ER_TABLES_DIFFERENT_METADATA 
#ALTER TABLE tp EXCHANGE PARTITION p0 WITH TABLE t_10 IGNORE;
CREATE INDEX id_tp_b USING BTREE ON tp (b);
--error ER_TABLES_DIFFERENT_METADATA 
ALTER TABLE tp EXCHANGE PARTITION p0 WITH TABLE t_10;
#--error ER_TABLES_DIFFERENT_METADATA 
#ALTER TABLE tp EXCHANGE PARTITION p0 WITH TABLE t_10 IGNORE;
DROP INDEX id_t_10_b ON t_10;
DROP INDEX id_tp_b ON tp;
ALTER TABLE t_10 ADD UNIQUE INDEX USING BTREE (a);
--error ER_TABLES_DIFFERENT_METADATA 
ALTER TABLE tp EXCHANGE PARTITION p0 WITH TABLE t_10;
#--error ER_TABLES_DIFFERENT_METADATA 
#ALTER TABLE tp EXCHANGE PARTITION p0 WITH TABLE t_10 IGNORE;
ALTER TABLE tp   ADD UNIQUE INDEX USING BTREE (a);
ALTER TABLE tp EXCHANGE PARTITION p0 WITH TABLE t_10;
show create table t_10;
show create table tp  ;
ALTER TABLE tp   DROP INDEX a;
ALTER TABLE t_10 DROP INDEX a;
ALTER TABLE tp   ADD UNIQUE INDEX USING BTREE (a,b);
ALTER TABLE t_10 ADD UNIQUE INDEX USING BTREE (a,b);
ALTER TABLE tp EXCHANGE PARTITION p0 WITH TABLE t_10;
show create table t_10;
show create table tp  ;

--source suite/parts/inc/part_exch_drop_tabs.inc


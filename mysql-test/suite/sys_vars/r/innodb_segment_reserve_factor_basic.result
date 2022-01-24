SET @start_value = @@innodb_segment_reserve_factor;
SELECT @start_value;
@start_value
12.5
SET GLOBAL innodb_segment_reserve_factor=default;
SELECT @@innodb_segment_reserve_factor;
@@innodb_segment_reserve_factor
12.500000
SET GLOBAL innodb_segment_reserve_factor = 0.0;
Warnings:
Warning	1292	Truncated incorrect innodb_segment_reserve_factor value: '0'
SELECT @@innodb_segment_reserve_factor;
@@innodb_segment_reserve_factor
0.030000
SET GLOBAL innodb_segment_reserve_factor = 0.1;
SELECT @@innodb_segment_reserve_factor;
@@innodb_segment_reserve_factor
0.100000
SET GLOBAL innodb_segment_reserve_factor = 0.5;
SELECT @@innodb_segment_reserve_factor;
@@innodb_segment_reserve_factor
0.500000
SET GLOBAL innodb_segment_reserve_factor = @start_value;
SELECT @@innodb_segment_reserve_factor;
@@innodb_segment_reserve_factor
12.500000

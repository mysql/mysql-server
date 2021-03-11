SET @start_value = @@session.secondary_engine_cost_threshold;
SELECT @start_value;
@start_value
100000
SET @start_value_global = @@global.secondary_engine_cost_threshold;
SELECT @start_value_global;
@start_value_global
100000
SET @@session.secondary_engine_cost_threshold = 8734635;
SET @@session.secondary_engine_cost_threshold = DEFAULT;
SELECT @@session.secondary_engine_cost_threshold;
@@session.secondary_engine_cost_threshold
100000.000000
SET @@session.secondary_engine_cost_threshold = 0;
SELECT @@session.secondary_engine_cost_threshold;
@@session.secondary_engine_cost_threshold
0.000000
SET @@session.secondary_engine_cost_threshold = 99;
SELECT @@session.secondary_engine_cost_threshold;
@@session.secondary_engine_cost_threshold
99.000000
SET @@session.secondary_engine_cost_threshold = 10;
SELECT @@session.secondary_engine_cost_threshold;
@@session.secondary_engine_cost_threshold
10.000000
SET @@session.secondary_engine_cost_threshold = 10000.01;
SELECT @@session.secondary_engine_cost_threshold;
@@session.secondary_engine_cost_threshold
10000.010000
SET @@session.secondary_engine_cost_threshold = 21;
SELECT @@session.secondary_engine_cost_threshold;
@@session.secondary_engine_cost_threshold
21.000000
SET @@session.secondary_engine_cost_threshold = -1;
Warnings:
Warning	1292	Truncated incorrect secondary_engine_cost_threshold value: '-1'
SELECT @@session.secondary_engine_cost_threshold;
@@session.secondary_engine_cost_threshold
0.000000
SET @@session.secondary_engine_cost_threshold = -1024;
Warnings:
Warning	1292	Truncated incorrect secondary_engine_cost_threshold value: '-1024'
SELECT @@session.secondary_engine_cost_threshold;
@@session.secondary_engine_cost_threshold
0.000000
SET @@session.secondary_engine_cost_threshold = 42949672950;
SELECT @@session.secondary_engine_cost_threshold;
@@session.secondary_engine_cost_threshold
42949672950.000000
SET @@session.secondary_engine_cost_threshold = ON;
ERROR 42000: Incorrect argument type to variable 'secondary_engine_cost_threshold'
SELECT @@session.secondary_engine_cost_threshold;
@@session.secondary_engine_cost_threshold
42949672950.000000
SET @@session.secondary_engine_cost_threshold = 'test';
ERROR 42000: Incorrect argument type to variable 'secondary_engine_cost_threshold'
SELECT @@session.secondary_engine_cost_threshold;
@@session.secondary_engine_cost_threshold
42949672950.000000
SELECT @@global.secondary_engine_cost_threshold,@@session.secondary_engine_cost_threshold;
@@global.secondary_engine_cost_threshold	@@session.secondary_engine_cost_threshold
100000.000000	42949672950.000000
SET @@global.secondary_engine_cost_threshold = 0;
SELECT @@global.secondary_engine_cost_threshold,@@session.secondary_engine_cost_threshold;
@@global.secondary_engine_cost_threshold	@@session.secondary_engine_cost_threshold
0.000000	42949672950.000000
SELECT @@session.secondary_engine_cost_threshold = VARIABLE_VALUE
FROM performance_schema.session_variables
WHERE VARIABLE_NAME='secondary_engine_cost_threshold';
@@session.secondary_engine_cost_threshold = VARIABLE_VALUE
1
SELECT @@global.secondary_engine_cost_threshold = VARIABLE_VALUE
FROM performance_schema.global_variables
WHERE VARIABLE_NAME='secondary_engine_cost_threshold';
@@global.secondary_engine_cost_threshold = VARIABLE_VALUE
1
SET @@session.secondary_engine_cost_threshold = TRUE;
SELECT @@session.secondary_engine_cost_threshold;
@@session.secondary_engine_cost_threshold
1.000000
SET @@session.secondary_engine_cost_threshold = FALSE;
SELECT @@session.secondary_engine_cost_threshold;
@@session.secondary_engine_cost_threshold
0.000000
SET @@session.secondary_engine_cost_threshold = 1;
SELECT @@secondary_engine_cost_threshold = @@session.secondary_engine_cost_threshold;
@@secondary_engine_cost_threshold = @@session.secondary_engine_cost_threshold
1
SET local.secondary_engine_cost_threshold = 1;
ERROR 42000: You have an error in your SQL syntax; check the manual that corresponds to your MySQL server version for the right syntax to use near 'local.secondary_engine_cost_threshold = 1' at line 1
SELECT local.secondary_engine_cost_threshold;
ERROR 42S02: Unknown table 'local' in field list
SET session.secondary_engine_cost_threshold = 1;
ERROR 42000: You have an error in your SQL syntax; check the manual that corresponds to your MySQL server version for the right syntax to use near 'session.secondary_engine_cost_threshold = 1' at line 1
SELECT session.secondary_engine_cost_threshold;
ERROR 42S02: Unknown table 'session' in field list
SELECT secondary_engine_cost_threshold;
ERROR 42S22: Unknown column 'secondary_engine_cost_threshold' in 'field list'
SET @@session.secondary_engine_cost_threshold = @start_value;
SET @@global.secondary_engine_cost_threshold = @start_value_global;
SELECT @@session.secondary_engine_cost_threshold,@@global.secondary_engine_cost_threshold;
@@session.secondary_engine_cost_threshold	@@global.secondary_engine_cost_threshold
100000.000000	100000.000000

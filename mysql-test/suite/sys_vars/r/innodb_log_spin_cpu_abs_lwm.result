SELECT COUNT(@@GLOBAL.innodb_log_spin_cpu_abs_lwm);
COUNT(@@GLOBAL.innodb_log_spin_cpu_abs_lwm)
1
1 Expected
SET @@GLOBAL.innodb_log_spin_cpu_abs_lwm=-1;
Warnings:
Warning	1292	Truncated incorrect innodb_log_spin_cpu_abs_lwm value: '-1'
SELECT @@innodb_log_spin_cpu_abs_lwm;
@@innodb_log_spin_cpu_abs_lwm
0
SET @@GLOBAL.innodb_log_spin_cpu_abs_lwm=1;
SELECT @@innodb_log_spin_cpu_abs_lwm;
@@innodb_log_spin_cpu_abs_lwm
1
SET @@GLOBAL.innodb_log_spin_cpu_abs_lwm=101;
SELECT @@innodb_log_spin_cpu_abs_lwm;
@@innodb_log_spin_cpu_abs_lwm
101
SELECT COUNT(@@GLOBAL.innodb_log_spin_cpu_abs_lwm);
COUNT(@@GLOBAL.innodb_log_spin_cpu_abs_lwm)
1
1 Expected
SELECT @@GLOBAL.innodb_log_spin_cpu_abs_lwm = VARIABLE_VALUE
FROM performance_schema.global_variables
WHERE VARIABLE_NAME='innodb_log_spin_cpu_abs_lwm';
@@GLOBAL.innodb_log_spin_cpu_abs_lwm = VARIABLE_VALUE
1
1 Expected
SELECT COUNT(@@GLOBAL.innodb_log_spin_cpu_abs_lwm);
COUNT(@@GLOBAL.innodb_log_spin_cpu_abs_lwm)
1
1 Expected
SELECT COUNT(VARIABLE_VALUE)
FROM performance_schema.global_variables
WHERE VARIABLE_NAME='innodb_log_spin_cpu_abs_lwm';
COUNT(VARIABLE_VALUE)
1
1 Expected
SELECT @@innodb_log_spin_cpu_abs_lwm = @@GLOBAL.innodb_log_spin_cpu_abs_lwm;
@@innodb_log_spin_cpu_abs_lwm = @@GLOBAL.innodb_log_spin_cpu_abs_lwm
1
1 Expected
SELECT COUNT(@@innodb_log_spin_cpu_abs_lwm);
COUNT(@@innodb_log_spin_cpu_abs_lwm)
1
1 Expected
SELECT COUNT(@@local.innodb_log_spin_cpu_abs_lwm);
ERROR HY000: Variable 'innodb_log_spin_cpu_abs_lwm' is a GLOBAL variable
Expected error 'Variable is a GLOBAL variable'
SELECT COUNT(@@SESSION.innodb_log_spin_cpu_abs_lwm);
ERROR HY000: Variable 'innodb_log_spin_cpu_abs_lwm' is a GLOBAL variable
Expected error 'Variable is a GLOBAL variable'
SELECT COUNT(@@GLOBAL.innodb_log_spin_cpu_abs_lwm);
COUNT(@@GLOBAL.innodb_log_spin_cpu_abs_lwm)
1
1 Expected
SELECT innodb_log_spin_cpu_abs_lwm = @@SESSION.innodb_log_spin_cpu_abs_lwm;
ERROR 42S22: Unknown column 'innodb_log_spin_cpu_abs_lwm' in 'field list'
SET GLOBAL innodb_log_spin_cpu_abs_lwm = 200;
SELECT @@innodb_log_spin_cpu_abs_lwm;
@@innodb_log_spin_cpu_abs_lwm
200
SET GLOBAL innodb_log_spin_cpu_abs_lwm = 100;
SELECT @@innodb_log_spin_cpu_abs_lwm;
@@innodb_log_spin_cpu_abs_lwm
100
SET GLOBAL innodb_log_spin_cpu_abs_lwm = 0;
SELECT @@innodb_log_spin_cpu_abs_lwm;
@@innodb_log_spin_cpu_abs_lwm
0
SET GLOBAL innodb_log_spin_cpu_abs_lwm = 80;
SELECT @@innodb_log_spin_cpu_abs_lwm;
@@innodb_log_spin_cpu_abs_lwm
80
SET GLOBAL innodb_log_spin_cpu_abs_lwm = 70;
SELECT @@innodb_log_spin_cpu_abs_lwm;
@@innodb_log_spin_cpu_abs_lwm
70
SET @@GLOBAL.innodb_log_spin_cpu_abs_lwm=80;
SELECT @@innodb_log_spin_cpu_abs_lwm;
@@innodb_log_spin_cpu_abs_lwm
80

SELECT COUNT(@@GLOBAL.innodb_log_spin_cpu_pct_hwm);
COUNT(@@GLOBAL.innodb_log_spin_cpu_pct_hwm)
1
1 Expected
SET @@GLOBAL.innodb_log_spin_cpu_pct_hwm=-1;
Warnings:
Warning	1292	Truncated incorrect innodb_log_spin_cpu_pct_hwm value: '-1'
SELECT @@innodb_log_spin_cpu_pct_hwm;
@@innodb_log_spin_cpu_pct_hwm
0
SET @@GLOBAL.innodb_log_spin_cpu_pct_hwm=101;
Warnings:
Warning	1292	Truncated incorrect innodb_log_spin_cpu_pct_hwm value: '101'
SELECT @@innodb_log_spin_cpu_pct_hwm;
@@innodb_log_spin_cpu_pct_hwm
100
SET @@GLOBAL.innodb_log_spin_cpu_pct_hwm=1;
SELECT @@innodb_log_spin_cpu_pct_hwm;
@@innodb_log_spin_cpu_pct_hwm
1
SELECT COUNT(@@GLOBAL.innodb_log_spin_cpu_pct_hwm);
COUNT(@@GLOBAL.innodb_log_spin_cpu_pct_hwm)
1
1 Expected
SELECT @@GLOBAL.innodb_log_spin_cpu_pct_hwm = VARIABLE_VALUE
FROM performance_schema.global_variables
WHERE VARIABLE_NAME='innodb_log_spin_cpu_pct_hwm';
@@GLOBAL.innodb_log_spin_cpu_pct_hwm = VARIABLE_VALUE
1
1 Expected
SELECT COUNT(@@GLOBAL.innodb_log_spin_cpu_pct_hwm);
COUNT(@@GLOBAL.innodb_log_spin_cpu_pct_hwm)
1
1 Expected
SELECT COUNT(VARIABLE_VALUE)
FROM performance_schema.global_variables
WHERE VARIABLE_NAME='innodb_log_spin_cpu_pct_hwm';
COUNT(VARIABLE_VALUE)
1
1 Expected
SELECT @@innodb_log_spin_cpu_pct_hwm = @@GLOBAL.innodb_log_spin_cpu_pct_hwm;
@@innodb_log_spin_cpu_pct_hwm = @@GLOBAL.innodb_log_spin_cpu_pct_hwm
1
1 Expected
SELECT COUNT(@@innodb_log_spin_cpu_pct_hwm);
COUNT(@@innodb_log_spin_cpu_pct_hwm)
1
1 Expected
SELECT COUNT(@@local.innodb_log_spin_cpu_pct_hwm);
ERROR HY000: Variable 'innodb_log_spin_cpu_pct_hwm' is a GLOBAL variable
Expected error 'Variable is a GLOBAL variable'
SELECT COUNT(@@SESSION.innodb_log_spin_cpu_pct_hwm);
ERROR HY000: Variable 'innodb_log_spin_cpu_pct_hwm' is a GLOBAL variable
Expected error 'Variable is a GLOBAL variable'
SELECT COUNT(@@GLOBAL.innodb_log_spin_cpu_pct_hwm);
COUNT(@@GLOBAL.innodb_log_spin_cpu_pct_hwm)
1
1 Expected
SELECT innodb_log_spin_cpu_pct_hwm;
ERROR 42S22: Unknown column 'innodb_log_spin_cpu_pct_hwm' in 'field list'
SET GLOBAL innodb_log_spin_cpu_pct_hwm = 100;
SELECT @@innodb_log_spin_cpu_pct_hwm;
@@innodb_log_spin_cpu_pct_hwm
100
SET GLOBAL innodb_log_spin_cpu_pct_hwm = 0;
SELECT @@innodb_log_spin_cpu_pct_hwm;
@@innodb_log_spin_cpu_pct_hwm
0
SET GLOBAL innodb_log_spin_cpu_pct_hwm = 50;
SELECT @@innodb_log_spin_cpu_pct_hwm;
@@innodb_log_spin_cpu_pct_hwm
50
SET GLOBAL innodb_log_spin_cpu_pct_hwm = 5;
SELECT @@innodb_log_spin_cpu_pct_hwm;
@@innodb_log_spin_cpu_pct_hwm
5
SET @@GLOBAL.innodb_log_spin_cpu_pct_hwm=50;
SELECT @@innodb_log_spin_cpu_pct_hwm;
@@innodb_log_spin_cpu_pct_hwm
50

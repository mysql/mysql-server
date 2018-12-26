'#-----------------------------------------------------------#'
'Test what happens when the innodb_log_wait_for_flush_spin_hwm is changed'
' in runtime (both decreased and increased).'
INSERT INTO t VALUES (1000, 'a');
SET GLOBAL innodb_log_wait_for_flush_spin_hwm = 100;
SELECT @@innodb_log_wait_for_flush_spin_hwm;
@@innodb_log_wait_for_flush_spin_hwm
100
INSERT INTO t VALUES (2000, 'b');
SET GLOBAL innodb_log_wait_for_flush_spin_hwm = 0;
SELECT @@innodb_log_wait_for_flush_spin_hwm;
@@innodb_log_wait_for_flush_spin_hwm
0
INSERT INTO t VALUES (3000, 'c');
SET GLOBAL innodb_log_wait_for_flush_spin_hwm = 80;
SELECT @@innodb_log_wait_for_flush_spin_hwm;
@@innodb_log_wait_for_flush_spin_hwm
80
INSERT INTO t VALUES (4000, 'd');
SET @conn_counter = 9;
CALL insert_rows();
SET @conn_counter = 8;
CALL insert_rows();
SET @conn_counter = 7;
CALL insert_rows();
SET @conn_counter = 6;
CALL insert_rows();
SET @conn_counter = 5;
CALL insert_rows();
SET @conn_counter = 4;
CALL insert_rows();
SET @conn_counter = 3;
CALL insert_rows();
SET @conn_counter = 2;
CALL insert_rows();
SET @conn_counter = 1;
CALL insert_rows();
Enable spinning always
SET GLOBAL innodb_log_spin_cpu_pct_hwm = 100;
SET GLOBAL innodb_log_spin_cpu_abs_lwm = 0;
SET GLOBAL innodb_log_wait_for_flush_spin_hwm = 100000;
SELECT @@innodb_log_wait_for_flush_spin_hwm;
@@innodb_log_wait_for_flush_spin_hwm
100000
SELECT @@innodb_log_spin_cpu_abs_lwm;
@@innodb_log_spin_cpu_abs_lwm
0
SELECT @@innodb_log_spin_cpu_pct_hwm;
@@innodb_log_spin_cpu_pct_hwm
100
Modifying innodb_log_wait_for_flush_spin_hwm
SET GLOBAL innodb_log_wait_for_flush_spin_hwm = 0;
SELECT @@innodb_log_wait_for_flush_spin_hwm;
@@innodb_log_wait_for_flush_spin_hwm
0
SET GLOBAL innodb_log_wait_for_flush_spin_hwm = 50;
SELECT @@innodb_log_wait_for_flush_spin_hwm;
@@innodb_log_wait_for_flush_spin_hwm
50
SET GLOBAL innodb_log_wait_for_flush_spin_hwm = 1000;
SELECT @@innodb_log_wait_for_flush_spin_hwm;
@@innodb_log_wait_for_flush_spin_hwm
1000
SET GLOBAL innodb_log_wait_for_flush_spin_hwm = 50;
SELECT @@innodb_log_wait_for_flush_spin_hwm;
@@innodb_log_wait_for_flush_spin_hwm
50
SET GLOBAL innodb_log_wait_for_flush_spin_hwm = 0;
SELECT @@innodb_log_wait_for_flush_spin_hwm;
@@innodb_log_wait_for_flush_spin_hwm
0
Modifying innodb_log_spin_cpu_abs_lwm
SET GLOBAL innodb_log_spin_cpu_abs_lwm = 0;
SELECT @@innodb_log_spin_cpu_abs_lwm;
@@innodb_log_spin_cpu_abs_lwm
0
SET GLOBAL innodb_log_spin_cpu_abs_lwm = 50;
SELECT @@innodb_log_spin_cpu_abs_lwm;
@@innodb_log_spin_cpu_abs_lwm
50
SET GLOBAL innodb_log_spin_cpu_abs_lwm = 100;
SELECT @@innodb_log_spin_cpu_abs_lwm;
@@innodb_log_spin_cpu_abs_lwm
100
SET GLOBAL innodb_log_spin_cpu_abs_lwm = 50;
SELECT @@innodb_log_spin_cpu_abs_lwm;
@@innodb_log_spin_cpu_abs_lwm
50
SET GLOBAL innodb_log_spin_cpu_abs_lwm = 0;
SELECT @@innodb_log_spin_cpu_abs_lwm;
@@innodb_log_spin_cpu_abs_lwm
0
Modifying innodb_log_spin_cpu_pct_hwm
SET GLOBAL innodb_log_spin_cpu_pct_hwm = 0;
SELECT @@innodb_log_spin_cpu_pct_hwm;
@@innodb_log_spin_cpu_pct_hwm
0
SET GLOBAL innodb_log_spin_cpu_pct_hwm = 5;
SELECT @@innodb_log_spin_cpu_pct_hwm;
@@innodb_log_spin_cpu_pct_hwm
5
SET GLOBAL innodb_log_spin_cpu_pct_hwm = 100;
SELECT @@innodb_log_spin_cpu_pct_hwm;
@@innodb_log_spin_cpu_pct_hwm
100
SET GLOBAL innodb_log_spin_cpu_pct_hwm = 5;
SELECT @@innodb_log_spin_cpu_pct_hwm;
@@innodb_log_spin_cpu_pct_hwm
5
SET GLOBAL innodb_log_spin_cpu_pct_hwm = 0;
SELECT @@innodb_log_spin_cpu_pct_hwm;
@@innodb_log_spin_cpu_pct_hwm
0
SET @@GLOBAL.innodb_log_spin_cpu_abs_lwm=80;
SET @@GLOBAL.innodb_log_spin_cpu_pct_hwm=50;
SET @@GLOBAL.innodb_log_wait_for_flush_spin_hwm=400;
SELECT @@innodb_log_wait_for_flush_spin_hwm;
@@innodb_log_wait_for_flush_spin_hwm
400
SELECT @@innodb_log_spin_cpu_abs_lwm;
@@innodb_log_spin_cpu_abs_lwm
80
SELECT @@innodb_log_spin_cpu_pct_hwm;
@@innodb_log_spin_cpu_pct_hwm
50

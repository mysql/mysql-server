SET @mysqlx_min_worker_threads_sav= @@global.mysqlx_min_worker_threads;
SET GLOBAL mysqlx_min_worker_threads=150;
Warnings:
Warning	1292	Truncated incorrect mysqlx_min_worker_threads value: '150'
SET @@global.mysqlx_min_worker_threads=15;
show variables like 'mysqlx_min_worker_threads';
Variable_name	Value
mysqlx_min_worker_threads	15
SET mysqlx_min_worker_threads=15;
ERROR HY000: Variable 'mysqlx_min_worker_threads' is a GLOBAL variable and should be set with SET GLOBAL
SET @@local.mysqlx_min_worker_threads=15;
ERROR HY000: Variable 'mysqlx_min_worker_threads' is a GLOBAL variable and should be set with SET GLOBAL
SET @@mysqlx_min_worker_threads=15;
ERROR HY000: Variable 'mysqlx_min_worker_threads' is a GLOBAL variable and should be set with SET GLOBAL
SET @@session.mysqlx_min_worker_threads=15;
ERROR HY000: Variable 'mysqlx_min_worker_threads' is a GLOBAL variable and should be set with SET GLOBAL
SET SESSION mysqlx_min_worker_threads=15;
ERROR HY000: Variable 'mysqlx_min_worker_threads' is a GLOBAL variable and should be set with SET GLOBAL
SET @@global.mysqlx_min_worker_threads=0;
Warnings:
Warning	1292	Truncated incorrect mysqlx_min_worker_threads value: '0'
SET @@global.mysqlx_min_worker_threads=-4324;
Warnings:
Warning	1292	Truncated incorrect mysqlx_min_worker_threads value: '-4324'
SET GLOBAL mysqlx_min_worker_threads=+23766;
Warnings:
Warning	1292	Truncated incorrect mysqlx_min_worker_threads value: '23766'
SET GLOBAL mysqlx_min_worker_threads='USDSI75u';
ERROR 42000: Incorrect argument type to variable 'mysqlx_min_worker_threads'
show variables like 'mysqlx_min_worker_threads';
Variable_name	Value
mysqlx_min_worker_threads	100
SET GLOBAL mysqlx_min_worker_threads=10;
SET @@global.mysqlx_min_worker_threads= @mysqlx_min_worker_threads_sav;

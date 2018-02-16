SELECT @@global.slave_parallel_type;
@@global.slave_parallel_type
DATABASE
SET GLOBAL slave_parallel_type= 'DATABASE';
SELECT @@global.slave_parallel_type;
@@global.slave_parallel_type
DATABASE
SET GLOBAL slave_parallel_type= 'LOGICAL_CLOCK';
SELECT @@global.slave_parallel_type;
@@global.slave_parallel_type
LOGICAL_CLOCK
SET GLOBAL slave_parallel_type= DEFAULT;
SELECT @@global.slave_parallel_type;
@@global.slave_parallel_type
DATABASE
SET GLOBAL slave_parallel_type= NULL;
ERROR 42000: Variable 'slave_parallel_type' can't be set to the value of 'NULL'
SELECT @@global.slave_parallel_type;
@@global.slave_parallel_type
DATABASE
SET GLOBAL slave_parallel_type= '';
ERROR 42000: Variable 'slave_parallel_type' can't be set to the value of ''
SELECT @@global.slave_parallel_type;
@@global.slave_parallel_type
DATABASE
SET GLOBAL slave_parallel_type= '0';
ERROR 42000: Variable 'slave_parallel_type' can't be set to the value of '0'
SELECT @@global.slave_parallel_type;
@@global.slave_parallel_type
DATABASE
SET GLOBAL slave_parallel_type= 'GARBAGE';
ERROR 42000: Variable 'slave_parallel_type' can't be set to the value of 'GARBAGE'
SELECT @@global.slave_parallel_type;
@@global.slave_parallel_type
DATABASE
SET SESSION slave_parallel_type= 'LOGICAL_CLOCK';
ERROR HY000: Variable 'slave_parallel_type' is a GLOBAL variable and should be set with SET GLOBAL
SELECT @@global.slave_parallel_type;
@@global.slave_parallel_type
DATABASE

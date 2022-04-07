include/master-slave.inc
Warnings:
Note	####	Sending passwords in plain text without SSL/TLS is extremely insecure.
Note	####	Storing MySQL user name or password information in the master info repository is not secure and is therefore not recommended. Please consider using the USER and PASSWORD connection options for START SLAVE; see the 'START SLAVE Syntax' in the MySQL Manual for more information.
[connection master]
Show the effect of the server command line option slave-skip-errors
being passed the alias ignore_ddl_errors and a numeric error code
SHOW VARIABLES LIKE 'slave_skip_errors';
Variable_name	Value
slave_skip_errors	1003,1007,1008,1050,1051,1054,1060,1061,1068,1091,1146
include/rpl_end.inc

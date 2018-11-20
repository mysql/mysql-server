SET @global_saved_tmp =  @@global.offline_mode;
SET @user_count= 1;
CREATE USER 'user1'@'localhost';
CREATE USER 'user2'@'localhost';
CREATE USER 'user3'@'localhost';
connect conu1,localhost,user1;
connect conu2,localhost,user2;
connect conu3,localhost,user3;
SET GLOBAL offline_mode = ON;
ERROR 42000: Access denied; you need (at least one of) the SUPER or SYSTEM_VARIABLES_ADMIN privilege(s) for this operation
connection default;
SELECT COUNT(USER) FROM INFORMATION_SCHEMA.PROCESSLIST;
COUNT(USER)
5
SHOW STATUS LIKE 'threads_connected';
Variable_name	Value
Threads_connected	4
SET GLOBAL offline_mode = ON;
SELECT COUNT(USER) FROM INFORMATION_SCHEMA.PROCESSLIST;
COUNT(USER)
2
SHOW STATUS LIKE 'threads_connected';
Variable_name	Value
Threads_connected	1
error ER_SERVER_OFFLINE_MODE
connection conu1;
connection default;
SELECT COUNT(USER) FROM INFORMATION_SCHEMA.PROCESSLIST;
COUNT(USER)
2
SHOW STATUS LIKE 'threads_connected';
Variable_name	Value
Threads_connected	1
SET GLOBAL offline_mode = OFF;
connect conu4,localhost,root;
SELECT COUNT(USER) FROM INFORMATION_SCHEMA.PROCESSLIST;
COUNT(USER)
3
SHOW STATUS LIKE 'threads_connected';
Variable_name	Value
Threads_connected	2
connection default;
SET GLOBAL offline_mode = OFF;
SELECT COUNT(USER) FROM INFORMATION_SCHEMA.PROCESSLIST;
COUNT(USER)
3
SHOW STATUS LIKE 'threads_connected';
Variable_name	Value
Threads_connected	2
disconnect conu1;
disconnect conu2;
disconnect conu3;
connect conu1,localhost,user1;
connect conu2,localhost,user2;
connect conu3,localhost,user3;
connection default;
SELECT @user_count;
@user_count
1
SELECT COUNT(USER) FROM INFORMATION_SCHEMA.PROCESSLIST;
COUNT(USER)
6
SHOW STATUS LIKE 'threads_connected';
Variable_name	Value
Threads_connected	5
SET GLOBAL offline_mode = ON;
SELECT COUNT(USER) FROM INFORMATION_SCHEMA.PROCESSLIST;
COUNT(USER)
3
SHOW STATUS LIKE 'threads_connected';
Variable_name	Value
Threads_connected	2
disconnect conu1;
disconnect conu2;
disconnect conu3;
disconnect conu4;
DROP USER 'user1'@'localhost';
DROP USER 'user2'@'localhost';
DROP USER 'user3'@'localhost';
# Restoring the original values.
SET @@global.offline_mode = @global_saved_tmp;

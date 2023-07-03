include/master-slave.inc
Warnings:
Note	####	Sending passwords in plain text without SSL/TLS is extremely insecure.
Note	####	Storing MySQL user name or password information in the master info repository is not secure and is therefore not recommended. Please consider using the USER and PASSWORD connection options for START SLAVE; see the 'START SLAVE Syntax' in the MySQL Manual for more information.
[connection master]
# Create user on the master
CREATE USER mohit@localhost IDENTIFIED BY 'mohit' PASSWORD HISTORY 1;
# Display binlog events on the master
include/show_binlog_events.inc
Log_name	Pos	Event_type	Server_id	End_log_pos	Info
master-bin.000001	#	Query	#	#	use `test`; CREATE USER 'mohit'@'localhost' IDENTIFIED WITH 'caching_sha2_password' AS '<non-deterministic-password-hash>' PASSWORD HISTORY 1
# Check the binlog contents on the slave
include/sync_slave_sql_with_master.inc
# Check password hisotry on the slave
SELECT COUNT(*) FROM mysql.password_history WHERE
User='mohit' AND Host='localhost';
COUNT(*)
1
DROP USER mohit@localhost;
include/rpl_end.inc

include/master-slave.inc
Warnings:
Note	####	Sending passwords in plain text without SSL/TLS is extremely insecure.
Note	####	Storing MySQL user name or password information in the master info repository is not secure and is therefore not recommended. Please consider using the USER and PASSWORD connection options for START SLAVE; see the 'START SLAVE Syntax' in the MySQL Manual for more information.
[connection master]
[connection slave]
include/stop_slave.inc
[connection master]
CREATE USER 'plug_user' IDENTIFIED WITH 'test_plugin_server' AS 'plug_user';
GRANT REPLICATION SLAVE ON *.* TO plug_user;
FLUSH PRIVILEGES;
[connection slave]
CHANGE MASTER TO 
MASTER_USER=     'plug_user',
MASTER_PASSWORD= 'plug_user',
MASTER_RETRY_COUNT= 0;
Warnings:
Note	####	Sending passwords in plain text without SSL/TLS is extremely insecure.
Note	####	Storing MySQL user name or password information in the master info repository is not secure and is therefore not recommended. Please consider using the USER and PASSWORD connection options for START SLAVE; see the 'START SLAVE Syntax' in the MySQL Manual for more information.
include/start_slave.inc
# Slave in-sync with master now.
SELECT user, plugin, authentication_string FROM mysql.user WHERE user LIKE 'plug_user';
user	plugin	authentication_string
plug_user	test_plugin_server	plug_user
# Cleanup (on slave).
include/stop_slave.inc
CHANGE MASTER TO MASTER_USER='root';
Warnings:
Note	####	Sending passwords in plain text without SSL/TLS is extremely insecure.
Note	####	Storing MySQL user name or password information in the master info repository is not secure and is therefore not recommended. Please consider using the USER and PASSWORD connection options for START SLAVE; see the 'START SLAVE Syntax' in the MySQL Manual for more information.
DROP USER 'plug_user';
# Cleanup (on master).
DROP USER 'plug_user';
include/rpl_end.inc

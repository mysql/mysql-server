include/master-slave.inc
Warnings:
Note	####	Sending passwords in plain text without SSL/TLS is extremely insecure.
Note	####	Storing MySQL user name or password information in the master info repository is not secure and is therefore not recommended. Please consider using the USER and PASSWORD connection options for START SLAVE; see the 'START SLAVE Syntax' in the MySQL Manual for more information.
[connection master]
#
# 1. Check that the parameter count is greater than 3 and less then 5,
#    i.e. channel_name, hostname and port must be provided.
#
SELECT asynchronous_connection_failover_add_source('', '127.0.0.1');
ERROR HY000: Can't initialize function 'asynchronous_connection_failover_add_source'; Wrong arguments: You need to specify all mandatory arguments.
include/assert.inc ['The performance_schema.replication_asynchronous_connection_failover table is empty']
SELECT asynchronous_connection_failover_add_source('', '127.0.0.1', 3100, 'net', 30, 11);
ERROR HY000: Can't initialize function 'asynchronous_connection_failover_add_source'; Wrong arguments: You must specify max 5 arguments.
include/assert.inc ['The performance_schema.replication_asynchronous_connection_failover table is empty']
#
# 2. Check that the channel name is provided.
#
SELECT asynchronous_connection_failover_add_source(9, '127.0.0.1', 3100);
ERROR HY000: Can't initialize function 'asynchronous_connection_failover_add_source'; Wrong arguments: You need to specify channel name.
include/assert.inc ['The performance_schema.replication_asynchronous_connection_failover table is empty']
#
# 3. Check host parameters values is provided and not empty.
#
SELECT asynchronous_connection_failover_add_source('', '', 3100);
ERROR HY000: Can't initialize function 'asynchronous_connection_failover_add_source'; Wrong arguments: You need to specify hostname.
include/assert.inc ['The performance_schema.replication_asynchronous_connection_failover table is empty']
#
# 4. Check port parameters values is provided.
#
SELECT asynchronous_connection_failover_add_source('', '127.0.0.1', '3100');
ERROR HY000: Can't initialize function 'asynchronous_connection_failover_add_source'; Wrong arguments: You need to specify value for port.
include/assert.inc ['The performance_schema.replication_asynchronous_connection_failover table is empty']
#
# 5. Check the weight parameter value is integer value between 1-100.
#
SELECT asynchronous_connection_failover_add_source('', '127.0.0.1', 3100, '', 0);
ERROR HY000: Can't initialize function 'asynchronous_connection_failover_add_source'; Wrong arguments: The weight argument value should be integer value between 1-100
include/assert.inc ['The performance_schema.replication_asynchronous_connection_failover table is empty']
SELECT asynchronous_connection_failover_add_source('', '127.0.0.1', 3100, '', 200);
ERROR HY000: Can't initialize function 'asynchronous_connection_failover_add_source'; Wrong arguments: The weight argument value should be integer value between 1-100
include/assert.inc ['The performance_schema.replication_asynchronous_connection_failover table is empty']
SELECT asynchronous_connection_failover_add_source('', '127.0.0.1', 3100, '', '100');
ERROR HY000: Can't initialize function 'asynchronous_connection_failover_add_source'; Wrong arguments: The weight argument value should be integer value between 1-100
include/assert.inc ['The performance_schema.replication_asynchronous_connection_failover table is empty']
#
# 6. Check the default value of weight parameter is 50.
#
SELECT asynchronous_connection_failover_add_source('', '127.0.0.1', 3100);
asynchronous_connection_failover_add_source('', '127.0.0.1', 3100)
The UDF asynchronous_connection_failover_add_source() executed successfully.
include/assert.inc ['There is one row in performance_schema.replication_asynchronous_connection_failover']
include/assert.inc ['There is one row in performance_schema.replication_asynchronous_connection_failover']
#
# 7. Check correct string value is provided for network_namespace.
#
SELECT asynchronous_connection_failover_add_source('', '127.0.0.1', 3100, 100);
ERROR HY000: Can't initialize function 'asynchronous_connection_failover_add_source'; Wrong arguments: You need to specify correct value for network_namespace.
include/assert.inc ['There is one row in performance_schema.replication_asynchronous_connection_failover']
SELECT asynchronous_connection_failover_add_source('', '127.0.0.1', 3100, '');
asynchronous_connection_failover_add_source('', '127.0.0.1', 3100, '')
Error executing asynchronous_connection_failover_add_source() UDF function.
include/assert.inc ['There is one row in performance_schema.replication_asynchronous_connection_failover']
#
# 8. Check that the parameter count is greater than 3 and less then 4,
#    i.e. channel_name, hostname and port must be provided.
#
SELECT asynchronous_connection_failover_delete_source('', '127.0.0.1');
ERROR HY000: Can't initialize function 'asynchronous_connection_failover_delete_source'; Wrong arguments: You need to specify all mandatory arguments.
include/assert.inc ['There is one row in performance_schema.replication_asynchronous_connection_failover']
SELECT asynchronous_connection_failover_delete_source('', '127.0.0.1', 3100, 'net', 30);
ERROR HY000: Can't initialize function 'asynchronous_connection_failover_delete_source'; Wrong arguments: You must specify max 4 arguments.
include/assert.inc ['There is one row in performance_schema.replication_asynchronous_connection_failover']
#
# 9. Check that the channel name is provided.
#
SELECT asynchronous_connection_failover_delete_source(9, '127.0.0.1', 3100);
ERROR HY000: Can't initialize function 'asynchronous_connection_failover_delete_source'; Wrong arguments: You need to specify channel name.
include/assert.inc ['There is one row in performance_schema.replication_asynchronous_connection_failover']
#
# 10. Check host parameters values is provided and not empty.
#
SELECT asynchronous_connection_failover_delete_source('', '', 3100);
ERROR HY000: Can't initialize function 'asynchronous_connection_failover_delete_source'; Wrong arguments: You need to specify hostname.
include/assert.inc ['There is one row in performance_schema.replication_asynchronous_connection_failover']
#
# 11. Check port parameters values is provided.
#
SELECT asynchronous_connection_failover_delete_source('', '127.0.0.1', '3100');
ERROR HY000: Can't initialize function 'asynchronous_connection_failover_delete_source'; Wrong arguments: You need to specify value for port.
include/assert.inc ['There is one row in performance_schema.replication_asynchronous_connection_failover']
#
# 12. Check correct string value is provided for network_namespace.
#
SELECT asynchronous_connection_failover_delete_source('', '127.0.0.1', 3100, 100);
ERROR HY000: Can't initialize function 'asynchronous_connection_failover_delete_source'; Wrong arguments: You need to specify correct value for network_namespace.
include/assert.inc ['There is one row in performance_schema.replication_asynchronous_connection_failover']
SELECT asynchronous_connection_failover_delete_source('', '127.0.0.1', 3100, '');
asynchronous_connection_failover_delete_source('', '127.0.0.1', 3100, '')
The UDF asynchronous_connection_failover_delete_source() executed successfully.
include/assert.inc ['The performance_schema.replication_asynchronous_connection_failover table is empty']
#
# 13. Delete non existing credentials.
#
SELECT asynchronous_connection_failover_delete_source('ch2', '127.0.0.1', 77100);
asynchronous_connection_failover_delete_source('ch2', '127.0.0.1', 77100)
Error no matching row was found to be deleted.
include/assert.inc ['The performance_schema.replication_asynchronous_connection_failover table is empty']
#
# 14. Verify user without SUPER or REPLICATION_SLAVE_ADMIN fails with
#     ER_SPECIFIC_ACCESS_DENIED_ERROR error when execute
#     asynchronous_connection_failover_add_source and
#     asynchronous_connection_failover_delete_source UDF.
#
CREATE USER 'async_user' IDENTIFIED BY 'pass';
[connection connect_async]
SELECT asynchronous_connection_failover_add_source('', '127.0.0.1', 3100);
ERROR 42000: Access denied; you need (at least one of) the SUPER or REPLICATION_SLAVE_ADMIN privilege(s) for this operation
SELECT asynchronous_connection_failover_delete_source('', '127.0.0.1', 3100);
ERROR 42000: Access denied; you need (at least one of) the SUPER or REPLICATION_SLAVE_ADMIN privilege(s) for this operation
SELECT * FROM mysql.replication_asynchronous_connection_failover;
ERROR 42000: SELECT command denied to user 'async_user'@'localhost' for table 'replication_asynchronous_connection_failover'
#
# 15. Verify user with REPLICATION_SLAVE_ADMIN privilege can execute
#     asynchronous_connection_failover_add_source and
#     asynchronous_connection_failover_delete_source UDF.
#
[connection master]
GRANT REPLICATION_SLAVE_ADMIN ON *.* TO 'async_user';
[connection connect_async]
SELECT asynchronous_connection_failover_add_source('', '127.0.0.1', 3100);
asynchronous_connection_failover_add_source('', '127.0.0.1', 3100)
The UDF asynchronous_connection_failover_add_source() executed successfully.
SELECT asynchronous_connection_failover_delete_source('', '127.0.0.1', 3100);
asynchronous_connection_failover_delete_source('', '127.0.0.1', 3100)
The UDF asynchronous_connection_failover_delete_source() executed successfully.
[connection master]
DROP USER 'async_user';
include/rpl_end.inc

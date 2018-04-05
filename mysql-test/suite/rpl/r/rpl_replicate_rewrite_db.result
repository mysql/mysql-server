include/master-slave.inc
Warnings:
Note	####	Sending passwords in plain text without SSL/TLS is extremely insecure.
Note	####	Storing MySQL user name or password information in the master info repository is not secure and is therefore not recommended. Please consider using the USER and PASSWORD connection options for START SLAVE; see the 'START SLAVE Syntax' in the MySQL Manual for more information.
[connection master]
include/rpl_stop_server.inc [server_number=1]
testing for normal fuctionality
include/rpl_start_server.inc [server_number=1 parameters: --replicate-rewrite-db='mysql->test']
[PASS]
include/rpl_stop_server.inc [server_number=1]
testing with single letter databases name.
include/rpl_start_server.inc [server_number=1 parameters: --replicate-rewrite-db='a->b']
[PASS]
check for '->' operator not found. Should FAIL with error
[FAIL]
check for empty "from" db name. Should FAIL with error
[FAIL]
check for empty "to" db name. Should FAIL with error
[FAIL]
[connection slave]
include/start_slave.inc
include/rpl_end.inc

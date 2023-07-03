include/master-slave.inc
Warnings:
Note	####	Sending passwords in plain text without SSL/TLS is extremely insecure.
Note	####	Storing MySQL user name or password information in the master info repository is not secure and is therefore not recommended. Please consider using the USER and PASSWORD connection options for START SLAVE; see the 'START SLAVE Syntax' in the MySQL Manual for more information.
[connection master]
START GROUP_REPLICATION;
ERROR HY000: The server is not configured properly to be an active member of the group. Please see more details on error log.
# Asserting that the error log has the description of the error
# happening in this case.
Occurrences of 'Group Replication plugin is not installed.' in the input file: 1
STOP GROUP_REPLICATION;
ERROR HY000: The server is not configured properly to be an active member of the group. Please see more details on error log.
# Asserting that the error log has the description of the error
# happening in this case.
Occurrences of 'Group Replication plugin is not installed.' in the input file: 2
include/rpl_end.inc

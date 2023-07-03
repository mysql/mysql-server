#
# Bug #22086705: ALTER NOT EXISTING DATABASE BREAKS REPLICATION
#
# Problem: Binlog is written before doing the change in the dd.
# Checking for existence is part of the dd update.
#
# Fix: Do the change in the dd before writing binlog. Non-existing
# databases will cause an early exit prior to writing binlog.
#
# Test written by Matthias Leich.
include/master-slave.inc
Warnings:
Note	####	Sending passwords in plain text without SSL/TLS is extremely insecure.
Note	####	Storing MySQL user name or password information in the master info repository is not secure and is therefore not recommended. Please consider using the USER and PASSWORD connection options for START SLAVE; see the 'START SLAVE Syntax' in the MySQL Manual for more information.
[connection master]
ALTER DATABASE does_not_exist CHARACTER SET  utf8;
ERROR 42Y07: Database 'does_not_exist' doesn't exist
include/sync_slave_sql_with_master.inc
include/sync_slave_sql_with_master.inc
include/rpl_end.inc

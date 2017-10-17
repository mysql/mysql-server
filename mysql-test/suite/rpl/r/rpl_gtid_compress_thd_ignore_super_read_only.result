include/master-slave.inc
Warnings:
Note	####	Sending passwords in plain text without SSL/TLS is extremely insecure.
Note	####	Storing MySQL user name or password information in the master info repository is not secure and is therefore not recommended. Please consider using the USER and PASSWORD connection options for START SLAVE; see the 'START SLAVE Syntax' in the MySQL Manual for more information.
[connection master]
include/rpl_reset.inc
[connection slave]
call mtr.add_suppression("You need to use --log-bin*");
SET @saved_value_super= @@GLOBAL.super_read_only;
SET @saved_value= @@GLOBAL.read_only;
SET GLOBAL super_read_only= 1;
include/assert.inc ['gtid_executed_compression_period global variable is set to 5']
[connection master]
CREATE TABLE t1(a INT) ENGINE=INNODB;
INSERT INTO t1 VALUES(1);
INSERT INTO t1 VALUES(2);
INSERT INTO t1 VALUES(3);
INSERT INTO t1 VALUES(4);
DROP TABLE t1;
include/sync_slave_sql_with_master.inc
#
# connection slave
#
include/assert.inc [committed gtids MASTER_UUID:1-6]
#
# Wait till gtid_executed table has one row and verify that
# it is MASTER_UUID:1:6
#
SELECT * FROM mysql.gtid_executed;
source_uuid	interval_start	interval_end
MASTER_UUID	1	6
"Clean up"
SET GLOBAL super_read_only= @saved_value_super;
SET GLOBAL read_only= @saved_value;
include/rpl_end.inc

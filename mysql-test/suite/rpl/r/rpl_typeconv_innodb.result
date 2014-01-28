include/master-slave.inc
Warnings:
Note	####	Sending passwords in plain text without SSL/TLS is extremely insecure.
Note	####	Storing MySQL user name or password information in the master info repository is not secure and is therefore not recommended. Please consider using the USER and PASSWORD connection options for START SLAVE; see the 'START SLAVE Syntax' in the MySQL Manual for more information.
[connection master]
SET @saved_slave_type_conversions = @@GLOBAL.SLAVE_TYPE_CONVERSIONS;
SET GLOBAL SLAVE_TYPE_CONVERSIONS = '';
CREATE TABLE t1(b1 BIT(1), b2 BIT(2), b3 BIT(3)) ENGINE=InnoDB;
INSERT INTO t1 VALUES (b'0', b'01', b'101');
include/sync_slave_sql_with_master.inc
include/diff_tables.inc [master:t1, slave:t1]
DROP TABLE t1;
include/sync_slave_sql_with_master.inc
SET GLOBAL SLAVE_TYPE_CONVERSIONS = @saved_slave_type_conversions;
include/rpl_end.inc

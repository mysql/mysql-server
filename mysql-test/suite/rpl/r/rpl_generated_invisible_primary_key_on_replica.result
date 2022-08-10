
##############################################################
# 1. In the replica enable, in the session and globally:
#    - sql_generate_invisible_primary_key
include/master-slave.inc
Warnings:
Note	####	Sending passwords in plain text without SSL/TLS is extremely insecure.
Note	####	Storing MySQL user name or password information in the master info repository is not secure and is therefore not recommended. Please consider using the USER and PASSWORD connection options for START SLAVE; see the 'START SLAVE Syntax' in the MySQL Manual for more information.
[connection master]
[connection slave]
include/save_sysvars.inc [ "GLOBAL.sql_generate_invisible_primary_key" ]
SET GLOBAL sql_generate_invisible_primary_key = ON;
SET SESSION sql_generate_invisible_primary_key = ON;

##############################################################
# 2. Create a table without a primary key on the source
#    The table should not have a primary key when replicated
[connection master]
CREATE TABLE t1(a int);
[connection slave]
include/start_slave.inc
[connection master]
include/sync_slave_sql_with_master.inc
include/assert.inc [The table only contains a column]
include/assert.inc [The table does not contain a generated invisilble primary key]

##############################################################
# 3. Create a table without a primary key on the replica
#    The table should have a generated primary key
CREATE TABLE t2(a int);
SHOW CREATE TABLE t2;
Table	Create Table
t2	CREATE TABLE `t2` (
  `my_row_id` bigint unsigned NOT NULL AUTO_INCREMENT /*!80023 INVISIBLE */,
  `a` int DEFAULT NULL,
  PRIMARY KEY (`my_row_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
include/assert.inc [The table contains 2 columns]
include/assert.inc [The table contains a generated invisilble primary key]

##############################################################
# 4. Cleanup
include/restore_sysvars.inc
DROP TABLE t2;
[connection master]
DROP TABLE t1;
include/rpl_end.inc

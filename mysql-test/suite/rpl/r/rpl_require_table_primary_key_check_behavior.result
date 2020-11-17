
# 0. Setup a master slave connection with a privilege user
include/master-slave.inc
Warnings:
Note	####	Sending passwords in plain text without SSL/TLS is extremely insecure.
Note	####	Storing MySQL user name or password information in the master info repository is not secure and is therefore not recommended. Please consider using the USER and PASSWORD connection options for START SLAVE; see the 'START SLAVE Syntax' in the MySQL Manual for more information.
[connection master]
[connection slave]
GRANT REPLICATION_APPLIER,CREATE,DROP ON *.* TO 'u'@'localhost';
CHANGE MASTER TO REQUIRE_TABLE_PRIMARY_KEY_CHECK = STREAM;

# 1. While REQUIRE_TABLE_PRIMARY_KEY_CHECK = STREAM.
#    Create a table on the master
#    The slave applier user lacks privileges for CREATE/ALTER queries
#    The slave applier fails while replicating.
[connection master]
SET @@global.sql_require_primary_key = 1;
CREATE TABLE t1 (a INT PRIMARY KEY);
[connection slave]
START SLAVE;
Warnings:
Warning	1287	'START SLAVE' is deprecated and will be removed in a future release. Please use START REPLICA instead
include/wait_for_slave_sql_error.inc [errno=1227]

# 2. While REQUIRE_TABLE_PRIMARY_KEY_CHECK = STREAM.
#    Give the slave applier user SESSION_VARIABLES_ADMIN privileges
#    The slave applier can now replicate CREATE/ALTER queries
STOP SLAVE;
Warnings:
Warning	1287	'STOP SLAVE' is deprecated and will be removed in a future release. Please use STOP REPLICA instead
GRANT SESSION_VARIABLES_ADMIN ON *.* TO 'u'@'localhost';
START SLAVE;
Warnings:
Warning	1287	'START SLAVE' is deprecated and will be removed in a future release. Please use START REPLICA instead
[connection master]
include/sync_slave_sql_with_master.inc

# 3. While REQUIRE_TABLE_PRIMARY_KEY_CHECK = ON.
#    Revoke privileges.
#    Create a table on the master with no primary key
#    The slave applier fails while replicating due to the lack of PK
STOP SLAVE;
Warnings:
Warning	1287	'STOP SLAVE' is deprecated and will be removed in a future release. Please use STOP REPLICA instead
REVOKE SESSION_VARIABLES_ADMIN ON *.* FROM 'u'@'localhost';
CHANGE MASTER TO REQUIRE_TABLE_PRIMARY_KEY_CHECK = ON;
[connection master]
CREATE TABLE t2 (a INT);
[connection slave]
START SLAVE;
Warnings:
Warning	1287	'START SLAVE' is deprecated and will be removed in a future release. Please use START REPLICA instead
include/wait_for_slave_sql_error.inc [errno=3750]
[connection master]
DROP TABLE t2;
RESET MASTER;
[connection slave]
STOP SLAVE;
Warnings:
Warning	1287	'STOP SLAVE' is deprecated and will be removed in a future release. Please use STOP REPLICA instead
RESET SLAVE;
Warnings:
Warning	1287	'RESET SLAVE' is deprecated and will be removed in a future release. Please use RESET REPLICA instead
RESET MASTER;

# 4. While REQUIRE_TABLE_PRIMARY_KEY_CHECK = ON.
#    Create a table on the master with a primary key
#    The slave applier replication is working properly
[connection master]
SET @@global.sql_require_primary_key = 0;
CREATE TABLE t3 (a INT PRIMARY KEY);
[connection slave]
START SLAVE;
Warnings:
Warning	1287	'START SLAVE' is deprecated and will be removed in a future release. Please use START REPLICA instead
[connection master]
include/sync_slave_sql_with_master.inc

# 5. While REQUIRE_TABLE_PRIMARY_KEY_CHECK = OFF.
#    Create a table on the master with a primary key and without
#    The slave applier replication replicates both
STOP SLAVE;
Warnings:
Warning	1287	'STOP SLAVE' is deprecated and will be removed in a future release. Please use STOP REPLICA instead
CHANGE MASTER TO REQUIRE_TABLE_PRIMARY_KEY_CHECK = OFF;
START SLAVE;
Warnings:
Warning	1287	'START SLAVE' is deprecated and will be removed in a future release. Please use START REPLICA instead
[connection master]
SET @@global.sql_require_primary_key = 1;
CREATE TABLE t4 (a INT PRIMARY KEY);
SET @@global.sql_require_primary_key = 0;
CREATE TABLE t5 (a INT);
include/sync_slave_sql_with_master.inc

# 6. Cleanup
[connection master]
DROP TABLE t1;
DROP TABLE t3;
DROP TABLE t4;
DROP TABLE t5;
SET @@global.sql_require_primary_key = SQL_REQUIRE_PRIMARY_KEY;
[connection slave]
CALL mtr.add_suppression(".*Access denied; you need .at least one of. the SUPER, SYSTEM_VARIABLES_ADMIN or SESSION_VARIABLES_ADMIN privilege.*");
CALL mtr.add_suppression(".*The slave coordinator and worker threads are stopped, possibly leaving data in inconsistent state.*");
CALL mtr.add_suppression(".*Unable to create or change a table without a primary key*");
include/stop_slave_sql.inc
CHANGE MASTER TO REQUIRE_TABLE_PRIMARY_KEY_CHECK = PRIMARY_KEY_CHECK_VALUE;
include/start_slave_sql.inc
include/rpl_end.inc

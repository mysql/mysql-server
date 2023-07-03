#
# WL#14722 Support IF NOT EXISTS clause
# in CREATE PROCEDURE/FUNCTION/TRIGGER
#
include/master-slave.inc
Warnings:
Note	####	Sending passwords in plain text without SSL/TLS is extremely insecure.
Note	####	Storing MySQL user name or password information in the master info repository is not secure and is therefore not recommended. Please consider using the USER and PASSWORD connection options for START SLAVE; see the 'START SLAVE Syntax' in the MySQL Manual for more information.
[connection master]

# Create objects on source
[On Source]
CREATE TABLE t1 (a INT);
CREATE PROCEDURE IF NOT EXISTS sp1() BEGIN END;
CREATE FUNCTION IF NOT EXISTS sf1() RETURNS INT DETERMINISTIC return 0;
CREATE FUNCTION IF NOT EXISTS metaphon RETURNS STRING SONAME "UDF_EXAMPLE_LIB";
CREATE TRIGGER IF NOT EXISTS trg1 BEFORE INSERT ON t1 FOR EACH ROW BEGIN END;

# Check if objects are created on replica
[On Replica]
include/sync_slave_sql_with_master.inc
SELECT routine_name FROM information_schema.routines WHERE routine_schema = "test";
ROUTINE_NAME
sf1
sp1
SELECT name FROM mysql.func WHERE name = 'metaphon';
name
metaphon
SELECT trigger_name FROM information_schema.triggers WHERE trigger_schema = "test";
TRIGGER_NAME
trg1

# Drop objects on replica
DROP PROCEDURE sp1;
DROP FUNCTION sf1;
DROP FUNCTION metaphon;
DROP TRIGGER trg1;

# Create objects with IF NOT EXISTS claus when objects already exist on source.
# These statements should re-create objects on replica
[On Source]
CREATE PROCEDURE IF NOT EXISTS sp1() BEGIN END;
Warnings:
Note	1304	PROCEDURE sp1 already exists
CREATE FUNCTION IF NOT EXISTS sf1() RETURNS INT DETERMINISTIC return 0;
Warnings:
Note	1304	FUNCTION sf1 already exists
CREATE FUNCTION IF NOT EXISTS metaphon RETURNS STRING SONAME "UDF_EXAMPLE_LIB";
Warnings:
Note	1125	Function 'metaphon' already exists
CREATE TRIGGER IF NOT EXISTS trg1 BEFORE INSERT ON t1 FOR EACH ROW BEGIN END;
Warnings:
Note	4099	Trigger 'trg1' already exists on the table 'test'.'t1'.

# Check if objects are re-created on replica
[On Replica]
include/sync_slave_sql_with_master.inc
SELECT routine_name FROM information_schema.routines WHERE routine_schema = "test";
ROUTINE_NAME
sf1
sp1
SELECT name FROM mysql.func WHERE name = 'metaphon';
name
metaphon
SELECT trigger_name FROM information_schema.triggers WHERE trigger_schema = "test";
TRIGGER_NAME
trg1

# Cleanup
[On Source]
DROP PROCEDURE sp1;
DROP FUNCTION sf1;
DROP FUNCTION metaphon;
DROP TRIGGER trg1;
DROP TABLE t1;
include/sync_slave_sql_with_master.inc
include/rpl_end.inc

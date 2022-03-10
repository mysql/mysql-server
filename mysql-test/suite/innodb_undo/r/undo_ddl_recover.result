#
# Test recovery when a crash happens during undo tablespace DDL.
#
#
# Create and populate the test-bed for Undo DDL Recover tests
#
CREATE TABLE t1 (c1 INT, c2 TEXT, c3 TEXT, PRIMARY KEY(c1));
CREATE PROCEDURE populate_t1()
BEGIN
DECLARE i INT DEFAULT 1;
WHILE (i <= 100) DO
INSERT INTO t1 VALUES (i, repeat('a', i * 100), repeat('b', 10000 - (i * 100)));
SET i = i + 1;
END WHILE;
END |
CREATE UNDO TABLESPACE undo_003 ADD DATAFILE 'undo_003.ibu';
CALL populate_t1();
DELETE FROM t1 WHERE (c1 % 5 = 0);
UPDATE t1 SET c2 = 'mysql' WHERE  (c1 % 4 = 0);
UPDATE t1 SET c3 = 'oracle' WHERE (c1 % 3 = 0);
ALTER UNDO TABLESPACE undo_003 SET INACTIVE;
#
# Show that undo_003 exists and is empty.
#
SELECT NAME, SPACE_TYPE, STATE FROM INFORMATION_SCHEMA.INNODB_TABLESPACES
WHERE SPACE_TYPE = 'Undo';
NAME	SPACE_TYPE	STATE
innodb_undo_001	Undo	active
innodb_undo_002	Undo	active
undo_003	Undo	empty
SELECT TABLESPACE_NAME, FILE_TYPE, FILE_NAME FROM INFORMATION_SCHEMA.FILES
WHERE TABLESPACE_NAME LIKE '%undo%' ORDER BY TABLESPACE_NAME;
TABLESPACE_NAME	FILE_TYPE	FILE_NAME
innodb_undo_001	UNDO LOG	./undo_001
innodb_undo_002	UNDO LOG	./undo_002
undo_003	UNDO LOG	./undo_003.ibu
#
# Crash before deleting the tablespace file during the DDL_LOG apply.
#
SET GLOBAL DEBUG = "+d,ddl_log_replay_delete_space_crash_before_drop";
DROP UNDO TABLESPACE undo_003;
"Table definition is not as expected!"
SET GLOBAL DEBUG = "-d,ddl_log_replay_delete_space_crash_before_drop";
#
# Show that undo_003 is fully dropped.
#
SELECT NAME, SPACE_TYPE, STATE FROM INFORMATION_SCHEMA.INNODB_TABLESPACES
WHERE SPACE_TYPE = 'Undo';
NAME	SPACE_TYPE	STATE
innodb_undo_001	Undo	active
innodb_undo_002	Undo	active
SELECT TABLESPACE_NAME, FILE_TYPE, FILE_NAME FROM INFORMATION_SCHEMA.FILES
WHERE FILE_NAME LIKE '%undo%' ORDER BY TABLESPACE_NAME;
TABLESPACE_NAME	FILE_TYPE	FILE_NAME
innodb_undo_001	UNDO LOG	./undo_001
innodb_undo_002	UNDO LOG	./undo_002
### Undo files in datadir ###
undo_001
undo_002
#
# Cleanup
#
DROP PROCEDURE populate_t1;
DROP TABLE t1;
# restart

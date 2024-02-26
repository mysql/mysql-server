# ===== csv_alter_table.1 =====
DROP TABLE IF EXISTS t1;
CREATE TABLE t1 (a int NOT NULL) ENGINE = CSV;
ALTER TABLE t1 ADD COLUMN b CHAR(5) NOT NULL;
DESC t1;
Field	Type	Null	Key	Default	Extra
a	int	NO		NULL	
b	char(5)	NO		NULL	
ALTER TABLE t1 DROP COLUMN b;
DESC t1;
Field	Type	Null	Key	Default	Extra
a	int	NO		NULL	
ALTER TABLE t1 MODIFY a BIGINT NOT NULL;
DESC t1;
Field	Type	Null	Key	Default	Extra
a	bigint	NO		NULL	
ALTER TABLE t1 CHANGE a a INT NOT NULL;
DESC t1;
Field	Type	Null	Key	Default	Extra
a	int	NO		NULL	
DROP TABLE t1;
# ===== csv_alter_table.2 =====
DROP TABLE IF EXISTS t1;
CREATE TABLE t1 (a int NOT NULL) ENGINE = CSV;
ALTER TABLE t1 ADD COLUMN b CHAR(5);
ERROR 42000: The storage engine for the table doesn't support nullable columns
DESC t1;
Field	Type	Null	Key	Default	Extra
a	int	NO		NULL	
ALTER TABLE t1 MODIFY a BIGINT;
ERROR 42000: The storage engine for the table doesn't support nullable columns
DESC t1;
Field	Type	Null	Key	Default	Extra
a	int	NO		NULL	
ALTER TABLE t1 CHANGE a a INT;
ERROR 42000: The storage engine for the table doesn't support nullable columns
DESC t1;
Field	Type	Null	Key	Default	Extra
a	int	NO		NULL	
DROP TABLE t1;

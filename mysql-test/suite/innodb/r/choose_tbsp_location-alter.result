#
# This is a copy of innodb-alter.test except using remote tablespaces
# and showing those files.
#
SET default_storage_engine=InnoDB;
SET GLOBAL innodb_file_per_table=ON;
SET NAMES utf8;
Warnings:
Warning	3719	'utf8' is currently an alias for the character set UTF8MB3, but will be an alias for UTF8MB4 in a future release. Please consider using UTF8MB4 in order to be unambiguous.
CREATE TABLE t1 (
c1 INT PRIMARY KEY, c2 INT DEFAULT 1, ct TEXT,
INDEX(c2))
ENGINE=InnoDB DATA DIRECTORY='MYSQL_TMP_DIR/alt_dir';
INSERT INTO t1 SET c1=1;
CREATE TABLE sys_tables SELECT * FROM INFORMATION_SCHEMA.INNODB_TABLES
WHERE NAME LIKE 'test/t%';
CREATE TABLE sys_indexes SELECT i.* FROM INFORMATION_SCHEMA.INNODB_INDEXES i
INNER JOIN sys_tables st ON i.TABLE_ID=st.TABLE_ID;
CREATE TABLE t1p LIKE t1;
CREATE TABLE t1c (c1 INT PRIMARY KEY, c2 INT, c3 INT, INDEX(c2), INDEX(c3),
CONSTRAINT t1c2 FOREIGN KEY (c2) REFERENCES t1(c2),
CONSTRAINT t1c3 FOREIGN KEY (c3) REFERENCES t1p(c2))
ENGINE=InnoDB DATA DIRECTORY='MYSQL_TMP_DIR/alt_dir';
CREATE TABLE sys_foreign SELECT i.*
FROM INFORMATION_SCHEMA.INNODB_FOREIGN i
WHERE FOR_NAME LIKE 'test/t%';
SELECT i.FOR_COL_NAME, i.REF_COL_NAME, i.POS FROM INFORMATION_SCHEMA.INNODB_FOREIGN_COLS i
INNER JOIN sys_foreign sf ON i.ID = sf.ID;
FOR_COL_NAME	REF_COL_NAME	POS
c2	c2	1
c3	c2	1
SELECT i.NAME,i.POS,i.MTYPE,i.PRTYPE,i.LEN
FROM INFORMATION_SCHEMA.INNODB_COLUMNS i
INNER JOIN sys_tables st ON i.TABLE_ID=st.TABLE_ID ORDER BY i.NAME,i.POS;
NAME	POS	MTYPE	PRTYPE	LEN
c1	0	6	1283	4
c2	1	6	1027	4
ct	2	5	16711932	10
SELECT si.NAME,i.POS,i.NAME FROM INFORMATION_SCHEMA.INNODB_FIELDS i
INNER JOIN sys_indexes si ON i.INDEX_ID=si.INDEX_ID ORDER BY si.NAME, i.POS;
NAME	POS	NAME
c2	0	c2
PRIMARY	0	c1
SELECT i.FOR_COL_NAME, i.REF_COL_NAME, i.POS FROM INFORMATION_SCHEMA.INNODB_FOREIGN_COLS i
INNER JOIN sys_foreign sf ON i.ID = sf.ID;
FOR_COL_NAME	REF_COL_NAME	POS
c2	c2	1
c3	c2	1
SHOW CREATE TABLE t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `c1` int NOT NULL,
  `c2` int DEFAULT '1',
  `ct` text,
  PRIMARY KEY (`c1`),
  KEY `c2` (`c2`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci DATA DIRECTORY='MYSQL_TMP_DIR/alt_dir/'
ALTER TABLE t1 ALTER c2 DROP DEFAULT;
SHOW CREATE TABLE t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `c1` int NOT NULL,
  `c2` int,
  `ct` text,
  PRIMARY KEY (`c1`),
  KEY `c2` (`c2`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci DATA DIRECTORY='MYSQL_TMP_DIR/alt_dir/'
### files in MYSQL_DATA_DIR/test
sys_foreign.ibd
sys_indexes.ibd
sys_tables.ibd
t1p.ibd
### files in MYSQL_TMP_DIR/alt_dir/test
t1.ibd
t1c.ibd
SELECT i.NAME,i.POS,i.MTYPE,i.PRTYPE,i.LEN
FROM INFORMATION_SCHEMA.INNODB_COLUMNS i
INNER JOIN sys_tables st ON i.TABLE_ID=st.TABLE_ID ORDER BY i.NAME,i.POS;
NAME	POS	MTYPE	PRTYPE	LEN
c1	0	6	1283	4
c2	1	6	1027	4
ct	2	5	16711932	10
SELECT si.NAME,i.POS,i.NAME FROM INFORMATION_SCHEMA.INNODB_FIELDS i
INNER JOIN sys_indexes si ON i.INDEX_ID=si.INDEX_ID ORDER BY si.NAME, i.POS;
NAME	POS	NAME
c2	0	c2
PRIMARY	0	c1
SELECT i.FOR_COL_NAME, i.REF_COL_NAME, i.POS FROM INFORMATION_SCHEMA.INNODB_FOREIGN_COLS i
INNER JOIN sys_foreign sf ON i.ID = sf.ID;
FOR_COL_NAME	REF_COL_NAME	POS
c2	c2	1
c3	c2	1
ALTER TABLE t1 CHANGE c2 c2 INT AFTER c1;
ALTER TABLE t1 CHANGE c1 c1 INT FIRST;
### files in MYSQL_DATA_DIR/test
sys_foreign.ibd
sys_indexes.ibd
sys_tables.ibd
t1p.ibd
### files in MYSQL_TMP_DIR/alt_dir/test
t1.ibd
t1c.ibd
SELECT i.NAME,i.POS,i.MTYPE,i.PRTYPE,i.LEN
FROM INFORMATION_SCHEMA.INNODB_COLUMNS i
INNER JOIN sys_tables st ON i.TABLE_ID=st.TABLE_ID ORDER BY i.NAME,i.POS;
NAME	POS	MTYPE	PRTYPE	LEN
c1	0	6	1283	4
c2	1	6	1027	4
ct	2	5	16711932	10
SELECT si.NAME,i.POS,i.NAME FROM INFORMATION_SCHEMA.INNODB_FIELDS i
INNER JOIN sys_indexes si ON i.INDEX_ID=si.INDEX_ID ORDER BY si.NAME, i.POS;
NAME	POS	NAME
c2	0	c2
PRIMARY	0	c1
SELECT i.FOR_COL_NAME, i.REF_COL_NAME, i.POS FROM INFORMATION_SCHEMA.INNODB_FOREIGN_COLS i
INNER JOIN sys_foreign sf ON i.ID = sf.ID;
FOR_COL_NAME	REF_COL_NAME	POS
c2	c2	1
c3	c2	1
ALTER TABLE t1 CHANGE C2 c3 INT;
### files in MYSQL_DATA_DIR/test
sys_foreign.ibd
sys_indexes.ibd
sys_tables.ibd
t1p.ibd
### files in MYSQL_TMP_DIR/alt_dir/test
t1.ibd
t1c.ibd
SELECT i.NAME,i.POS,i.MTYPE,i.PRTYPE,i.LEN
FROM INFORMATION_SCHEMA.INNODB_COLUMNS i
INNER JOIN sys_tables st ON i.TABLE_ID=st.TABLE_ID ORDER BY i.NAME,i.POS;
NAME	POS	MTYPE	PRTYPE	LEN
c1	0	6	1283	4
c3	1	6	1027	4
ct	2	5	16711932	10
SELECT si.NAME,i.POS,i.NAME FROM INFORMATION_SCHEMA.INNODB_FIELDS i
INNER JOIN sys_indexes si ON i.INDEX_ID=si.INDEX_ID ORDER BY si.NAME, i.POS;
NAME	POS	NAME
c2	0	c3
PRIMARY	0	c1
SELECT i.FOR_COL_NAME, i.REF_COL_NAME, i.POS FROM INFORMATION_SCHEMA.INNODB_FOREIGN_COLS i
INNER JOIN sys_foreign sf ON i.ID = sf.ID;
FOR_COL_NAME	REF_COL_NAME	POS
c2	c3	1
c3	c2	1
ALTER TABLE t1 CHANGE c3 C INT;
### files in MYSQL_DATA_DIR/test
sys_foreign.ibd
sys_indexes.ibd
sys_tables.ibd
t1p.ibd
### files in MYSQL_TMP_DIR/alt_dir/test
t1.ibd
t1c.ibd
SELECT i.NAME,i.POS,i.MTYPE,i.PRTYPE,i.LEN
FROM INFORMATION_SCHEMA.INNODB_COLUMNS i
INNER JOIN sys_tables st ON i.TABLE_ID=st.TABLE_ID ORDER BY i.NAME,i.POS;
NAME	POS	MTYPE	PRTYPE	LEN
C	1	6	1027	4
c1	0	6	1283	4
ct	2	5	16711932	10
SELECT si.NAME,i.POS,i.NAME FROM INFORMATION_SCHEMA.INNODB_FIELDS i
INNER JOIN sys_indexes si ON i.INDEX_ID=si.INDEX_ID ORDER BY si.NAME, i.POS;
NAME	POS	NAME
c2	0	C
PRIMARY	0	c1
SELECT i.FOR_COL_NAME, i.REF_COL_NAME, i.POS FROM INFORMATION_SCHEMA.INNODB_FOREIGN_COLS i
INNER JOIN sys_foreign sf ON i.ID = sf.ID;
FOR_COL_NAME	REF_COL_NAME	POS
c2	C	1
c3	c2	1
ALTER TABLE t1 CHANGE C Cöŀumň_TWO INT;
### files in MYSQL_DATA_DIR/test
sys_foreign.ibd
sys_indexes.ibd
sys_tables.ibd
t1p.ibd
### files in MYSQL_TMP_DIR/alt_dir/test
t1.ibd
t1c.ibd
SELECT i.FOR_COL_NAME, i.REF_COL_NAME, i.POS FROM INFORMATION_SCHEMA.INNODB_FOREIGN_COLS i
INNER JOIN sys_foreign sf ON i.ID = sf.ID;
FOR_COL_NAME	REF_COL_NAME	POS
c2	Cöŀumň_TWO	1
c3	c2	1
SELECT i.NAME,i.POS,i.MTYPE,i.PRTYPE,i.LEN
FROM INFORMATION_SCHEMA.INNODB_COLUMNS i
INNER JOIN sys_tables st ON i.TABLE_ID=st.TABLE_ID ORDER BY i.NAME,i.POS;
NAME	POS	MTYPE	PRTYPE	LEN
c1	0	6	1283	4
Cöŀumň_TWO	1	6	1027	4
ct	2	5	16711932	10
SELECT si.NAME,i.POS,i.NAME FROM INFORMATION_SCHEMA.INNODB_FIELDS i
INNER JOIN sys_indexes si ON i.INDEX_ID=si.INDEX_ID ORDER BY si.NAME, i.POS;
NAME	POS	NAME
c2	0	Cöŀumň_TWO
PRIMARY	0	c1
SELECT i.FOR_COL_NAME, i.REF_COL_NAME, i.POS FROM INFORMATION_SCHEMA.INNODB_FOREIGN_COLS i
INNER JOIN sys_foreign sf ON i.ID = sf.ID;
FOR_COL_NAME	REF_COL_NAME	POS
c2	Cöŀumň_TWO	1
c3	c2	1
ALTER TABLE t1 CHANGE cöĿǖmň_two c3 INT;
ERROR 42S22: Unknown column 'cöĿǖmň_two' in 't1'
ALTER TABLE t1 CHANGE cÖĿUMŇ_two c3 INT, RENAME TO t3;
### files in MYSQL_DATA_DIR/test
sys_foreign.ibd
sys_indexes.ibd
sys_tables.ibd
t1p.ibd
### files in MYSQL_TMP_DIR/alt_dir/test
t1c.ibd
t3.ibd
SELECT st.NAME, i.NAME
FROM sys_tables st INNER JOIN INFORMATION_SCHEMA.INNODB_TABLES i
ON i.TABLE_ID=st.TABLE_ID;
NAME	NAME
test/t1	test/t3
SHOW CREATE TABLE t3;
Table	Create Table
t3	CREATE TABLE `t3` (
  `c1` int NOT NULL,
  `c3` int DEFAULT NULL,
  `ct` text,
  PRIMARY KEY (`c1`),
  KEY `c2` (`c3`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci DATA DIRECTORY='MYSQL_TMP_DIR/alt_dir/'
SHOW CREATE TABLE t1c;
Table	Create Table
t1c	CREATE TABLE `t1c` (
  `c1` int NOT NULL,
  `c2` int DEFAULT NULL,
  `c3` int DEFAULT NULL,
  PRIMARY KEY (`c1`),
  KEY `c2` (`c2`),
  KEY `c3` (`c3`),
  CONSTRAINT `t1c2` FOREIGN KEY (`c2`) REFERENCES `t3` (`c3`),
  CONSTRAINT `t1c3` FOREIGN KEY (`c3`) REFERENCES `t1p` (`c2`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci DATA DIRECTORY='MYSQL_TMP_DIR/alt_dir/'
ALTER TABLE t3 CHANGE c3
`12345678901234567890123456789012345678901234567890123456789012345` INT;
ERROR 42000: Identifier name '12345678901234567890123456789012345678901234567890123456789012345' is too long
ALTER TABLE t3 CHANGE c3
`1234567890123456789012345678901234567890123456789012345678901234` INT;
### files in MYSQL_DATA_DIR/test
sys_foreign.ibd
sys_indexes.ibd
sys_tables.ibd
t1p.ibd
### files in MYSQL_TMP_DIR/alt_dir/test
t1c.ibd
t3.ibd
SHOW CREATE TABLE t3;
Table	Create Table
t3	CREATE TABLE `t3` (
  `c1` int NOT NULL,
  `1234567890123456789012345678901234567890123456789012345678901234` int DEFAULT NULL,
  `ct` text,
  PRIMARY KEY (`c1`),
  KEY `c2` (`1234567890123456789012345678901234567890123456789012345678901234`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci DATA DIRECTORY='MYSQL_TMP_DIR/alt_dir/'
ALTER TABLE t3 CHANGE
`1234567890123456789012345678901234567890123456789012345678901234`
`倀倁倂倃倄倅倆倇倈倉倊個倌倍倎倏倐們倒倓倔倕倖倗倘候倚倛倜倝倞借倠倡倢倣値倥倦倧倨倩倪倫倬倭倮倯倰倱倲倳倴倵倶倷倸倹债倻值倽倾倿偀` INT;
ERROR 42000: Identifier name '倀倁倂倃倄倅倆倇倈倉倊個倌倍倎倏倐們倒倓倔倕倖倗倘候倚倛倜倝倞借倠?' is too long
ALTER TABLE t3 CHANGE
`1234567890123456789012345678901234567890123456789012345678901234`
`倀倁倂倃倄倅倆倇倈倉倊個倌倍倎倏倐們倒倓倔倕倖倗倘候倚倛倜倝倞借倠倡倢倣値倥倦倧倨倩倪倫倬倭倮倯倰倱倲倳倴倵倶倷倸倹债倻值倽倾倿ä` INT;
ERROR 42000: Identifier name '倀倁倂倃倄倅倆倇倈倉倊個倌倍倎倏倐們倒倓倔倕倖倗倘候倚倛倜倝倞借倠?' is too long
ALTER TABLE t3 CHANGE
`1234567890123456789012345678901234567890123456789012345678901234`
`倀倁倂倃倄倅倆倇倈倉倊個倌倍倎倏倐們倒倓倔倕倖倗倘候倚倛倜倝倞借倠倡倢倣値倥倦倧倨倩倪倫倬倭倮倯倰倱倲倳倴倵倶倷倸倹债倻值倽倾ä` INT;
### files in MYSQL_DATA_DIR/test
sys_foreign.ibd
sys_indexes.ibd
sys_tables.ibd
t1p.ibd
### files in MYSQL_TMP_DIR/alt_dir/test
t1c.ibd
t3.ibd
ALTER TABLE t3 CHANGE
`倀倁倂倃倄倅倆倇倈倉倊個倌倍倎倏倐們倒倓倔倕倖倗倘候倚倛倜倝倞借倠倡倢倣値倥倦倧倨倩倪倫倬倭倮倯倰倱倲倳倴倵倶倷倸倹债倻值倽倾Ä`
c3 INT;
### files in MYSQL_DATA_DIR/test
sys_foreign.ibd
sys_indexes.ibd
sys_tables.ibd
t1p.ibd
### files in MYSQL_TMP_DIR/alt_dir/test
t1c.ibd
t3.ibd
ALTER TABLE t3 CHANGE c3 𐌀𐌁𐌂𐌃𐌄𐌅𐌆𐌇𐌈𐌉𐌊𐌋𐌌𐌍𐌎𐌏𐌐𐌑𐌒𐌓𐌔𐌕𐌖𐌗𐌘𐌙𐌚𐌛𐌜 INT;
ERROR HY000: Invalid utf8mb3 character string: '\xF0\x90\x8C\x80\xF0\x90\x8C\x81\xF0\x90\x8C\x82\xF0\x90\x8C\x83'
ALTER TABLE t3 CHANGE c3 😲 INT;
ERROR HY000: Invalid utf8mb3 character string: '\xF0\x9F\x98\xB2'
ALTER TABLE t3 RENAME TO t2;
### files in MYSQL_DATA_DIR/test
sys_foreign.ibd
sys_indexes.ibd
sys_tables.ibd
t1p.ibd
### files in MYSQL_TMP_DIR/alt_dir/test
t1c.ibd
t2.ibd
SELECT st.NAME, i.NAME
FROM sys_tables st INNER JOIN INFORMATION_SCHEMA.INNODB_TABLES i
ON i.TABLE_ID=st.TABLE_ID;
NAME	NAME
test/t1	test/t2
SHOW CREATE TABLE t2;
Table	Create Table
t2	CREATE TABLE `t2` (
  `c1` int NOT NULL,
  `c3` int DEFAULT NULL,
  `ct` text,
  PRIMARY KEY (`c1`),
  KEY `c2` (`c3`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci DATA DIRECTORY='MYSQL_TMP_DIR/alt_dir/'
RENAME TABLE t2 TO t1;
SELECT st.NAME, i.NAME
FROM sys_tables st INNER JOIN INFORMATION_SCHEMA.INNODB_TABLES i
ON i.TABLE_ID=st.TABLE_ID;
NAME	NAME
test/t1	test/t1
### files in MYSQL_DATA_DIR/test
sys_foreign.ibd
sys_indexes.ibd
sys_tables.ibd
t1p.ibd
### files in MYSQL_TMP_DIR/alt_dir/test
t1.ibd
t1c.ibd
SELECT i.NAME,i.POS,i.MTYPE,i.PRTYPE,i.LEN
FROM INFORMATION_SCHEMA.INNODB_COLUMNS i
INNER JOIN sys_tables st ON i.TABLE_ID=st.TABLE_ID ORDER BY i.NAME,i.POS;
NAME	POS	MTYPE	PRTYPE	LEN
c1	0	6	1283	4
c3	1	6	1027	4
ct	2	5	16711932	10
SELECT si.NAME,i.POS,i.NAME FROM INFORMATION_SCHEMA.INNODB_FIELDS i
INNER JOIN sys_indexes si ON i.INDEX_ID=si.INDEX_ID ORDER BY si.NAME, i.POS;
NAME	POS	NAME
c2	0	c3
PRIMARY	0	c1
SELECT i.FOR_COL_NAME, i.REF_COL_NAME, i.POS FROM INFORMATION_SCHEMA.INNODB_FOREIGN_COLS i
INNER JOIN sys_foreign sf ON i.ID = sf.ID;
FOR_COL_NAME	REF_COL_NAME	POS
c2	c3	1
c3	c2	1
ALTER TABLE t1 DROP INDEX c2;
ERROR HY000: Cannot drop index 'c2': needed in a foreign key constraint
ALTER TABLE t1 DROP INDEX c4;
ERROR 42000: Can't DROP 'c4'; check that column/key exists
ALTER TABLE t1c DROP FOREIGN KEY c2;
ERROR 42000: Can't DROP 'c2'; check that column/key exists
ALTER TABLE t1c DROP FOREIGN KEY t1c2, DROP FOREIGN KEY c2;
ERROR 42000: Can't DROP 'c2'; check that column/key exists
ALTER TABLE t1c DROP FOREIGN KEY t1c2, DROP FOREIGN KEY c2, DROP INDEX c2;
ERROR 42000: Can't DROP 'c2'; check that column/key exists
ALTER TABLE t1c DROP INDEX c2;
ERROR HY000: Cannot drop index 'c2': needed in a foreign key constraint
ALTER TABLE t1c DROP FOREIGN KEY ẗ1C2;
ERROR 42000: Can't DROP 'ẗ1C2'; check that column/key exists
SHOW CREATE TABLE t1c;
Table	Create Table
t1c	CREATE TABLE `t1c` (
  `c1` int NOT NULL,
  `c2` int DEFAULT NULL,
  `c3` int DEFAULT NULL,
  PRIMARY KEY (`c1`),
  KEY `c2` (`c2`),
  KEY `c3` (`c3`),
  CONSTRAINT `t1c2` FOREIGN KEY (`c2`) REFERENCES `t1` (`c3`),
  CONSTRAINT `t1c3` FOREIGN KEY (`c3`) REFERENCES `t1p` (`c2`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci DATA DIRECTORY='MYSQL_TMP_DIR/alt_dir/'
SET foreign_key_checks=0;
DROP TABLE t1p;
SET foreign_key_checks=1;
SHOW CREATE TABLE t1c;
Table	Create Table
t1c	CREATE TABLE `t1c` (
  `c1` int NOT NULL,
  `c2` int DEFAULT NULL,
  `c3` int DEFAULT NULL,
  PRIMARY KEY (`c1`),
  KEY `c2` (`c2`),
  KEY `c3` (`c3`),
  CONSTRAINT `t1c2` FOREIGN KEY (`c2`) REFERENCES `t1` (`c3`),
  CONSTRAINT `t1c3` FOREIGN KEY (`c3`) REFERENCES `t1p` (`c2`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci DATA DIRECTORY='MYSQL_TMP_DIR/alt_dir/'
### files in MYSQL_DATA_DIR/test
sys_foreign.ibd
sys_indexes.ibd
sys_tables.ibd
### files in MYSQL_TMP_DIR/alt_dir/test
t1.ibd
t1c.ibd
SELECT i.NAME,i.POS,i.MTYPE,i.PRTYPE,i.LEN
FROM INFORMATION_SCHEMA.INNODB_COLUMNS i
INNER JOIN sys_tables st ON i.TABLE_ID=st.TABLE_ID ORDER BY i.NAME,i.POS;
NAME	POS	MTYPE	PRTYPE	LEN
c1	0	6	1283	4
c3	1	6	1027	4
ct	2	5	16711932	10
SELECT si.NAME,i.POS,i.NAME FROM INFORMATION_SCHEMA.INNODB_FIELDS i
INNER JOIN sys_indexes si ON i.INDEX_ID=si.INDEX_ID ORDER BY si.NAME, i.POS;
NAME	POS	NAME
c2	0	c3
PRIMARY	0	c1
SELECT i.FOR_COL_NAME, i.REF_COL_NAME, i.POS FROM INFORMATION_SCHEMA.INNODB_FOREIGN_COLS i
INNER JOIN sys_foreign sf ON i.ID = sf.ID;
FOR_COL_NAME	REF_COL_NAME	POS
c2	c3	1
c3	c2	1
CREATE TABLE t1p (c1 INT PRIMARY KEY, c2 INT, INDEX(c2))
ENGINE=InnoDB DATA DIRECTORY='MYSQL_TMP_DIR/alt_dir';
ALTER TABLE t1c DROP INDEX C2, DROP INDEX C3;
ERROR HY000: Cannot drop index 'c2': needed in a foreign key constraint
ALTER TABLE t1c DROP INDEX C3;
ERROR HY000: Cannot drop index 'c3': needed in a foreign key constraint
SET foreign_key_checks=0;
ALTER TABLE t1c DROP INDEX C3;
ERROR HY000: Cannot drop index 'c3': needed in a foreign key constraint
### files in MYSQL_DATA_DIR/test
sys_foreign.ibd
sys_indexes.ibd
sys_tables.ibd
### files in MYSQL_TMP_DIR/alt_dir/test
t1.ibd
t1c.ibd
t1p.ibd
SET foreign_key_checks=1;
SHOW CREATE TABLE t1c;
Table	Create Table
t1c	CREATE TABLE `t1c` (
  `c1` int NOT NULL,
  `c2` int DEFAULT NULL,
  `c3` int DEFAULT NULL,
  PRIMARY KEY (`c1`),
  KEY `c2` (`c2`),
  KEY `c3` (`c3`),
  CONSTRAINT `t1c2` FOREIGN KEY (`c2`) REFERENCES `t1` (`c3`),
  CONSTRAINT `t1c3` FOREIGN KEY (`c3`) REFERENCES `t1p` (`c2`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci DATA DIRECTORY='MYSQL_TMP_DIR/alt_dir/'
SELECT i.NAME,i.POS,i.MTYPE,i.PRTYPE,i.LEN
FROM INFORMATION_SCHEMA.INNODB_COLUMNS i
INNER JOIN sys_tables st ON i.TABLE_ID=st.TABLE_ID ORDER BY i.NAME,i.POS;
NAME	POS	MTYPE	PRTYPE	LEN
c1	0	6	1283	4
c3	1	6	1027	4
ct	2	5	16711932	10
SELECT si.NAME,i.POS,i.NAME FROM INFORMATION_SCHEMA.INNODB_FIELDS i
INNER JOIN sys_indexes si ON i.INDEX_ID=si.INDEX_ID ORDER BY si.NAME, i.POS;
NAME	POS	NAME
c2	0	c3
PRIMARY	0	c1
SELECT i.FOR_COL_NAME, i.REF_COL_NAME, i.POS FROM INFORMATION_SCHEMA.INNODB_FOREIGN_COLS i
INNER JOIN sys_foreign sf ON i.ID = sf.ID;
FOR_COL_NAME	REF_COL_NAME	POS
c2	c3	1
c3	c2	1
ALTER TABLE t1c DROP FOREIGN KEY t1C3;
### files in MYSQL_DATA_DIR/test
sys_foreign.ibd
sys_indexes.ibd
sys_tables.ibd
### files in MYSQL_TMP_DIR/alt_dir/test
t1.ibd
t1c.ibd
t1p.ibd
SHOW CREATE TABLE t1c;
Table	Create Table
t1c	CREATE TABLE `t1c` (
  `c1` int NOT NULL,
  `c2` int DEFAULT NULL,
  `c3` int DEFAULT NULL,
  PRIMARY KEY (`c1`),
  KEY `c2` (`c2`),
  KEY `c3` (`c3`),
  CONSTRAINT `t1c2` FOREIGN KEY (`c2`) REFERENCES `t1` (`c3`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci DATA DIRECTORY='MYSQL_TMP_DIR/alt_dir/'
SELECT i.NAME,i.POS,i.MTYPE,i.PRTYPE,i.LEN
FROM INFORMATION_SCHEMA.INNODB_COLUMNS i
INNER JOIN sys_tables st ON i.TABLE_ID=st.TABLE_ID ORDER BY i.NAME,i.POS;
NAME	POS	MTYPE	PRTYPE	LEN
c1	0	6	1283	4
c3	1	6	1027	4
ct	2	5	16711932	10
SELECT si.NAME,i.POS,i.NAME FROM INFORMATION_SCHEMA.INNODB_FIELDS i
INNER JOIN sys_indexes si ON i.INDEX_ID=si.INDEX_ID ORDER BY si.NAME, i.POS;
NAME	POS	NAME
c2	0	c3
PRIMARY	0	c1
SELECT i.FOR_COL_NAME, i.REF_COL_NAME, i.POS FROM INFORMATION_SCHEMA.INNODB_FOREIGN_COLS i
INNER JOIN sys_foreign sf ON i.ID = sf.ID;
FOR_COL_NAME	REF_COL_NAME	POS
c2	c3	1
ALTER TABLE t1c DROP INDEX c2, DROP FOREIGN KEY t1C2;
### files in MYSQL_DATA_DIR/test
sys_foreign.ibd
sys_indexes.ibd
sys_tables.ibd
### files in MYSQL_TMP_DIR/alt_dir/test
t1.ibd
t1c.ibd
t1p.ibd
SHOW CREATE TABLE t1c;
Table	Create Table
t1c	CREATE TABLE `t1c` (
  `c1` int NOT NULL,
  `c2` int DEFAULT NULL,
  `c3` int DEFAULT NULL,
  PRIMARY KEY (`c1`),
  KEY `c3` (`c3`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci DATA DIRECTORY='MYSQL_TMP_DIR/alt_dir/'
SELECT i.NAME,i.POS,i.MTYPE,i.PRTYPE,i.LEN
FROM INFORMATION_SCHEMA.INNODB_COLUMNS i
INNER JOIN sys_tables st ON i.TABLE_ID=st.TABLE_ID ORDER BY i.NAME,i.POS;
NAME	POS	MTYPE	PRTYPE	LEN
c1	0	6	1283	4
c3	1	6	1027	4
ct	2	5	16711932	10
SELECT si.NAME,i.POS,i.NAME FROM INFORMATION_SCHEMA.INNODB_FIELDS i
INNER JOIN sys_indexes si ON i.INDEX_ID=si.INDEX_ID ORDER BY si.NAME, i.POS;
NAME	POS	NAME
c2	0	c3
PRIMARY	0	c1
SELECT i.FOR_COL_NAME, i.REF_COL_NAME, i.POS FROM INFORMATION_SCHEMA.INNODB_FOREIGN_COLS i
INNER JOIN sys_foreign sf ON i.ID = sf.ID;
FOR_COL_NAME	REF_COL_NAME	POS
ALTER TABLE t1 DROP INDEX c2, CHANGE c3 c2 INT;
### files in MYSQL_DATA_DIR/test
sys_foreign.ibd
sys_indexes.ibd
sys_tables.ibd
### files in MYSQL_TMP_DIR/alt_dir/test
t1.ibd
t1c.ibd
t1p.ibd
SELECT i.NAME,i.POS,i.MTYPE,i.PRTYPE,i.LEN
FROM INFORMATION_SCHEMA.INNODB_COLUMNS i
INNER JOIN sys_tables st ON i.TABLE_ID=st.TABLE_ID ORDER BY i.NAME,i.POS;
NAME	POS	MTYPE	PRTYPE	LEN
c1	0	6	1283	4
c2	1	6	1027	4
ct	2	5	16711932	10
SELECT si.NAME,i.POS,i.NAME FROM INFORMATION_SCHEMA.INNODB_FIELDS i
INNER JOIN sys_indexes si ON i.INDEX_ID=si.INDEX_ID ORDER BY si.NAME, i.POS;
NAME	POS	NAME
PRIMARY	0	c1
SELECT i.FOR_COL_NAME, i.REF_COL_NAME, i.POS FROM INFORMATION_SCHEMA.INNODB_FOREIGN_COLS i
INNER JOIN sys_foreign sf ON i.ID = sf.ID;
FOR_COL_NAME	REF_COL_NAME	POS
CREATE TABLE t1o LIKE t1;
ALTER TABLE t1 ADD FULLTEXT INDEX (ct),
CHANGE c1 pk INT, ALTER c2 SET DEFAULT 42, RENAME TO tt,
ALGORITHM=INPLACE, LOCK=NONE;
ERROR 0A000: LOCK=NONE is not supported. Reason: Fulltext index creation requires a lock. Try LOCK=SHARED.
ALTER TABLE t1 ADD FULLTEXT INDEX (ct),
CHANGE c1 pk INT, ALTER c2 SET DEFAULT 42, RENAME TO tt,
ALGORITHM=INPLACE, LOCK=SHARED;
Warnings:
Warning	124	InnoDB rebuilding table to add column FTS_DOC_ID
### files in MYSQL_DATA_DIR/test
sys_foreign.ibd
sys_indexes.ibd
sys_tables.ibd
t1o.ibd
### files in MYSQL_TMP_DIR/alt_dir/test
fts_aux_index_1.ibd
fts_aux_index_2.ibd
fts_aux_index_3.ibd
fts_aux_index_4.ibd
fts_aux_index_5.ibd
fts_aux_index_6.ibd
fts_aux_being_deleted.ibd
fts_aux_being_deleted_cache.ibd
fts_aux_config.ibd
fts_aux_deleted.ibd
fts_aux_deleted_cache.ibd
t1c.ibd
t1p.ibd
tt.ibd
SELECT i.NAME,i.POS,i.MTYPE,i.PRTYPE,i.LEN
FROM INFORMATION_SCHEMA.INNODB_COLUMNS i
INNER JOIN sys_tables st ON i.TABLE_ID=st.TABLE_ID ORDER BY i.NAME,i.POS;
NAME	POS	MTYPE	PRTYPE	LEN
SELECT si.NAME,i.POS,i.NAME FROM INFORMATION_SCHEMA.INNODB_FIELDS i
INNER JOIN sys_indexes si ON i.INDEX_ID=si.INDEX_ID ORDER BY si.NAME, i.POS;
NAME	POS	NAME
SELECT i.FOR_COL_NAME, i.REF_COL_NAME, i.POS FROM INFORMATION_SCHEMA.INNODB_FOREIGN_COLS i
INNER JOIN sys_foreign sf ON i.ID = sf.ID;
FOR_COL_NAME	REF_COL_NAME	POS
SHOW CREATE TABLE tt;
Table	Create Table
tt	CREATE TABLE `tt` (
  `pk` int NOT NULL,
  `c2` int DEFAULT '42',
  `ct` text,
  PRIMARY KEY (`pk`),
  FULLTEXT KEY `ct` (`ct`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci DATA DIRECTORY='MYSQL_TMP_DIR/alt_dir/'
ALTER TABLE t1o CHANGE c1 dB_row_Id INT, ALGORITHM=COPY;
ERROR 42000: Incorrect column name 'dB_row_Id'
ALTER TABLE t1o CHANGE c1 dB_row_Id INT, ALGORITHM=INPLACE;
ERROR 42000: Incorrect column name 'DB_ROW_ID'
ALTER TABLE t1o CHANGE c1 DB_TRX_ID INT;
ERROR 42000: Incorrect column name 'DB_TRX_ID'
ALTER TABLE t1o CHANGE c1 db_roll_ptr INT;
ERROR 42000: Incorrect column name 'db_roll_ptr'
ALTER TABLE t1o ADD FULLTEXT INDEX(ct), CHANGE c1 FTS_DOC_ID INT,
ALGORITHM=COPY;
ERROR HY000: Column 'FTS_DOC_ID' is of wrong type for an InnoDB FULLTEXT index
ALTER TABLE t1o ADD FULLTEXT INDEX(ct), CHANGE c1 FTS_DOC_ID INT,
ALGORITHM=INPLACE;
ERROR HY000: Column 'FTS_DOC_ID' is of wrong type for an InnoDB FULLTEXT index
ALTER TABLE t1o ADD FULLTEXT INDEX(ct), CHANGE c1 FTS_Doc_ID INT,
ALGORITHM=INPLACE;
ERROR HY000: Column 'FTS_Doc_ID' is of wrong type for an InnoDB FULLTEXT index
ALTER TABLE t1o ADD FULLTEXT INDEX(ct),
CHANGE c1 FTS_DOC_ID BIGINT UNSIGNED NOT NULL,
ALGORITHM=INPLACE;
ERROR 0A000: ALGORITHM=INPLACE is not supported. Reason: Cannot change column type INPLACE. Try ALGORITHM=COPY.
CREATE TABLE t1n LIKE t1o;
ALTER TABLE t1n ADD FULLTEXT INDEX(ct);
Warnings:
Warning	124	InnoDB rebuilding table to add column FTS_DOC_ID
### files in MYSQL_DATA_DIR/test
fts_aux_index_1.ibd
fts_aux_index_2.ibd
fts_aux_index_3.ibd
fts_aux_index_4.ibd
fts_aux_index_5.ibd
fts_aux_index_6.ibd
fts_aux_being_deleted.ibd
fts_aux_being_deleted_cache.ibd
fts_aux_config.ibd
fts_aux_deleted.ibd
fts_aux_deleted_cache.ibd
sys_foreign.ibd
sys_indexes.ibd
sys_tables.ibd
t1n.ibd
t1o.ibd
### files in MYSQL_TMP_DIR/alt_dir/test
fts_aux_index_1.ibd
fts_aux_index_2.ibd
fts_aux_index_3.ibd
fts_aux_index_4.ibd
fts_aux_index_5.ibd
fts_aux_index_6.ibd
fts_aux_being_deleted.ibd
fts_aux_being_deleted_cache.ibd
fts_aux_config.ibd
fts_aux_deleted.ibd
fts_aux_deleted_cache.ibd
t1c.ibd
t1p.ibd
tt.ibd
ALTER TABLE t1n CHANGE c1 Fts_DOC_ID INT, ALGORITHM=INPLACE;
ERROR HY000: Column 'Fts_DOC_ID' is of wrong type for an InnoDB FULLTEXT index
ALTER TABLE t1n CHANGE c1 Fts_DOC_ID INT, ALGORITHM=COPY;
ERROR HY000: Column 'Fts_DOC_ID' is of wrong type for an InnoDB FULLTEXT index
ALTER TABLE t1n CHANGE FTS_DOC_ID c11 INT, ALGORITHM=INPLACE;
ERROR 42S22: Unknown column 'FTS_DOC_ID' in 't1n'
ALTER TABLE t1n CHANGE c1 FTS_DOC_ïD INT, ALGORITHM=INPLACE;
### files in MYSQL_DATA_DIR/test
fts_aux_index_1.ibd
fts_aux_index_2.ibd
fts_aux_index_3.ibd
fts_aux_index_4.ibd
fts_aux_index_5.ibd
fts_aux_index_6.ibd
fts_aux_being_deleted.ibd
fts_aux_being_deleted_cache.ibd
fts_aux_config.ibd
fts_aux_deleted.ibd
fts_aux_deleted_cache.ibd
sys_foreign.ibd
sys_indexes.ibd
sys_tables.ibd
t1n.ibd
t1o.ibd
### files in MYSQL_TMP_DIR/alt_dir/test
fts_aux_index_1.ibd
fts_aux_index_2.ibd
fts_aux_index_3.ibd
fts_aux_index_4.ibd
fts_aux_index_5.ibd
fts_aux_index_6.ibd
fts_aux_being_deleted.ibd
fts_aux_being_deleted_cache.ibd
fts_aux_config.ibd
fts_aux_deleted.ibd
fts_aux_deleted_cache.ibd
t1c.ibd
t1p.ibd
tt.ibd
ALTER TABLE t1n CHANGE FTS_DOC_ÏD c1 INT, ALGORITHM=INPLACE;
### files in MYSQL_DATA_DIR/test
fts_aux_index_1.ibd
fts_aux_index_2.ibd
fts_aux_index_3.ibd
fts_aux_index_4.ibd
fts_aux_index_5.ibd
fts_aux_index_6.ibd
fts_aux_being_deleted.ibd
fts_aux_being_deleted_cache.ibd
fts_aux_config.ibd
fts_aux_deleted.ibd
fts_aux_deleted_cache.ibd
sys_foreign.ibd
sys_indexes.ibd
sys_tables.ibd
t1n.ibd
t1o.ibd
### files in MYSQL_TMP_DIR/alt_dir/test
fts_aux_index_1.ibd
fts_aux_index_2.ibd
fts_aux_index_3.ibd
fts_aux_index_4.ibd
fts_aux_index_5.ibd
fts_aux_index_6.ibd
fts_aux_being_deleted.ibd
fts_aux_being_deleted_cache.ibd
fts_aux_config.ibd
fts_aux_deleted.ibd
fts_aux_deleted_cache.ibd
t1c.ibd
t1p.ibd
tt.ibd
ALTER TABLE t1n CHANGE c1 c2 INT, CHANGE c2 ct INT, CHANGE ct c1 TEXT,
ALGORITHM=INPLACE;
### files in MYSQL_DATA_DIR/test
fts_aux_index_1.ibd
fts_aux_index_2.ibd
fts_aux_index_3.ibd
fts_aux_index_4.ibd
fts_aux_index_5.ibd
fts_aux_index_6.ibd
fts_aux_being_deleted.ibd
fts_aux_being_deleted_cache.ibd
fts_aux_config.ibd
fts_aux_deleted.ibd
fts_aux_deleted_cache.ibd
sys_foreign.ibd
sys_indexes.ibd
sys_tables.ibd
t1n.ibd
t1o.ibd
### files in MYSQL_TMP_DIR/alt_dir/test
fts_aux_index_1.ibd
fts_aux_index_2.ibd
fts_aux_index_3.ibd
fts_aux_index_4.ibd
fts_aux_index_5.ibd
fts_aux_index_6.ibd
fts_aux_being_deleted.ibd
fts_aux_being_deleted_cache.ibd
fts_aux_config.ibd
fts_aux_deleted.ibd
fts_aux_deleted_cache.ibd
t1c.ibd
t1p.ibd
tt.ibd
SHOW CREATE TABLE t1n;
Table	Create Table
t1n	CREATE TABLE `t1n` (
  `c2` int NOT NULL,
  `ct` int DEFAULT NULL,
  `c1` text,
  PRIMARY KEY (`c2`),
  FULLTEXT KEY `ct` (`c1`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
ALTER TABLE t1n CHANGE c2 c1 INT, CHANGE ct c2 INT, CHANGE c1 ct TEXT,
ALGORITHM=COPY;
### files in MYSQL_DATA_DIR/test
fts_aux_index_1.ibd
fts_aux_index_2.ibd
fts_aux_index_3.ibd
fts_aux_index_4.ibd
fts_aux_index_5.ibd
fts_aux_index_6.ibd
fts_aux_being_deleted.ibd
fts_aux_being_deleted_cache.ibd
fts_aux_config.ibd
fts_aux_deleted.ibd
fts_aux_deleted_cache.ibd
sys_foreign.ibd
sys_indexes.ibd
sys_tables.ibd
t1n.ibd
t1o.ibd
### files in MYSQL_TMP_DIR/alt_dir/test
fts_aux_index_1.ibd
fts_aux_index_2.ibd
fts_aux_index_3.ibd
fts_aux_index_4.ibd
fts_aux_index_5.ibd
fts_aux_index_6.ibd
fts_aux_being_deleted.ibd
fts_aux_being_deleted_cache.ibd
fts_aux_config.ibd
fts_aux_deleted.ibd
fts_aux_deleted_cache.ibd
t1c.ibd
t1p.ibd
tt.ibd
SHOW CREATE TABLE t1n;
Table	Create Table
t1n	CREATE TABLE `t1n` (
  `c1` int NOT NULL,
  `c2` int DEFAULT NULL,
  `ct` text,
  PRIMARY KEY (`c1`),
  FULLTEXT KEY `ct` (`ct`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
ALTER TABLE t1n ADD INDEX(c2), CHANGE c2 c4 INT, ALGORITHM=INPLACE;
ERROR 42000: Key column 'c2' doesn't exist in table
ALTER TABLE t1n ADD INDEX(c2), CHANGE c2 c4 INT, ALGORITHM=COPY;
ERROR 42000: Key column 'c2' doesn't exist in table
ALTER TABLE t1n ADD INDEX(c4), CHANGE c2 c4 INT, ALGORITHM=INPLACE;
### files in MYSQL_DATA_DIR/test
fts_aux_index_1.ibd
fts_aux_index_2.ibd
fts_aux_index_3.ibd
fts_aux_index_4.ibd
fts_aux_index_5.ibd
fts_aux_index_6.ibd
fts_aux_being_deleted.ibd
fts_aux_being_deleted_cache.ibd
fts_aux_config.ibd
fts_aux_deleted.ibd
fts_aux_deleted_cache.ibd
sys_foreign.ibd
sys_indexes.ibd
sys_tables.ibd
t1n.ibd
t1o.ibd
### files in MYSQL_TMP_DIR/alt_dir/test
fts_aux_index_1.ibd
fts_aux_index_2.ibd
fts_aux_index_3.ibd
fts_aux_index_4.ibd
fts_aux_index_5.ibd
fts_aux_index_6.ibd
fts_aux_being_deleted.ibd
fts_aux_being_deleted_cache.ibd
fts_aux_config.ibd
fts_aux_deleted.ibd
fts_aux_deleted_cache.ibd
t1c.ibd
t1p.ibd
tt.ibd
SHOW CREATE TABLE t1n;
Table	Create Table
t1n	CREATE TABLE `t1n` (
  `c1` int NOT NULL,
  `c4` int DEFAULT NULL,
  `ct` text,
  PRIMARY KEY (`c1`),
  KEY `c4` (`c4`),
  FULLTEXT KEY `ct` (`ct`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
ALTER TABLE t1n DROP INDEX c4;
### files in MYSQL_DATA_DIR/test
fts_aux_index_1.ibd
fts_aux_index_2.ibd
fts_aux_index_3.ibd
fts_aux_index_4.ibd
fts_aux_index_5.ibd
fts_aux_index_6.ibd
fts_aux_being_deleted.ibd
fts_aux_being_deleted_cache.ibd
fts_aux_config.ibd
fts_aux_deleted.ibd
fts_aux_deleted_cache.ibd
sys_foreign.ibd
sys_indexes.ibd
sys_tables.ibd
t1n.ibd
t1o.ibd
### files in MYSQL_TMP_DIR/alt_dir/test
fts_aux_index_1.ibd
fts_aux_index_2.ibd
fts_aux_index_3.ibd
fts_aux_index_4.ibd
fts_aux_index_5.ibd
fts_aux_index_6.ibd
fts_aux_being_deleted.ibd
fts_aux_being_deleted_cache.ibd
fts_aux_config.ibd
fts_aux_deleted.ibd
fts_aux_deleted_cache.ibd
t1c.ibd
t1p.ibd
tt.ibd
ALTER TABLE t1n CHANGE c4 c1 INT, ADD INDEX(c1), ALGORITHM=INPLACE;
ERROR 42S21: Duplicate column name 'c1'
ALTER TABLE t1n CHANGE c4 c11 INT, ADD INDEX(c11), ALGORITHM=INPLACE;
### files in MYSQL_DATA_DIR/test
fts_aux_index_1.ibd
fts_aux_index_2.ibd
fts_aux_index_3.ibd
fts_aux_index_4.ibd
fts_aux_index_5.ibd
fts_aux_index_6.ibd
fts_aux_being_deleted.ibd
fts_aux_being_deleted_cache.ibd
fts_aux_config.ibd
fts_aux_deleted.ibd
fts_aux_deleted_cache.ibd
sys_foreign.ibd
sys_indexes.ibd
sys_tables.ibd
t1n.ibd
t1o.ibd
### files in MYSQL_TMP_DIR/alt_dir/test
fts_aux_index_1.ibd
fts_aux_index_2.ibd
fts_aux_index_3.ibd
fts_aux_index_4.ibd
fts_aux_index_5.ibd
fts_aux_index_6.ibd
fts_aux_being_deleted.ibd
fts_aux_being_deleted_cache.ibd
fts_aux_config.ibd
fts_aux_deleted.ibd
fts_aux_deleted_cache.ibd
t1c.ibd
t1p.ibd
tt.ibd
SHOW CREATE TABLE t1n;
Table	Create Table
t1n	CREATE TABLE `t1n` (
  `c1` int NOT NULL,
  `c11` int DEFAULT NULL,
  `ct` text,
  PRIMARY KEY (`c1`),
  KEY `c11` (`c11`),
  FULLTEXT KEY `ct` (`ct`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
DROP TABLE t1n;
ALTER TABLE t1o MODIFY c1 BIGINT UNSIGNED NOT NULL;
### files in MYSQL_DATA_DIR/test
sys_foreign.ibd
sys_indexes.ibd
sys_tables.ibd
t1o.ibd
### files in MYSQL_TMP_DIR/alt_dir/test
fts_aux_index_1.ibd
fts_aux_index_2.ibd
fts_aux_index_3.ibd
fts_aux_index_4.ibd
fts_aux_index_5.ibd
fts_aux_index_6.ibd
fts_aux_being_deleted.ibd
fts_aux_being_deleted_cache.ibd
fts_aux_config.ibd
fts_aux_deleted.ibd
fts_aux_deleted_cache.ibd
t1c.ibd
t1p.ibd
tt.ibd
ALTER TABLE t1o ADD FULLTEXT INDEX(ct),
CHANGE c1 FTS_DOC_ID BIGINT UNSIGNED NOT NULL,
ALGORITHM=INPLACE;
### files in MYSQL_DATA_DIR/test
fts_aux_index_1.ibd
fts_aux_index_2.ibd
fts_aux_index_3.ibd
fts_aux_index_4.ibd
fts_aux_index_5.ibd
fts_aux_index_6.ibd
fts_aux_being_deleted.ibd
fts_aux_being_deleted_cache.ibd
fts_aux_config.ibd
fts_aux_deleted.ibd
fts_aux_deleted_cache.ibd
sys_foreign.ibd
sys_indexes.ibd
sys_tables.ibd
t1o.ibd
### files in MYSQL_TMP_DIR/alt_dir/test
fts_aux_index_1.ibd
fts_aux_index_2.ibd
fts_aux_index_3.ibd
fts_aux_index_4.ibd
fts_aux_index_5.ibd
fts_aux_index_6.ibd
fts_aux_being_deleted.ibd
fts_aux_being_deleted_cache.ibd
fts_aux_config.ibd
fts_aux_deleted.ibd
fts_aux_deleted_cache.ibd
t1c.ibd
t1p.ibd
tt.ibd
ALTER TABLE t1o CHANGE FTS_DOC_ID foo_id BIGINT UNSIGNED NOT NULL,
LOCK=NONE;
ERROR 0A000: LOCK=NONE is not supported. Reason: Cannot drop or rename FTS_DOC_ID. Try LOCK=SHARED.
SELECT sc.pos FROM information_schema.innodb_columns sc
INNER JOIN information_schema.innodb_tables st
ON sc.TABLE_ID=st.TABLE_ID
WHERE st.NAME='test/t1o' AND sc.NAME='FTS_DOC_ID';
pos
0
SHOW CREATE TABLE t1o;
Table	Create Table
t1o	CREATE TABLE `t1o` (
  `FTS_DOC_ID` bigint unsigned NOT NULL,
  `c2` int DEFAULT NULL,
  `ct` text,
  PRIMARY KEY (`FTS_DOC_ID`),
  FULLTEXT KEY `ct` (`ct`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
ALTER TABLE t1o CHANGE FTS_DOC_ID foo_id BIGINT UNSIGNED NOT NULL,
DROP INDEX ct, LOCK=NONE;
### files in MYSQL_DATA_DIR/test
sys_foreign.ibd
sys_indexes.ibd
sys_tables.ibd
t1o.ibd
### files in MYSQL_TMP_DIR/alt_dir/test
fts_aux_index_1.ibd
fts_aux_index_2.ibd
fts_aux_index_3.ibd
fts_aux_index_4.ibd
fts_aux_index_5.ibd
fts_aux_index_6.ibd
fts_aux_being_deleted.ibd
fts_aux_being_deleted_cache.ibd
fts_aux_config.ibd
fts_aux_deleted.ibd
fts_aux_deleted_cache.ibd
t1c.ibd
t1p.ibd
tt.ibd
SHOW CREATE TABLE t1o;
Table	Create Table
t1o	CREATE TABLE `t1o` (
  `foo_id` bigint unsigned NOT NULL,
  `c2` int DEFAULT NULL,
  `ct` text,
  PRIMARY KEY (`foo_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
DROP TABLE t1c, t1p, sys_tables, sys_indexes, sys_foreign;
CREATE TABLE sys_tables SELECT * FROM INFORMATION_SCHEMA.INNODB_TABLES
WHERE NAME='test/t1o';
CREATE TABLE sys_indexes SELECT i.* FROM INFORMATION_SCHEMA.INNODB_INDEXES i
INNER JOIN sys_tables st ON i.TABLE_ID=st.TABLE_ID;
CREATE TABLE sys_foreign SELECT i.*
FROM INFORMATION_SCHEMA.INNODB_FOREIGN i WHERE FOR_NAME='test/t1o';
SELECT i.NAME,i.POS,i.MTYPE,i.PRTYPE,i.LEN
FROM INFORMATION_SCHEMA.INNODB_COLUMNS i
INNER JOIN sys_tables st ON i.TABLE_ID=st.TABLE_ID ORDER BY i.NAME,i.POS;
NAME	POS	MTYPE	PRTYPE	LEN
c2	1	6	1027	4
ct	2	5	16711932	10
foo_id	0	6	1800	8
SELECT si.NAME,i.POS,i.NAME FROM INFORMATION_SCHEMA.INNODB_FIELDS i
INNER JOIN sys_indexes si ON i.INDEX_ID=si.INDEX_ID ORDER BY si.NAME, i.POS;
NAME	POS	NAME
PRIMARY	0	foo_id
SELECT i.FOR_COL_NAME, i.REF_COL_NAME, i.POS FROM INFORMATION_SCHEMA.INNODB_FOREIGN_COLS i
INNER JOIN sys_foreign sf ON i.ID = sf.ID;
FOR_COL_NAME	REF_COL_NAME	POS
ALTER TABLE t1o ADD UNIQUE INDEX FTS_DOC_ID_INDEX(foo_id);
### files in MYSQL_DATA_DIR/test
sys_foreign.ibd
sys_indexes.ibd
sys_tables.ibd
t1o.ibd
### files in MYSQL_TMP_DIR/alt_dir/test
fts_aux_index_1.ibd
fts_aux_index_2.ibd
fts_aux_index_3.ibd
fts_aux_index_4.ibd
fts_aux_index_5.ibd
fts_aux_index_6.ibd
fts_aux_being_deleted.ibd
fts_aux_being_deleted_cache.ibd
fts_aux_config.ibd
fts_aux_deleted.ibd
fts_aux_deleted_cache.ibd
tt.ibd
ALTER TABLE t1o CHANGE foo_id FTS_DOC_ID BIGINT UNSIGNED NOT NULL,
ADD FULLTEXT INDEX(ct);
### files in MYSQL_DATA_DIR/test
fts_aux_index_1.ibd
fts_aux_index_2.ibd
fts_aux_index_3.ibd
fts_aux_index_4.ibd
fts_aux_index_5.ibd
fts_aux_index_6.ibd
fts_aux_being_deleted.ibd
fts_aux_being_deleted_cache.ibd
fts_aux_config.ibd
fts_aux_deleted.ibd
fts_aux_deleted_cache.ibd
sys_foreign.ibd
sys_indexes.ibd
sys_tables.ibd
t1o.ibd
### files in MYSQL_TMP_DIR/alt_dir/test
fts_aux_index_1.ibd
fts_aux_index_2.ibd
fts_aux_index_3.ibd
fts_aux_index_4.ibd
fts_aux_index_5.ibd
fts_aux_index_6.ibd
fts_aux_being_deleted.ibd
fts_aux_being_deleted_cache.ibd
fts_aux_config.ibd
fts_aux_deleted.ibd
fts_aux_deleted_cache.ibd
tt.ibd
ALTER TABLE t1o CHANGE FTS_DOC_ID foo_id BIGINT UNSIGNED NOT NULL;
ERROR HY000: Index 'FTS_DOC_ID_INDEX' is of wrong type for an InnoDB FULLTEXT index
DROP TABLE sys_indexes;
CREATE TABLE sys_indexes SELECT i.* FROM INFORMATION_SCHEMA.INNODB_INDEXES i
INNER JOIN sys_tables st ON i.TABLE_ID=st.TABLE_ID;
SELECT i.NAME,i.POS,i.MTYPE,i.PRTYPE,i.LEN
FROM INFORMATION_SCHEMA.INNODB_COLUMNS i
INNER JOIN sys_tables st ON i.TABLE_ID=st.TABLE_ID ORDER BY i.NAME,i.POS;
NAME	POS	MTYPE	PRTYPE	LEN
c2	1	6	1027	4
ct	2	5	16711932	10
FTS_DOC_ID	0	6	1800	8
SELECT si.NAME,i.POS,i.NAME FROM INFORMATION_SCHEMA.INNODB_FIELDS i
INNER JOIN sys_indexes si ON i.INDEX_ID=si.INDEX_ID ORDER BY si.NAME, i.POS;
NAME	POS	NAME
ct	0	ct
FTS_DOC_ID_INDEX	0	FTS_DOC_ID
PRIMARY	0	FTS_DOC_ID
SELECT i.FOR_COL_NAME, i.REF_COL_NAME, i.POS FROM INFORMATION_SCHEMA.INNODB_FOREIGN_COLS i
INNER JOIN sys_foreign sf ON i.ID = sf.ID;
FOR_COL_NAME	REF_COL_NAME	POS
#
# Cleanup
#
DROP TABLE tt, t1o, sys_tables, sys_indexes, sys_foreign;
### files in MYSQL_DATA_DIR/test
### files in MYSQL_TMP_DIR/alt_dir/test

include/master-slave.inc
Warnings:
Note	####	Sending passwords in plain text without SSL/TLS is extremely insecure.
Note	####	Storing MySQL user name or password information in the master info repository is not secure and is therefore not recommended. Please consider using the USER and PASSWORD connection options for START SLAVE; see the 'START SLAVE Syntax' in the MySQL Manual for more information.
[connection master]
# Create table with columns of binary data types and insert uuid values
CREATE TABLE uuid_table ( pkey INT NOT NULL PRIMARY KEY AUTO_INCREMENT, bin BINARY(16), vbn VARBINARY(16), tbl TINYBLOB, ttx TINYTEXT CHARACTER SET binary, blb BLOB );
INSERT INTO uuid_table VALUES( NULL, UUID_TO_BIN('12345678123456781234567812345678'),
UUID_TO_BIN('12345678123456781234567812345678'),
UUID_TO_BIN('12345678123456781234567812345678'),
UUID_TO_BIN('12345678123456781234567812345678'),
UUID_TO_BIN('12345678123456781234567812345678'));
INSERT INTO uuid_table VALUES( NULL, UUID_TO_BIN('12345678-1234-5678-1234-567812345678'),
UUID_TO_BIN('12345678-1234-5678-1234-567812345678'),
UUID_TO_BIN('12345678-1234-5678-1234-567812345678'),
UUID_TO_BIN('12345678-1234-5678-1234-567812345678'),
UUID_TO_BIN('12345678-1234-5678-1234-567812345678'));
INSERT INTO uuid_table VALUES( NULL, UUID_TO_BIN('{c8eb4b15-cb09-48bb-bbb2-e6a0b6b4d5c7}'),
UUID_TO_BIN('{c8eb4b15-cb09-48bb-bbb2-e6a0b6b4d5c7}'),
UUID_TO_BIN('{c8eb4b15-cb09-48bb-bbb2-e6a0b6b4d5c7}'),
UUID_TO_BIN('{c8eb4b15-cb09-48bb-bbb2-e6a0b6b4d5c7}'),
UUID_TO_BIN('{c8eb4b15-cb09-48bb-bbb2-e6a0b6b4d5c7}'));
INSERT INTO uuid_table(bin) VALUES ( UUID_TO_BIN('{c8eb4b15cb09-48bb-bbb2-e6a0b6b4d5c7}'));
ERROR HY000: Incorrect string value: '{c8eb4b15cb09-48bb-bbb2-e6a0b6b4d5c7}' for function uuid_to_bin
INSERT INTO uuid_table(bin) VALUES ( UUID_TO_BIN('{c8eb4b15-cb09-48bb-bbb2-e6a0b6b4d5c76}'));
ERROR HY000: Incorrect string value: '{c8eb4b15-cb09-48bb-bbb2-e6a0b6b4d5c76}' for function uuid_to_bin
INSERT INTO uuid_table(bin) VALUES ( UUID_TO_BIN('{c8eb4b15-cb09-48bb-bbb2-e6a0b6b4d5c}'));
ERROR HY000: Incorrect string value: '{c8eb4b15-cb09-48bb-bbb2-e6a0b6b4d5c}' for function uuid_to_bin
INSERT INTO uuid_table(bin) VALUES ( UUID_TO_BIN('{c8eb4b15-cb09-48bb-bbb2e6-a0b6b4d5c7}'));
ERROR HY000: Incorrect string value: '{c8eb4b15-cb09-48bb-bbb2e6-a0b6b4d5c7}' for function uuid_to_bin
INSERT INTO uuid_table(bin) VALUES ( UUID_TO_BIN('{c8eb4b15-cb09-48bb-bbb2-e6a0b6b4d}5c7'));
ERROR HY000: Incorrect string value: '{c8eb4b15-cb09-48bb-bbb2-e6a0b6b4d}5c7' for function uuid_to_bin
include/sync_slave_sql_with_master.inc
[Connection Slave]
# check whether table was created and uuids were inserted on slave
SELECT BIN_TO_UUID(bin) AS a, BIN_TO_UUID(vbn) AS b, BIN_TO_UUID(tbl) AS c, BIN_TO_UUID(ttx) AS d, BIN_TO_UUID(blb) AS e
FROM uuid_table
WHERE pkey <= 2;
a	b	c	d	e
12345678-1234-5678-1234-567812345678	12345678-1234-5678-1234-567812345678	12345678-1234-5678-1234-567812345678	12345678-1234-5678-1234-567812345678	12345678-1234-5678-1234-567812345678
12345678-1234-5678-1234-567812345678	12345678-1234-5678-1234-567812345678	12345678-1234-5678-1234-567812345678	12345678-1234-5678-1234-567812345678	12345678-1234-5678-1234-567812345678
SELECT HEX(UUID_TO_BIN(BIN_TO_UUID(bin, TRUE))) AS a, BIN_TO_UUID(vbn, TRUE) AS b, HEX(UUID_TO_BIN(BIN_TO_UUID(tbl, TRUE))) AS c, BIN_TO_UUID(ttx, IS_UUID(BIN_TO_UUID(ttx))) AS d, HEX(UUID_TO_BIN(BIN_TO_UUID(blb, FALSE))) AS e
FROM uuid_table
WHERE pkey = 3;
a	b	c	d	e
CB0948BB4B15C8EBBBB2E6A0B6B4D5C7	cb0948bb-4b15-c8eb-bbb2-e6a0b6b4d5c7	CB0948BB4B15C8EBBBB2E6A0B6B4D5C7	cb0948bb-4b15-c8eb-bbb2-e6a0b6b4d5c7	C8EB4B15CB0948BBBBB2E6A0B6B4D5C7
SELECT IS_UUID(BIN_TO_UUID(bin)) AS a, IS_UUID(BIN_TO_UUID(vbn)) AS b, IS_UUID(BIN_TO_UUID(tbl)) AS c, IS_UUID(BIN_TO_UUID(ttx)) AS d, IS_UUID(BIN_TO_UUID(blb)) AS e
FROM uuid_table
WHERE pkey <= 3;
a	b	c	d	e
1	1	1	1	1
1	1	1	1	1
1	1	1	1	1
include/diff_tables.inc [master:uuid_table, slave:uuid_table]
[Connection Master]
# Update values in table
UPDATE uuid_table SET bin = (UUID_TO_BIN('c8eb4b15-cb09-48bb-bbb2-e6a0b6b4d5c7', IS_UUID('12345678-1234-5678-1234-567812345678')))
WHERE pkey = 1;
include/sync_slave_sql_with_master.inc
[Connection Slave]
# Check whether value is updated on slave
SELECT HEX(UUID_TO_BIN(BIN_TO_UUID(bin))), BIN_TO_UUID(vbn), HEX(UUID_TO_BIN(BIN_TO_UUID(tbl))), BIN_TO_UUID(ttx), HEX(UUID_TO_BIN(BIN_TO_UUID(blb)))
FROM uuid_table
WHERE pkey = 1;
HEX(UUID_TO_BIN(BIN_TO_UUID(bin)))	BIN_TO_UUID(vbn)	HEX(UUID_TO_BIN(BIN_TO_UUID(tbl)))	BIN_TO_UUID(ttx)	HEX(UUID_TO_BIN(BIN_TO_UUID(blb)))
48BBCB09C8EB4B15BBB2E6A0B6B4D5C7	12345678-1234-5678-1234-567812345678	12345678123456781234567812345678	12345678-1234-5678-1234-567812345678	12345678123456781234567812345678
include/diff_tables.inc [master:uuid_table, slave:uuid_table]
[Connection Master]
# Delete a row from the table
DELETE FROM uuid_table WHERE blb = UUID_TO_BIN('{c8eb4b15-cb09-48bb-bbb2-e6a0b6b4d5c7}');
include/sync_slave_sql_with_master.inc
[Connection Slave]
# Check whether row has been deleted
SELECT * FROM uuid_table WHERE pkey = 3;
pkey	bin	vbn	tbl	ttx	blb
include/diff_tables.inc [master:uuid_table, slave:uuid_table]
[Connection Master]
# Create a table and insert invalid values of uuid
CREATE TABLE invalid_uuid ( pkey INT NOT NULL PRIMARY KEY AUTO_INCREMENT, col VARCHAR(50) );
INSERT into invalid_uuid VALUES ( NULL, '1234567812345678123456781234567' ),
( NULL, '1234-5678-1234-567812345678-12345678' ),
( NULL, '123456781234567812345678123456789' ),
( NULL, '12345678-1234-5678-1234567812345678' ),
( NULL, '{123456781234567812345678123456}78' );
include/sync_slave_sql_with_master.inc
[Connection Slave]
# Check whether table has been created on slave
SELECT BIN_TO_UUID(UNHEX(col)) FROM invalid_uuid;
ERROR HY000: Incorrect string value: '\x01#Eg\x81#Eg\x81#Eg\x81#Eg\x89' for function bin_to_uuid
include/diff_tables.inc [master:invalid_uuid, slave:invalid_uuid]
[Connection Master]
# create table with generated column
CREATE TABLE uuid_gencol (col1 VARCHAR(100), gcol2 BINARY(16) AS (UUID_TO_BIN(col1)) VIRTUAL, INDEX(gcol2));
INSERT INTO uuid_gencol (col1) VALUES
('{12345678-1234-5678-1234-567812345678}'),
('12345679123456781234567812345678'),
('12345670-1234-5678-1234-567812345678');
include/sync_slave_sql_with_master.inc
[Connection Slave]
# Check whether table is created
EXPLAIN SELECT * FROM uuid_gencol WHERE gcol2=x'12345679123456781234567812345678';
id	select_type	table	partitions	type	possible_keys	key	key_len	ref	rows	filtered	Extra
1	SIMPLE	uuid_gencol	NULL	ref	gcol2	gcol2	17	const	1	100.00	Using where
Warnings:
Note	1003	/* select#1 */ select `test`.`uuid_gencol`.`col1` AS `col1`,`test`.`uuid_gencol`.`gcol2` AS `gcol2` from `test`.`uuid_gencol` where (`test`.`uuid_gencol`.`gcol2` = 0x12345679123456781234567812345678)
SELECT col1, BIN_TO_UUID(gcol2) FROM uuid_gencol WHERE gcol2=x'12345679123456781234567812345678';
col1	BIN_TO_UUID(gcol2)
12345679123456781234567812345678	12345679-1234-5678-1234-567812345678
EXPLAIN SELECT * FROM uuid_gencol WHERE UUID_TO_BIN(col1)=x'12345679123456781234567812345678';
id	select_type	table	partitions	type	possible_keys	key	key_len	ref	rows	filtered	Extra
1	SIMPLE	uuid_gencol	NULL	ref	gcol2	gcol2	17	const	1	100.00	Using where
Warnings:
Note	1003	/* select#1 */ select `test`.`uuid_gencol`.`col1` AS `col1`,`test`.`uuid_gencol`.`gcol2` AS `gcol2` from `test`.`uuid_gencol` where (`test`.`uuid_gencol`.`gcol2` = 0x12345679123456781234567812345678)
SELECT col1, HEX(UUID_TO_BIN(BIN_TO_UUID(gcol2))) FROM uuid_gencol WHERE UUID_TO_BIN(col1)=x'12345679123456781234567812345678';
col1	HEX(UUID_TO_BIN(BIN_TO_UUID(gcol2)))
12345679123456781234567812345678	12345679123456781234567812345678
include/diff_tables.inc [master:uuid_gencol, slave:uuid_gencol]
[Connection Master]
# Drop tables and clean up
DROP TABLE uuid_table;
DROP TABLE invalid_uuid;
DROP TABLE uuid_gencol;
include/sync_slave_sql_with_master.inc
[Connection Slave]
# Check whether table is dropped
SELECT * FROM uuid_table;
ERROR 42S02: Table 'test.uuid_table' doesn't exist
include/rpl_end.inc

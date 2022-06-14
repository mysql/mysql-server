#
# BUG#30869674 - OPTIMIZE TABLE ON MYISAM CAN INCREASE TABLE SIZE (~2X) AND REDUCE PERFORMANCE
#
DROP DATABASE IF EXISTS test_bug30869674;
SET sql_mode = 'NO_ENGINE_SUBSTITUTION';
CREATE DATABASE test_bug30869674;
USE test_bug30869674;
CREATE TABLE t1(id int, name varchar(255), description varchar(255), count int, primary key(id)) ENGINE=myisam;
INSERT INTO t1 VALUES (1, "test1", "description1", 1), (2, "test2", "description2", 2), (3, "test3", "description3", 3);
FLUSH TABLES;
SELECT * FROM test_bug30869674.t1;
id	name	description	count
1	test1	description1	1
2	test2	description2	2
3	test3	description3	3
UPDATE t1 SET name="testing test2" WHERE id=2;
FLUSH TABLES;
Checking MyISAM file: MYSQLD_DATADIR/test_bug30869674/t1
Data records:       3   Deleted blocks:       0
- check file-size
- check record delete-chain
- check key delete-chain
- check index reference
- check data record references index: 1
Key:  1:  Keyblocks used:   3%  Packed:    0%  Max levels:  1
Total:    Keyblocks used:   3%  Packed:    0%

- check records and index references
Records:                 3    M.recordlength:       31   Packed:            98%
Recordspace used:       98%   Empty space:           1%  Blocks/Record:   1.33
Record blocks:           4    Delete blocks:         0
Record data:            95    Deleted data:          0
Lost space:              2    Linkdata:             23
OPTIMIZE TABLE t1;
Table	Op	Msg_type	Msg_text
test_bug30869674.t1	optimize	status	OK
Checking MyISAM file: MYSQLD_DATADIR/test_bug30869674/t1
Data records:       3   Deleted blocks:       0
- check file-size
- check record delete-chain
- check key delete-chain
- check index reference
- check data record references index: 1
Key:  1:  Keyblocks used:   3%  Packed:    0%  Max levels:  1
Total:    Keyblocks used:   3%  Packed:    0%

- check records and index references
Records:                 3    M.recordlength:       31   Packed:            98%
Recordspace used:      100%   Empty space:           0%  Blocks/Record:   1.00
Record blocks:           3    Delete blocks:         0
Record data:            95    Deleted data:          0
Lost space:              0    Linkdata:              9
SELECT * FROM test_bug30869674.t1;
id	name	description	count
1	test1	description1	1
2	testing test2	description2	2
3	test3	description3	3
DROP DATABASE test_bug30869674;

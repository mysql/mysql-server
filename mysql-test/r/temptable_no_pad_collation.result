#
# Prepare
#
SET @big_tables_saved = @@big_tables;
SET @optimizer_switch_saved = @@optimizer_switch;
#
# Check if selected collations have required padding mode
#
SELECT pad_attribute FROM information_schema.collations
WHERE collation_name = 'utf8mb4_0900_ai_ci';
pad_attribute
NO PAD
SELECT pad_attribute FROM information_schema.collations
WHERE collation_name = 'utf8mb4_general_ci';
pad_attribute
PAD SPACE
#
# Create test tables
#
CREATE TABLE table_char_no_pad (
f1 CHAR(20) COLLATE utf8mb4_0900_ai_ci
);
INSERT INTO table_char_no_pad VALUES ('ABC  ');
INSERT INTO table_char_no_pad VALUES ('XYZ');
INSERT INTO table_char_no_pad VALUES ('XYZ ');
INSERT INTO table_char_no_pad VALUES ('ABC ');
CREATE TABLE table_varchar_no_pad (
f1 VARCHAR(20) COLLATE utf8mb4_0900_ai_ci
);
INSERT INTO table_varchar_no_pad VALUES ('ABC  ');
INSERT INTO table_varchar_no_pad VALUES ('XYZ');
INSERT INTO table_varchar_no_pad VALUES ('XYZ ');
INSERT INTO table_varchar_no_pad VALUES ('ABC ');
CREATE TABLE table_char_pad_space (
f1 CHAR(20) COLLATE utf8mb4_general_ci
);
INSERT INTO table_char_pad_space VALUES ('ABC  ');
INSERT INTO table_char_pad_space VALUES ('XYZ');
INSERT INTO table_char_pad_space VALUES ('XYZ ');
INSERT INTO table_char_pad_space VALUES ('ABC ');
CREATE TABLE table_varchar_pad_space (
f1 VARCHAR(20) COLLATE utf8mb4_general_ci
);
INSERT INTO table_varchar_pad_space VALUES ('ABC  ');
INSERT INTO table_varchar_pad_space VALUES ('XYZ');
INSERT INTO table_varchar_pad_space VALUES ('XYZ ');
INSERT INTO table_varchar_pad_space VALUES ('ABC ');
SHOW STATUS LIKE 'Created_tmp_tables';
Variable_name	Value
Created_tmp_tables	2
SHOW STATUS LIKE 'Created_tmp_disk_tables';
Variable_name	Value
Created_tmp_disk_tables	0
#
# Disable optimization to force creating BTREE indexes.
#
SET @@optimizer_switch = "derived_merge=off";
#
# Test with Temptable engine
#
SET SESSION big_tables = 0;
SET @@internal_tmp_mem_storage_engine = TempTable;
SELECT f1, COUNT(*) FROM table_varchar_no_pad GROUP BY f1;
f1	COUNT(*)
ABC 	1
ABC  	1
XYZ	1
XYZ 	1
SELECT f1, COUNT(*) FROM table_char_no_pad GROUP BY f1;
f1	COUNT(*)
ABC	2
XYZ	2
SELECT TRIM(f1), COUNT(*) FROM table_varchar_pad_space GROUP BY f1;
TRIM(f1)	COUNT(*)
ABC	2
XYZ	2
SELECT f1, COUNT(*) FROM table_char_pad_space GROUP BY f1;
f1	COUNT(*)
ABC	2
XYZ	2
SELECT f1 FROM (SELECT * FROM table_varchar_no_pad) AS dt WHERE f1 = 'ABC';
f1
SELECT f1 FROM (SELECT * FROM table_char_no_pad) AS dt WHERE f1 = 'ABC';
f1
ABC
ABC
SELECT f1 FROM (SELECT * FROM table_varchar_pad_space) AS dt WHERE f1 = 'ABC';
f1
ABC 
ABC  
SELECT f1 FROM (SELECT * FROM table_char_pad_space) AS dt WHERE f1 = 'ABC';
f1
ABC
ABC
SHOW STATUS LIKE 'Created_tmp_tables';
Variable_name	Value
Created_tmp_tables	10
SHOW STATUS LIKE 'Created_tmp_disk_tables';
Variable_name	Value
Created_tmp_disk_tables	0
#
# Test with Heap engine
#
SET SESSION big_tables = 0;
SET @@internal_tmp_mem_storage_engine = MEMORY;
SELECT f1, COUNT(*) FROM table_varchar_no_pad GROUP BY f1;
f1	COUNT(*)
ABC 	1
ABC  	1
XYZ	1
XYZ 	1
SELECT f1, COUNT(*) FROM table_char_no_pad GROUP BY f1;
f1	COUNT(*)
ABC	2
XYZ	2
SELECT TRIM(f1), COUNT(*) FROM table_varchar_pad_space GROUP BY f1;
TRIM(f1)	COUNT(*)
ABC	2
XYZ	2
SELECT f1, COUNT(*) FROM table_char_pad_space GROUP BY f1;
f1	COUNT(*)
ABC	2
XYZ	2
SELECT f1 FROM (SELECT * FROM table_varchar_no_pad) AS dt WHERE f1 = 'ABC';
f1
SELECT f1 FROM (SELECT * FROM table_char_no_pad) AS dt WHERE f1 = 'ABC';
f1
ABC
ABC
SELECT f1 FROM (SELECT * FROM table_varchar_pad_space) AS dt WHERE f1 = 'ABC';
f1
ABC 
ABC  
SELECT f1 FROM (SELECT * FROM table_char_pad_space) AS dt WHERE f1 = 'ABC';
f1
ABC
ABC
SHOW STATUS LIKE 'Created_tmp_tables';
Variable_name	Value
Created_tmp_tables	18
SHOW STATUS LIKE 'Created_tmp_disk_tables';
Variable_name	Value
Created_tmp_disk_tables	0
#
# Restore default engine
#
SET @@internal_tmp_mem_storage_engine = default;
#
# Test with disk temp table engine
#
SET SESSION big_tables = 1;
SELECT f1, COUNT(*) FROM table_varchar_no_pad GROUP BY f1;
f1	COUNT(*)
ABC 	1
ABC  	1
XYZ	1
XYZ 	1
SELECT f1, COUNT(*) FROM table_char_no_pad GROUP BY f1;
f1	COUNT(*)
ABC	2
XYZ	2
SELECT TRIM(f1), COUNT(*) FROM table_varchar_pad_space GROUP BY f1;
TRIM(f1)	COUNT(*)
ABC	2
XYZ	2
SELECT f1, COUNT(*) FROM table_char_pad_space GROUP BY f1;
f1	COUNT(*)
ABC	2
XYZ	2
SELECT f1 FROM (SELECT * FROM table_varchar_no_pad) AS dt WHERE f1 = 'ABC';
f1
SELECT f1 FROM (SELECT * FROM table_char_no_pad) AS dt WHERE f1 = 'ABC';
f1
ABC
ABC
SELECT f1 FROM (SELECT * FROM table_varchar_pad_space) AS dt WHERE f1 = 'ABC';
f1
ABC 
ABC  
SELECT f1 FROM (SELECT * FROM table_char_pad_space) AS dt WHERE f1 = 'ABC';
f1
ABC
ABC
SHOW STATUS LIKE 'Created_tmp_tables';
Variable_name	Value
Created_tmp_tables	26
SHOW STATUS LIKE 'Created_tmp_disk_tables';
Variable_name	Value
Created_tmp_disk_tables	8
#
# Cleanup
#
SET SESSION big_tables = @big_tables_saved;
SET @@optimizer_switch = @optimizer_switch_saved;
DROP TABLE table_varchar_no_pad;
DROP TABLE table_char_no_pad;
DROP TABLE table_varchar_pad_space;
DROP TABLE table_char_pad_space;

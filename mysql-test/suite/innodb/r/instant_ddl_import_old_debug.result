# ------------------------------------------------------------
# Exported table t1 (in older version) having
#     c1, c2 normal columns
#     c3 INSTANT ADD column with default def_c3.
#     c4 INSTANT ADD column with default def_c4.
#       +------+------+--------+--------+
#       | c1   | c2   | c3     | c4     |
#       +------+------+--------+--------+
#       | r1c1 | r1c2 | c3_def | c4_def |
#       | r2c1 | r2c2 | r2c3   | c4_def |
#       | r3c1 | r3c2 | r3c3   | r3c4   |
#       +------+------+--------+--------+
# ------------------------------------------------------------
# Copy and unzip the ibd/cfg files.
# listing MYSQL_TMP_DIR/old_instant_comp
t1.cfg
t1.ibd
# ------------------------------------------------------------
# Scenario 1 : No INSTANT ADD/DROP Columns in target table
# ------------------------------------------------------------
create table t1 (c1 char(10), c2 char(10), c3 char(10), c4 char(10)) row_format=compact;
# Metadata from INFORMATION_SCHEMA.TABLES
NAME	N_COLS	INSTANT_COLS	TOTAL_ROW_VERSIONS	INITIAL_COLUMN_COUNTS	CURRENT_COLUMN_COUNTS	TOTAL_COLUMN_COUNTS
test/t1	7	0	0	4	4	4
# Metadata from INFORMATION_SCHEMA.COLUMNS
NAME	POS	MTYPE	PRTYPE	HAS_DEFAULT	VERSION_ADDED	VERSION_DROPPED
c1	0	13	16711934	0	0	0
c2	1	13	16711934	0	0	0
c3	2	13	16711934	0	0	0
c4	3	13	16711934	0	0	0
# DD Metadata of table
NAME	SE_PRIVATE_DATA
t1	NULL
# DD Metadata of columns in table
NAME	ORDINAL_POSITION	TYPE	HAS_NO_DEFAULT	HIDDEN	SE_PRIVATE_DATA
c1	1	MYSQL_TYPE_STRING	0	Visible	table_id=TABLE_ID;
c2	2	MYSQL_TYPE_STRING	0	Visible	table_id=TABLE_ID;
c3	3	MYSQL_TYPE_STRING	0	Visible	table_id=TABLE_ID;
c4	4	MYSQL_TYPE_STRING	0	Visible	table_id=TABLE_ID;
DB_ROW_ID	5	MYSQL_TYPE_INT24	0	SE	table_id=TABLE_ID;
DB_TRX_ID	6	MYSQL_TYPE_INT24	0	SE	table_id=TABLE_ID;
DB_ROLL_PTR	7	MYSQL_TYPE_LONGLONG	0	SE	table_id=TABLE_ID;
# IMPORT
ALTER TABLE t1 DISCARD TABLESPACE;
# Copy cfg/ibd files from temp location
ALTER TABLE t1 IMPORT TABLESPACE;
SELECT * FROM t1 ORDER BY c1;
c1	c2	c3	c4
r1c1	r1c2	c3_def	c4_def
r2c1	r2c2	r2c3	c4_def
r3c1	r3c2	r3c3	r3c4
# Metadata from INFORMATION_SCHEMA.TABLES
NAME	N_COLS	INSTANT_COLS	TOTAL_ROW_VERSIONS	INITIAL_COLUMN_COUNTS	CURRENT_COLUMN_COUNTS	TOTAL_COLUMN_COUNTS
test/t1	7	2	0	4	4	4
# Metadata from INFORMATION_SCHEMA.COLUMNS
NAME	POS	MTYPE	PRTYPE	HAS_DEFAULT	VERSION_ADDED	VERSION_DROPPED
c1	0	13	16711934	0	0	0
c2	1	13	16711934	0	0	0
c3	2	13	16711934	1	0	0
c4	3	13	16711934	1	0	0
# DD Metadata of table
NAME	SE_PRIVATE_DATA
t1	discard=0;instant_col=2;
# DD Metadata of columns in table
NAME	ORDINAL_POSITION	TYPE	HAS_NO_DEFAULT	HIDDEN	SE_PRIVATE_DATA
c1	1	MYSQL_TYPE_STRING	0	Visible	table_id=TABLE_ID;
c2	2	MYSQL_TYPE_STRING	0	Visible	table_id=TABLE_ID;
c3	3	MYSQL_TYPE_STRING	0	Visible	default=63335f64656620202020;table_id=TABLE_ID;
c4	4	MYSQL_TYPE_STRING	0	Visible	default=63345f64656620202020;table_id=TABLE_ID;
DB_ROW_ID	5	MYSQL_TYPE_INT24	0	SE	table_id=TABLE_ID;
DB_TRX_ID	6	MYSQL_TYPE_INT24	0	SE	table_id=TABLE_ID;
DB_ROLL_PTR	7	MYSQL_TYPE_LONGLONG	0	SE	table_id=TABLE_ID;
# Cleanup
DROP TABLE t1;
# ------------------------------------------------------------
# Scenario 2 : No INSTANT ADD/DROP. Column mismatch
# ------------------------------------------------------------
create table t1 (c1 char(10), c2 char(20), c3 char(10), c4 char(10)) row_format=compact;
# IMPORT
ALTER TABLE t1 DISCARD TABLESPACE;
# Copy cfg/ibd files from temp location
ALTER TABLE t1 IMPORT TABLESPACE;
ERROR HY000: Schema mismatch (Column c2 length mismatch.)
# Cleanup
DROP TABLE t1;
# ------------------------------------------------------------
# Scenario 3 : INSTANT ADD/DROP Columns in target table
# ------------------------------------------------------------
create table t1 (c1 char(10), c2 char(10)) row_format=compact;
ALTER TABLE t1 ADD COLUMN c3 char(10) DEFAULT "c3_def", ADD COLUMN c4 char(10) DEFAULT "c4_def";
# Metadata from INFORMATION_SCHEMA.TABLES
NAME	N_COLS	INSTANT_COLS	TOTAL_ROW_VERSIONS	INITIAL_COLUMN_COUNTS	CURRENT_COLUMN_COUNTS	TOTAL_COLUMN_COUNTS
test/t1	7	0	1	2	4	4
# Metadata from INFORMATION_SCHEMA.COLUMNS
NAME	POS	MTYPE	PRTYPE	HAS_DEFAULT	VERSION_ADDED	VERSION_DROPPED
c1	0	13	16711934	0	0	0
c2	1	13	16711934	0	0	0
c3	2	13	16711934	1	1	0
c4	3	13	16711934	1	1	0
# DD Metadata of table
NAME	SE_PRIVATE_DATA
t1	NULL
# DD Metadata of columns in table
NAME	ORDINAL_POSITION	TYPE	HAS_NO_DEFAULT	HIDDEN	SE_PRIVATE_DATA
c1	1	MYSQL_TYPE_STRING	0	Visible	physical_pos=3;table_id=TABLE_ID;
c2	2	MYSQL_TYPE_STRING	0	Visible	physical_pos=4;table_id=TABLE_ID;
c3	3	MYSQL_TYPE_STRING	0	Visible	default=63335f64656620202020;physical_pos=5;table_id=TABLE_ID;version_added=1;
c4	4	MYSQL_TYPE_STRING	0	Visible	default=63345f64656620202020;physical_pos=6;table_id=TABLE_ID;version_added=1;
DB_ROW_ID	5	MYSQL_TYPE_INT24	0	SE	physical_pos=0;table_id=TABLE_ID;
DB_TRX_ID	6	MYSQL_TYPE_INT24	0	SE	physical_pos=1;table_id=TABLE_ID;
DB_ROLL_PTR	7	MYSQL_TYPE_LONGLONG	0	SE	physical_pos=2;table_id=TABLE_ID;
# IMPORT
ALTER TABLE t1 DISCARD TABLESPACE;
# Copy cfg/ibd files from temp location
ALTER TABLE t1 IMPORT TABLESPACE;
ERROR HY000: Schema mismatch (Target table has INSTANT columns but the .cfg file is from earlier release with INSTANT column in the source table. Instant metadata can't match. Please create target table with no INSTANT column and try IMPORT.)
# Cleanup
DROP TABLE t1;
# ------------------------------------------------------------
# Scenario 4 : INSTANT ADD/DROP. Column mismatch
# ------------------------------------------------------------
create table t1 (c1 char(10), c2 char(10)) row_format=compact;
ALTER TABLE t1 ADD COLUMN c3 char(20) DEFAULT "c3_def", ADD COLUMN c4 char(10) DEFAULT "c4_def";
# Metadata from INFORMATION_SCHEMA.TABLES
NAME	N_COLS	INSTANT_COLS	TOTAL_ROW_VERSIONS	INITIAL_COLUMN_COUNTS	CURRENT_COLUMN_COUNTS	TOTAL_COLUMN_COUNTS
test/t1	7	0	1	2	4	4
# Metadata from INFORMATION_SCHEMA.COLUMNS
NAME	POS	MTYPE	PRTYPE	HAS_DEFAULT	VERSION_ADDED	VERSION_DROPPED
c1	0	13	16711934	0	0	0
c2	1	13	16711934	0	0	0
c3	2	13	16711934	1	1	0
c4	3	13	16711934	1	1	0
# DD Metadata of table
NAME	SE_PRIVATE_DATA
t1	NULL
# DD Metadata of columns in table
NAME	ORDINAL_POSITION	TYPE	HAS_NO_DEFAULT	HIDDEN	SE_PRIVATE_DATA
c1	1	MYSQL_TYPE_STRING	0	Visible	physical_pos=3;table_id=TABLE_ID;
c2	2	MYSQL_TYPE_STRING	0	Visible	physical_pos=4;table_id=TABLE_ID;
c3	3	MYSQL_TYPE_STRING	0	Visible	default=63335f6465662020202020202020202020202020;physical_pos=5;table_id=TABLE_ID;version_added=1;
c4	4	MYSQL_TYPE_STRING	0	Visible	default=63345f64656620202020;physical_pos=6;table_id=TABLE_ID;version_added=1;
DB_ROW_ID	5	MYSQL_TYPE_INT24	0	SE	physical_pos=0;table_id=TABLE_ID;
DB_TRX_ID	6	MYSQL_TYPE_INT24	0	SE	physical_pos=1;table_id=TABLE_ID;
DB_ROLL_PTR	7	MYSQL_TYPE_LONGLONG	0	SE	physical_pos=2;table_id=TABLE_ID;
# IMPORT
ALTER TABLE t1 DISCARD TABLESPACE;
# Copy cfg/ibd files from temp location
ALTER TABLE t1 IMPORT TABLESPACE;
ERROR HY000: Schema mismatch (Column c3 length mismatch.)
# Cleanup
DROP TABLE t1;
# ------------------------------------------------------------
# Scenario 5 : INSTANT ADD/DROP. Default value mismatch
# ------------------------------------------------------------
create table t1 (c1 char(10), c2 char(10)) row_format=compact;
ALTER TABLE t1 ADD COLUMN c3 char(10) DEFAULT "def_c3", ADD COLUMN c4 char(10) DEFAULT "c4_def";
# Metadata from INFORMATION_SCHEMA.TABLES
NAME	N_COLS	INSTANT_COLS	TOTAL_ROW_VERSIONS	INITIAL_COLUMN_COUNTS	CURRENT_COLUMN_COUNTS	TOTAL_COLUMN_COUNTS
test/t1	7	0	1	2	4	4
# Metadata from INFORMATION_SCHEMA.COLUMNS
NAME	POS	MTYPE	PRTYPE	HAS_DEFAULT	VERSION_ADDED	VERSION_DROPPED
c1	0	13	16711934	0	0	0
c2	1	13	16711934	0	0	0
c3	2	13	16711934	1	1	0
c4	3	13	16711934	1	1	0
# DD Metadata of table
NAME	SE_PRIVATE_DATA
t1	NULL
# DD Metadata of columns in table
NAME	ORDINAL_POSITION	TYPE	HAS_NO_DEFAULT	HIDDEN	SE_PRIVATE_DATA
c1	1	MYSQL_TYPE_STRING	0	Visible	physical_pos=3;table_id=TABLE_ID;
c2	2	MYSQL_TYPE_STRING	0	Visible	physical_pos=4;table_id=TABLE_ID;
c3	3	MYSQL_TYPE_STRING	0	Visible	default=6465665f633320202020;physical_pos=5;table_id=TABLE_ID;version_added=1;
c4	4	MYSQL_TYPE_STRING	0	Visible	default=63345f64656620202020;physical_pos=6;table_id=TABLE_ID;version_added=1;
DB_ROW_ID	5	MYSQL_TYPE_INT24	0	SE	physical_pos=0;table_id=TABLE_ID;
DB_TRX_ID	6	MYSQL_TYPE_INT24	0	SE	physical_pos=1;table_id=TABLE_ID;
DB_ROLL_PTR	7	MYSQL_TYPE_LONGLONG	0	SE	physical_pos=2;table_id=TABLE_ID;
# IMPORT
ALTER TABLE t1 DISCARD TABLESPACE;
# Copy cfg/ibd files from temp location
ALTER TABLE t1 IMPORT TABLESPACE;
ERROR HY000: Schema mismatch (Target table has INSTANT columns but the .cfg file is from earlier release with INSTANT column in the source table. Instant metadata can't match. Please create target table with no INSTANT column and try IMPORT.)
# Cleanup
DROP TABLE t1;
###########
# CLEANUP #
###########

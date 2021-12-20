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
NAME	N_COLS	INSTANT_COLS	TOTAL_ROW_VERSIONS
test/t1	7	0	0
# Metadata from INFORMATION_SCHEMA.COLUMNS
NAME	POS	MTYPE	PRTYPE	HAS_DEFAULT
c1	0	13	16711934	0
c2	1	13	16711934	0
c3	2	13	16711934	0
c4	3	13	16711934	0
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
NAME	N_COLS	INSTANT_COLS	TOTAL_ROW_VERSIONS
test/t1	7	2	0
# Metadata from INFORMATION_SCHEMA.COLUMNS
NAME	POS	MTYPE	PRTYPE	HAS_DEFAULT
c1	0	13	16711934	0
c2	1	13	16711934	0
c3	2	13	16711934	1
c4	3	13	16711934	1
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
NAME	N_COLS	INSTANT_COLS	TOTAL_ROW_VERSIONS
test/t1	7	0	1
# Metadata from INFORMATION_SCHEMA.COLUMNS
NAME	POS	MTYPE	PRTYPE	HAS_DEFAULT
c1	0	13	16711934	0
c2	1	13	16711934	0
c3	2	13	16711934	1
c4	3	13	16711934	1
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
NAME	N_COLS	INSTANT_COLS	TOTAL_ROW_VERSIONS
test/t1	7	0	1
# Metadata from INFORMATION_SCHEMA.COLUMNS
NAME	POS	MTYPE	PRTYPE	HAS_DEFAULT
c1	0	13	16711934	0
c2	1	13	16711934	0
c3	2	13	16711934	1
c4	3	13	16711934	1
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
NAME	N_COLS	INSTANT_COLS	TOTAL_ROW_VERSIONS
test/t1	7	0	1
# Metadata from INFORMATION_SCHEMA.COLUMNS
NAME	POS	MTYPE	PRTYPE	HAS_DEFAULT
c1	0	13	16711934	0
c2	1	13	16711934	0
c3	2	13	16711934	1
c4	3	13	16711934	1
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

# ------------------------------------------------------------
# Start server with old datadir with
#   Table t1_part and t2_part
#     having 2 partitions p0 and p1
#     having c1, c2 normal columns
#       +------+------+
#       | c1   | c2   |
#       +------+------+
#       | 10   | r1c2 |
#       +------+------+
#
#   Table t3_part (partition p0, p1)
#     having c1, c2 normal columns
#     having c3 INSTANT ADD column with default value
#     partition p2 added.
#     having c4 INSTANT ADD column with default value
#       +------+------+--------+--------+
#       | c1   | c2   | c3     | c4     |
#       +------+------+--------+--------+
#       |   10 | r1c2 | def_c3 | def_c4 |
#       +------+------+--------+--------+
# ------------------------------------------------------------
# Stop the running the server
# Copy and unzip the datadir.
# Restart the server against the unzipped datadir
# restart: --datadir=DATADIR --innodb-page-size=16k
# ------------------------------------------------------------
# TEST 1 : Old version tables have no INSTANT ADD Columns
# ------------------------------------------------------------
# ------------------------------------------------------------
# Check metadata of upgraded table t1_part [c1, c2]
# Expectation : New INSTANT METADATA is not present.
# ------------------------------------------------------------
# Metadata from INFORMATION_SCHEMA.TABLES
NAME	N_COLS	INSTANT_COLS	TOTAL_ROW_VERSIONS
test/t1_part#p#p0	5	0	0
# Metadata from INFORMATION_SCHEMA.COLUMNS
NAME	POS	MTYPE	PRTYPE	HAS_DEFAULT
c1	0	6	1027	0
c2	1	13	16711934	0
# Metadata from INFORMATION_SCHEMA.TABLES
NAME	N_COLS	INSTANT_COLS	TOTAL_ROW_VERSIONS
test/t1_part#p#p1	5	0	0
# Metadata from INFORMATION_SCHEMA.COLUMNS
NAME	POS	MTYPE	PRTYPE	HAS_DEFAULT
c1	0	6	1027	0
c2	1	13	16711934	0
# ------------------------------------------------------------
# INSTANT DROP a column and check metadata t1_part [c1, -c2]
# Expectation : New INSTANT METADATA is populated.
# ------------------------------------------------------------
ALTER TABLE t1_part DROP COLUMN c2, ALGORITHM=INSTANT;
# Metadata from INFORMATION_SCHEMA.TABLES
NAME	N_COLS	INSTANT_COLS	TOTAL_ROW_VERSIONS
test/t1_part#p#p0	4	0	1
# Metadata from INFORMATION_SCHEMA.COLUMNS
NAME	POS	MTYPE	PRTYPE	HAS_DEFAULT
c1	0	6	1027	0
# Metadata from INFORMATION_SCHEMA.TABLES
NAME	N_COLS	INSTANT_COLS	TOTAL_ROW_VERSIONS
test/t1_part#p#p1	4	0	1
# Metadata from INFORMATION_SCHEMA.COLUMNS
NAME	POS	MTYPE	PRTYPE	HAS_DEFAULT
c1	0	6	1027	0
# ------------------------------------------------------------
# Check metadata of upgraded table t2_part [c1, c2]
# Expectation : New INSTANT METADATA is not present.
# ------------------------------------------------------------
# Metadata from INFORMATION_SCHEMA.TABLES
NAME	N_COLS	INSTANT_COLS	TOTAL_ROW_VERSIONS
test/t2_part#p#p0	5	0	0
# Metadata from INFORMATION_SCHEMA.COLUMNS
NAME	POS	MTYPE	PRTYPE	HAS_DEFAULT
c1	0	6	1027	0
c2	1	13	16711934	0
# Metadata from INFORMATION_SCHEMA.TABLES
NAME	N_COLS	INSTANT_COLS	TOTAL_ROW_VERSIONS
test/t2_part#p#p1	5	0	0
# Metadata from INFORMATION_SCHEMA.COLUMNS
NAME	POS	MTYPE	PRTYPE	HAS_DEFAULT
c1	0	6	1027	0
c2	1	13	16711934	0
# ------------------------------------------------------------
# INSTANT ADD a column and check metadata t2_part [c1, c2, +c3]
# Expectation : New INSTANT METADATA is populated.
# ------------------------------------------------------------
ALTER TABLE t2_part ADD COLUMN c3 CHAR(10) DEFAULT "c3_def", ALGORITHM=INSTANT;
# Metadata from INFORMATION_SCHEMA.TABLES
NAME	N_COLS	INSTANT_COLS	TOTAL_ROW_VERSIONS
test/t2_part#p#p0	6	0	1
# Metadata from INFORMATION_SCHEMA.COLUMNS
NAME	POS	MTYPE	PRTYPE	HAS_DEFAULT
c1	0	6	1027	0
c2	1	13	16711934	0
c3	2	13	16711934	1
# Metadata from INFORMATION_SCHEMA.TABLES
NAME	N_COLS	INSTANT_COLS	TOTAL_ROW_VERSIONS
test/t2_part#p#p1	6	0	1
# Metadata from INFORMATION_SCHEMA.COLUMNS
NAME	POS	MTYPE	PRTYPE	HAS_DEFAULT
c1	0	6	1027	0
c2	1	13	16711934	0
c3	2	13	16711934	1
# ------------------------------------------------------------
# INSTANT DROP a column and check metadata t2_part [c1, -c2, c3]
# Expectation : New INSTANT METADATA is updated.
# ------------------------------------------------------------
ALTER TABLE t2_part DROP COLUMN c2, ALGORITHM=INSTANT;
# Metadata from INFORMATION_SCHEMA.TABLES
NAME	N_COLS	INSTANT_COLS	TOTAL_ROW_VERSIONS
test/t2_part#p#p0	5	0	2
# Metadata from INFORMATION_SCHEMA.COLUMNS
NAME	POS	MTYPE	PRTYPE	HAS_DEFAULT
c1	0	6	1027	0
c3	1	13	16711934	1
# Metadata from INFORMATION_SCHEMA.TABLES
NAME	N_COLS	INSTANT_COLS	TOTAL_ROW_VERSIONS
test/t2_part#p#p1	5	0	2
# Metadata from INFORMATION_SCHEMA.COLUMNS
NAME	POS	MTYPE	PRTYPE	HAS_DEFAULT
c1	0	6	1027	0
c3	1	13	16711934	1
DROP TABLE t1_part;
DROP TABLE t2_part;
# ------------------------------------------------------------
# TEST 2 : Old version tables have INSTANT ADD Columns
# ------------------------------------------------------------
# ------------------------------------------------------------
# Check metadata of upgraded table t3_part [c1, c2, c3, c4]
# Expectation : Old INSTANT METADATA is present and has
#               different INSTANT_COLS for p2.
#               No default value for c3 for p2.
#               New INSTANT METADATA is not present.
# ------------------------------------------------------------
# Metadata from INFORMATION_SCHEMA.TABLES
NAME	N_COLS	INSTANT_COLS	TOTAL_ROW_VERSIONS
test/t3_part#p#p0	7	2	0
# Metadata from INFORMATION_SCHEMA.COLUMNS
NAME	POS	MTYPE	PRTYPE	HAS_DEFAULT
c1	0	6	1027	0
c2	1	13	16711934	0
c3	2	13	16711934	1
c4	3	13	16711934	1
# Metadata from INFORMATION_SCHEMA.TABLES
NAME	N_COLS	INSTANT_COLS	TOTAL_ROW_VERSIONS
test/t3_part#p#p1	7	2	0
# Metadata from INFORMATION_SCHEMA.COLUMNS
NAME	POS	MTYPE	PRTYPE	HAS_DEFAULT
c1	0	6	1027	0
c2	1	13	16711934	0
c3	2	13	16711934	1
c4	3	13	16711934	1
# Metadata from INFORMATION_SCHEMA.TABLES
NAME	N_COLS	INSTANT_COLS	TOTAL_ROW_VERSIONS
test/t3_part#p#p2	7	3	0
# Metadata from INFORMATION_SCHEMA.COLUMNS
NAME	POS	MTYPE	PRTYPE	HAS_DEFAULT
c1	0	6	1027	0
c2	1	13	16711934	0
c3	2	13	16711934	0
c4	3	13	16711934	1
# -----------------------------------------------------------------------
# INSTANT ADD a new column and check metadata t3_part [c1, c2, c3, c4, +c5]
# Expectation : Old INSTANT METADATA stays same.
#               New INSTANT METADATA is populated.
#               Default value for c5 in each partition.
# -----------------------------------------------------------------------
ALTER TABLE t3_part ADD COLUMN c5 CHAR(10) DEFAULT "c5_def", ALGORITHM=INSTANT;
# Metadata from INFORMATION_SCHEMA.TABLES
NAME	N_COLS	INSTANT_COLS	TOTAL_ROW_VERSIONS
test/t3_part#p#p0	8	2	1
# Metadata from INFORMATION_SCHEMA.COLUMNS
NAME	POS	MTYPE	PRTYPE	HAS_DEFAULT
c1	0	6	1027	0
c2	1	13	16711934	0
c3	2	13	16711934	1
c4	3	13	16711934	1
c5	4	13	16711934	1
# Metadata from INFORMATION_SCHEMA.TABLES
NAME	N_COLS	INSTANT_COLS	TOTAL_ROW_VERSIONS
test/t3_part#p#p1	8	2	1
# Metadata from INFORMATION_SCHEMA.COLUMNS
NAME	POS	MTYPE	PRTYPE	HAS_DEFAULT
c1	0	6	1027	0
c2	1	13	16711934	0
c3	2	13	16711934	1
c4	3	13	16711934	1
c5	4	13	16711934	1
# Metadata from INFORMATION_SCHEMA.TABLES
NAME	N_COLS	INSTANT_COLS	TOTAL_ROW_VERSIONS
test/t3_part#p#p2	8	3	1
# Metadata from INFORMATION_SCHEMA.COLUMNS
NAME	POS	MTYPE	PRTYPE	HAS_DEFAULT
c1	0	6	1027	0
c2	1	13	16711934	0
c3	2	13	16711934	0
c4	3	13	16711934	1
c5	4	13	16711934	1
# --------------------------------------------------------------------
# Add a new partition p3.
# Expectation : Old INSTANT METADATA stays same.
#               Default value for c5 in each partition.
# -----------------------------------------------------------------------
ALTER TABLE t3_part ADD PARTITION (PARTITION p3 VALUES LESS THAN (400));
# Metadata from INFORMATION_SCHEMA.TABLES
NAME	N_COLS	INSTANT_COLS	TOTAL_ROW_VERSIONS
test/t3_part#p#p0	8	2	1
# Metadata from INFORMATION_SCHEMA.COLUMNS
NAME	POS	MTYPE	PRTYPE	HAS_DEFAULT
c1	0	6	1027	0
c2	1	13	16711934	0
c3	2	13	16711934	1
c4	3	13	16711934	1
c5	4	13	16711934	1
# Metadata from INFORMATION_SCHEMA.TABLES
NAME	N_COLS	INSTANT_COLS	TOTAL_ROW_VERSIONS
test/t3_part#p#p1	8	2	1
# Metadata from INFORMATION_SCHEMA.COLUMNS
NAME	POS	MTYPE	PRTYPE	HAS_DEFAULT
c1	0	6	1027	0
c2	1	13	16711934	0
c3	2	13	16711934	1
c4	3	13	16711934	1
c5	4	13	16711934	1
# Metadata from INFORMATION_SCHEMA.TABLES
NAME	N_COLS	INSTANT_COLS	TOTAL_ROW_VERSIONS
test/t3_part#p#p2	8	3	1
# Metadata from INFORMATION_SCHEMA.COLUMNS
NAME	POS	MTYPE	PRTYPE	HAS_DEFAULT
c1	0	6	1027	0
c2	1	13	16711934	0
c3	2	13	16711934	0
c4	3	13	16711934	1
c5	4	13	16711934	1
# Metadata from INFORMATION_SCHEMA.TABLES
NAME	N_COLS	INSTANT_COLS	TOTAL_ROW_VERSIONS
test/t3_part#p#p3	8	0	1
# Metadata from INFORMATION_SCHEMA.COLUMNS
NAME	POS	MTYPE	PRTYPE	HAS_DEFAULT
c1	0	6	1027	0
c2	1	13	16711934	0
c3	2	13	16711934	0
c4	3	13	16711934	0
c5	4	13	16711934	1
# --------------------------------------------------------------------
# INSTANT DROP a normal column and check metadata t3_part [c1, -c2, c3, c4, c5]
# Expectation : Old INSTANT METADATA stays same.
#               New INSTANT METADATA is updated.
# ---------------------------------------------------------------------
ALTER TABLE t3_part DROP COLUMN c2, ALGORITHM=INSTANT;
# Metadata from INFORMATION_SCHEMA.TABLES
NAME	N_COLS	INSTANT_COLS	TOTAL_ROW_VERSIONS
test/t3_part#p#p0	7	2	2
# Metadata from INFORMATION_SCHEMA.COLUMNS
NAME	POS	MTYPE	PRTYPE	HAS_DEFAULT
c1	0	6	1027	0
c3	1	13	16711934	1
c4	2	13	16711934	1
c5	3	13	16711934	1
# Metadata from INFORMATION_SCHEMA.TABLES
NAME	N_COLS	INSTANT_COLS	TOTAL_ROW_VERSIONS
test/t3_part#p#p1	7	2	2
# Metadata from INFORMATION_SCHEMA.COLUMNS
NAME	POS	MTYPE	PRTYPE	HAS_DEFAULT
c1	0	6	1027	0
c3	1	13	16711934	1
c4	2	13	16711934	1
c5	3	13	16711934	1
# Metadata from INFORMATION_SCHEMA.TABLES
NAME	N_COLS	INSTANT_COLS	TOTAL_ROW_VERSIONS
test/t3_part#p#p2	7	3	2
# Metadata from INFORMATION_SCHEMA.COLUMNS
NAME	POS	MTYPE	PRTYPE	HAS_DEFAULT
c1	0	6	1027	0
c3	1	13	16711934	0
c4	2	13	16711934	1
c5	3	13	16711934	1
# Metadata from INFORMATION_SCHEMA.TABLES
NAME	N_COLS	INSTANT_COLS	TOTAL_ROW_VERSIONS
test/t3_part#p#p3	7	0	2
# Metadata from INFORMATION_SCHEMA.COLUMNS
NAME	POS	MTYPE	PRTYPE	HAS_DEFAULT
c1	0	6	1027	0
c3	1	13	16711934	0
c4	2	13	16711934	0
c5	3	13	16711934	1
# -----------------------------------------------------------------------
# INSTANT DROP an INSTANT column and check metadata t3_part [c1, ~c2, ~c3, c4, c5]
# Expectation : Old INSTANT METADATA stays same.
#               New INSTANT METADATA is updated.
# -----------------------------------------------------------------------
ALTER TABLE t3_part DROP COLUMN c3, ALGORITHM=INSTANT;
# Metadata from INFORMATION_SCHEMA.TABLES
NAME	N_COLS	INSTANT_COLS	TOTAL_ROW_VERSIONS
test/t3_part#p#p0	6	2	3
# Metadata from INFORMATION_SCHEMA.COLUMNS
NAME	POS	MTYPE	PRTYPE	HAS_DEFAULT
c1	0	6	1027	0
c4	1	13	16711934	1
c5	2	13	16711934	1
# Metadata from INFORMATION_SCHEMA.TABLES
NAME	N_COLS	INSTANT_COLS	TOTAL_ROW_VERSIONS
test/t3_part#p#p1	6	2	3
# Metadata from INFORMATION_SCHEMA.COLUMNS
NAME	POS	MTYPE	PRTYPE	HAS_DEFAULT
c1	0	6	1027	0
c4	1	13	16711934	1
c5	2	13	16711934	1
# Metadata from INFORMATION_SCHEMA.TABLES
NAME	N_COLS	INSTANT_COLS	TOTAL_ROW_VERSIONS
test/t3_part#p#p2	6	3	3
# Metadata from INFORMATION_SCHEMA.COLUMNS
NAME	POS	MTYPE	PRTYPE	HAS_DEFAULT
c1	0	6	1027	0
c4	1	13	16711934	0
c5	2	13	16711934	1
# Metadata from INFORMATION_SCHEMA.TABLES
NAME	N_COLS	INSTANT_COLS	TOTAL_ROW_VERSIONS
test/t3_part#p#p3	6	0	3
# Metadata from INFORMATION_SCHEMA.COLUMNS
NAME	POS	MTYPE	PRTYPE	HAS_DEFAULT
c1	0	6	1027	0
c4	1	13	16711934	0
c5	2	13	16711934	1
# -----------------------------------------------------------------------------
# INSTANT DROP newly added column and check metadata t3_part [c1, ~c2, ~c3, c4, -c5]
# Expectation : Old INSTANT METADATA stays same.
#               New INSTANT METADATA is updated.
# -----------------------------------------------------------------------------
ALTER TABLE t3_part DROP COLUMN c5, ALGORITHM=INSTANT;
# Metadata from INFORMATION_SCHEMA.TABLES
NAME	N_COLS	INSTANT_COLS	TOTAL_ROW_VERSIONS
test/t3_part#p#p0	5	2	4
# Metadata from INFORMATION_SCHEMA.COLUMNS
NAME	POS	MTYPE	PRTYPE	HAS_DEFAULT
c1	0	6	1027	0
c4	1	13	16711934	1
# Metadata from INFORMATION_SCHEMA.TABLES
NAME	N_COLS	INSTANT_COLS	TOTAL_ROW_VERSIONS
test/t3_part#p#p1	5	2	4
# Metadata from INFORMATION_SCHEMA.COLUMNS
NAME	POS	MTYPE	PRTYPE	HAS_DEFAULT
c1	0	6	1027	0
c4	1	13	16711934	1
# Metadata from INFORMATION_SCHEMA.TABLES
NAME	N_COLS	INSTANT_COLS	TOTAL_ROW_VERSIONS
test/t3_part#p#p2	5	3	4
# Metadata from INFORMATION_SCHEMA.COLUMNS
NAME	POS	MTYPE	PRTYPE	HAS_DEFAULT
c1	0	6	1027	0
c4	1	13	16711934	0
# Metadata from INFORMATION_SCHEMA.TABLES
NAME	N_COLS	INSTANT_COLS	TOTAL_ROW_VERSIONS
test/t3_part#p#p3	5	0	4
# Metadata from INFORMATION_SCHEMA.COLUMNS
NAME	POS	MTYPE	PRTYPE	HAS_DEFAULT
c1	0	6	1027	0
c4	1	13	16711934	1
DROP TABLE t3_part;
# -------------------------------------------------
# TEST 3 : New tables with INSTANT ADD/DROP Columns
# -------------------------------------------------
CREATE TABLE t1_part (c1 INT, c2 CHAR(10)) PARTITION BY RANGE(c1) (PARTITION p0 VALUES LESS THAN (100), PARTITION p1 VALUES LESS THAN (200));
INSERT INTO t1_part VALUES(10, "r1c2");
# ------------------------------------------------------------
# Check metadata of table t1_part [c1, c2]
# Expectation : Old INSTANT METADATA is not present
#               New INSTANT METADATA is not present.
# ------------------------------------------------------------
# Metadata from INFORMATION_SCHEMA.TABLES
NAME	N_COLS	INSTANT_COLS	TOTAL_ROW_VERSIONS
test/t1_part#p#p0	5	0	0
# Metadata from INFORMATION_SCHEMA.COLUMNS
NAME	POS	MTYPE	PRTYPE	HAS_DEFAULT
c1	0	6	1027	0
c2	1	13	16711934	0
# Metadata from INFORMATION_SCHEMA.TABLES
NAME	N_COLS	INSTANT_COLS	TOTAL_ROW_VERSIONS
test/t1_part#p#p1	5	0	0
# Metadata from INFORMATION_SCHEMA.COLUMNS
NAME	POS	MTYPE	PRTYPE	HAS_DEFAULT
c1	0	6	1027	0
c2	1	13	16711934	0
# -----------------------------------------------------------------------
# INSTANT DROP a column and check metadata t1_part [c1, -c2]
# Expectation : Old INSTANT METADATA is not present.
#               New INSTANT METADATA is populated.
# -----------------------------------------------------------------------
ALTER TABLE t1_part DROP COLUMN c2, ALGORITHM=INSTANT;
# Metadata from INFORMATION_SCHEMA.TABLES
NAME	N_COLS	INSTANT_COLS	TOTAL_ROW_VERSIONS
test/t1_part#p#p0	4	0	1
# Metadata from INFORMATION_SCHEMA.COLUMNS
NAME	POS	MTYPE	PRTYPE	HAS_DEFAULT
c1	0	6	1027	0
# Metadata from INFORMATION_SCHEMA.TABLES
NAME	N_COLS	INSTANT_COLS	TOTAL_ROW_VERSIONS
test/t1_part#p#p1	4	0	1
# Metadata from INFORMATION_SCHEMA.COLUMNS
NAME	POS	MTYPE	PRTYPE	HAS_DEFAULT
c1	0	6	1027	0
# -----------------------------------------------------------------------
# INSTANT ADD a column and check metadata t1_part [c1, ~c2, +c3]
# Expectation : Old INSTANT METADATA not present.
#               New INSTANT METADATA is updated.
# -----------------------------------------------------------------------
ALTER TABLE t1_part ADD COLUMN c3 CHAR(10) DEFAULT "c3_def", ALGORITHM=INSTANT;
# Metadata from INFORMATION_SCHEMA.TABLES
NAME	N_COLS	INSTANT_COLS	TOTAL_ROW_VERSIONS
test/t1_part#p#p0	5	0	2
# Metadata from INFORMATION_SCHEMA.COLUMNS
NAME	POS	MTYPE	PRTYPE	HAS_DEFAULT
c1	0	6	1027	0
c3	1	13	16711934	1
# Metadata from INFORMATION_SCHEMA.TABLES
NAME	N_COLS	INSTANT_COLS	TOTAL_ROW_VERSIONS
test/t1_part#p#p1	5	0	2
# Metadata from INFORMATION_SCHEMA.COLUMNS
NAME	POS	MTYPE	PRTYPE	HAS_DEFAULT
c1	0	6	1027	0
c3	1	13	16711934	1
# --------------------------------------------------------------------
# Add a new partition p2.
# Expectation : Old INSTANT METADATA not present.
#               New INSTANT METADATA is updated.
#               Default value for c3 in each partition.
# -----------------------------------------------------------------------
ALTER TABLE t1_part ADD PARTITION (PARTITION p2 VALUES LESS THAN (300));
# Metadata from INFORMATION_SCHEMA.TABLES
NAME	N_COLS	INSTANT_COLS	TOTAL_ROW_VERSIONS
test/t1_part#p#p0	5	0	2
# Metadata from INFORMATION_SCHEMA.COLUMNS
NAME	POS	MTYPE	PRTYPE	HAS_DEFAULT
c1	0	6	1027	0
c3	1	13	16711934	1
# Metadata from INFORMATION_SCHEMA.TABLES
NAME	N_COLS	INSTANT_COLS	TOTAL_ROW_VERSIONS
test/t1_part#p#p1	5	0	2
# Metadata from INFORMATION_SCHEMA.COLUMNS
NAME	POS	MTYPE	PRTYPE	HAS_DEFAULT
c1	0	6	1027	0
c3	1	13	16711934	1
# Metadata from INFORMATION_SCHEMA.TABLES
NAME	N_COLS	INSTANT_COLS	TOTAL_ROW_VERSIONS
test/t1_part#p#p2	5	0	2
# Metadata from INFORMATION_SCHEMA.COLUMNS
NAME	POS	MTYPE	PRTYPE	HAS_DEFAULT
c1	0	6	1027	0
c3	1	13	16711934	1
# ----------------------------------------------------------------------------
# INSTANT DROP the INSTANT ADD column and check metadata t1_part [c1, ~c2, -c3]
# Expectation : Old INSTANT METADATA is not present.
#               New INSTANT METADATA is updated.
# ----------------------------------------------------------------------------
ALTER TABLE t1_part DROP COLUMN c3, ALGORITHM=INSTANT;
# Metadata from INFORMATION_SCHEMA.TABLES
NAME	N_COLS	INSTANT_COLS	TOTAL_ROW_VERSIONS
test/t1_part#p#p0	4	0	3
# Metadata from INFORMATION_SCHEMA.COLUMNS
NAME	POS	MTYPE	PRTYPE	HAS_DEFAULT
c1	0	6	1027	0
# Metadata from INFORMATION_SCHEMA.TABLES
NAME	N_COLS	INSTANT_COLS	TOTAL_ROW_VERSIONS
test/t1_part#p#p1	4	0	3
# Metadata from INFORMATION_SCHEMA.COLUMNS
NAME	POS	MTYPE	PRTYPE	HAS_DEFAULT
c1	0	6	1027	0
# Metadata from INFORMATION_SCHEMA.TABLES
NAME	N_COLS	INSTANT_COLS	TOTAL_ROW_VERSIONS
test/t1_part#p#p2	4	0	3
# Metadata from INFORMATION_SCHEMA.COLUMNS
NAME	POS	MTYPE	PRTYPE	HAS_DEFAULT
c1	0	6	1027	0
DROP TABLE t1_part;
###########
# CLEANUP #
###########
# Shutdown server
# Remove copied files
# Restarting server to restore server state
# restart

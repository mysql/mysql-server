# ------------------------------------------------------------
# Start server with old datadir with
#   Table t1 and t2
#     having c1, c2, c3 normal columns
#       +------+------+------+
#       | c1   | c2   | c3   |
#       +------+------+------+
#       | r1c1 | r1c2 | r1c3 |
#       +------+------+------+
#
#   Table t3
#     having c1, c2 normal columns
#     having c3 INSTANT ADD column with NULL default value
#     having c4 INSTANT ADD column with default value
#       +------+------+------+--------+
#       | c1   | c2   | c3   | c4     |
#       +------+------+------+--------+
#       | r1c1 | r1c2 | NULL | c4_def |
#       +------+------+------+--------+
# ------------------------------------------------------------
# Stop the running the server
# Copy and unzip the datadir.
# Restart the server against the unzipped datadir
# restart: --datadir=DATADIR --innodb-page-size=16k
# ------------------------------------------------------------
# TEST 1 : Old version tables have no INSTANT ADD Columns
# ------------------------------------------------------------
# ------------------------------------------------------------
# Check metadat of upgraded table t1 [c1, c2, c3]
# Expectation : New INSTANT METADATA is not present.
# ------------------------------------------------------------
# Metadata from INFORMATION_SCHEMA.TABLES
NAME	N_COLS	INSTANT_COLS	TOTAL_ROW_VERSIONS
test/t1	6	0	0
# Metadata from INFORMATION_SCHEMA.COLUMNS
NAME	POS	MTYPE	PRTYPE	HAS_DEFAULT
c1	0	13	16711934	0
c2	1	13	16711934	0
c3	2	13	16711934	0
# ------------------------------------------------------------
# INSTANT DROP a column and check metadata t1 [c1, c3, -c2]
# Expectation : New INSTANT METADATA is populated.
# ------------------------------------------------------------
ALTER TABLE t1 DROP COLUMN c2, ALGORITHM=INSTANT;
# Metadata from INFORMATION_SCHEMA.TABLES
NAME	N_COLS	INSTANT_COLS	TOTAL_ROW_VERSIONS
test/t1	5	0	1
# Metadata from INFORMATION_SCHEMA.COLUMNS
NAME	POS	MTYPE	PRTYPE	HAS_DEFAULT
c1	0	13	16711934	0
c3	1	13	16711934	0
# ------------------------------------------------------------
# Check metadat of upgraded table t2 [c1, c2, c3]
# Expectation : New INSTANT METADATA is not present.
# ------------------------------------------------------------
# Metadata from INFORMATION_SCHEMA.TABLES
NAME	N_COLS	INSTANT_COLS	TOTAL_ROW_VERSIONS
test/t2	6	0	0
# Metadata from INFORMATION_SCHEMA.COLUMNS
NAME	POS	MTYPE	PRTYPE	HAS_DEFAULT
c1	0	13	16711934	0
c2	1	13	16711934	0
c3	2	13	16711934	0
# ------------------------------------------------------------
# INSTANT ADD a column and check metadata t2 [c1, c2, c3, +c4]
# Expectation : New INSTANT METADATA is populated.
# ------------------------------------------------------------
ALTER TABLE t2 ADD COLUMN c4 CHAR(10) DEFAULT "c4_def", ALGORITHM=INSTANT;
# Metadata from INFORMATION_SCHEMA.TABLES
NAME	N_COLS	INSTANT_COLS	TOTAL_ROW_VERSIONS
test/t2	7	0	1
# Metadata from INFORMATION_SCHEMA.COLUMNS
NAME	POS	MTYPE	PRTYPE	HAS_DEFAULT
c1	0	13	16711934	0
c2	1	13	16711934	0
c3	2	13	16711934	0
c4	3	13	16711934	1
# ------------------------------------------------------------
# INSTANT DROP a column and check metadata t2 [c1, c2, -c3, c4]
# Expectation : New INSTANT METADATA is updated.
# ------------------------------------------------------------
ALTER TABLE t2 DROP COLUMN c3, ALGORITHM=INSTANT;
# Metadata from INFORMATION_SCHEMA.TABLES
NAME	N_COLS	INSTANT_COLS	TOTAL_ROW_VERSIONS
test/t2	6	0	2
# Metadata from INFORMATION_SCHEMA.COLUMNS
NAME	POS	MTYPE	PRTYPE	HAS_DEFAULT
c1	0	13	16711934	0
c2	1	13	16711934	0
c4	2	13	16711934	1
DROP TABLE t1;
DROP TABLE t2;
# ------------------------------------------------------------
# TEST 2 : Old version tables have INSTANT ADD Columns
# ------------------------------------------------------------
# ------------------------------------------------------------
# Check metadat of upgraded table t3 [c1, c2, c3, c4]
# Expectation : Old INSTANT METADATA is present
#               New INSTANT METADATA is not present.
# ------------------------------------------------------------
# Metadata from INFORMATION_SCHEMA.TABLES
NAME	N_COLS	INSTANT_COLS	TOTAL_ROW_VERSIONS
test/t3	7	2	0
# Metadata from INFORMATION_SCHEMA.COLUMNS
NAME	POS	MTYPE	PRTYPE	HAS_DEFAULT
c1	0	13	16711934	0
c2	1	13	16711934	0
c3	2	13	16711934	1
c4	3	13	16711934	1
# --------------------------------------------------------------------
# INSTANT DROP a normal column and check metadata t3 [c1, -c2, c3, c4]
# Expectation : Old INSTANT METADATA stays same.
#               New INSTANT METADATA is populated.
# ---------------------------------------------------------------------
ALTER TABLE t3 DROP COLUMN c2, ALGORITHM=INSTANT;
# Metadata from INFORMATION_SCHEMA.TABLES
NAME	N_COLS	INSTANT_COLS	TOTAL_ROW_VERSIONS
test/t3	6	2	1
# Metadata from INFORMATION_SCHEMA.COLUMNS
NAME	POS	MTYPE	PRTYPE	HAS_DEFAULT
c1	0	13	16711934	0
c3	1	13	16711934	1
c4	2	13	16711934	1
# -----------------------------------------------------------------------
# INSTANT DROP an INSTANT column and check metadata t3 [c1, ~c2, -c3, c4]
# Expectation : Old INSTANT METADATA stays same.
#               New INSTANT METADATA is updated.
# -----------------------------------------------------------------------
ALTER TABLE t3 DROP COLUMN c3, ALGORITHM=INSTANT;
# Metadata from INFORMATION_SCHEMA.TABLES
NAME	N_COLS	INSTANT_COLS	TOTAL_ROW_VERSIONS
test/t3	5	2	2
# Metadata from INFORMATION_SCHEMA.COLUMNS
NAME	POS	MTYPE	PRTYPE	HAS_DEFAULT
c1	0	13	16711934	0
c4	1	13	16711934	1
# -----------------------------------------------------------------------
# INSTANT ADD a new column and check metadata t3 [c1, ~c2, ~c3, c4, +c5]
# Expectation : Old INSTANT METADATA stays same.
#               New INSTANT METADATA is updated.
# -----------------------------------------------------------------------
ALTER TABLE t3 ADD COLUMN c5 CHAR(10) DEFAULT "c5_def", ALGORITHM=INSTANT;
# Metadata from INFORMATION_SCHEMA.TABLES
NAME	N_COLS	INSTANT_COLS	TOTAL_ROW_VERSIONS
test/t3	6	2	3
# Metadata from INFORMATION_SCHEMA.COLUMNS
NAME	POS	MTYPE	PRTYPE	HAS_DEFAULT
c1	0	13	16711934	0
c4	1	13	16711934	1
c5	2	13	16711934	1
# -----------------------------------------------------------------------------
# INSTANT DROP newly added column and check metadata t3 [c1, ~c2, ~c3, c4, -c5]
# Expectation : Old INSTANT METADATA stays same.
#               New INSTANT METADATA is updated.
# -----------------------------------------------------------------------------
ALTER TABLE t3 DROP COLUMN c5, ALGORITHM=INSTANT;
# Metadata from INFORMATION_SCHEMA.TABLES
NAME	N_COLS	INSTANT_COLS	TOTAL_ROW_VERSIONS
test/t3	5	2	4
# Metadata from INFORMATION_SCHEMA.COLUMNS
NAME	POS	MTYPE	PRTYPE	HAS_DEFAULT
c1	0	13	16711934	0
c4	1	13	16711934	1
DROP TABLE t3;
# -------------------------------------------------
# TEST 3 : New tables with INSTANT ADD/DROP Columns
# -------------------------------------------------
CREATE TABLE t1 (c1 char(10), c2 char(10), c3 char(10));
# ------------------------------------------------------------
# Check metadat of table t1 [c1, c2, c3]
# Expectation : Old INSTANT METADATA is not present
#               New INSTANT METADATA is not present.
# ------------------------------------------------------------
# Metadata from INFORMATION_SCHEMA.TABLES
NAME	N_COLS	INSTANT_COLS	TOTAL_ROW_VERSIONS
test/t1	6	0	0
# Metadata from INFORMATION_SCHEMA.COLUMNS
NAME	POS	MTYPE	PRTYPE	HAS_DEFAULT
c1	0	13	16711934	0
c2	1	13	16711934	0
c3	2	13	16711934	0
# -----------------------------------------------------------------------
# INSTANT DROP a column and check metadata t1 [c1, -c2, c3]
# Expectation : Old INSTANT METADATA is not present.
#               New INSTANT METADATA is populated.
# -----------------------------------------------------------------------
ALTER TABLE t1 DROP COLUMN c2, ALGORITHM=INSTANT;
# Metadata from INFORMATION_SCHEMA.TABLES
NAME	N_COLS	INSTANT_COLS	TOTAL_ROW_VERSIONS
test/t1	5	0	1
# Metadata from INFORMATION_SCHEMA.COLUMNS
NAME	POS	MTYPE	PRTYPE	HAS_DEFAULT
c1	0	13	16711934	0
c3	1	13	16711934	0
# -----------------------------------------------------------------------
# INSTANT ADD a column and check metadata t1 [+c0, c1, ~c2, c3]
# Expectation : Old INSTANT METADATA not present.
#               New INSTANT METADATA is updated.
# -----------------------------------------------------------------------
ALTER TABLE t1 ADD COLUMN c0 CHAR(10) DEFAULT "c0_def" FIRST, ALGORITHM=INSTANT;
# Metadata from INFORMATION_SCHEMA.TABLES
NAME	N_COLS	INSTANT_COLS	TOTAL_ROW_VERSIONS
test/t1	6	0	2
# Metadata from INFORMATION_SCHEMA.COLUMNS
NAME	POS	MTYPE	PRTYPE	HAS_DEFAULT
c0	0	13	16711934	1
c1	1	13	16711934	0
c3	2	13	16711934	0
# ----------------------------------------------------------------------------
# INSTANT DROP the INSTANT ADD column and check metadata t1 [-c0, c1, ~c2, c3]
# Expectation : Old INSTANT METADATA is not present.
#               New INSTANT METADATA is updated.
# ----------------------------------------------------------------------------
ALTER TABLE t1 DROP COLUMN c0, ALGORITHM=INSTANT;
# Metadata from INFORMATION_SCHEMA.TABLES
NAME	N_COLS	INSTANT_COLS	TOTAL_ROW_VERSIONS
test/t1	5	0	3
# Metadata from INFORMATION_SCHEMA.COLUMNS
NAME	POS	MTYPE	PRTYPE	HAS_DEFAULT
c1	0	13	16711934	0
c3	1	13	16711934	0
DROP TABLE t1;
###########
# CLEANUP #
###########
# Shutdown server
# Remove copied files
# Restarting server to restore server state
# restart

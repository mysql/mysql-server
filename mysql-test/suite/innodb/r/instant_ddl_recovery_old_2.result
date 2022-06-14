#########
# SETUP #
#########
# Stop the running the server
# Copy and unzip the datadir.
# Restart the server against the unzipped datadir
# restart: --datadir=DATADIR --innodb-page-size=16k
##############################################
# Test instant ADD COLUMN for REDUNDANT format
##############################################
#########
# SETUP #
#########
# Metadata from INFORMATION_SCHEMA.TABLES
NAME	N_COLS	INSTANT_COLS	TOTAL_ROW_VERSIONS	INITIAL_COLUMN_COUNTS	CURRENT_COLUMN_COUNTS	TOTAL_COLUMN_COUNTS
test/t1_redundant	6	2	0	3	3	3
# Metadata from INFORMATION_SCHEMA.COLUMNS
NAME	POS	MTYPE	PRTYPE	HAS_DEFAULT	VERSION_ADDED	VERSION_DROPPED
c1	0	13	16711934	0	0	0
c2	1	13	16711934	0	0	0
c3	2	13	16711934	1	0	0
# DD Metadata of table
NAME	SE_PRIVATE_DATA
t1_redundant	instant_col=2;
# DD Metadata of columns in table
NAME	ORDINAL_POSITION	TYPE	HAS_NO_DEFAULT	HIDDEN	SE_PRIVATE_DATA
c1	1	MYSQL_TYPE_STRING	0	Visible	table_id=TABLE_ID;
c2	2	MYSQL_TYPE_STRING	0	Visible	table_id=TABLE_ID;
c3	3	MYSQL_TYPE_STRING	0	Visible	default_null=1;table_id=TABLE_ID;
DB_ROW_ID	4	MYSQL_TYPE_INT24	0	SE	table_id=TABLE_ID;
DB_TRX_ID	5	MYSQL_TYPE_INT24	0	SE	table_id=TABLE_ID;
DB_ROLL_PTR	6	MYSQL_TYPE_LONGLONG	0	SE	table_id=TABLE_ID;
SELECT * FROM t1_redundant;
c1	c2	c3
r1c1	r1c2	NULL
SET GLOBAL innodb_log_checkpoint_now = ON;
SET GLOBAL innodb_page_cleaner_disabled_debug = 1;
SET GLOBAL innodb_dict_stats_disabled_debug = 1;
SET GLOBAL innodb_master_thread_disabled_debug = 1;
INSERT INTO t1_redundant VALUES ("r2c1", "r2c2", "r2c3");
# Restart running server.
SELECT * FROM t1_redundant;
c1	c2	c3
r1c1	r1c2	NULL
r2c1	r2c2	r2c3
# Metadata from INFORMATION_SCHEMA.TABLES
NAME	N_COLS	INSTANT_COLS	TOTAL_ROW_VERSIONS	INITIAL_COLUMN_COUNTS	CURRENT_COLUMN_COUNTS	TOTAL_COLUMN_COUNTS
test/t1_redundant	6	2	0	3	3	3
# Metadata from INFORMATION_SCHEMA.COLUMNS
NAME	POS	MTYPE	PRTYPE	HAS_DEFAULT	VERSION_ADDED	VERSION_DROPPED
c1	0	13	16711934	0	0	0
c2	1	13	16711934	0	0	0
c3	2	13	16711934	1	0	0
# DD Metadata of table
NAME	SE_PRIVATE_DATA
t1_redundant	instant_col=2;
# DD Metadata of columns in table
NAME	ORDINAL_POSITION	TYPE	HAS_NO_DEFAULT	HIDDEN	SE_PRIVATE_DATA
c1	1	MYSQL_TYPE_STRING	0	Visible	table_id=TABLE_ID;
c2	2	MYSQL_TYPE_STRING	0	Visible	table_id=TABLE_ID;
c3	3	MYSQL_TYPE_STRING	0	Visible	default_null=1;table_id=TABLE_ID;
DB_ROW_ID	4	MYSQL_TYPE_INT24	0	SE	table_id=TABLE_ID;
DB_TRX_ID	5	MYSQL_TYPE_INT24	0	SE	table_id=TABLE_ID;
DB_ROLL_PTR	6	MYSQL_TYPE_LONGLONG	0	SE	table_id=TABLE_ID;
###########
# CLEANUP #
###########
DROP TABLE t1_redundant;
############################################
# Test instant ADD COLUMN for DYNAMIC format
############################################
#########
# SETUP #
#########
# Metadata from INFORMATION_SCHEMA.TABLES
NAME	N_COLS	INSTANT_COLS	TOTAL_ROW_VERSIONS	INITIAL_COLUMN_COUNTS	CURRENT_COLUMN_COUNTS	TOTAL_COLUMN_COUNTS
test/t1_dynamic	6	2	0	3	3	3
# Metadata from INFORMATION_SCHEMA.COLUMNS
NAME	POS	MTYPE	PRTYPE	HAS_DEFAULT	VERSION_ADDED	VERSION_DROPPED
c1	0	13	16711934	0	0	0
c2	1	13	16711934	0	0	0
c3	2	13	16711934	1	0	0
# DD Metadata of table
NAME	SE_PRIVATE_DATA
t1_dynamic	instant_col=2;
# DD Metadata of columns in table
NAME	ORDINAL_POSITION	TYPE	HAS_NO_DEFAULT	HIDDEN	SE_PRIVATE_DATA
c1	1	MYSQL_TYPE_STRING	0	Visible	table_id=TABLE_ID;
c2	2	MYSQL_TYPE_STRING	0	Visible	table_id=TABLE_ID;
c3	3	MYSQL_TYPE_STRING	0	Visible	default_null=1;table_id=TABLE_ID;
DB_ROW_ID	4	MYSQL_TYPE_INT24	0	SE	table_id=TABLE_ID;
DB_TRX_ID	5	MYSQL_TYPE_INT24	0	SE	table_id=TABLE_ID;
DB_ROLL_PTR	6	MYSQL_TYPE_LONGLONG	0	SE	table_id=TABLE_ID;
SELECT * FROM t1_dynamic;
c1	c2	c3
r1c1	r1c2	NULL
SET GLOBAL innodb_log_checkpoint_now = ON;
SET GLOBAL innodb_page_cleaner_disabled_debug = 1;
SET GLOBAL innodb_dict_stats_disabled_debug = 1;
SET GLOBAL innodb_master_thread_disabled_debug = 1;
INSERT INTO t1_dynamic VALUES ("r2c1", "r2c2", "r2c3");
# Restart running server.
SELECT * FROM t1_dynamic;
c1	c2	c3
r1c1	r1c2	NULL
r2c1	r2c2	r2c3
# Metadata from INFORMATION_SCHEMA.TABLES
NAME	N_COLS	INSTANT_COLS	TOTAL_ROW_VERSIONS	INITIAL_COLUMN_COUNTS	CURRENT_COLUMN_COUNTS	TOTAL_COLUMN_COUNTS
test/t1_dynamic	6	2	0	3	3	3
# Metadata from INFORMATION_SCHEMA.COLUMNS
NAME	POS	MTYPE	PRTYPE	HAS_DEFAULT	VERSION_ADDED	VERSION_DROPPED
c1	0	13	16711934	0	0	0
c2	1	13	16711934	0	0	0
c3	2	13	16711934	1	0	0
# DD Metadata of table
NAME	SE_PRIVATE_DATA
t1_dynamic	instant_col=2;
# DD Metadata of columns in table
NAME	ORDINAL_POSITION	TYPE	HAS_NO_DEFAULT	HIDDEN	SE_PRIVATE_DATA
c1	1	MYSQL_TYPE_STRING	0	Visible	table_id=TABLE_ID;
c2	2	MYSQL_TYPE_STRING	0	Visible	table_id=TABLE_ID;
c3	3	MYSQL_TYPE_STRING	0	Visible	default_null=1;table_id=TABLE_ID;
DB_ROW_ID	4	MYSQL_TYPE_INT24	0	SE	table_id=TABLE_ID;
DB_TRX_ID	5	MYSQL_TYPE_INT24	0	SE	table_id=TABLE_ID;
DB_ROLL_PTR	6	MYSQL_TYPE_LONGLONG	0	SE	table_id=TABLE_ID;
###########
# CLEANUP #
###########
DROP TABLE t1_dynamic;
############################################
# Test instant ADD COLUMN for COMPACT format
############################################
#########
# SETUP #
#########
# Metadata from INFORMATION_SCHEMA.TABLES
NAME	N_COLS	INSTANT_COLS	TOTAL_ROW_VERSIONS	INITIAL_COLUMN_COUNTS	CURRENT_COLUMN_COUNTS	TOTAL_COLUMN_COUNTS
test/t1_compact	6	2	0	3	3	3
# Metadata from INFORMATION_SCHEMA.COLUMNS
NAME	POS	MTYPE	PRTYPE	HAS_DEFAULT	VERSION_ADDED	VERSION_DROPPED
c1	0	13	16711934	0	0	0
c2	1	13	16711934	0	0	0
c3	2	13	16711934	1	0	0
# DD Metadata of table
NAME	SE_PRIVATE_DATA
t1_compact	instant_col=2;
# DD Metadata of columns in table
NAME	ORDINAL_POSITION	TYPE	HAS_NO_DEFAULT	HIDDEN	SE_PRIVATE_DATA
c1	1	MYSQL_TYPE_STRING	0	Visible	table_id=TABLE_ID;
c2	2	MYSQL_TYPE_STRING	0	Visible	table_id=TABLE_ID;
c3	3	MYSQL_TYPE_STRING	0	Visible	default_null=1;table_id=TABLE_ID;
DB_ROW_ID	4	MYSQL_TYPE_INT24	0	SE	table_id=TABLE_ID;
DB_TRX_ID	5	MYSQL_TYPE_INT24	0	SE	table_id=TABLE_ID;
DB_ROLL_PTR	6	MYSQL_TYPE_LONGLONG	0	SE	table_id=TABLE_ID;
SELECT * FROM t1_compact;
c1	c2	c3
r1c1	r1c2	NULL
SET GLOBAL innodb_log_checkpoint_now = ON;
SET GLOBAL innodb_page_cleaner_disabled_debug = 1;
SET GLOBAL innodb_dict_stats_disabled_debug = 1;
SET GLOBAL innodb_master_thread_disabled_debug = 1;
INSERT INTO t1_compact VALUES ("r2c1", "r2c2", "r2c3");
# Restart running server.
SELECT * FROM t1_compact;
c1	c2	c3
r1c1	r1c2	NULL
r2c1	r2c2	r2c3
# Metadata from INFORMATION_SCHEMA.TABLES
NAME	N_COLS	INSTANT_COLS	TOTAL_ROW_VERSIONS	INITIAL_COLUMN_COUNTS	CURRENT_COLUMN_COUNTS	TOTAL_COLUMN_COUNTS
test/t1_compact	6	2	0	3	3	3
# Metadata from INFORMATION_SCHEMA.COLUMNS
NAME	POS	MTYPE	PRTYPE	HAS_DEFAULT	VERSION_ADDED	VERSION_DROPPED
c1	0	13	16711934	0	0	0
c2	1	13	16711934	0	0	0
c3	2	13	16711934	1	0	0
# DD Metadata of table
NAME	SE_PRIVATE_DATA
t1_compact	instant_col=2;
# DD Metadata of columns in table
NAME	ORDINAL_POSITION	TYPE	HAS_NO_DEFAULT	HIDDEN	SE_PRIVATE_DATA
c1	1	MYSQL_TYPE_STRING	0	Visible	table_id=TABLE_ID;
c2	2	MYSQL_TYPE_STRING	0	Visible	table_id=TABLE_ID;
c3	3	MYSQL_TYPE_STRING	0	Visible	default_null=1;table_id=TABLE_ID;
DB_ROW_ID	4	MYSQL_TYPE_INT24	0	SE	table_id=TABLE_ID;
DB_TRX_ID	5	MYSQL_TYPE_INT24	0	SE	table_id=TABLE_ID;
DB_ROLL_PTR	6	MYSQL_TYPE_LONGLONG	0	SE	table_id=TABLE_ID;
###########
# CLEANUP #
###########
DROP TABLE t1_compact;
###########
# CLEANUP #
###########
# Shutdown server
# Remove copied files
# Restarting server to restore server state
# restart

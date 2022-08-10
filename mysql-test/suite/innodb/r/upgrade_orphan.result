#
# Bug#26997500 - UPGRADE FAILS DUE TO ORPHAN .FRM AND CANNOT BE RESUMED
#
# Set different paths for --datadir
# Copy the remote tablespace & DB zip files from suite location to working location.
# Check that the file exists in the working folder.
# Unzip the zip file.
# These files are added to test error scenario, delete from for upgrade testing.
# Remove myisam partitioned tables. There are used for negative testing.
# Create an orphan .frm
# start server and we expect failure because of orphan .frm(sakila/zzz.frm)
# Remove orphan .frm
# Retry upgrade on the failed datadir
# Sanity Testing after Server start
CREATE SCHEMA test123;
CREATE TABLE test123.t1(a int);
INSERT INTO test123.t1 values(1);
SELECT * FROM test123.t1;
a
1
DROP SCHEMA test123;
SHOW DATABASES;
Database
aview
events
foreign_keys
information_schema
mysql
performance_schema
sakila
sp
sys
tablespace
test
test_mismatch
triggers
view_with_column_names
# Check table structures
SHOW CREATE TABLE mysql.user;
Table	Create Table
user	CREATE TABLE `user` (
  `Host` char(255) CHARACTER SET ascii COLLATE ascii_general_ci NOT NULL DEFAULT '',
  `User` char(32) COLLATE utf8mb3_bin NOT NULL DEFAULT '',
  `Select_priv` enum('N','Y') CHARACTER SET utf8mb3 COLLATE utf8mb3_general_ci NOT NULL DEFAULT 'N',
  `Insert_priv` enum('N','Y') CHARACTER SET utf8mb3 COLLATE utf8mb3_general_ci NOT NULL DEFAULT 'N',
  `Update_priv` enum('N','Y') CHARACTER SET utf8mb3 COLLATE utf8mb3_general_ci NOT NULL DEFAULT 'N',
  `Delete_priv` enum('N','Y') CHARACTER SET utf8mb3 COLLATE utf8mb3_general_ci NOT NULL DEFAULT 'N',
  `Create_priv` enum('N','Y') CHARACTER SET utf8mb3 COLLATE utf8mb3_general_ci NOT NULL DEFAULT 'N',
  `Drop_priv` enum('N','Y') CHARACTER SET utf8mb3 COLLATE utf8mb3_general_ci NOT NULL DEFAULT 'N',
  `Reload_priv` enum('N','Y') CHARACTER SET utf8mb3 COLLATE utf8mb3_general_ci NOT NULL DEFAULT 'N',
  `Shutdown_priv` enum('N','Y') CHARACTER SET utf8mb3 COLLATE utf8mb3_general_ci NOT NULL DEFAULT 'N',
  `Process_priv` enum('N','Y') CHARACTER SET utf8mb3 COLLATE utf8mb3_general_ci NOT NULL DEFAULT 'N',
  `File_priv` enum('N','Y') CHARACTER SET utf8mb3 COLLATE utf8mb3_general_ci NOT NULL DEFAULT 'N',
  `Grant_priv` enum('N','Y') CHARACTER SET utf8mb3 COLLATE utf8mb3_general_ci NOT NULL DEFAULT 'N',
  `References_priv` enum('N','Y') CHARACTER SET utf8mb3 COLLATE utf8mb3_general_ci NOT NULL DEFAULT 'N',
  `Index_priv` enum('N','Y') CHARACTER SET utf8mb3 COLLATE utf8mb3_general_ci NOT NULL DEFAULT 'N',
  `Alter_priv` enum('N','Y') CHARACTER SET utf8mb3 COLLATE utf8mb3_general_ci NOT NULL DEFAULT 'N',
  `Show_db_priv` enum('N','Y') CHARACTER SET utf8mb3 COLLATE utf8mb3_general_ci NOT NULL DEFAULT 'N',
  `Super_priv` enum('N','Y') CHARACTER SET utf8mb3 COLLATE utf8mb3_general_ci NOT NULL DEFAULT 'N',
  `Create_tmp_table_priv` enum('N','Y') CHARACTER SET utf8mb3 COLLATE utf8mb3_general_ci NOT NULL DEFAULT 'N',
  `Lock_tables_priv` enum('N','Y') CHARACTER SET utf8mb3 COLLATE utf8mb3_general_ci NOT NULL DEFAULT 'N',
  `Execute_priv` enum('N','Y') CHARACTER SET utf8mb3 COLLATE utf8mb3_general_ci NOT NULL DEFAULT 'N',
  `Repl_slave_priv` enum('N','Y') CHARACTER SET utf8mb3 COLLATE utf8mb3_general_ci NOT NULL DEFAULT 'N',
  `Repl_client_priv` enum('N','Y') CHARACTER SET utf8mb3 COLLATE utf8mb3_general_ci NOT NULL DEFAULT 'N',
  `Create_view_priv` enum('N','Y') CHARACTER SET utf8mb3 COLLATE utf8mb3_general_ci NOT NULL DEFAULT 'N',
  `Show_view_priv` enum('N','Y') CHARACTER SET utf8mb3 COLLATE utf8mb3_general_ci NOT NULL DEFAULT 'N',
  `Create_routine_priv` enum('N','Y') CHARACTER SET utf8mb3 COLLATE utf8mb3_general_ci NOT NULL DEFAULT 'N',
  `Alter_routine_priv` enum('N','Y') CHARACTER SET utf8mb3 COLLATE utf8mb3_general_ci NOT NULL DEFAULT 'N',
  `Create_user_priv` enum('N','Y') CHARACTER SET utf8mb3 COLLATE utf8mb3_general_ci NOT NULL DEFAULT 'N',
  `Event_priv` enum('N','Y') CHARACTER SET utf8mb3 COLLATE utf8mb3_general_ci NOT NULL DEFAULT 'N',
  `Trigger_priv` enum('N','Y') CHARACTER SET utf8mb3 COLLATE utf8mb3_general_ci NOT NULL DEFAULT 'N',
  `Create_tablespace_priv` enum('N','Y') CHARACTER SET utf8mb3 COLLATE utf8mb3_general_ci NOT NULL DEFAULT 'N',
  `ssl_type` enum('','ANY','X509','SPECIFIED') CHARACTER SET utf8mb3 COLLATE utf8mb3_general_ci NOT NULL DEFAULT '',
  `ssl_cipher` blob NOT NULL,
  `x509_issuer` blob NOT NULL,
  `x509_subject` blob NOT NULL,
  `max_questions` int unsigned NOT NULL DEFAULT '0',
  `max_updates` int unsigned NOT NULL DEFAULT '0',
  `max_connections` int unsigned NOT NULL DEFAULT '0',
  `max_user_connections` int unsigned NOT NULL DEFAULT '0',
  `plugin` char(64) COLLATE utf8mb3_bin NOT NULL DEFAULT 'caching_sha2_password',
  `authentication_string` text COLLATE utf8mb3_bin,
  `password_expired` enum('N','Y') CHARACTER SET utf8mb3 COLLATE utf8mb3_general_ci NOT NULL DEFAULT 'N',
  `password_last_changed` timestamp NULL DEFAULT NULL,
  `password_lifetime` smallint unsigned DEFAULT NULL,
  `account_locked` enum('N','Y') CHARACTER SET utf8mb3 COLLATE utf8mb3_general_ci NOT NULL DEFAULT 'N',
  `Create_role_priv` enum('N','Y') CHARACTER SET utf8mb3 COLLATE utf8mb3_general_ci NOT NULL DEFAULT 'N',
  `Drop_role_priv` enum('N','Y') CHARACTER SET utf8mb3 COLLATE utf8mb3_general_ci NOT NULL DEFAULT 'N',
  `Password_reuse_history` smallint unsigned DEFAULT NULL,
  `Password_reuse_time` smallint unsigned DEFAULT NULL,
  `Password_require_current` enum('N','Y') CHARACTER SET utf8mb3 COLLATE utf8mb3_general_ci DEFAULT NULL,
  `User_attributes` json DEFAULT NULL,
  PRIMARY KEY (`Host`,`User`)
) /*!50100 TABLESPACE `mysql` */ ENGINE=InnoDB DEFAULT CHARSET=utf8mb3 COLLATE=utf8mb3_bin STATS_PERSISTENT=0 ROW_FORMAT=DYNAMIC COMMENT='Users and global privileges'
SHOW CREATE TABLE mysql.innodb_table_stats;
Table	Create Table
innodb_table_stats	CREATE TABLE `innodb_table_stats` (
  `database_name` varchar(64) COLLATE utf8mb3_bin NOT NULL,
  `table_name` varchar(199) COLLATE utf8mb3_bin NOT NULL,
  `last_update` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
  `n_rows` bigint unsigned NOT NULL,
  `clustered_index_size` bigint unsigned NOT NULL,
  `sum_of_other_index_sizes` bigint unsigned NOT NULL,
  PRIMARY KEY (`database_name`,`table_name`)
) /*!50100 TABLESPACE `mysql` */ ENGINE=InnoDB DEFAULT CHARSET=utf8mb3 COLLATE=utf8mb3_bin STATS_PERSISTENT=0 ROW_FORMAT=DYNAMIC
SHOW CREATE TABLE mysql.innodb_index_stats;
Table	Create Table
innodb_index_stats	CREATE TABLE `innodb_index_stats` (
  `database_name` varchar(64) COLLATE utf8mb3_bin NOT NULL,
  `table_name` varchar(199) COLLATE utf8mb3_bin NOT NULL,
  `index_name` varchar(64) COLLATE utf8mb3_bin NOT NULL,
  `last_update` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
  `stat_name` varchar(64) COLLATE utf8mb3_bin NOT NULL,
  `stat_value` bigint unsigned NOT NULL,
  `sample_size` bigint unsigned DEFAULT NULL,
  `stat_description` varchar(1024) COLLATE utf8mb3_bin NOT NULL,
  PRIMARY KEY (`database_name`,`table_name`,`index_name`,`stat_name`)
) /*!50100 TABLESPACE `mysql` */ ENGINE=InnoDB DEFAULT CHARSET=utf8mb3 COLLATE=utf8mb3_bin STATS_PERSISTENT=0 ROW_FORMAT=DYNAMIC
SELECT COLUMN_NAME,CHARACTER_MAXIMUM_LENGTH
FROM INFORMATION_SCHEMA.COLUMNS col
JOIN INFORMATION_SCHEMA.TABLES tab ON col.table_name=tab.table_name
WHERE col.TABLE_NAME LIKE '%innodb_%_stats'
    AND col.COLUMN_NAME LIKE 'table_name';
COLUMN_NAME	CHARACTER_MAXIMUM_LENGTH
table_name	199
table_name	199
SHOW CREATE TABLE test.t_compressed;
Table	Create Table
t_compressed	CREATE TABLE `t_compressed` (
  `c1` int NOT NULL,
  `c2` varchar(20) DEFAULT NULL,
  PRIMARY KEY (`c1`)
) ENGINE=InnoDB DEFAULT CHARSET=latin1 ROW_FORMAT=COMPRESSED
SHOW CREATE TABLE test.t_compressed2;
Table	Create Table
t_compressed2	CREATE TABLE `t_compressed2` (
  `c1` int NOT NULL,
  `c2` varchar(20) DEFAULT NULL,
  PRIMARY KEY (`c1`)
) ENGINE=InnoDB DEFAULT CHARSET=latin1 ROW_FORMAT=COMPRESSED KEY_BLOCK_SIZE=4
SHOW CREATE TABLE test.t_compressed3;
Table	Create Table
t_compressed3	CREATE TABLE `t_compressed3` (
  `a` int DEFAULT NULL
) ENGINE=InnoDB DEFAULT CHARSET=latin1 ROW_FORMAT=COMPRESSED KEY_BLOCK_SIZE=4
SHOW CREATE TABLE test.t_dynamic;
Table	Create Table
t_dynamic	CREATE TABLE `t_dynamic` (
  `c1` int NOT NULL,
  `c2` varchar(20) DEFAULT NULL,
  PRIMARY KEY (`c1`)
) ENGINE=InnoDB DEFAULT CHARSET=latin1 ROW_FORMAT=DYNAMIC
SHOW CREATE TABLE test.t_index;
Table	Create Table
t_index	CREATE TABLE `t_index` (
  `c1` int NOT NULL,
  `c2` int NOT NULL,
  PRIMARY KEY (`c1`),
  KEY `c2` (`c2`) USING BTREE
) ENGINE=MyISAM DEFAULT CHARSET=latin1
SHOW CREATE TABLE test.vt2;
Table	Create Table
vt2	CREATE TABLE `vt2` (
  `c1` text NOT NULL,
  `c2` text GENERATED ALWAYS AS (substr(`c1`,1,2)) VIRTUAL,
  PRIMARY KEY (`c1`(20)),
  UNIQUE KEY `c2` (`c2`(2))
) ENGINE=InnoDB DEFAULT CHARSET=latin1
SELECT * FROM test.vt2;
c1	c2
abcd	ab
SHOW CREATE TABLE test.t_gen_stored;
Table	Create Table
t_gen_stored	CREATE TABLE `t_gen_stored` (
  `a` varchar(50) DEFAULT NULL,
  `b` varchar(10) GENERATED ALWAYS AS (substr(`a`,5)) STORED
) ENGINE=InnoDB DEFAULT CHARSET=latin1
SHOW CREATE TABLE test.jemp;
Table	Create Table
jemp	CREATE TABLE `jemp` (
  `c` json DEFAULT NULL,
  `g` int GENERATED ALWAYS AS (json_extract(`c`,_utf8mb4'$.id')) VIRTUAL,
  KEY `i` (`g`)
) ENGINE=InnoDB DEFAULT CHARSET=latin1
SHOW CREATE TABLE test.t_gen_stored_myisam;
Table	Create Table
t_gen_stored_myisam	CREATE TABLE `t_gen_stored_myisam` (
  `a` varchar(50) DEFAULT NULL,
  `b` varchar(10) GENERATED ALWAYS AS (substr(`a`,5)) STORED
) ENGINE=MyISAM DEFAULT CHARSET=latin1
SHOW CREATE TABLE test.t_gen_stored_myisam2;
Table	Create Table
t_gen_stored_myisam2	CREATE TABLE `t_gen_stored_myisam2` (
  `a` varchar(50) DEFAULT NULL,
  `b` varchar(10) GENERATED ALWAYS AS (substr(`a`,5)) STORED,
  KEY `a` (`a`,`b`)
) ENGINE=MyISAM DEFAULT CHARSET=latin1
SHOW CREATE TABLE test.jemp_myisam;
Table	Create Table
jemp_myisam	CREATE TABLE `jemp_myisam` (
  `c` json DEFAULT NULL,
  `g` int GENERATED ALWAYS AS (json_extract(`c`,_utf8mb4'$.id')) VIRTUAL
) ENGINE=MyISAM DEFAULT CHARSET=latin1
SELECT TABLE_SCHEMA, TABLE_NAME, TABLE_TYPE, ENGINE, VERSION, ROW_FORMAT
FROM INFORMATION_SCHEMA.tables WHERE table_schema='test';
TABLE_SCHEMA	TABLE_NAME	TABLE_TYPE	ENGINE	VERSION	ROW_FORMAT
test	child	BASE TABLE	InnoDB	10	Dynamic
test	geom	BASE TABLE	InnoDB	10	Dynamic
test	jemp	BASE TABLE	InnoDB	10	Dynamic
test	jemp_myisam	BASE TABLE	MyISAM	10	Dynamic
test	opening_lines	BASE TABLE	InnoDB	10	Dynamic
test	parent	BASE TABLE	InnoDB	10	Dynamic
test	t_blackhole	BASE TABLE	BLACKHOLE	10	Fixed
test	t_blob	BASE TABLE	InnoDB	10	Dynamic
test	t_blob_myisam	BASE TABLE	InnoDB	10	Dynamic
test	t_compressed	BASE TABLE	InnoDB	10	Compressed
test	t_compressed2	BASE TABLE	InnoDB	10	Compressed
test	t_compressed3	BASE TABLE	InnoDB	10	Compressed
test	t_dynamic	BASE TABLE	InnoDB	10	Dynamic
test	t_gen_stored	BASE TABLE	InnoDB	10	Dynamic
test	t_gen_stored_myisam	BASE TABLE	MyISAM	10	Dynamic
test	t_gen_stored_myisam2	BASE TABLE	MyISAM	10	Dynamic
test	t_index	BASE TABLE	MyISAM	10	Fixed
test	t_json	BASE TABLE	InnoDB	10	Dynamic
test	t_myisam_compressed	BASE TABLE	MyISAM	10	Dynamic
test	t_myisam_compressed2	BASE TABLE	MyISAM	10	Compressed
test	t_myisam_compressed3	BASE TABLE	MyISAM	10	Compressed
test	t_sc~!@#$%^&*(	BASE TABLE	InnoDB	10	Dynamic
test	vt2	BASE TABLE	InnoDB	10	Dynamic
# Check for views
# Invalid view
SHOW CREATE VIEW aview.view_invalid;
View	Create View	character_set_client	collation_connection
view_invalid	CREATE ALGORITHM=UNDEFINED DEFINER=`root`@`localhost` SQL SECURITY DEFINER VIEW `aview`.`view_invalid` AS select `aview`.`t`.`a` AS `column1`,`aview`.`t`.`b` AS `column2` from `aview`.`t`	utf8mb3	utf8mb3_general_ci
Warnings:
Warning	1356	View 'aview.view_invalid' references invalid table(s) or column(s) or function(s) or definer/invoker of view lack rights to use them
# Valid views
SHOW CREATE VIEW aview.view_2;
View	Create View	character_set_client	collation_connection
view_2	CREATE ALGORITHM=UNDEFINED DEFINER=`root`@`localhost` SQL SECURITY DEFINER VIEW `aview`.`view_2` AS select `aview`.`t1`.`a` AS `column1`,`aview`.`t1`.`b` AS `column2`,`aview`.`t1`.`c` AS `column3`,`aview`.`t2`.`a` AS `column4` from (`aview`.`t1` join `aview`.`t2`)	utf8mb3	utf8mb3_general_ci
SHOW CREATE VIEW aview.second_view;
View	Create View	character_set_client	collation_connection
second_view	CREATE ALGORITHM=UNDEFINED DEFINER=`root`@`localhost` SQL SECURITY DEFINER VIEW `aview`.`second_view` AS select `aview`.`view_2`.`column1` AS `column1`,`aview`.`view_2`.`column2` AS `column2`,`aview`.`view_2`.`column3` AS `column3`,`aview`.`view_2`.`column4` AS `column4` from `aview`.`view_2`	utf8mb3	utf8mb3_general_ci
SELECT * FROM aview.view_2;
column1	column2	column3	column4
50	ABCD	EFGH	100
SELECT * FROM aview.second_view;
column1	column2	column3	column4
50	ABCD	EFGH	100
SHOW CREATE VIEW aview.mixed_view;
View	Create View	character_set_client	collation_connection
mixed_view	CREATE ALGORITHM=MERGE DEFINER=`root`@`localhost` SQL SECURITY INVOKER VIEW `aview`.`mixed_view` AS select `aview`.`second_view`.`column3` AS `column3`,`aview`.`second_view`.`column4` AS `column4`,`hello`('FUNC') AS `hello('FUNC')` from `aview`.`second_view` WITH CASCADED CHECK OPTION	utf8mb3	utf8mb3_general_ci
SHOW CREATE VIEW aview.mixed_view2;
View	Create View	character_set_client	collation_connection
mixed_view2	CREATE ALGORITHM=MERGE DEFINER=`root`@`localhost` SQL SECURITY DEFINER VIEW `aview`.`mixed_view2` AS select `aview`.`second_view`.`column3` AS `column3`,`aview`.`second_view`.`column4` AS `column4`,`hello`('FUNC') AS `hello('FUNC')` from `aview`.`second_view` WITH LOCAL CHECK OPTION	utf8mb3	utf8mb3_general_ci
SELECT * FROM aview.mixed_view2;
column3	column4	hello('FUNC')
EFGH	100	Hello, FUNC!
# View with different definer
SHOW CREATE VIEW aview.view_user;
View	Create View	character_set_client	collation_connection
view_user	CREATE ALGORITHM=UNDEFINED DEFINER=`jeffrey`@`localhost` SQL SECURITY DEFINER VIEW `aview`.`view_user` AS select 1 AS `1`	utf8mb3	utf8mb3_general_ci
SHOW CREATE VIEW aview.view_user2;
View	Create View	character_set_client	collation_connection
view_user2	CREATE ALGORITHM=UNDEFINED DEFINER=`abcd`@`localhost` SQL SECURITY INVOKER VIEW `aview`.`view_user2` AS select 1 AS `1`	utf8mb3	utf8mb3_general_ci
# View with different character_set and connection collation
SHOW CREATE VIEW aview.view_character_set;
View	Create View	character_set_client	collation_connection
view_character_set	CREATE ALGORITHM=UNDEFINED DEFINER=`jeffrey`@`localhost` SQL SECURITY DEFINER VIEW `aview`.`view_character_set` AS select 1 AS `1`	latin1	latin1_swedish_ci
# Check for General tablespace
SHOW CREATE TABLE tablespace.t2;
Table	Create Table
t2	CREATE TABLE `t2` (
  `a` int DEFAULT NULL,
  `b` varchar(50) DEFAULT NULL
) /*!50100 TABLESPACE `tbsp1` */ ENGINE=InnoDB DEFAULT CHARSET=latin1
SHOW CREATE TABLE tablespace.t3;
Table	Create Table
t3	CREATE TABLE `t3` (
  `a` int NOT NULL,
  `b` varchar(50) DEFAULT NULL,
  `c` varchar(50) DEFAULT NULL,
  PRIMARY KEY (`a`)
) /*!50100 TABLESPACE `tbsp1` */ ENGINE=InnoDB DEFAULT CHARSET=latin1
SELECT * FROM tablespace.t3;
a	b	c
11	he	ra
SHOW CREATE TABLE tablespace.t4;
Table	Create Table
t4	CREATE TABLE `t4` (
  `a` int DEFAULT NULL
) /*!50100 TABLESPACE `tbsp2` */ ENGINE=InnoDB DEFAULT CHARSET=latin1
# Check for table with blackhole engine
SHOW CREATE TABLE test.t_blackhole;
Table	Create Table
t_blackhole	CREATE TABLE `t_blackhole` (
  `a` int NOT NULL,
  PRIMARY KEY (`a`)
) ENGINE=BLACKHOLE DEFAULT CHARSET=latin1
# Check for table with special character in name
SHOW CREATE TABLE `test`.`t_sc~!@#$%^&*(`;
Table	Create Table
t_sc~!@#$%^&*(	CREATE TABLE `t_sc~!@#$%^&*(` (
  `a` int NOT NULL,
  PRIMARY KEY (`a`)
) ENGINE=InnoDB DEFAULT CHARSET=latin1
# Check for JSON types
SHOW CREATE TABLE test.t_json;
Table	Create Table
t_json	CREATE TABLE `t_json` (
  `jdoc` json DEFAULT NULL
) ENGINE=InnoDB DEFAULT CHARSET=latin1
# Check for geometry data types
SHOW CREATE TABLE test.geom;
Table	Create Table
geom	CREATE TABLE `geom` (
  `g` geometry DEFAULT NULL,
  `pt` point NOT NULL,
  `ml` multipoint DEFAULT NULL,
  `multi` multilinestring NOT NULL,
  SPATIAL KEY `pt` (`pt`)
) ENGINE=InnoDB DEFAULT CHARSET=latin1
# Restart on the upgraded data dir
# restart: --loose-skip-log-bin --skip-log-replica-updates --skip-replica-preserve-commit-order --datadir=MYSQLD_DATADIR1 --log-error=MYSQLD_LOG --innodb-page-size=16k
# Execute FTS queries that modify FTS pages
select count(*) from sakila.film_text;
count(*)
1000
select * from sakila.film_text where film_id = 984;
film_id	title	description
984	WONDERFUL DROP	A Boring Panorama of a Woman And a Madman who must Overcome a Butler in A U-Boat
select * from sakila.film_text where match(title,description) against("SCISSORHANDS");
film_id	title	description
770	SCISSORHANDS SLUMS	A Awe-Inspiring Drama of a Girl And a Technical Writer who must Meet a Feminist in The Canadian Rockies
delete from sakila.film_text where film_id = 984;
SET GLOBAL innodb_optimize_fulltext_only=ON;
optimize table sakila.film_text;
Table	Op	Msg_type	Msg_text
sakila.film_text	optimize	status	OK
# Restart the server with default options.
# restart

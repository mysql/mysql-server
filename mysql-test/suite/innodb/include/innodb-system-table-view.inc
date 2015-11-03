# This is the test for Information Schema System Table View
# that displays the InnoDB system table content through
# information schema tables.

--source include/have_innodb.inc
--source include/have_innodb_zip.inc
--source include/have_no_undo_tablespaces.inc

--disable_query_log
SET default_storage_engine=InnoDB;
LET $MYSQLD_DATADIR = `select @@datadir`;
LET $INNODB_PAGE_SIZE = `select @@innodb_page_size`;
--enable_query_log

# The IDs of mysql.innodb_table_stats and mysql.innodb_index_stats are
# unpredictable, probably they depend on whether mtr has created the
# database for this test from scratch or is using a previously created
# database where those tables have been dropped and recreated. If we can
# force mtr to use a freshly created database for this test then the following
# complications can be removed and the test be reverted to the version
# it was before the patch that adds this comment.
--let $table_stats_id = `SELECT table_id FROM information_schema.innodb_sys_tables WHERE name = 'mysql/innodb_table_stats'`
--let $index_stats_id = `SELECT table_id FROM information_schema.innodb_sys_tables WHERE name = 'mysql/innodb_index_stats'`
--let $rep_table_1 = `SELECT table_id FROM information_schema.innodb_sys_tables WHERE name = 'mysql/slave_master_info'`
--let $rep_table_2 = `SELECT table_id FROM information_schema.innodb_sys_tables WHERE name = 'mysql/slave_relay_log_info'`
--let $rep_table_3 = `SELECT table_id FROM information_schema.innodb_sys_tables WHERE name = 'mysql/slave_worker_info'`
# The sys_config table may not exist on embedded tests
--let $sys_config = `SELECT IFNULL((SELECT table_id FROM information_schema.innodb_sys_tables WHERE name = 'sys/sys_config'), 0)`

--disable_query_log
--eval SELECT * FROM INFORMATION_SCHEMA.INNODB_SYS_TABLES WHERE table_id NOT IN ($table_stats_id, $index_stats_id, $rep_table_1, $rep_table_2, $rep_table_3, $sys_config) ORDER BY table_id;

--eval SELECT table_id,pos,mtype,prtype,len,name FROM INFORMATION_SCHEMA.INNODB_SYS_COLUMNS WHERE table_id NOT IN ($table_stats_id, $index_stats_id, $rep_table_1, $rep_table_2, $rep_table_3, $sys_config) ORDER BY table_id, pos;

# The SELECT * version of the query below has been moved to innodb_4k,
# innodb_8k & innodb_16k since the root page number changes with page size.
--eval SELECT index_id,table_id,type,n_fields,space,name FROM INFORMATION_SCHEMA.INNODB_SYS_INDEXES WHERE table_id NOT IN ($table_stats_id, $index_stats_id, $rep_table_1, $rep_table_2, $rep_table_3, $sys_config) ORDER BY index_id;
--enable_query_log

SELECT index_id,pos,name FROM INFORMATION_SCHEMA.INNODB_SYS_FIELDS
       WHERE name NOT IN ('database_name', 'table_name', 'index_name', 'stat_name', 'id', 'host', 'port', 'variable')
       ORDER BY index_id, pos;

--sorted_result
SELECT * FROM INFORMATION_SCHEMA.INNODB_SYS_FOREIGN;

--sorted_result
SELECT * FROM INFORMATION_SCHEMA.INNODB_SYS_FOREIGN_COLS;

--disable_warnings
DROP TABLE IF EXISTS t_redundant, t_compact, t_compressed, t_dynamic;
--enable_warnings

CREATE TABLE t_redundant (a INT KEY, b TEXT) ROW_FORMAT=REDUNDANT ENGINE=innodb;
CREATE TABLE t_compact (a INT KEY, b TEXT) ROW_FORMAT=COMPACT ENGINE=innodb;
CREATE TABLE t_compressed (a INT KEY, b TEXT) ROW_FORMAT=COMPRESSED ENGINE=innodb KEY_BLOCK_SIZE=2;
CREATE TABLE t_dynamic (a INT KEY, b TEXT) ROW_FORMAT=DYNAMIC ENGINE=innodb;

--source suite/innodb/include/show_i_s_tables.inc
--source suite/innodb/include/show_i_s_tablespaces.inc

DROP TABLE t_redundant, t_compact, t_compressed, t_dynamic;

SELECT count(*) FROM INFORMATION_SCHEMA.INNODB_SYS_TABLESTATS
 WHERE name NOT LIKE 'sys/%';

# Create a foreign key constraint, and verify the information
# in INFORMATION_SCHEMA.INNODB_SYS_FOREIGN and
# INFORMATION_SCHEMA.INNODB_SYS_FOREIGN_COLS
CREATE TABLE parent (id INT NOT NULL,
                     PRIMARY KEY (id)) ENGINE=INNODB;

CREATE TABLE child (id INT, parent_id INT,
                    INDEX par_ind (parent_id),
		    CONSTRAINT constraint_test
                    FOREIGN KEY (parent_id) REFERENCES parent(id)
                      ON DELETE CASCADE) ENGINE=INNODB;

--sorted_result
SELECT * FROM INFORMATION_SCHEMA.INNODB_SYS_FOREIGN;

--sorted_result
SELECT * FROM INFORMATION_SCHEMA.INNODB_SYS_FOREIGN_COLS;

# Insert a row in the table "parent", and see whether that reflected in
# INNODB_SYS_TABLESTATS
INSERT INTO parent VALUES(1);

--sorted_result
SELECT name, num_rows, ref_count
FROM INFORMATION_SCHEMA.INNODB_SYS_TABLESTATS
WHERE name LIKE "%parent";

--sorted_result
SELECT NAME, FLAG, N_COLS FROM INFORMATION_SCHEMA.INNODB_SYS_TABLES
 WHERE name NOT LIKE 'sys/%';

--sorted_result
SELECT name, n_fields
from INFORMATION_SCHEMA.INNODB_SYS_INDEXES
WHERE table_id In (SELECT table_id from
	INFORMATION_SCHEMA.INNODB_SYS_TABLES
	WHERE name LIKE "%parent%");

--sorted_result
SELECT name, n_fields
from INFORMATION_SCHEMA.INNODB_SYS_INDEXES
WHERE table_id In (SELECT table_id from
	INFORMATION_SCHEMA.INNODB_SYS_TABLES
	WHERE name LIKE "%child%");

--sorted_result
SELECT name, pos, mtype, len
from INFORMATION_SCHEMA.INNODB_SYS_COLUMNS
WHERE table_id In (SELECT table_id from
	INFORMATION_SCHEMA.INNODB_SYS_TABLES
	WHERE name LIKE "%child%");

DROP TABLE child;

DROP TABLE parent;

# Create table with 2 columns in the foreign key constraint
CREATE TABLE parent (id INT NOT NULL, newid INT NOT NULL,
                     PRIMARY KEY (id, newid)) ENGINE=INNODB;

CREATE TABLE child (id INT, parent_id INT,
                    INDEX par_ind (parent_id),
		    CONSTRAINT constraint_test
                    FOREIGN KEY (id, parent_id) REFERENCES parent(id, newid)
                      ON DELETE CASCADE) ENGINE=INNODB;

--sorted_result
SELECT * FROM INFORMATION_SCHEMA.INNODB_SYS_FOREIGN;

--sorted_result
SELECT * FROM INFORMATION_SCHEMA.INNODB_SYS_FOREIGN_COLS;

INSERT INTO parent VALUES(1, 9);

# Nested query will open the table handle twice
--sorted_result
SELECT * FROM parent WHERE id IN (SELECT id FROM parent);

--sorted_result
SELECT name, num_rows, ref_count
FROM INFORMATION_SCHEMA.INNODB_SYS_TABLESTATS
WHERE name LIKE "%parent";

DROP TABLE child;

DROP TABLE parent;

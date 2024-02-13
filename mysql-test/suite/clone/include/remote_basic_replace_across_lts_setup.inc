--let $HOST = 127.0.0.1
--let $PORT =`select @@port`
--let $USER = root

--let clone_remote_replace = 1
--let clone_inst_number = 2

# Windows does not have mysqld_safe. Skip instance monitoring if windows
let $have_windows = `SELECT CONVERT(@@version_compile_os using latin1) IN ("Win32", "Win64", "Windows")`;

if (!$have_windows) {
  # After cloning is complete, the server is shutdown. At this point, the
  # monitoring instance (mysqld_safe in most cases) will automatically restart
  # the server. See clone_connection_begin.inc where the server is started under
  # mysqld_safe and state is reset in clone_connection_end.inc
  --source include/have_mysqld_safe.inc
  --let inst_monitor = 1
}

--source ../include/clone_connection_begin.inc
--source include/count_sessions.inc
--let $CLONE_DATADIR = $MYSQL_TMP_DIR/data_new

--disable_query_log
call mtr.add_suppression("\\[Warning\\] .*MY-\\d+.* Non innodb table: .* is not cloned and is empty.");
call mtr.add_suppression("\\[Warning\\] .*MY-\\d+.* Clone removing all user data for provisioning: .*");
--enable_query_log

--echo # Setup the tables
CREATE TABLE t1(col1 INT PRIMARY KEY, col2 char(64), FULLTEXT KEY fts_index(col2));

INSERT INTO t1 VALUES(10, 'clone row 1');
INSERT INTO t1 VALUES(20, 'clone row 2');
INSERT INTO t1 VALUES(30, 'clone row 3');

SELECT * from t1 ORDER BY col1;

# Create MyIsam and CSV tables in common schema
CREATE TABLE t_myisam(col1 INT PRIMARY KEY, col2 char(64)) ENGINE=MyISAM;
INSERT INTO t_myisam VALUES(10, 'myisam not cloned row 1');
SELECT * from t_myisam ORDER BY col1;

CREATE TABLE t_csv(col1 INT NOT NULL, col2 char(64) NOT NULL) ENGINE=CSV;
INSERT INTO t_csv VALUES(10, 'csv not cloned row 1');
SELECT * from t_csv ORDER BY col1;

# Create MyIsam and CSV tables in separate schema
CREATE SCHEMA non_innodb;
CREATE TABLE non_innodb.t_myisam(col1 INT PRIMARY KEY, col2 char(64)) ENGINE=MyISAM;
INSERT INTO non_innodb.t_myisam VALUES(10, 'myisam not cloned row 1');
INSERT INTO non_innodb.t_myisam VALUES(20, 'myisam not cloned row 2');
INSERT INTO non_innodb.t_myisam VALUES(30, 'myisam not cloned row 3');
SELECT * from non_innodb.t_myisam ORDER BY col1;

CREATE TABLE non_innodb.t_csv(col1 INT NOT NULL, col2 char(64) NOT NULL) ENGINE=CSV;
INSERT INTO non_innodb.t_csv VALUES(10, 'csv not cloned row 1');
INSERT INTO non_innodb.t_csv VALUES(20, 'csv not cloned row 2');
INSERT INTO non_innodb.t_csv VALUES(30, 'csv not cloned row 3');
SELECT * from non_innodb.t_csv ORDER BY col1;

# Create MyIsam, CSV and InnoDB tables in separate schema, set it to read only
CREATE SCHEMA schema_read_only;
CREATE TABLE schema_read_only.t_myisam(col1 INT PRIMARY KEY, col2 char(64))
  ENGINE=MyISAM;
INSERT INTO schema_read_only.t_myisam VALUES(10, 'myisam not cloned row 1');
INSERT INTO schema_read_only.t_myisam VALUES(20, 'myisam not cloned row 2');
INSERT INTO schema_read_only.t_myisam VALUES(30, 'myisam not cloned row 3');
SELECT * from schema_read_only.t_myisam ORDER BY col1;

CREATE TABLE schema_read_only.t_csv(col1 INT NOT NULL, col2 char(64) NOT NULL)
  ENGINE=CSV;
INSERT INTO schema_read_only.t_csv VALUES(10, 'csv not cloned row 1');
INSERT INTO schema_read_only.t_csv VALUES(20, 'csv not cloned row 2');
INSERT INTO schema_read_only.t_csv VALUES(30, 'csv not cloned row 3');
SELECT * from schema_read_only.t_csv ORDER BY col1;

CREATE TABLE schema_read_only.t_innodb(col1 INT PRIMARY KEY, col2 char(64))
  ENGINE=InnoDB;
INSERT INTO schema_read_only.t_innodb VALUES(10, 'innodb cloned row 1');
INSERT INTO schema_read_only.t_innodb VALUES(20, 'innodb cloned row 2');
INSERT INTO schema_read_only.t_innodb VALUES(30, 'innodb cloned row 3');
SELECT * from schema_read_only.t_innodb ORDER BY col1;

ALTER SCHEMA schema_read_only READ ONLY=1;

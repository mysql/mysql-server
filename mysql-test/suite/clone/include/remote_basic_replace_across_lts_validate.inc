# Validate data
SELECT * from t1 ORDER BY col1;
INSERT INTO t1 VALUES(40, 'clone row 4');
SELECT * from t1 ORDER BY col1;

# Check MyISAM and CSV table data
INSERT INTO t_myisam VALUES(40, 'myisam not cloned row 4');
SELECT * from t_myisam ORDER BY col1;

INSERT INTO t_csv VALUES(40, 'csv not cloned row 4');
SELECT * from t_csv ORDER BY col1;

INSERT INTO non_innodb.t_myisam VALUES(40, 'myisam not cloned row 4');
SELECT * from non_innodb.t_myisam ORDER BY col1;

INSERT INTO non_innodb.t_csv VALUES(40, 'csv not cloned row 4');
SELECT * from non_innodb.t_csv ORDER BY col1;

# Check tables in read only schema
--error ER_SCHEMA_READ_ONLY
INSERT INTO schema_read_only.t_myisam VALUES(40, 'myisam not cloned row 4');
SELECT * from schema_read_only.t_myisam ORDER BY col1;

--error ER_SCHEMA_READ_ONLY
INSERT INTO schema_read_only.t_csv VALUES(40, 'csv not cloned row 4');
SELECT * from schema_read_only.t_csv ORDER BY col1;

--error ER_SCHEMA_READ_ONLY
INSERT INTO schema_read_only.t_innodb VALUES(40, 'innodb cloned row 4');
SELECT * from schema_read_only.t_innodb ORDER BY col1;

SHOW CREATE SCHEMA schema_read_only;

--source include/wait_until_count_sessions.inc

--connection clone_conn_1

--echo # Cleanup
if (!$clone_remote_replace) {
  --let restart_parameters="restart:"
  --source include/restart_mysqld.inc
}
--connection default

SELECT * from t1 ORDER BY col1;

DROP TABLE t1;

DROP TABLE t_myisam;
DROP TABLE t_csv;

DROP TABLE non_innodb.t_myisam;
DROP TABLE non_innodb.t_csv;

DROP SCHEMA non_innodb;

ALTER SCHEMA schema_read_only READ ONLY=0;
DROP SCHEMA schema_read_only;

if (!$clone_remote_replace) {
  --force-rmdir $CLONE_DATADIR
}

--echo # Clean recipient by cloning the cleaned up donor
if ($clone_remote_replace) {
  --connection clone_conn_1
  --let $clone_err = ER_SCHEMA_READ_ONLY
  --source ../include/clone_command.inc
  --let $clone_err = 0

  ALTER SCHEMA schema_read_only READ ONLY=0;
  --source ../include/clone_command.inc
}
--source ../include/clone_connection_end.inc

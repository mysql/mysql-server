## Common file for controlled DDL execution with concurrent Clone
##
## 1. ddl_op         : DDL operation - CREATE TABLE,  DROP TABLE,  ALTER TABLE ...
## 2. ddl_stmt       : DDL statement text following table name
## 3. ddl_post       : Any other follow up clause like partition
## 4. ddl_table_extn : Second table name extension - new [t1_new, t2_new ...]
## 5. dml_stmt1      : DML to be executed after DDL during stage-1 FILE COPY
## 6. dml_stmt11     : DML to be executed after DDL before sending DDL metadata in FILE COPY
## 7. dml_stmt2      : DML to be executed after DDL during stage-1 PAGE COPY
## 8. dml_stmt21     : DML to be executed after DDL before sending DDL metadata in PAGE COPY
## 9. ddl_skip_check : Skip checking data. Used tables are dropped
##10. ddl_encryption : restart server with keyring plugin
--source include/big_test.inc
--source include/not_parallel.inc
--source include/have_component_keyring_file.inc

--source include/count_sessions.inc

## Get rid of the binary logs from previous run.
RESET BINARY LOGS AND GTIDS;

--let $CLONE_DATADIR = $MYSQL_TMP_DIR/data_new
--let $MYSQLD_DATADIR= `select @@datadir;`

if ($remote_clone) {
  SET GLOBAL DEBUG = '+d,remote_release_clone_file_pin';
}

if (!$remote_clone) {
  SET GLOBAL DEBUG = '+d,local_release_clone_file_pin';
}

if ($ddl_encryption) {
  SET GLOBAL DEBUG = '+d,log_redo_with_invalid_master_key';
}

if ($ddl_redo_encrypt) {
  --let $skip_ddl = 1
}

--let $ddl_exec = 1
--let $table_num = 0

if (!$skip_ddl) {
  --source ../include/clone_exec_ddl.inc
}

if ($post_dml) {
  --let $after_dml = _after_dml
  --source ../include/clone_exec_ddl.inc
  --let $after_dml =
}

SET DEBUG_SYNC = 'clone_before_init_meta SIGNAL start_ddl_file_init WAIT_FOR resume_file_init';
SET DEBUG_SYNC = 'clone_file_copy SIGNAL start_ddl_file WAIT_FOR resume_file';
SET DEBUG_SYNC = 'clone_file_copy_end_before_ack SIGNAL start_ddl_file_ack WAIT_FOR resume_file_ack';
SET DEBUG_SYNC = 'clone_before_file_ddl_meta SIGNAL start_ddl_file_meta WAIT_FOR resume_file_meta';
SET DEBUG_SYNC = 'clone_page_copy SIGNAL start_ddl_page WAIT_FOR resume_page';
SET DEBUG_SYNC = 'clone_before_page_ddl_meta SIGNAL start_ddl_page_meta WAIT_FOR resume_page_meta';
SET DEBUG_SYNC = 'clone_redo_copy SIGNAL start_ddl_redo WAIT_FOR resume_redo';
--source ../include/clone_command_send.inc

if ($ddl_file_copy) {
  --let $skip_ddl =
}

connect (con1,localhost,root,,);

--echo # In connection CON1

if (!$remote_clone) {
  --echo # Waiting for clone to reach 'Initial metadata transfer'
  SET DEBUG_SYNC = 'now WAIT_FOR start_ddl_file_init';
  --echo # Wait finished
}

--let $table_num = 01

if (!$skip_ddl) {
  --source ../include/clone_exec_ddl.inc
}

if ($post_dml) {
  --let $after_dml = _after_dml
  --source ../include/clone_exec_ddl.inc
  --let $after_dml =
}

SET DEBUG_SYNC = 'now SIGNAL resume_file_init';

--echo # Waiting for clone to reach 'file copy'
SET DEBUG_SYNC = 'now WAIT_FOR start_ddl_file';
--echo # Wait finished

--let $table_num = 1

if($remote_clone) {
  --echo # For remote clone, let the donor progress concurrently
  --echo # otherwise file pins may not be released causing deadlock
  SET DEBUG_SYNC = 'clone_notify_ddl SIGNAL resume_file';
}

if (!$skip_ddl) {
  --source ../include/clone_exec_ddl.inc
}

--eval $dml_stmt1

--echo # Flush all dirty pages to track
SET GLOBAL innodb_buf_flush_list_now = 1;

if ($post_dml) {
  --let $after_dml = _after_dml
  --source ../include/clone_exec_ddl.inc
  --let $after_dml =
}

SET DEBUG_SYNC = 'now SIGNAL resume_file';

# For remote clone, DDL is executed concurrently. This is to
# make sure that clone doesn't enter into page copy state
SET DEBUG_SYNC = 'now WAIT_FOR start_ddl_file_ack';
SET DEBUG_SYNC = 'now SIGNAL resume_file_ack';

if (!$remote_clone) {
  --echo # Waiting for clone to reach DDL metadata for 'file copy'
  SET DEBUG_SYNC = 'now WAIT_FOR start_ddl_file_meta';
  --echo # Wait finished
}

if(!$in_place_ddl) {
  --let $table_num = 11

  if (!$skip_ddl) {
    --source ../include/clone_exec_ddl.inc
  }

  --eval $dml_stmt11

  if ($post_dml) {
    --let $after_dml = _after_dml
    --source ../include/clone_exec_ddl.inc
    --let $after_dml =
  }
}

SET DEBUG_SYNC = 'now SIGNAL resume_file_meta';

--echo # Waiting for clone to reach 'page copy'
SET DEBUG_SYNC = 'now WAIT_FOR start_ddl_page';
--echo # Wait finished

if ($ddl_page_copy) {
  --let $skip_ddl =
}

# In place DDL currently waits for page copy to complete. Allow
# clone to proceed concurrently.

if($in_place_ddl) {
  SET DEBUG_SYNC = 'clone_notify_ddl SIGNAL resume_page';
}

--let $table_num = 2

if (!$skip_ddl) {
  --source ../include/clone_exec_ddl.inc
}

--eval $dml_stmt2

if ($post_dml) {
  --let $after_dml = _after_dml
  --source ../include/clone_exec_ddl.inc
  --let $after_dml =
}

SET DEBUG_SYNC = 'now SIGNAL resume_page';

if (!$remote_clone) {
  --echo # Waiting for clone to reach DDL metadata for 'page copy'
  SET DEBUG_SYNC = 'now WAIT_FOR start_ddl_page_meta';
  --echo # Wait finished
}

if ($remote_clone) {
  --echo # Waiting for clone to reach 'redo copy'
  SET DEBUG_SYNC = 'now WAIT_FOR start_ddl_redo';
  --echo # Wait finished
}

--let $table_num = 21

if (!$skip_ddl) {
  --source ../include/clone_exec_ddl.inc
}

--eval $dml_stmt21

if ($post_dml) {
  --let $after_dml = _after_dml
  --source ../include/clone_exec_ddl.inc
  --let $after_dml =
}

if (!$remote_clone) {
  SET DEBUG_SYNC = 'now SIGNAL resume_page_meta';

  --echo # Waiting for clone to reach 'redo copy'
  SET DEBUG_SYNC = 'now WAIT_FOR start_ddl_redo';
  --echo # Wait finished
}

--let $table_num = 3

if (!$skip_ddl) {
  --source ../include/clone_exec_ddl.inc
}

if ($post_dml) {
  --let $after_dml = _after_dml
  --source ../include/clone_exec_ddl.inc
  --let $after_dml =
}

SET DEBUG_SYNC = 'now SIGNAL resume_redo';

connection default;
--echo # In connection DEFAULT
--echo # Waiting for clone to complete
--reap
--echo # Wait finished

if ($remote_clone) {
  SET GLOBAL DEBUG = '-d,remote_release_clone_file_pin';
}

if (!$remote_clone) {
  SET GLOBAL DEBUG = '-d,local_release_clone_file_pin';
}

--disconnect con1
--source include/wait_until_count_sessions.inc

--echo # Restart server on cloned data directory
if ($ddl_encryption) {
  --let keyring_status=`SELECT STATUS_VALUE FROM performance_schema.keyring_component_status WHERE STATUS_KEY = "Component_status"`
  if (!$keyring_status){
    --source suite/component_keyring_file/inc/setup_component_customized.inc
  }
  --replace_result $CLONE_DATADIR CLONE_DATADIR $MYSQL_TMP_DIR MYSQL_TMP_DIR $PLUGIN_DIR_OPT PLUGIN_DIR_OPT
  --let restart_parameters="restart: --datadir=$CLONE_DATADIR $PLUGIN_DIR_OPT"
}

if (!$ddl_encryption) {
  --replace_result $CLONE_DATADIR CLONE_DATADIR
  --let restart_parameters="restart: --datadir=$CLONE_DATADIR"
}

--source include/restart_mysqld.inc

--let $ddl_exec =

--echo # Check cloned data
SHOW TABLES;

if (!$ddl_skip_check) {
  --let $ddl_show = 1

  --let $table_num = 1
  --source ../include/clone_exec_ddl.inc

  --let $table_num = 11
  --source ../include/clone_exec_ddl.inc

  --let $table_num = 2
  --source ../include/clone_exec_ddl.inc

  --let $ddl_show =

  if ($dml_stmt_check) {
    --eval $dml_stmt_check
  }
}

--echo # Restart server back on base data directory

if ($ddl_encryption) {
  --replace_result $MYSQL_TMP_DIR MYSQL_TMP_DIR $ENCRYPT_DATADIR ENCRYPT_DATADIR $PLUGIN_DIR_OPT PLUGIN_DIR_OPT
  --let restart_parameters=restart: --datadir=$ENCRYPT_DATADIR $PLUGIN_DIR_OPT
}

if (!$ddl_encryption) {

  if (!$skip_space_validation) {
    --let restart_parameters=restart:
  }

  if ($skip_space_validation) {
    --let restart_parameters=restart: --skip-innodb-validate-tablespace-paths
  }
}

--source include/restart_mysqld.inc

if ($undo_encryption) {
  SET GLOBAL innodb_undo_log_encrypt = ON;
  SHOW VARIABLES LIKE 'innodb_undo_log_encrypt';
}

--force-rmdir $CLONE_DATADIR
SET DEBUG_SYNC = 'RESET';

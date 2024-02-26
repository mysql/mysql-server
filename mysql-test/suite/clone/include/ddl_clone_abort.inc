# Test DDL command aborting clone on failure

--let $CLONE_DATADIR = $MYSQL_TMP_DIR/data_new
--let $MYSQLD_DATADIR= `select @@datadir;`

SET GLOBAL DEBUG = '+d,clone_ddl_abort_wait_page_copy';
SET DEBUG_SYNC = 'clone_page_copy SIGNAL start_ddl WAIT_FOR resume_clone';
--source ../include/clone_command_send.inc

--connection con1
--echo # In connection CON1
SET DEBUG_SYNC = 'now WAIT_FOR start_ddl';
SET DEBUG_SYNC = 'clone_notify_ddl_after_state_block SIGNAL resume_clone';

if ($ddl_error) {
  --error ER_INTERNAL_ERROR
}
--eval $ddl_alter_abort

--connection default
--echo # In connection DEFAULT
if (!$ddl_error) {
  --error ER_CLONE_DDL_IN_PROGRESS
}
--reap

SET GLOBAL DEBUG = '-d,clone_ddl_abort_wait_page_copy';

--force-rmdir $CLONE_DATADIR
SET DEBUG_SYNC = 'RESET';

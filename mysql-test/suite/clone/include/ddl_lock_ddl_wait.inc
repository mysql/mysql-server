## Clone DDL lock DDL timeout

SET DEBUG_SYNC = 'clone_file_copy SIGNAL start_ddl WAIT_FOR resume_clone';
--source ../include/clone_command_send.inc

connection con1;
--echo # In connection CON1
SET DEBUG_SYNC = 'now WAIT_FOR start_ddl';
--error ER_LOCK_WAIT_TIMEOUT
--eval $ddl_text
SET DEBUG_SYNC = 'now SIGNAL resume_clone';

connection default;
--echo # In connection DEFAULT
--reap
--force-rmdir $CLONE_DATADIR
SET DEBUG_SYNC = 'RESET';

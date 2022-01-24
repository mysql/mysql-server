## Clone DDL lock timeout

--connection con1
--echo # In connection CON1

SET DEBUG_SYNC = 'clone_notify_ddl SIGNAL resume_clone WAIT_FOR resume_ddl';
--send_eval $ddl_text

--connection default
--echo # In connection DEFAULT
SET DEBUG_SYNC = 'now WAIT_FOR resume_clone';

--let $clone_err = ER_LOCK_WAIT_TIMEOUT
--let $clone_remote_err = ER_CLONE_DONOR
--source ../include/clone_command.inc
--let $clone_err = 0
--let $clone_remote_err = 0

SET DEBUG_SYNC = 'now SIGNAL resume_ddl';

--connection con1
--echo # In connection CON1
--reap

--connection default
--echo # In connection DEFAULT

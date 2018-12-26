# Force a change buffer merge, so that --innodb-read-only will not
# refuse startup, in case there exist buffered changes from earlier tests.
SET GLOBAL innodb_fast_shutdown = 0;

--disable_query_log
CALL mtr.add_suppression("Skip re-populating collations and character sets tables in InnoDB read-only mode.");
CALL mtr.add_suppression("Skip updating information_schema metadata in InnoDB read-only mode.");
CALL mtr.add_suppression("Skipped updating resource group metadata in InnoDB read only mode.");
--enable_query_log

let $restart_parameters = restart: --innodb-read-only;
--source include/restart_mysqld.inc
let $restart_parameters=;

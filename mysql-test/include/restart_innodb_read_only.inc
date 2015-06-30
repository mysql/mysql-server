# Force a change buffer merge, so that --innodb-read-only will not
# refuse startup, in case there exist buffered changes from earlier tests.
SET GLOBAL innodb_fast_shutdown = 0;

let $restart_parameters = restart: --innodb-read-only;
--source include/restart_mysqld.inc
let $restart_parameters=;

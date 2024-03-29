# Force a change buffer merge, so that --innodb-read-only will not
# refuse startup, in case there exist buffered changes from earlier tests.
SET GLOBAL innodb_fast_shutdown = 0;

--source include/add_mtr_suppression_for_innodb_readonly.inc

let $restart_parameters=restart: --innodb-read-only;
if ($extra_restart_parameters) {
  let $restart_parameters=$restart_parameters $extra_restart_parameters;
  # Clear for future usages:
  let $extra_restart_parameters=;
}

--source include/restart_mysqld.inc
let $restart_parameters=;

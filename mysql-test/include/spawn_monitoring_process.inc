# ==== Purpose ====
#
# Launch a mysqld monitoring process if needed
#
# ==== Usage ====
#
# --source include/spawn_monitoring_process.inc
#
# Please see include/restart_with_mysqld_safe.inc
#

--let $include_filename= spawn_monitoring_process.inc
--source include/begin_include_file.inc

let $have_windows = `SELECT CONVERT(@@version_compile_os using latin1) IN ("Win32", "Win64", "Windows")`;

if (!$have_windows) {
  --source include/restart_with_mysqld_safe.inc
}

--let $include_filename= spawn_monitoring_process.inc
--source include/end_include_file.inc

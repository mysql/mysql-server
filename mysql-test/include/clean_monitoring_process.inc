# ==== Purpose ====
#
# Clean monitoring process related tasks if needed
#
# ==== Usage ====
#
# --source include/clean_monitoring_process.inc
#
# Please see include/restart_with_mysqld_safe_cleanup.inc
#

--let $include_filename= clean_monitoring_process.inc
--source include/begin_include_file.inc

let $have_windows = `SELECT CONVERT(@@version_compile_os using latin1) IN ("Win32", "Win64", "Windows")`;

if (!$have_windows) {
  --source include/restart_with_mysqld_safe_cleanup.inc
}

--let $include_filename= clean_monitoring_process.inc
--source include/end_include_file.inc

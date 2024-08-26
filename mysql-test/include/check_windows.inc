# ==== Purpose ====
#
# Check if we are executing on Windows.
#
# ==== Usage ====
#
# --source include/check_windows.inc
# # Now check if $have_windows is 0 or 1

--let $have_windows = `SELECT CONVERT(@@version_compile_os using latin1) IN ('Win32', 'Win64', 'Windows')`

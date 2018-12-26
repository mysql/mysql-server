# ==== Purpose ====
#
# Tell mtr that all servers must be restarted in case the test is
# skipped.
#
# ==== Usage ====
#
# --source include/force_restart_if_skipped.inc
#
# ==== See also ====
#
# include/force_restart.inc

--let $_force_restart_datadir= `SELECT @@datadir`
--append_file $_force_restart_datadir/mtr/force_restart_if_skipped
1
EOF

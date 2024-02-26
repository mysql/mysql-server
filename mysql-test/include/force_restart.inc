# ==== Purpose ====
#
# Tell mtr that all servers must be restarted after the test has
# finished.
#
# ==== Usage ====
#
# --source include/force_restart.inc
#
# ==== See also ====
#
# include/force_restart_if_skipped.inc

--let $_force_restart_datadir= `SELECT @@datadir`
--append_file $_force_restart_datadir/mtr/force_restart
1
EOF

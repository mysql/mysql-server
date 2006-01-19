--connect (dflt_server_con,localhost,root,,mysql,$IM_MYSQLD1_PORT,$IM_MYSQLD1_SOCK)
--connection dflt_server_con

--source include/not_windows.inc

# check that CSV engine was compiled in, as IM the test suite uses
# logs tables-specific option and the option is not present if CSV
# (and => the log tables) are not in.
# NOTE: In future we should remove this check and make the test suite
# to pass correct opyions to IM depending on the CSV presence
--source include/have_csv.inc

--connection default
--disconnect dflt_server_con

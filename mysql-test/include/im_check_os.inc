--connect (dflt_server_con,localhost,root,,mysql,$IM_MYSQLD1_PORT,$IM_MYSQLD1_SOCK)
--connection dflt_server_con

--source include/not_windows.inc

--connection default
--disconnect dflt_server_con

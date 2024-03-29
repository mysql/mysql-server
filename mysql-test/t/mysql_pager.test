--source include/not_windows.inc

--echo Bug #54466 client 5.5 built from source lacks "pager" support
--exec $MYSQL --pager test -e "select 1 as a"

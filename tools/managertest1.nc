def_exec server /usr/sbin/mysqld --socket=/tmp/temp.sock --skip-grant --skip-net --datadir=/tmp
set_exec_con server root localhost /tmp/temp.sock
set_exec_stdout server /tmp/mysqld.err
set_exec_stderr server /tmp/mysqld.err
start_exec server 3
show_exec
query server show variables like '%max_heap%';
stop_exec server 3
def_exec server /usr/sbin/mysqld --socket=/tmp/temp.sock --skip-grant --skip-net --datadir=/tmp -O max_heap_table_size=5000
show_exec
start_exec server 3
query server show variables like '%max_heap%';
show_exec
stop_exec server 3
show_exec
quit

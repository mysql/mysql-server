root secret
def_exec server /usr/sbin/mysqld --socket=/tmp/temp.sock --skip-grant --skip-net --datadir=/tmp
set_exec_con server root localhost /tmp/temp.sock
start_exec server 3
show_exec
stop_exec server 3
show_exec
start_exec server 3
show_exec
stop_exec server 3
show_exec
shutdown

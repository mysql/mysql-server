# Example mysql config file.
# You can copy this to one of:
# @sysconfdir@/my.cnf to set global options,
# mysql-data-dir/my.cnf to set server-specific options (in this
# installation this directory is @localstatedir@) or
# ~/.my.cnf to set user-specific options.
# 
# One can use all long options that the program supports.
# Run the program with --help to get a list of available options

# This will be passed to all mysql clients
[client]
#password	= my_password
port		= @MYSQL_TCP_PORT@
socket		= @MYSQL_UNIX_ADDR@

# Here is entries for some specific programs
# The following values assume you have at least 32M ram

# The MySQL server
[mysqld]
port		= @MYSQL_TCP_PORT@
socket		= @MYSQL_UNIX_ADDR@
skip-locking
set-variable	= key_buffer=16M
set-variable	= max_allowed_packet=1M
set-variable	= thread_stack=128K
# Start logging
log

[mysqldump]
quick
set-variable	= max_allowed_packet=16M

[mysql]
no-auto-rehash

[isamchk]
set-variable	= key_buffer=16M

# Example mysql config file for small systems.
#
# This is for a system with little memory (<= 64M) where MySQL is only used
# from time to time and it's important that the mysqld deamon
# doesn't use much resources.
#
# You can copy this file to
# /etc/mf.cnf to set global options,
# mysql-data-dir/my.cnf to set server-specific options (in this
# installation this directory is @localstatedir@) or
# ~/.my.cnf to set user-specific options.
#
# One can in this file use all long options that the program supports.
# If you want to know which options a program support, run the program
# with --help option.

# The following options will be passed to all MySQL clients
[client]
#password	= your_password
port		= @MYSQL_TCP_PORT@
socket		= @MYSQL_UNIX_ADDR@

# Here follows entries for some specific programs

# The MySQL server
[mysqld]
port		= @MYSQL_TCP_PORT@
socket		= @MYSQL_UNIX_ADDR@
skip-locking
set-variable	= key_buffer=16K
set-variable	= max_allowed_packet=1M
set-variable	= thread_stack=64K
set-variable	= table_cache=4
set-variable	= sort_buffer=64K
set-variable	= net_buffer_length=2K

# Uncomment the following if you are NOT using BDB tables
#skip-bdb

# Uncomment the following if you want to log updates
#log-update

[mysqldump]
quick
set-variable	= max_allowed_packet=16M

[mysql]
no-auto-rehash
#safe-updates	# Remove the comment character if you are not familiar with SQL

[isamchk]
set-variable	= key_buffer=8M
set-variable	= sort_buffer=8M

[myisamchk]
set-variable	= key_buffer=8M
set-variable	= sort_buffer=8M

[mysqlhotcopy]
interactive-timeout

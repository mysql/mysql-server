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
server-id	= 1

# Uncomment the following if you want to log updates
#log-bin

# Uncomment the following if you are NOT using BDB tables
#skip-bdb

# Uncomment the following if you are using Innobase tables
#innobase_data_home_dir = @localstatedir@/
#innobase_log_group_home_dir = @localstatedir@/
#innobase_log_arch_dir = @localstatedir@/
#innobase_data_file_path = ibdata1:25M;ibdata2:37M;ibdata3:100M;ibdata4:300M
#set-variable = innobase_mirrored_log_groups=1
#set-variable = innobase_log_files_in_group=3
#set-variable = innobase_log_file_size=5M
#set-variable = innobase_log_buffer_size=8M
#innobase_flush_log_at_trx_commit=1
#innobase_log_archive=0
#set-variable = innobase_buffer_pool_size=16M
#set-variable = innobase_additional_mem_pool_size=2M
#set-variable = innobase_file_io_threads=4
#set-variable = innobase_lock_wait_timeout=50

[mysqldump]
quick
set-variable	= max_allowed_packet=16M

[mysql]
no-auto-rehash
# Remove the next comment character if you are not familiar with SQL
#safe-updates

[isamchk]
set-variable	= key_buffer=8M
set-variable	= sort_buffer=8M

[myisamchk]
set-variable	= key_buffer=8M
set-variable	= sort_buffer=8M

[mysqlhotcopy]
interactive-timeout

# Example mysql config file for medium systems.
#
# This is for a system with little memory (32M - 64M) where MySQL plays
# a important part and systems up to 128M very MySQL is used together with
# other programs (like a web server)
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
set-variable	= key_buffer=16M
set-variable	= max_allowed_packet=1M
set-variable	= table_cache=64
set-variable	= sort_buffer=512K
set-variable	= net_buffer_length=8K
set-variable	= myisam_sort_buffer_size=8M
log-bin
server-id	= 1

# Point the following paths to different dedicated disks
#tmpdir		= /tmp/		
#log-update 	= /path-to-dedicated-directory/hostname

# Uncomment the following if you are using BDB tables
#set-variable	= bdb_cache_size=4M
#set-variable	= bdb_max_lock=10000

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
set-variable	= key_buffer=20M
set-variable	= sort_buffer=20M
set-variable	= read_buffer=2M
set-variable	= write_buffer=2M

[myisamchk]
set-variable	= key_buffer=20M
set-variable	= sort_buffer=20M
set-variable	= read_buffer=2M
set-variable	= write_buffer=2M

[mysqlhotcopy]
interactive-timeout

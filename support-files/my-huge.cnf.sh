# Example mysql config file for very large systems.
#
# This is for large system with memory of 1G-2G where the system runs mainly
# MySQL.
#
# You can copy this file to
# /etc/my.cnf to set global options,
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
set-variable	= key_buffer=384M
set-variable	= max_allowed_packet=1M
set-variable	= table_cache=512
set-variable	= sort_buffer=2M
set-variable	= record_buffer=2M
set-variable	= thread_cache=8
# Try number of CPU's*2 for thread_concurrency
set-variable	= thread_concurrency=8
set-variable	= myisam_sort_buffer_size=64M

# Replication Master Server (default)
log-bin             # required for replication
server-id	= 1 # required unique id between 1 and 2^32 - 1
                    # defaults to 1 if master-host is not set
                    # but will not function as a master if omitted

# Replication Slave Server (comment out master section to use this)
#master-host     =   # MUST BE SET
#master-user     =   # MUST BE SET
#master-password =   # MUST BE SET
#master-port     =   # optional--defaults to 3306
#log-bin             # not required for slaves, but recommended
#server-id       = 2 # required unique id between 2 and 2^32 - 1
                    # (and different from the master)
                    # defaults to 2 if master-host is set
                    # but will not function as a slave if omitted

# Point the following paths to different dedicated disks
#tmpdir		= /tmp/		
#log-update 	= /path-to-dedicated-directory/hostname

# Uncomment the following if you are using BDB tables
#set-variable	= bdb_cache_size=384M
#set-variable	= bdb_max_lock=100000

# Uncomment the following if you are using InnoDB tables
#innodb_data_home_dir = @localstatedir@/
#innodb_data_file_path = ibdata1:2000M;ibdata2:10M:autoextend
#innodb_log_group_home_dir = @localstatedir@/
#innodb_log_arch_dir = @localstatedir@/
# You can set .._buffer_pool_size up to 50 - 80 %
# of RAM but beware of setting memory usage too high
#set-variable = innodb_buffer_pool_size=384M
#set-variable = innodb_additional_mem_pool_size=20M
# Set .._log_file_size to 25 % of buffer pool size
#set-variable = innodb_log_file_size=100M
#set-variable = innodb_log_buffer_size=8M
#innodb_flush_log_at_trx_commit=1
#set-variable = innodb_lock_wait_timeout=50

[mysqldump]
quick
set-variable	= max_allowed_packet=16M

[mysql]
no-auto-rehash
# Remove the next comment character if you are not familiar with SQL
#safe-updates

[isamchk]
set-variable	= key_buffer=256M
set-variable	= sort_buffer=256M
set-variable	= read_buffer=2M
set-variable	= write_buffer=2M

[myisamchk]
set-variable	= key_buffer=256M
set-variable	= sort_buffer=256M
set-variable	= read_buffer=2M
set-variable	= write_buffer=2M

[mysqlhotcopy]
interactive-timeout

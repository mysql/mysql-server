#BEGIN CONFIG INFO
#DESCR:  4G,Innodb only,ACID, Few Connections heavy queries
#TYPE: SYSTEM
#END CONFIG INFO

# This is example config file for systems with 4G of memory running mostly MySQL
# using MyISAM only tables and running complex queries with few connections
# 




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
# But note, only client programs shipped by MySQL are guarantied to read it
# If you wish your software to read this section you would need to specify
# it as an option during MySQL client library initialization
[client]
#password	= your_password
port		= @MYSQL_TCP_PORT@
socket		= @MYSQL_UNIX_ADDR@

# ********** Here follows entries for some specific programs

# The MySQL server
[mysqld]
# generic configuration options

port		= @MYSQL_TCP_PORT@
socket		= @MYSQL_UNIX_ADDR@


# Back Log is a number of connection OS can keep in queue, before MySQL
# connection manager thread has processed them. If you have very intensive
# connection rate and experience "connection refused" errors you might need
# to increase this value
back_log = 50


# Don't listen on a TCP/IP port at all. This can be a security enhancement,
# if all processes that need to connect to mysqld run on the same host.
# All interaction with mysqld must be made via Unix sockets or named pipes.
# Note that using this option without enabling named pipes on Windows
# (via the "enable-named-pipe" option) will render mysqld useless!
#skip-networking

# Maximum amount of concurrent sessions MySQL server will allow
# One of these connections will be reserved for user with SUPER privelege
# to allow administrator to login even if server is overloaded.
max_connections = 100


# Maximum amount of errors allowed per host. If this limit is reached
# host will be blocked from connection MySQL server until "flush hosts"
# is run or server restart. Invalid passwords as any other errors at 
# connect phase results in increasing this value. See 
# Aborted_Connects status variable for global counter.
max_connect_errors = 10


# Amount of tables server can keep open at the time. Each table 
# may require up to 2 file handlers (for MERGE tables even more)
# so make sure to have amount of open files allowed at least 4096
# see open-files-limit in [mysqld_safe]
table_cache = 2048

# Do not use file level locking. Enabled file locking give performance
# hit, so use it only in case you have serveral database instances
# running on the same files (note some restrictions still apply!) 
# or if you use other software relaying on locking MyISAM tables 
# on file level
#enable-locking

# This packets limits maximum size of BLOB server can handle 
# as well as maximum query size server can process
# enlarged dynamically, for each connection
max_allowed_packet = 16M

# Binary log cache is used for logging transactions to binary log
# all statements from transactions are buffered in binary log cache
# and wrote to the binary log at once on commit
# if transaction is large than this value disk temporary file is used.
# This buffer is allocated per connection on first update statement 
# in transaction
binlog_cache_size = 1M


# Maximum allowed size for single HEAP (in memory) table
# This option is protection from accidential creation of the HEAP
# table which would take all the memory resources
max_heap_table_size=64M


# Sort buffer used to perform sorts for some of ORDER BY and 
# GROUP BY queries. If sorted data does not fit into sort buffer
# Disk based merge sort is used - See sort_merge_passes.
# Allocated per thread if sort is needed
sort_buffer_size = 8M

# This buffer is used for optimization of full joins (joins without indexes)
# Such joins are very bad for performance in most cases anyway, but having
# this variable large reduces performance impact.
# see select_full_join status variable for full joins count
# Allocated per thread if full join is found
join_buffer_size=8M


# Cache threads on disconnect instead of destroying them
# thread cache allows to greatly reduce amount of thread
# creations needed if you have a lot of connections
thread_cache = 8


# Try number of CPU's*(2..4) for thread_concurrency
# This value makes sense only on few systems (as Solaris)
# which support thread_concurrency() setting
thread_concurrency = 8


# Query cache is used to cache SELECT results and later return
# them without actual query execution for exactly the same query
# Having query cache enabled may give great benefit if your have
# typical queries and rarely changed tabled
# see Qcache_lowmem_prunes status variable to check if current 
# value is enough for your load
# Note: In case your table change all the time or you never have
# textually same queries query cache maay bring slowdown
# instead of performance improvement
query_cache_size = 64M

# Cache only result sets which are smaller than this limit
# This setting is protection of very large result set overwriting
# all queries in query cache
query_cache_limit = 2M

# Minimum word length to be indexed by full text search index
# you might wish to decrease it if you need to search on shorter words
ft_min_word_len = 4

# If your system supports memlock() function you might use this option
# while running MySQL to keep it locking in memory, avoid potential
# swapping out in case of high memory pressure. Good for performance.
#memlock

# Table type which is used by default, if not specified by CREATE TABLE
# it affects only tables explicitly created by user.
default_table_type = MYISAM

# Thread stack size to use. This amount of memory is always reserved at 
# connection time. MySQL itself usually needs no more than 64K of memory,
# while if you use your own stack hungry UDF functions or OS requires more 
# stack for some operations, you might need to set it higher
thread_stack = 192K

# Set default transaction isolation level. Levels available are:
# READ-UNCOMMITED, READ-COMMITED, REPEATABLE-READ, SERIALIZABLE
transaction_isolation = REPEATABLE-READ

# Maximum size for internal in memory temporary table. If table 
# grows larger it is automatically converted to disk based table
# This limitaion is for single table. There can be many of them.
tmp_table_size = 64M

# binary logging is required for acting MASTER in replication
# You also need binary log if you need ability to do point
# in time recovery from your latest backup
log_bin

# If you're using chaining replication A->B->C you might wish to
# turn on this option on server B. It makes updates done by
# slave thread also logged in binary log. Normally they are not
#log_slave_updates


# Full query log. Every query (even with incorrect syntax) server gets goes here.
# Useful for debugging. Normally is disabled in production
#log

# If you have  any problems with MySQL server you might enable Warnings logging and 
# examine error log for possible explanations. 
#log_warnings

# Log slow queries. Slow queries are queries which take more than defined amount of time
# or which do not use indexes well, if log_long_format is enabled
# It is notmally good idea to have this on if you frequently add new queries to the system
log_slow_queries


# All queries taking more than this amount of time will be trated as slow. Do not use value 1
# here as this will result even in very fast queries logged sometimes, as MySQL measures time with
# second accuracy only.
long_query_time = 2

# Log more information in slow query log. Normally it is good to have this on.
# It results in logging of queries not using indexes additionally to long running queries.
log_long_format



# Temporary directory is used by MySQL for storing temporary files, for example
# used to do disk based large sorts, as well as for internal and explicit 
# temporary tables.
# It might be good to set it to swapfs/tmpfs filesystem if you do not have very
# large temporary files created or set it to dedicated disk
# You can specify several paths here spliting them by ";" they will be used in
# round-robin fashion
#tmpdir		= /tmp


#***  Replication related settings 


# This value is required both for master ans slave
# If you have single master it is typical to use value 1 for it
# required unique id between 1 and 2^32 - 1
# defaults to 1 if master-host is not set
# but will not function as a master if omitted
server-id = 1


# To configure this server as Replication Slave you will need
# to set its server_id to some unique value, different from Master 
# and all slaves in the group.
# You also can disable log-bin as logs are not required (while recomended)
# for slaves
# 
#
# The recomended way to set MASTER settings for the slave are:
# Use the CHANGE MASTER TO command (fully described in our manual) -
#    the syntax is:
#
#    CHANGE MASTER TO MASTER_HOST=<host>, MASTER_PORT=<port>,
#    MASTER_USER=<user>, MASTER_PASSWORD=<password> ;
#
#    where you replace <host>, <user>, <password> by quoted strings and
#    <port> by the master's port number (3306 by default).
#
#    Example:
#
#    CHANGE MASTER TO MASTER_HOST='125.564.12.1', MASTER_PORT=3306,
#    MASTER_USER='joe', MASTER_PASSWORD='secret';
#
# However if you need to replicate slave configuration over several boxes
# you can use old approach:
#
#    Set the variables below. However, in case you choose this method, then
#    start replication for the first time (even unsuccessfully, for example
#    if you mistyped the password in master-password and the slave fails to
#    connect), the slave will create a master.info file, and any later
#    change in this file to the variables' values below will be ignored and
#    overridden by the content of the master.info file, unless you shutdown
#    the slave server, delete master.info and restart the slaver server.
#    For that reason, you may want to leave the lines below untouched
#    (commented) and instead use CHANGE MASTER TO (see above)
#
#
# The replication master for this slave - required
#master-host     =   <hostname>
#
# The username the slave will use for authentication when connecting
# to the master - required
#master-user     =   <username>
#
# The password the slave will authenticate with when connecting to
# the master - required
#master-password =   <password>
#
# The port the master is listening on.
# optional - defaults to 3306
#master-port     =  <port>

# Make Slave ReadOnly.  Only user with SUPER privelege and slave 
# thread will be able to modify it. You might use it to ensure 
# no applications will accidently modify slave instead of master
#read_only



#*** MyISAM Specific options


# Size of Key Buffer, used to cache index blocks for MyISAM tables
# Do not set it larger than 30% of available memory, as some memory
# is required by OS to cache rows.
# Even if you're not using MyISAM tables still set it to 8-64M
# as it will be used for internal temporary disk tables.
key_buffer_size = 32M

# Size of buffer used for doing full table scans for MyISAM tables
# allocated per thread, as full scan is needed
read_buffer_size = 2M

# Buffer is used for caching the rows while doing Sorts 
# Allocated per thread, then needed
read_rnd_buffer_size = 16M

# The bulk insert tree is used for optimization of index modification
# for bulk inserts (hundreds+ values) and LOAD DATA INFILE
# Do not set larger than key_buffer_size  for optimal performance
# This buffer is allocated than bulk insert is detected
bulk_insert_buffer_size = 64M


# This buffer is allocated than MySQL needs to rebuild the Index,
# in REPAIR, OPTIMZE, ALTER table statements as well as in 
# LOAD DATA INFILE to empty table
# it is allocated per thread so be careful with large settings.
myisam_sort_buffer_size = 128M

# Maximum size of temporary (sort) file index rebuild  can use.
# If sort is estimated to take larger amount of space, mush slower
# (keycache) index rebuild method will be used 
myisam_max_sort_file_size = 10G

# Use sort method in case the difference between sort file and 
# Table index file is estimated to be less than this value
myisam_max_extra_sort_file_size = 10G

# If table has more than one index MyISAM can use more than one thread
# to repair them in parallel. It makes sense if you have multiple of 
# CPUs and planty of memory.
myisam_repair_threads = 1

# Automatically check and repair not properly closed MyISAM tables
myisam_recover



#*** BDB Specific options 

# Use this option if you have BDB tables enabled but you do not plan to use them
skip-bdb


#*** INNODB Specific options

# Use this option if you have INNODB tables enabled but you do not plan to use them
#skip-innodb

# Additional memory pool is used by Innodb to store metadata information. 
# If Innodb needs more memory for this purpose to allocate it from OS
# As it is fast enough on most recent OS you normally do not need to set it higher
# SHOW INNODB STATUS will show current amount of it in use
innodb_additional_mem_pool_size = 16M

# Innodb, unlike MyISAM uses bufferpool to cache both indexes and row data
# so you would normally wish to have it large up to 50-70% of your memory size
# Note on 32bit systems you might be limited to 2-3.5G of user level memory 
# per process so do not set it too high.
innodb_buffer_pool_size = 2G

# Innodb stores data in one or several files forming tablespace. If you have 
# single logical drive for your data, single autoextending file would be good enough
# In other case single file per device is often good choice. 
# You may setup Innodb to use Raw disk partitions as well. Refer to the manual.
innodb_data_file_path = ibdata1:10M:autoextend


# Set this option if you would like Innodb tablespace files to be stored in other 
# location. Default is MySQL datadir.
#innodb_data_home_dir

# Number of IO threads to use for async IO operations.  This value is hardcoded to
# 4 on Unix 
innodb_file_io_threads = 4


# If you run into Innodb tablespace corruption, setting this to nonzero value will 
# likely help you to dump your tables. Start from value 1 and increase it until
# you're able to dump the table successfully.
#innodb_force_recovery=1

# Number of threads allowed inside of Innodb kernel. Best setting highly depends
# on the application, hardware as well as OS scheduler properties
# Too high value may lead to thread thrashing
innodb_thread_concurrency = 16


# If set to 1 Innodb will flush(fsync) logs to the disk at each transaction commit
# which offers full ACID behavior, however if you can afford few last commited transaction
# lost you can set this value to 2 or 0. Innodb will anyway flush the log file once 
# per second.  0 - do not flush file at all.  2 - flush it to OS buffers but not to the disk.
innodb_flush_log_at_trx_commit = 1


# Innodb uses fast shutdown by default. However you can disable it to make Innodb to do
# purge and Insert buffer merge on shutdown. It may increase shutdown time a lot but
# Innodb will have not need to do it after next startup
#innodb_fast_shutdown

# Buffer Innodb shall use for buffering log data. As soon as it is full Innodb
# will have to flush it. As it is flushed once per second anyway even with
# long transactions it does not make sense to have it very large.
innodb_log_buffer_size = 8M

# Size of log file in group. You shall set combined size of log files large 25%-100% of
# your buffer pool size to avoid not needed buffer pool flush activity on log file
# overwrite.  Note however larger logfile size will increase time needed for recovery
# process.
innodb_log_file_size = 256M

# Total number of files in the log group. Value 2-3 is usually good enough.
innodb_log_files_in_group = 3

# Location for Innodb log files. Default is MySQL datadir.  You may wish to
# point it to dedicated hard drive or RAID1 volume for improved performance
#innodb_log_group_home_dir

# Maximum allowed Percentage of dirty pages in Innodb buffer pool.
# If it is reached Innodb will start flushing them agressively not to run 
# out of clean pages at all. This is a soft limit, not guarantied to be held.
innodb_max_dirty_pages_pct = 90


# Set flush method Innodb will use for Log.  Tablespace always uses doublewrite flush logic.
#innodb_flush_method

# How long Innodb transaction shall wait for lock to be granted before giving up.
# This value does not correspond to deadlock resolution. Innodb will detect Deadlock
# as soon as it is formed.
innodb_lock_wait_timeout = 120



[mysqldump]
# Do not buffer whole result set in memory before writing it to file
# required for dumping very large tables
quick

max_allowed_packet = 16M

[mysql]
no-auto-rehash

# Remove the next comment character if you are not familiar with SQL
#safe-updates

[isamchk]
key_buffer = 512M
sort_buffer_size = 512M
read_buffer = 8M
write_buffer = 8M

[myisamchk]
key_buffer = 512M
sort_buffer_size = 512M
read_buffer = 8M
write_buffer = 8M

[mysqlhotcopy]
interactive-timeout

[mysqld_safe]
# Increase amount of open files allowed per process
# Warning: Make sure you have global system limit high enough
# The high value is required for large number of opened tables
open-files-limit = 8192

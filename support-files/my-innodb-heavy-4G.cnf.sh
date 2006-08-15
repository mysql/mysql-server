#BEGIN CONFIG INFO
#DESCR: 4GB RAM, InnoDB only, ACID, few connections, heavy queries
#TYPE: SYSTEM
#END CONFIG INFO

#
# This is a MySQL example config file for systems with 4GB of memory
# running mostly MySQL using InnoDB only tables and performing complex
# queries with few connections.
# 
# You can copy this file to /etc/my.cnf to set global options,
# mysql-data-dir/my.cnf to set server-specific options 
# (@localstatedir@ for this installation) or to
# ~/.my.cnf to set user-specific options.
#
# In this file, you can use all long options that a program supports.
# If you want to know which options a program supports, run the program
# with the "--help" option.
#
# More detailed information about the individual options can also be
# found in the manual.
#

#
# The following options will be read by MySQL client applications.
# Note that only client applications shipped by MySQL are guaranteed
# to read this section. If you want your own MySQL client program to
# honor these values, you need to specify it as an option during the
# MySQL client library initialization.
#
[client]
#password	= [your_password]
port		= @MYSQL_TCP_PORT@
socket		= @MYSQL_UNIX_ADDR@

# *** Application-specific options follow here ***

#
# The MySQL server
#
[mysqld]

# generic configuration options
port		= @MYSQL_TCP_PORT@
socket		= @MYSQL_UNIX_ADDR@

# back_log is the number of connections the operating system can keep in
# the listen queue, before the MySQL connection manager thread has
# processed them. If you have a very high connection rate and experience
# "connection refused" errors, you might need to increase this value.
# Check your OS documentation for the maximum value of this parameter.
# Attempting to set back_log higher than your operating system limit
# will have no effect.
back_log = 50

# Don't listen on a TCP/IP port at all. This can be a security
# enhancement, if all processes that need to connect to mysqld run
# on the same host.  All interaction with mysqld must be made via Unix
# sockets or named pipes.
# Note that using this option without enabling named pipes on Windows
# (via the "enable-named-pipe" option) will render mysqld useless!
#skip-networking

# The maximum amount of concurrent sessions the MySQL server will
# allow. One of these connections will be reserved for a user with
# SUPER privileges to allow the administrator to login even if the
# connection limit has been reached.
max_connections = 100

# Maximum amount of errors allowed per host. If this limit is reached,
# the host will be blocked from connecting to the MySQL server until
# "FLUSH HOSTS" has been run or the server was restarted. Invalid
# passwords and other errors during the connect phase result in
# increasing this value. See the "Aborted_connects" status variable for
# global counter.
max_connect_errors = 10

# The number of open tables for all threads. Increasing this value
# increases the number of file descriptors that mysqld requires.
# Therefore you have to make sure to set the amount of open files
# allowed to at least 4096 in the variable "open-files-limit" in
# section [mysqld_safe]
table_cache = 2048

# Enable external file level locking. Enabled file locking will have a
# negative impact on performance, so only use it in case you have
# multiple database instances running on the same files (note some
# restrictions still apply!) or if you use other software relying on
# locking MyISAM tables on file level.
#external-locking

# The maximum size of a query packet the server can handle as well as
# maximum query size server can process (Important when working with
# large BLOBs).  enlarged dynamically, for each connection.
max_allowed_packet = 16M

# The size of the cache to hold the SQL statements for the binary log
# during a transaction. If you often use big, multi-statement
# transactions you can increase this value to get more performance. All
# statements from transactions are buffered in the binary log cache and
# are being written to the binary log at once after the COMMIT.  If the
# transaction is larger than this value, temporary file on disk is used
# instead.  This buffer is allocated per connection on first update
# statement in transaction
binlog_cache_size = 1M

# Maximum allowed size for a single HEAP (in memory) table. This option
# is a protection against the accidential creation of a very large HEAP
# table which could otherwise use up all memory resources.
max_heap_table_size = 64M

# Sort buffer is used to perform sorts for some ORDER BY and GROUP BY
# queries. If sorted data does not fit into the sort buffer, a disk
# based merge sort is used instead - See the "Sort_merge_passes"
# status variable. Allocated per thread if sort is needed.
sort_buffer_size = 8M

# This buffer is used for the optimization of full JOINs (JOINs without
# indexes). Such JOINs are very bad for performance in most cases
# anyway, but setting this variable to a large value reduces the
# performance impact. See the "Select_full_join" status variable for a
# count of full JOINs. Allocated per thread if full join is found
join_buffer_size = 8M

# How many threads we should keep in a cache for reuse. When a client
# disconnects, the client's threads are put in the cache if there aren't
# more than thread_cache_size threads from before.  This greatly reduces
# the amount of thread creations needed if you have a lot of new
# connections. (Normally this doesn't give a notable performance
# improvement if you have a good thread implementation.)
thread_cache_size = 8

# This permits the application to give the threads system a hint for the
# desired number of threads that should be run at the same time.  This
# value only makes sense on systems that support the thread_concurrency()
# function call (Sun Solaris, for example).
# You should try [number of CPUs]*(2..4) for thread_concurrency
thread_concurrency = 8

# Query cache is used to cache SELECT results and later return them
# without actual executing the same query once again. Having the query
# cache enabled may result in significant speed improvements, if your
# have a lot of identical queries and rarely changing tables. See the
# "Qcache_lowmem_prunes" status variable to check if the current value
# is high enough for your load.
# Note: In case your tables change very often or if your queries are
# textually different every time, the query cache may result in a
# slowdown instead of a performance improvement.
query_cache_size = 64M

# Only cache result sets that are smaller than this limit. This is to
# protect the query cache of a very large result set overwriting all
# other query results.
query_cache_limit = 2M

# Minimum word length to be indexed by the full text search index.
# You might wish to decrease it if you need to search for shorter words.
# Note that you need to rebuild your FULLTEXT index, after you have
# modified this value.
ft_min_word_len = 4

# If your system supports the memlock() function call, you might want to
# enable this option while running MySQL to keep it locked in memory and
# to avoid potential swapping out in case of high memory pressure. Good
# for performance.
#memlock

# Table type which is used by default when creating new tables, if not
# specified differently during the CREATE TABLE statement.
default_table_type = MYISAM

# Thread stack size to use. This amount of memory is always reserved at
# connection time. MySQL itself usually needs no more than 64K of
# memory, while if you use your own stack hungry UDF functions or your
# OS requires more stack for some operations, you might need to set this
# to a higher value.
thread_stack = 192K

# Set the default transaction isolation level. Levels available are:
# READ-UNCOMMITTED, READ-COMMITTED, REPEATABLE-READ, SERIALIZABLE
transaction_isolation = REPEATABLE-READ

# Maximum size for internal (in-memory) temporary tables. If a table
# grows larger than this value, it is automatically converted to disk
# based table This limitation is for a single table. There can be many
# of them.
tmp_table_size = 64M

# Enable binary logging. This is required for acting as a MASTER in a
# replication configuration. You also need the binary log if you need
# the ability to do point in time recovery from your latest backup.
log-bin=mysql-bin

# If you're using replication with chained slaves (A->B->C), you need to
# enable this option on server B. It enables logging of updates done by
# the slave thread into the slave's binary log.
#log_slave_updates

# Enable the full query log. Every query (even ones with incorrect
# syntax) that the server receives will be logged. This is useful for
# debugging, it is usually disabled in production use.
#log

# Print warnings to the error log file.  If you have any problem with
# MySQL you should enable logging of warnings and examine the error log
# for possible explanations. 
#log_warnings

# Log slow queries. Slow queries are queries which take more than the
# amount of time defined in "long_query_time" or which do not use
# indexes well, if log_long_format is enabled. It is normally good idea
# to have this turned on if you frequently add new queries to the
# system.
log_slow_queries

# All queries taking more than this amount of time (in seconds) will be
# trated as slow. Do not use "1" as a value here, as this will result in
# even very fast queries being logged from time to time (as MySQL
# currently measures time with second accuracy only).
long_query_time = 2

# Log more information in the slow query log. Normally it is good to
# have this turned on. This will enable logging of queries that are not
# using indexes in addition to long running queries.
log_long_format

# The directory used by MySQL for storing temporary files. For example,
# it is used to perform disk based large sorts, as well as for internal
# and explicit temporary tables. It might be good to put it on a
# swapfs/tmpfs filesystem, if you do not create very large temporary
# files. Alternatively you can put it on dedicated disk. You can
# specify multiple paths here by separating them by ";" - they will then
# be used in a round-robin fashion.
#tmpdir = /tmp


# ***  Replication related settings 


# Unique server identification number between 1 and 2^32-1. This value
# is required for both master and slave hosts. It defaults to 1 if
# "master-host" is not set, but will MySQL will not function as a master
# if it is omitted.
server-id = 1

# Replication Slave (comment out master section to use this)
#
# To configure this host as a replication slave, you can choose between
# two methods :
#
# 1) Use the CHANGE MASTER TO command (fully described in our manual) -
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
# OR
#
# 2) Set the variables below. However, in case you choose this method, then
#    start replication for the first time (even unsuccessfully, for example
#    if you mistyped the password in master-password and the slave fails to
#    connect), the slave will create a master.info file, and any later
#    changes in this file to the variable values below will be ignored and
#    overridden by the content of the master.info file, unless you shutdown
#    the slave server, delete master.info and restart the slaver server.
#    For that reason, you may want to leave the lines below untouched
#    (commented) and instead use CHANGE MASTER TO (see above)
#
# required unique id between 2 and 2^32 - 1
# (and different from the master)
# defaults to 2 if master-host is set
# but will not function as a slave if omitted
#server-id = 2
#
# The replication master for this slave - required
#master-host = <hostname>
#
# The username the slave will use for authentication when connecting
# to the master - required
#master-user = <username>
#
# The password the slave will authenticate with when connecting to
# the master - required
#master-password = <password>
#
# The port the master is listening on.
# optional - defaults to 3306
#master-port = <port>

# Make the slave read-only. Only users with the SUPER privilege and the
# replication slave thread will be able to modify data on it. You can
# use this to ensure that no applications will accidently modify data on
# the slave instead of the master
#read_only


#*** MyISAM Specific options


# Size of the Key Buffer, used to cache index blocks for MyISAM tables.
# Do not set it larger than 30% of your available memory, as some memory
# is also required by the OS to cache rows. Even if you're not using
# MyISAM tables, you should still set it to 8-64M as it will also be
# used for internal temporary disk tables.
key_buffer_size = 32M

# Size of the buffer used for doing full table scans of MyISAM tables.
# Allocated per thread, if a full scan is needed.
read_buffer_size = 2M

# When reading rows in sorted order after a sort, the rows are read
# through this buffer to avoid disk seeks. You can improve ORDER BY
# performance a lot, if set this to a high value.
# Allocated per thread, when needed.
read_rnd_buffer_size = 16M

# MyISAM uses special tree-like cache to make bulk inserts (that is,
# INSERT ... SELECT, INSERT ... VALUES (...), (...), ..., and LOAD DATA
# INFILE) faster. This variable limits the size of the cache tree in
# bytes per thread. Setting it to 0 will disable this optimisation.  Do
# not set it larger than "key_buffer_size" for optimal performance.
# This buffer is allocated when a bulk insert is detected.
bulk_insert_buffer_size = 64M

# This buffer is allocated when MySQL needs to rebuild the index in
# REPAIR, OPTIMIZE, ALTER table statements as well as in LOAD DATA INFILE
# into an empty table. It is allocated per thread so be careful with
# large settings.
myisam_sort_buffer_size = 128M

# The maximum size of the temporary file MySQL is allowed to use while
# recreating the index (during REPAIR, ALTER TABLE or LOAD DATA INFILE.
# If the file-size would be bigger than this, the index will be created
# through the key cache (which is slower).
myisam_max_sort_file_size = 10G

# If the temporary file used for fast index creation would be bigger
# than using the key cache by the amount specified here, then prefer the
# key cache method.  This is mainly used to force long character keys in
# large tables to use the slower key cache method to create the index.
myisam_max_extra_sort_file_size = 10G

# If a table has more than one index, MyISAM can use more than one
# thread to repair them by sorting in parallel. This makes sense if you
# have multiple CPUs and plenty of memory.
myisam_repair_threads = 1

# Automatically check and repair not properly closed MyISAM tables.
myisam_recover


# *** INNODB Specific options ***

# Use this option if you have a MySQL server with InnoDB support enabled
# but you do not plan to use it. This will save memory and disk space
# and speed up some things.
#skip-innodb

# Additional memory pool that is used by InnoDB to store metadata
# information.  If InnoDB requires more memory for this purpose it will
# start to allocate it from the OS.  As this is fast enough on most
# recent operating systems, you normally do not need to change this
# value. SHOW INNODB STATUS will display the current amount used.
innodb_additional_mem_pool_size = 16M

# InnoDB, unlike MyISAM, uses a buffer pool to cache both indexes and
# row data. The bigger you set this the less disk I/O is needed to
# access data in tables. On a dedicated database server you may set this
# parameter up to 80% of the machine physical memory size. Do not set it
# too large, though, because competition of the physical memory may
# cause paging in the operating system.  Note that on 32bit systems you
# might be limited to 2-3.5G of user level memory per process, so do not
# set it too high.
innodb_buffer_pool_size = 2G

# InnoDB stores data in one or more data files forming the tablespace.
# If you have a single logical drive for your data, a single
# autoextending file would be good enough. In other cases, a single file
# per device is often a good choice. You can configure InnoDB to use raw
# disk partitions as well - please refer to the manual for more info
# about this.
innodb_data_file_path = ibdata1:10M:autoextend

# Set this option if you would like the InnoDB tablespace files to be
# stored in another location. By default this is the MySQL datadir.
#innodb_data_home_dir = <directory>

# Number of IO threads to use for async IO operations. This value is
# hardcoded to 4 on Unix, but on Windows disk I/O may benefit from a
# larger number.
innodb_file_io_threads = 4

# If you run into InnoDB tablespace corruption, setting this to a nonzero
# value will likely help you to dump your tables. Start from value 1 and
# increase it until you're able to dump the table successfully.
#innodb_force_recovery=1

# Number of threads allowed inside the InnoDB kernel. The optimal value
# depends highly on the application, hardware as well as the OS
# scheduler properties. A too high value may lead to thread thrashing.
innodb_thread_concurrency = 16

# If set to 1, InnoDB will flush (fsync) the transaction logs to the
# disk at each commit, which offers full ACID behavior. If you are
# willing to compromise this safety, and you are running small
# transactions, you may set this to 0 or 2 to reduce disk I/O to the
# logs. Value 0 means that the log is only written to the log file and
# the log file flushed to disk approximately once per second. Value 2
# means the log is written to the log file at each commit, but the log
# file is only flushed to disk approximately once per second.
innodb_flush_log_at_trx_commit = 1

# Speed up InnoDB shutdown. This will disable InnoDB to do a full purge
# and insert buffer merge on shutdown. It may increase shutdown time a
# lot, but InnoDB will have to do it on the next startup instead.
#innodb_fast_shutdown

# The size of the buffer InnoDB uses for buffering log data. As soon as
# it is full, InnoDB will have to flush it to disk. As it is flushed
# once per second anyway, it does not make sense to have it very large
# (even with long transactions). 
innodb_log_buffer_size = 8M

# Size of each log file in a log group. You should set the combined size
# of log files to about 25%-100% of your buffer pool size to avoid
# unneeded buffer pool flush activity on log file overwrite. However,
# note that a larger logfile size will increase the time needed for the
# recovery process.
innodb_log_file_size = 256M

# Total number of files in the log group. A value of 2-3 is usually good
# enough.
innodb_log_files_in_group = 3

# Location of the InnoDB log files. Default is the MySQL datadir. You
# may wish to point it to a dedicated hard drive or a RAID1 volume for
# improved performance
#innodb_log_group_home_dir

# Maximum allowed percentage of dirty pages in the InnoDB buffer pool.
# If it is reached, InnoDB will start flushing them out agressively to
# not run out of clean pages at all. This is a soft limit, not
# guaranteed to be held.
innodb_max_dirty_pages_pct = 90

# The flush method InnoDB will use for Log. The tablespace always uses
# doublewrite flush logic. The default value is "fdatasync", another
# option is "O_DSYNC".
#innodb_flush_method=O_DSYNC

# How long an InnoDB transaction should wait for a lock to be granted
# before being rolled back. InnoDB automatically detects transaction
# deadlocks in its own lock table and rolls back the transaction. If you
# use the LOCK TABLES command, or other transaction-safe storage engines
# than InnoDB in the same transaction, then a deadlock may arise which
# InnoDB cannot notice. In cases like this the timeout is useful to
# resolve the situation.
innodb_lock_wait_timeout = 120


[mysqldump]
# Do not buffer the whole result set in memory before writing it to
# file. Required for dumping very large tables
quick

max_allowed_packet = 16M

[mysql]
no-auto-rehash

# Only allow UPDATEs and DELETEs that use keys.
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
# Increase the amount of open files allowed per process. Warning: Make
# sure you have set the global system limit high enough! The high value
# is required for a large number of opened tables
open-files-limit = 8192

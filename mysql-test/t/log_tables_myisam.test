#
# Bug #18559 log tables cannot change engine, and gets deadlocked when
# dropping w/ log on
#

--source include/force_myisam_default.inc
--source include/have_myisam.inc
--source include/no_valgrind_without_big.inc

--disable_ps_protocol
use mysql;
#
# Check that log tables work and we can do basic selects. This also
# tests truncate, which works in a special mode with the log tables
#
SET @old_log_output = @@global.log_output;
SET GLOBAL log_output="FILE,TABLE";
truncate table general_log;
--replace_column 1 TIMESTAMP 2 USER_HOST 3 THREAD_ID
select * from general_log;
truncate table slow_log;
--replace_column 1 TIMESTAMP 2 USER_HOST
select * from slow_log;

# check that appropriate error messages are given when one attempts to alter
# or drop a log tables, while corresponding logs are enabled
--error ER_BAD_LOG_STATEMENT
alter table mysql.general_log engine=myisam;
--error ER_BAD_LOG_STATEMENT
alter table mysql.slow_log engine=myisam;

--error ER_BAD_LOG_STATEMENT
drop table mysql.general_log;
--error ER_BAD_LOG_STATEMENT
drop table mysql.slow_log;

# check that one can alter log tables to MyISAM
set global general_log='OFF';

# cannot convert another log table
--error ER_BAD_LOG_STATEMENT
alter table mysql.slow_log engine=myisam;

# alter both tables
set global slow_query_log='OFF';
# check that both tables use CSV engine
show create table mysql.general_log;
show create table mysql.slow_log;

alter table mysql.general_log engine=myisam;
alter table mysql.slow_log engine=myisam;

# check that the tables were converted
show create table mysql.general_log;
show create table mysql.slow_log;

# enable log tables and check that new tables indeed work
set global general_log='ON';
set global slow_query_log='ON';

--replace_column 1 TIMESTAMP 2 USER_HOST 3 THREAD_ID
select * from mysql.general_log;

# check that flush of myisam-based log tables work fine
flush logs;

# check locking of myisam-based log tables

--error ER_CANT_LOCK_LOG_TABLE
lock tables mysql.general_log WRITE;

--error ER_CANT_LOCK_LOG_TABLE
lock tables mysql.slow_log WRITE;

#
# This attemts to get TL_READ_NO_INSERT lock, which is incompatible with
# TL_WRITE_CONCURRENT_INSERT. This should fail. We issue this error as log
# tables are always opened and locked by the logger.
#

--error ER_CANT_LOCK_LOG_TABLE
lock tables mysql.general_log READ;

--error ER_CANT_LOCK_LOG_TABLE
lock tables mysql.slow_log READ;

# check that we can drop them
set global general_log='OFF';
set global slow_query_log='OFF';

# check that alter table doesn't work for other engines
set @save_storage_engine= @@session.default_storage_engine;
set default_storage_engine= MEMORY;
# After fixing bug#35765 the error behaivor changed:
# If compiled in/enabled ER_UNSUPORTED_LOG_ENGINE
# If not (i.e. not existant) it will show a warning
# and use the current one.
--error ER_UNKNOWN_STORAGE_ENGINE
alter table mysql.slow_log engine=NonExistentEngine;
--error ER_UNSUPORTED_LOG_ENGINE
alter table mysql.slow_log engine=memory;
set default_storage_engine= @save_storage_engine;

drop table mysql.slow_log;
drop table mysql.general_log;

# check that table share cleanup is performed correctly (double drop)

--error ER_BAD_TABLE_ERROR
drop table mysql.general_log;
--error ER_BAD_TABLE_ERROR
drop table mysql.slow_log;

# recreate tables and enable logs

CREATE TABLE `general_log` (
  `event_time` timestamp(6) NOT NULL DEFAULT CURRENT_TIMESTAMP(6)
                         ON UPDATE CURRENT_TIMESTAMP(6),
  `user_host` mediumtext NOT NULL,
  `thread_id` bigint(21) unsigned NOT NULL,
  `server_id` int(10) unsigned NOT NULL,
  `command_type` varchar(64) NOT NULL,
  `argument` mediumblob NOT NULL
) ENGINE=CSV DEFAULT CHARSET=utf8mb3 COMMENT='General log';

CREATE TABLE `slow_log` (
  `start_time` timestamp(6) NOT NULL DEFAULT CURRENT_TIMESTAMP(6)
                         ON UPDATE CURRENT_TIMESTAMP(6),
  `user_host` mediumtext NOT NULL,
  `query_time` time(6) NOT NULL,
  `lock_time` time(6) NOT NULL,
  `rows_sent` int(11) NOT NULL,
  `rows_examined` int(11) NOT NULL,
  `db` varchar(512) NOT NULL,
  `last_insert_id` int(11) NOT NULL,
  `insert_id` int(11) NOT NULL,
  `server_id` int(10) unsigned NOT NULL,
  `sql_text` mediumblob NOT NULL,
  `thread_id` bigint(21) unsigned NOT NULL
) ENGINE=CSV DEFAULT CHARSET=utf8mb3 COMMENT='Slow log';

set global general_log='ON';
set global slow_query_log='ON';
SET GLOBAL log_output=@old_log_output;
TRUNCATE TABLE mysql.general_log;

--enable_ps_protocol
use test;


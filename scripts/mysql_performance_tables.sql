--
-- PERFORMANCE SCHEMA INSTALLATION
-- Note that this script is also reused by mysql_upgrade,
-- so we have to be very careful here to not destroy any
-- existing database named 'performance_schema' if it
-- can contain user data.
-- In case of downgrade, it's ok to drop unknown tables
-- from a future version, as long as they belong to the
-- performance schema engine.
--

set @have_old_pfs= (select count(*) from information_schema.schemata where schema_name='performance_schema');

SET @l1="SET @broken_tables = (select count(*) from information_schema.tables";
SET @l2=" where engine != \'PERFORMANCE_SCHEMA\' and table_schema=\'performance_schema\')";
SET @cmd=concat(@l1,@l2);

-- Work around for bug#49542
SET @str = IF(@have_old_pfs = 1, @cmd, 'SET @broken_tables = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

SET @l1="SET @broken_views = (select count(*) from information_schema.views";
SET @l2=" where table_schema='performance_schema')";
SET @cmd=concat(@l1,@l2);

-- Work around for bug#49542
SET @str = IF(@have_old_pfs = 1, @cmd, 'SET @broken_views = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

SET @broken_routines = (select count(*) from mysql.proc where db='performance_schema');

SET @broken_events = (select count(*) from mysql.event where db='performance_schema');

SET @broken_pfs= (select @broken_tables + @broken_views + @broken_routines + @broken_events);

--
-- The performance schema database.
-- Only drop and create the database if this is safe (no broken_pfs).
-- This database is created, even in --without-perfschema builds,
-- so that the database name is always reserved by the MySQL implementation.
-- This script must be executed AFTER we have fixed the proc table, to
-- avoid errors with old proc tables.
--

SET @cmd= "DROP DATABASE IF EXISTS performance_schema";

SET @str = IF(@broken_pfs = 0, @cmd, 'SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

SET @cmd= "CREATE DATABASE performance_schema character set utf8";

SET @str = IF(@broken_pfs = 0, @cmd, 'SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

--
-- From this point, only create the performance schema tables
-- if the server is build with performance schema
--

set @have_pfs= (select count(engine) from information_schema.engines where engine='PERFORMANCE_SCHEMA' and support != 'NO');

--
-- TABLE COND_INSTANCES
--

SET @l1="CREATE TABLE performance_schema.cond_instances(";
SET @l2="NAME VARCHAR(128) not null,";
SET @l3="OBJECT_INSTANCE_BEGIN BIGINT not null";
SET @l4=")ENGINE=PERFORMANCE_SCHEMA;";

SET @cmd=concat(@l1,@l2,@l3,@l4);

SET @str = IF(@have_pfs = 1, @cmd, 'SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

--
-- TABLE EVENTS_WAITS_CURRENT
--

SET @l1="CREATE TABLE performance_schema.events_waits_current(";
SET @l2="THREAD_ID INTEGER not null,";
SET @l3="EVENT_ID BIGINT unsigned not null,";
SET @l4="EVENT_NAME VARCHAR(128) not null,";
SET @l5="SOURCE VARCHAR(64),";
SET @l6="TIMER_START BIGINT unsigned,";
SET @l7="TIMER_END BIGINT unsigned,";
SET @l8="TIMER_WAIT BIGINT unsigned,";
SET @l9="SPINS INTEGER unsigned,";
SET @l10="OBJECT_SCHEMA VARCHAR(64),";
SET @l11="OBJECT_NAME VARCHAR(512),";
SET @l12="OBJECT_TYPE VARCHAR(64),";
SET @l13="OBJECT_INSTANCE_BEGIN BIGINT not null,";
SET @l14="NESTING_EVENT_ID BIGINT unsigned,";
SET @l15="OPERATION VARCHAR(16) not null,";
SET @l16="NUMBER_OF_BYTES BIGINT unsigned,";
SET @l17="FLAGS INTEGER unsigned";
SET @l18=")ENGINE=PERFORMANCE_SCHEMA;";

SET @cmd=concat(@l1,@l2,@l3,@l4,@l5,@l6,@l7,@l8,@l9,@l10,@l11,@l12,@l13,@l14,@l15,@l16,@l17,@l18);

SET @str = IF(@have_pfs = 1, @cmd, 'SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

--
-- TABLE EVENTS_WAITS_HISTORY
--

SET @l1="CREATE TABLE performance_schema.events_waits_history(";
-- lines 2 to 18 are unchanged from EVENTS_WAITS_CURRENT

SET @cmd=concat(@l1,@l2,@l3,@l4,@l5,@l6,@l7,@l8,@l9,@l10,@l11,@l12,@l13,@l14,@l15,@l16,@l17,@l18);

SET @str = IF(@have_pfs = 1, @cmd, 'SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

--
-- TABLE EVENTS_WAITS_HISTORY_LONG
--

SET @l1="CREATE TABLE performance_schema.events_waits_history_long(";
-- lines 2 to 18 are unchanged from EVENTS_WAITS_CURRENT

SET @cmd=concat(@l1,@l2,@l3,@l4,@l5,@l6,@l7,@l8,@l9,@l10,@l11,@l12,@l13,@l14,@l15,@l16,@l17,@l18);

SET @str = IF(@have_pfs = 1, @cmd, 'SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

--
-- TABLE EVENTS_WAITS_SUMMARY_BY_INSTANCE
--

SET @l1="CREATE TABLE performance_schema.events_waits_summary_by_instance(";
SET @l2="EVENT_NAME VARCHAR(128) not null,";
SET @l3="OBJECT_INSTANCE_BEGIN BIGINT not null,";
SET @l4="COUNT_STAR BIGINT unsigned not null,";
SET @l5="SUM_TIMER_WAIT BIGINT unsigned not null,";
SET @l6="MIN_TIMER_WAIT BIGINT unsigned not null,";
SET @l7="AVG_TIMER_WAIT BIGINT unsigned not null,";
SET @l8="MAX_TIMER_WAIT BIGINT unsigned not null";
SET @l9=")ENGINE=PERFORMANCE_SCHEMA;";

SET @cmd=concat(@l1,@l2,@l3,@l4,@l5,@l6,@l7,@l8,@l9);

SET @str = IF(@have_pfs = 1, @cmd, 'SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

--
-- TABLE EVENTS_WAITS_SUMMARY_BY_THREAD_BY_EVENT_NAME
--

SET @l1="CREATE TABLE performance_schema.events_waits_summary_by_thread_by_event_name(";
SET @l2="THREAD_ID INTEGER not null,";
SET @l3="EVENT_NAME VARCHAR(128) not null,";
SET @l4="COUNT_STAR BIGINT unsigned not null,";
SET @l5="SUM_TIMER_WAIT BIGINT unsigned not null,";
SET @l6="MIN_TIMER_WAIT BIGINT unsigned not null,";
SET @l7="AVG_TIMER_WAIT BIGINT unsigned not null,";
SET @l8="MAX_TIMER_WAIT BIGINT unsigned not null";
SET @l9=")ENGINE=PERFORMANCE_SCHEMA;";

SET @cmd=concat(@l1,@l2,@l3,@l4,@l5,@l6,@l7,@l8,@l9);

SET @str = IF(@have_pfs = 1, @cmd, 'SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

--
-- TABLE EVENTS_WAITS_SUMMARY_GLOBAL_BY_EVENT_NAME
--

SET @l1="CREATE TABLE performance_schema.events_waits_summary_global_by_event_name(";
SET @l2="EVENT_NAME VARCHAR(128) not null,";
SET @l3="COUNT_STAR BIGINT unsigned not null,";
SET @l4="SUM_TIMER_WAIT BIGINT unsigned not null,";
SET @l5="MIN_TIMER_WAIT BIGINT unsigned not null,";
SET @l6="AVG_TIMER_WAIT BIGINT unsigned not null,";
SET @l7="MAX_TIMER_WAIT BIGINT unsigned not null";
SET @l8=")ENGINE=PERFORMANCE_SCHEMA;";

SET @cmd=concat(@l1,@l2,@l3,@l4,@l5,@l6,@l7,@l8);

SET @str = IF(@have_pfs = 1, @cmd, 'SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

--
-- TABLE FILE_INSTANCES
--

SET @l1="CREATE TABLE performance_schema.file_instances(";
SET @l2="FILE_NAME VARCHAR(512) not null,";
SET @l3="EVENT_NAME VARCHAR(128) not null,";
SET @l4="OPEN_COUNT INTEGER unsigned not null";
SET @l5=")ENGINE=PERFORMANCE_SCHEMA;";

SET @cmd=concat(@l1,@l2,@l3,@l4,@l5);

SET @str = IF(@have_pfs = 1, @cmd, 'SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

--
-- TABLE FILE_SUMMARY_BY_EVENT_NAME
--

SET @l1="CREATE TABLE performance_schema.file_summary_by_event_name(";
SET @l2="EVENT_NAME VARCHAR(128) not null,";
SET @l3="COUNT_READ BIGINT unsigned not null,";
SET @l4="COUNT_WRITE BIGINT unsigned not null,";
SET @l5="SUM_NUMBER_OF_BYTES_READ BIGINT unsigned not null,";
SET @l6="SUM_NUMBER_OF_BYTES_WRITE BIGINT unsigned not null";
SET @l7=")ENGINE=PERFORMANCE_SCHEMA;";

SET @cmd=concat(@l1,@l2,@l3,@l4,@l5,@l6,@l7);

SET @str = IF(@have_pfs = 1, @cmd, 'SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

--
-- TABLE FILE_SUMMARY_BY_INSTANCE
--

SET @l1="CREATE TABLE performance_schema.file_summary_by_instance(";
SET @l2="FILE_NAME VARCHAR(512) not null,";
SET @l3="EVENT_NAME VARCHAR(128) not null,";
SET @l4="COUNT_READ BIGINT unsigned not null,";
SET @l5="COUNT_WRITE BIGINT unsigned not null,";
SET @l6="SUM_NUMBER_OF_BYTES_READ BIGINT unsigned not null,";
SET @l7="SUM_NUMBER_OF_BYTES_WRITE BIGINT unsigned not null";
SET @l8=")ENGINE=PERFORMANCE_SCHEMA;";

SET @cmd=concat(@l1,@l2,@l3,@l4,@l5,@l6,@l7,@l8);

SET @str = IF(@have_pfs = 1, @cmd, 'SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

--
-- TABLE MUTEX_INSTANCES
--

SET @l1="CREATE TABLE performance_schema.mutex_instances(";
SET @l2="NAME VARCHAR(128) not null,";
SET @l3="OBJECT_INSTANCE_BEGIN BIGINT not null,";
SET @l4="LOCKED_BY_THREAD_ID INTEGER";
SET @l5=")ENGINE=PERFORMANCE_SCHEMA;";

SET @cmd=concat(@l1,@l2,@l3,@l4,@l5);

SET @str = IF(@have_pfs = 1, @cmd, 'SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

--
-- TABLE PERFORMANCE_TIMERS
--

SET @l1="CREATE TABLE performance_schema.performance_timers(";
SET @l2="TIMER_NAME ENUM ('CYCLE', 'NANOSECOND', 'MICROSECOND', 'MILLISECOND', 'TICK') not null,";
SET @l3="TIMER_FREQUENCY BIGINT,";
SET @l4="TIMER_RESOLUTION BIGINT,";
SET @l5="TIMER_OVERHEAD BIGINT";
SET @l6=") ENGINE=PERFORMANCE_SCHEMA;";

SET @cmd=concat(@l1,@l2,@l3,@l4,@l5,@l6);

SET @str = IF(@have_pfs = 1, @cmd, 'SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

--
-- TABLE RWLOCK_INSTANCES
--

SET @l1="CREATE TABLE performance_schema.rwlock_instances(";
SET @l2="NAME VARCHAR(128) not null,";
SET @l3="OBJECT_INSTANCE_BEGIN BIGINT not null,";
SET @l4="WRITE_LOCKED_BY_THREAD_ID INTEGER,";
SET @l5="READ_LOCKED_BY_COUNT INTEGER unsigned not null";
SET @l6=")ENGINE=PERFORMANCE_SCHEMA;";

SET @cmd=concat(@l1,@l2,@l3,@l4,@l5,@l6);

SET @str = IF(@have_pfs = 1, @cmd, 'SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

--
-- TABLE SETUP_CONSUMERS
--

SET @l1="CREATE TABLE performance_schema.setup_consumers(";
SET @l2="NAME VARCHAR(64) not null,";
SET @l3="ENABLED ENUM ('YES', 'NO') not null";
SET @l4=")ENGINE=PERFORMANCE_SCHEMA;";

SET @cmd=concat(@l1,@l2,@l3,@l4);

SET @str = IF(@have_pfs = 1, @cmd, 'SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

--
-- TABLE SETUP_INSTRUMENTS
--

SET @l1="CREATE TABLE performance_schema.setup_instruments(";
SET @l2="NAME VARCHAR(128) not null,";
SET @l3="ENABLED ENUM ('YES', 'NO') not null,";
SET @l4="TIMED ENUM ('YES', 'NO') not null";
SET @l5=")ENGINE=PERFORMANCE_SCHEMA;";

SET @cmd=concat(@l1,@l2,@l3,@l4,@l5);

SET @str = IF(@have_pfs = 1, @cmd, 'SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

--
-- TABLE SETUP_TIMERS
--

SET @l1="CREATE TABLE performance_schema.setup_timers(";
SET @l2="NAME VARCHAR(64) not null,";
SET @l3="TIMER_NAME ENUM ('CYCLE', 'NANOSECOND', 'MICROSECOND', 'MILLISECOND', 'TICK') not null";
SET @l4=")ENGINE=PERFORMANCE_SCHEMA;";

SET @cmd=concat(@l1,@l2,@l3,@l4);

SET @str = IF(@have_pfs = 1, @cmd, 'SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

--
-- TABLE THREADS
--

SET @l1="CREATE TABLE performance_schema.threads(";
SET @l2="THREAD_ID INTEGER not null,";
SET @l3="PROCESSLIST_ID INTEGER,";
SET @l4="NAME VARCHAR(128) not null";
SET @l5=")ENGINE=PERFORMANCE_SCHEMA;";

SET @cmd=concat(@l1,@l2,@l3,@l4,@l5);

SET @str = IF(@have_pfs = 1, @cmd, 'SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

--
-- Unlike 'performance_schema', the 'mysql' database is reserved already,
-- so no user procedure is supposed to be there.
--
-- NOTE: until upgrade is finished, stored routines are not available,
-- because system tables (e.g. mysql.proc) might be not usable.
--
drop procedure if exists mysql.die;
create procedure mysql.die() signal sqlstate 'HY000' set message_text='Unexpected content found in the performance_schema database.';

--
-- For broken upgrades, SIGNAL the error
--

SET @cmd="call mysql.die()";

SET @str = IF(@broken_pfs > 0, @cmd, 'SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

drop procedure mysql.die;

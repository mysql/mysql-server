
# This script tests our own time zone support functions

# Preparing playground
--disable_warnings
drop table if exists t1, t2;
drop function if exists f1;
--enable_warnings
SET sql_mode = 'NO_ENGINE_SUBSTITUTION';

#
# Let us first check +HH:MM style timezones
#
create table t1 (ts timestamp);

set time_zone='+00:00';
select unix_timestamp(utc_timestamp())-unix_timestamp(current_timestamp());
insert into t1 (ts) values ('2003-03-30 02:30:00');

set time_zone='+10:30';
select unix_timestamp(utc_timestamp())-unix_timestamp(current_timestamp());
insert into t1 (ts) values ('2003-03-30 02:30:00');

set time_zone='-10:00';
select unix_timestamp(utc_timestamp())-unix_timestamp(current_timestamp());
insert into t1 (ts) values ('2003-03-30 02:30:00');

# Here we will get different results
select * from t1;

drop table t1;


#
# Let us try DB specified time zones
#
select Name from mysql.time_zone_name where Name in
  ('UTC','Universal','MET','Europe/Moscow','leap/Europe/Moscow','Japan','CET','US/Pacific');

create table t1 (i int, ts timestamp);

set time_zone='MET';

# We check common date time value and non existent or ambiguios values
# Normal value without DST
insert into t1 (i, ts) values
  (unix_timestamp('2003-03-01 00:00:00'),'2003-03-01 00:00:00');
# Values around and in spring time-gap
insert into t1 (i, ts) values
  (unix_timestamp('2003-03-30 01:59:59'),'2003-03-30 01:59:59'),
  (unix_timestamp('2003-03-30 02:30:00'),'2003-03-30 02:30:00'),
  (unix_timestamp('2003-03-30 03:00:00'),'2003-03-30 03:00:00');
# Values around and in spring time-gap
insert into t1 (i, ts) values
  (unix_timestamp(20030330015959),20030330015959),
  (unix_timestamp(20030330023000),20030330023000),
  (unix_timestamp(20030330030000),20030330030000);
# Normal value with DST
insert into t1 (i, ts) values
  (unix_timestamp('2003-05-01 00:00:00'),'2003-05-01 00:00:00');
# Ambiguos values (also check for determenism)
insert into t1 (i, ts) values
  (unix_timestamp('2003-10-26 01:00:00'),'2003-10-26 01:00:00'),
  (unix_timestamp('2003-10-26 02:00:00'),'2003-10-26 02:00:00'),
  (unix_timestamp('2003-10-26 02:59:59'),'2003-10-26 02:59:59'),
  (unix_timestamp('2003-10-26 04:00:00'),'2003-10-26 04:00:00'),
  (unix_timestamp('2003-10-26 02:59:59'),'2003-10-26 02:59:59');

set time_zone='UTC';

select * from t1;

truncate table t1;

# Simple check for 'Europe/Moscow' time zone just for showing that it works
set time_zone='Europe/Moscow';
insert into t1 (i, ts) values
  (unix_timestamp('2004-01-01 00:00:00'),'2004-01-01 00:00:00'),
  (unix_timestamp('2004-03-28 02:30:00'),'2004-03-28 02:30:00'),
  (unix_timestamp('2004-08-01 00:00:00'),'2003-08-01 00:00:00'),
  (unix_timestamp('2004-10-31 02:30:00'),'2004-10-31 02:30:00');
select * from t1;
truncate table t1;


#
# Check for time zone with leap seconds
# Values in ts column must be the same but values in i column should
# differ from corresponding values for Europe/Moscow a bit.
#
set time_zone='leap/Europe/Moscow';
insert into t1 (i, ts) values
  (unix_timestamp('2004-01-01 00:00:00'),'2004-01-01 00:00:00'),
  (unix_timestamp('2004-03-28 02:30:00'),'2004-03-28 02:30:00'),
  (unix_timestamp('2004-08-01 00:00:00'),'2003-08-01 00:00:00'),
  (unix_timestamp('2004-10-31 02:30:00'),'2004-10-31 02:30:00');
select * from t1;
truncate table t1;
# Let us test leap jump
insert into t1 (i, ts) values
  (unix_timestamp('1981-07-01 03:59:59'),'1981-07-01 03:59:59'),
  (unix_timestamp('1981-07-01 04:00:00'),'1981-07-01 04:00:00');
select * from t1;
# Additional 60ieth second!
select from_unixtime(362793609);

drop table t1;


#
# Let us test range for TIMESTAMP
#
create table t1 (ts timestamp);
set time_zone='UTC';
insert into t1 values ('0000-00-00 00:00:00'),('1969-12-31 23:59:59'),
                      ('1970-01-01 00:00:00'),('1970-01-01 00:00:01'),
                      ('2038-01-19 03:14:07'),('2038-01-19 03:14:08');
select * from t1;
truncate table t1;
# MET time zone has range shifted by one hour
set time_zone='MET';
insert into t1 values ('0000-00-00 00:00:00'),('1970-01-01 00:30:00'),
                      ('1970-01-01 01:00:00'),('1970-01-01 01:00:01'),
                      ('2038-01-19 04:14:07'),('2038-01-19 04:14:08');
select * from t1;
truncate table t1;
# same for +01:30 time zone
set time_zone='+01:30';
insert into t1 values ('0000-00-00 00:00:00'),('1970-01-01 01:00:00'),
                      ('1970-01-01 01:30:00'),('1970-01-01 01:30:01'),
                      ('2038-01-19 04:44:07'),('2038-01-19 04:44:08');
select * from t1;

drop table t1;


#
# Test of show variables
#
show variables like 'time_zone';
set time_zone = default;
show variables like 'time_zone';


#
# Let us try some invalid time zone specifications
#
--error 1298
set time_zone= '0';
--error 1298
set time_zone= '0:0';
--error 1298
set time_zone= '-20:00';
--error 1298
set time_zone= '+20:00';
--error 1298
set time_zone= 'Some/Unknown/Time/Zone';


# Let us check that aliases for time zones work and they are
# case-insensitive
select convert_tz(now(),'UTC', 'Universal') = now();
select convert_tz(now(),'utc', 'UTC') = now();


#
# Let us test CONVERT_TZ function (may be func_time.test is better place).
#
select convert_tz('1917-11-07 12:00:00', 'MET', 'UTC');
select convert_tz('1970-01-01 01:00:00', 'MET', 'UTC');
select convert_tz('1970-01-01 01:00:01', 'MET', 'UTC');
select convert_tz('2003-03-01 00:00:00', 'MET', 'UTC');
select convert_tz('2003-03-30 01:59:59', 'MET', 'UTC');
select convert_tz('2003-03-30 02:30:00', 'MET', 'UTC');
select convert_tz('2003-03-30 03:00:00', 'MET', 'UTC');
select convert_tz('2003-05-01 00:00:00', 'MET', 'UTC');
select convert_tz('2003-10-26 01:00:00', 'MET', 'UTC');
select convert_tz('2003-10-26 02:00:00', 'MET', 'UTC');
select convert_tz('2003-10-26 02:59:59', 'MET', 'UTC');
select convert_tz('2003-10-26 04:00:00', 'MET', 'UTC');
select convert_tz('2038-01-19 04:14:07', 'MET', 'UTC');
# moved to func_unixtime.inc
# select convert_tz('2038-01-19 04:14:08', 'MET', 'UTC');
# select convert_tz('2103-01-01 04:00:00', 'MET', 'UTC');

# Let us test variable time zone argument
create table t1 (tz varchar(3));
insert into t1 (tz) values ('MET'), ('UTC');
select tz, convert_tz('2003-12-31 00:00:00',tz,'UTC'), convert_tz('2003-12-31 00:00:00','UTC',tz) from t1 order by tz;
drop table t1;

# Parameters to CONVERT_TZ() what should give NULL
select convert_tz('2003-12-31 04:00:00', NULL, 'UTC');
select convert_tz('2003-12-31 04:00:00', 'SomeNotExistingTimeZone', 'UTC');
select convert_tz('2003-12-31 04:00:00', 'MET', 'SomeNotExistingTimeZone');
select convert_tz('2003-12-31 04:00:00', 'MET', NULL);
select convert_tz( NULL, 'MET', 'UTC');

#
# Test for bug #4508 "CONVERT_TZ() function with new time zone as param
# crashes server." (Was caused by improperly worked mechanism of time zone
# dynamical loading).
#
create table t1 (ts timestamp);
set timestamp=1000000000;
insert into t1 (ts) values (now());
select convert_tz(ts, @@time_zone, 'Japan') from t1;
drop table t1;

#
# Test for bug #7705 "CONVERT_TZ() crashes with subquery/WHERE on index
# column". Queries in which one of time zone arguments of CONVERT_TZ() is
# determined as constant only at val() stage (not at fix_fields() stage),
# should not crash server.
#
select convert_tz('2005-01-14 17:00:00', 'UTC', custTimeZone) from (select 'UTC' as custTimeZone) as tmp;

#
# Test for bug #7899 "CREATE TABLE .. SELECT .. and CONVERT_TZ() function
# does not work well together". The following statement should return only
# one NULL row and not result of full join.
#
create table t1 select convert_tz(NULL, NULL, NULL);
select * from t1;
drop table t1;

# End of 4.1 tests

#
# Test for bug #11081 "Using a CONVERT_TZ function in a stored function
# or trigger fails".
#
SET @old_log_bin_trust_function_creators = @@global.log_bin_trust_function_creators;
SET GLOBAL log_bin_trust_function_creators = 1;

create table t1 (ldt datetime, udt datetime);
create function f1(i datetime) returns datetime
  return convert_tz(i, 'UTC', 'Europe/Moscow');
create trigger t1_bi before insert on t1 for each row
  set new.udt:= convert_tz(new.ldt, 'Europe/Moscow', 'UTC');
# This should work without errors
insert into t1 (ldt) values ('2006-04-19 16:30:00');
select * from t1;
# This should work without errors as well
select ldt, f1(udt) as ldt2 from t1;
drop table t1;
drop function f1;

SET @@global.log_bin_trust_function_creators= @old_log_bin_trust_function_creators;

# End of 5.0 tests


#
# BUG#9953: CONVERT_TZ requires mysql.time_zone_name to be locked
# BUG#19339: CONVERT_TZ(): overly aggressive in locking time_zone_name
# table
#
--disable_warnings
DROP TABLE IF EXISTS t1;
--enable_warnings

CREATE TABLE t1 (t TIMESTAMP);
INSERT INTO t1 VALUES (NULL), (NULL);

LOCK TABLES t1 WRITE;

# The following two queries should not return error that time zone
# tables aren't locked.  We use IS NULL below to supress timestamp
# result.
SELECT CONVERT_TZ(NOW(), 'UTC', 'Europe/Moscow') IS NULL;
UPDATE t1 SET t = CONVERT_TZ(t, 'UTC', 'Europe/Moscow');

UNLOCK TABLES;

DROP TABLE t1;

--echo #
--echo # Bug #55424: convert_tz crashes when fed invalid data
--echo #

CREATE TABLE t1 (a SET('x') NOT NULL);
INSERT INTO t1 VALUES ('');
SELECT CONVERT_TZ(1, a, 1) FROM t1;
SELECT CONVERT_TZ(1, 1, a) FROM t1;
DROP TABLE t1;

--echo End of 5.1 tests

--echo #
--echo # Start of 5.6 tests
--echo #
SET time_zone='Europe/Moscow';
CREATE TABLE t1 (a TIMESTAMP, b VARCHAR(30));
# Testing values around winter/summer time change
# Expect non-zero UNIX_TIMESTAMP value for all of them
INSERT INTO t1 VALUES
('2003-03-30 01:59:59', 'Before the gap'),
('2003-03-30 02:30:00', 'Inside the gap'),
('2003-03-30 03:00:00',  'After the gap');
SELECT a, UNIX_TIMESTAMP(a), b FROM t1;
DROP TABLE t1;
SELECT UNIX_TIMESTAMP('2003-03-30 01:59:59'), 'Before the gap' AS b;
SELECT UNIX_TIMESTAMP('2003-03-30 02:30:00'), 'Inside the gap' AS b;
SELECT UNIX_TIMESTAMP('2003-03-30 03:00:00'), 'After the gap' AS b;
SET time_zone=DEFAULT;
SET sql_mode = default;
--echo #
--echo # End of 5.6 tests
--echo #

--echo #
--echo # Bug #21459795 "ASSERTION FAILURE WHEN PREVIOUSLY UNUSED TIME
--echo #                ZONE IS USED IN FUNCTION/TRIGGER".
--echo #
CREATE FUNCTION f1() RETURNS DATETIME RETURN CONVERT_TZ('2015-01-01 00:00:00', 'UTC', 'No-such-time-zone');
--echo # Below statement caused assertion failure.
SELECT f1();
DROP FUNCTION f1;

--echo #
--echo # Bug#33840652 Wrong result for UNIX_TIMESTAMP for time zones west of UTC close to
--echo #           UNIX epoch 1970-01-01
--echo # Bug#33837691 UNIX_TIMESTAMP() works differently in 8.0.28 than in previous versions
--echo #

--echo #
--echo # Test west of UTC at and around UNIX epoch.
--echo # We cannot test at max value here since it differs between 32 and 64
--echo # bits time OSes. Refer to specific tests func_unixtime_{32,64}bits
--echo #
SET time_zone = "US/Pacific";
SELECT FROM_UNIXTIME(0);
SELECT UNIX_TIMESTAMP("1969-12-31 15:59:59");
SELECT UNIX_TIMESTAMP("1969-12-31 16:00:00");
SELECT UNIX_TIMESTAMP("1969-12-31 16:00:01");
SELECT UNIX_TIMESTAMP("1970-01-01 00:00:01");
SELECT UNIX_TIMESTAMP("2022-01-01 16:00:01");

SET time_zone = DEFAULT;

# The include statement below is a temp one for tests that are yet to
#be ported to run with InnoDB,
#but needs to be kept for tests that would need MyISAM in future.
--source include/force_myisam_default.inc
--source include/have_myisam.inc

#
# Test of handling time zone with leap seconds.
#
# This test should be run with TZ=:$MYSQL_TEST_DIR/std_data/Moscow_leap
# This implies that this test should be run only on systems that interpret 
# characters after colon in TZ variable as path to zoneinfo file.
#
# Check that we have successfully set time zone with leap seconds.
--source include/have_moscow_leap_timezone.inc

# Initial clean-up
--disable_warnings
drop table if exists t1;
--enable_warnings

#
# Let us check behavior of conversion from broken-down representation
# to time_t representation, for normal, non-existent and ambigious dates
# (This check is similar to the one in timezone2.test in 4.1)
#
create table t1 (i int, c varchar(20));
# Normal value without DST
insert into t1 values
  (unix_timestamp("2004-01-01 00:00:00"), "2004-01-01 00:00:00");
# Values around and in spring time-gap
insert into t1 values
  (unix_timestamp("2004-03-28 01:59:59"), "2004-03-28 01:59:59"),
  (unix_timestamp("2004-03-28 02:30:00"), "2004-03-28 02:30:00"),
  (unix_timestamp("2004-03-28 03:00:00"), "2004-03-28 03:00:00");
# Normal value with DST
insert into t1 values
  (unix_timestamp('2004-05-01 00:00:00'),'2004-05-01 00:00:00');
# Ambiguos values (also check for determenism)
insert into t1 values
  (unix_timestamp('2004-10-31 01:00:00'),'2004-10-31 01:00:00'),
  (unix_timestamp('2004-10-31 02:00:00'),'2004-10-31 02:00:00'),
  (unix_timestamp('2004-10-31 02:59:59'),'2004-10-31 02:59:59'),
  (unix_timestamp('2004-10-31 04:00:00'),'2004-10-31 04:00:00'),
  (unix_timestamp('2004-10-31 02:59:59'),'2004-10-31 02:59:59');
# Test of leap
insert into t1 values
  (unix_timestamp('1981-07-01 03:59:59'),'1981-07-01 03:59:59'),
  (unix_timestamp('1981-07-01 04:00:00'),'1981-07-01 04:00:00');

insert into t1 values
  (unix_timestamp('2009-01-01 02:59:59'),'2009-01-01 02:59:59'),
  (unix_timestamp('2009-01-01 03:00:00'),'2009-01-01 03:00:00');

select i, from_unixtime(i), c from t1;
drop table t1;

#
# Test for bug #6387 "Queried timestamp values do not match the 
# inserted". my_gmt_sec() function was not working properly if we
# had time zone with leap seconds 
#
create table t1 (ts timestamp);
insert into t1 values (19730101235900), (20040101235900);
select * from t1;
drop table t1;

#
# Test Bug #39920: MySQL cannot deal with Leap Second expression in string
# literal
#

# 2009-01-01 02:59:59, 2009-01-01 02:59:60 and 2009-01-01 03:00:00
SELECT FROM_UNIXTIME(1230768022), FROM_UNIXTIME(1230768023), FROM_UNIXTIME(1230768024);

# End of 4.1 tests

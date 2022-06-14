--echo #
--echo # Time related tests with canon differences for hosts with UNIX time
--echo # representation of 32 or 64 bits.
--echo # When we discontinue support for 32 bits time platforms, these can be
--echo # moved (back) to where they came from.
--echo #
--echo # ------ 1. func_time.test   BEGIN
--echo #
--echo # check for from_unixtime(2^31-1) and from_unixtime(2^31)
select from_unixtime(2147483647);
select from_unixtime(2147483648);
select from_unixtime(0);
select from_unixtime(-1);
select from_unixtime(-2147483648);

--echo #
--echo # Some more tests for bug #9191 "TIMESTAMP/from_unixtime() no
--echo # longer accepts 2^31-1". Here we test that from_unixtime and
--echo # unix_timestamp are consistent, when working with boundary dates.
--echo #
select unix_timestamp(from_unixtime(2147483647));
select unix_timestamp(from_unixtime(2147483648));

--echo # check for invalid dates

--echo # bad year
select unix_timestamp('2039-01-20 01:00:00');
select unix_timestamp('1968-01-20 01:00:00');
--echo # bad month
select unix_timestamp('2038-02-10 01:00:00');
select unix_timestamp('1969-11-20 01:00:00');
--echo # bad day
select unix_timestamp('2038-01-20 01:00:00');
select unix_timestamp('1969-12-30 01:00:00');

--echo #
--echo # Check negative shift (we subtract several days for boundary dates during
--echo # conversion).
select unix_timestamp('2038-01-17 12:00:00');

--echo #
--echo # Check positive shift. (it happens only on
--echo # platfroms with unsigned time_t, such as QNX)
--echo #
select unix_timestamp('1970-01-01 03:00:01');

--echo # check bad date, close to the boundary (we cut them off in the very end)
select unix_timestamp('2038-01-19 07:14:07');

--echo # ------ 1. func_time.test   END

--echo # ------ 2. time_zone.test   BEGIN
--echo #
--echo # Test for fix for Bug#2523 Check that boundary dates are processed
--echo # correctly.
--echo #
set time_zone="+01:00";
select unix_timestamp('1970-01-01 01:00:00'),
       unix_timestamp('1970-01-01 01:00:01'),
       unix_timestamp('2038-01-19 04:14:07'),
       unix_timestamp('2038-01-19 04:14:08');
set time_zone=default;
--echo # ------ 2. time_zone.test   END

--echo # ------ 3. time_zone2.test   BEGIN
select convert_tz('2038-01-19 04:14:08', '+01:00', 'UTC');
select convert_tz('2103-01-01 04:00:00', 'MET', 'UTC');

--echo Will work on a 64 bits time system, no-op on 32 bits time
select convert_tz('3001-01-18 23:59:59', 'UTC', '+01:00');
--echo Will not work, beyond range of UNIX timestamp support
select convert_tz('3001-01-19 00:00:00', 'UTC', '+01:00');


--echo # ------ 3. time_zone2.test   END

--echo # ------ 4. type_temporal_fractional.test   BEGIN
--echo #
--echo # Testing rounding with FROM_UNIXTIME
--echo #
SET time_zone='+00:00';
CREATE TABLE t1 (a DECIMAL(20,7));
INSERT INTO t1 VALUES
  (32536771199.999999),
  (32536771199.9999990),
  (32536771199.9999991),
  (32536771199.9999992),
  (32536771199.9999993),
  (32536771199.9999994),
  (32536771199.9999995),
  (32536771199.9999996),
  (32536771199.9999997),
  (32536771199.9999998),
  (32536771199.9999999);
SELECT a, FROM_UNIXTIME(a) FROM t1;
DROP TABLE t1;
SET time_zone=DEFAULT;

--echo #
--echo # Bug #28671811 FROM_UNIXTIME ACCEPTS AN OUT OF RANGE PARAMETER
--echo #               THROUGH ROUNDING
--echo #

SET time_zone='+00:00';

SELECT
  FROM_UNIXTIME(2147483647) AS c1,
  FROM_UNIXTIME(2147483648) AS c2,
  FROM_UNIXTIME(2147483647.9999999) AS c3,
  FROM_UNIXTIME(32536771199) AS c4,
  FROM_UNIXTIME(32536771199.9999999) AS c5;

SET time_zone=default;

SELECT
  FROM_UNIXTIME(2147483647) AS c1,
  FROM_UNIXTIME(2147483648) AS c2,
  FROM_UNIXTIME(2147483647.9999999) AS c3,
  FROM_UNIXTIME(32536771199) AS c4,
  FROM_UNIXTIME(32536771199.9999999) AS c5;

SET sql_mode=time_truncate_fractional;
SET time_zone='+00:00';

SELECT
  FROM_UNIXTIME(2147483647) AS c1,
  FROM_UNIXTIME(2147483648) AS c2,
  FROM_UNIXTIME(2147483647.9999999) AS c3,
  FROM_UNIXTIME(32536771199) AS c4,
  FROM_UNIXTIME(32536771199.9999999) AS c5;

SET time_zone=default;

SELECT
  FROM_UNIXTIME(2147483647) AS c1,
  FROM_UNIXTIME(2147483648) AS c2,
  FROM_UNIXTIME(2147483647.9999999) AS c3,
  FROM_UNIXTIME(32536771199) AS c4,
  FROM_UNIXTIME(32536771199.9999999) AS c5;

SET sql_mode=default;

--echo # ------ 4. type_temporal_fractional.test   END

--echo # ------ 5 and 6. type_timestamp{_explicit}.test   BEGIN
#
# Bug#41370: TIMESTAMP field does not accepts NULL from FROM_UNIXTIME()
#
# Update: Since WL#14630, FROM_UNIXTIME('9999999999') no longer
# gives NULL, but a real UNIX timestamp outside 32 bits time range, so
# this test is now changed. To retain the test of NULL from
# FROM_UNIXTIME we now add FROM_UNIXTIME(99999999990), i.e. a date
# beyond UTC 23:59:59, January 18, 3001. This will (still) return NULL.
#
SET SESSION explicit_defaults_for_timestamp=OFF;
SET SESSION sql_mode = 'NO_ENGINE_SUBSTITUTION';
SET TIMESTAMP=1000000019;
CREATE TABLE t1 ( f1 INT(11) NOT NULL AUTO_INCREMENT PRIMARY KEY,
                  f2 TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
                  f3 TIMESTAMP NOT NULL default '0000-00-00 00:00:00');
INSERT INTO t1 (f2,f3) VALUES (NOW(), "0000-00-00 00:00:00");
INSERT INTO t1 (f2,f3) VALUES (NOW(), NULL);
INSERT INTO t1 (f2,f3) VALUES (NOW(), ASCII(NULL));
INSERT INTO t1 (f2,f3) VALUES (NOW(), FROM_UNIXTIME('9999999999'));
INSERT INTO t1 (f2,f3) VALUES (NOW(), TIME(NULL));
INSERT INTO t1 (f2,f3) VALUES (NOW(), FROM_UNIXTIME(99999999990));
UPDATE t1 SET f2=NOW(), f3=FROM_UNIXTIME('9999999999') WHERE f1=1;
SELECT f1,f2-f3 FROM t1;

DROP TABLE t1;

CREATE TABLE t1 ( f1 INT(11) NOT NULL AUTO_INCREMENT PRIMARY KEY,
                  f2 TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
                  f3 TIMESTAMP NOT NULL default '0000-00-00 00:00:00');
SET SESSION explicit_defaults_for_timestamp=DEFAULT;
INSERT INTO t1 (f2,f3) VALUES (NOW(), "0000-00-00 00:00:00");
--error ER_BAD_NULL_ERROR
INSERT INTO t1 (f2,f3) VALUES (NOW(), NULL);
--error ER_BAD_NULL_ERROR
INSERT INTO t1 (f2,f3) VALUES (NOW(), ASCII(NULL));
# This gives NULL on 32 bits time platforms but works on 64 bits time platforms
INSERT INTO t1 (f2,f3) VALUES (NOW(), COALESCE(FROM_UNIXTIME('9999999999'), "0000-00-00 00:00:00"));
--error ER_BAD_NULL_ERROR
INSERT INTO t1 (f2,f3) VALUES (NOW(), TIME(NULL));
--error ER_BAD_NULL_ERROR
INSERT INTO t1 (f2,f3) VALUES (NOW(), FROM_UNIXTIME(99999999990));
UPDATE t1 SET f2=NOW(), f3=FROM_UNIXTIME('9999999999') WHERE f1=1;
SELECT f1,f2-f3 FROM t1;

DROP TABLE t1;
SET SESSION sql_mode=DEFAULT;
SET TIMESTAMP=0;
--echo # ------ 5 and 6. type_timestamp{_explicit}.test   END

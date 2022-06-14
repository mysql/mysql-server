--echo #
--echo # Tests of time zone handling in conjunction with daylight savings, DST.
--echo # We currently use the CET time zone, which sets clock back one hour on
--echo # the last Sunday of October, and sets it forward one hour on the last
--echo # Sunday of March.
--echo #
--echo # Only times before 2021 are valid testing material, as The European
--echo # Parliament's Transport and Tourism Committee has decided to end the
--echo # seasonal clock change that year.
--echo #
--echo # See https://en.wikipedia.org/wiki/Summer_time_in_Europe.
--echo #

SET time_zone = 'CET';

CREATE TABLE ts1 ( a TIMESTAMP );
CREATE TABLE dt1 ( a DATETIME );

CREATE TABLE ts2 ( a TIMESTAMP );
CREATE TABLE dt2 ( a DATETIME );

CREATE TABLE ts3 ( a TIMESTAMP );
CREATE TABLE dt3 ( a DATETIME );

CREATE TABLE ts4 ( a TIMESTAMP );
CREATE TABLE dt4 ( a DATETIME );

--echo # Daylight savings overlap, which occurs when clocks are set back one
--echo # hour during the night of e.g. 2018-10-28, as happens for CET.
--echo # One second after local time 02:59:59, the new local time is 02:00:00.
--echo #
--echo # The times here are in UTC (i.e. +00:00,) and can be mapped straight to
--echo # CET, or CEST. 01:00 UTC corresponds to 03:00 CEST, but at this point
--echo # in CEST time the clocks are set back and hence we find ourselves at
--echo # 02:00 CET. Times after this show only a one hour displacement until
--echo # 2019-03-31, the last Sunday of March.

INSERT INTO ts1 VALUES ('2018-10-28 00:30:00+00:00'),
                       ('2018-10-28 00:59:00+00:00'),
                       ('2018-10-28 01:00:00+00:00'),
                       ('2018-10-28 01:30:00+00:00');

SELECT * FROM ts1;

--echo # Repeated for DATETIME.

INSERT INTO dt1 VALUES ('2018-10-28 00:30:00+00:00'),
                       ('2018-10-28 00:59:00+00:00'),
                       ('2018-10-28 01:00:00+00:00'),
                       ('2018-10-28 01:30:00+00:00');

SELECT * FROM dt1;

--echo # Same test, but with an initial displacement from UTC in the hypothetical
--echo # time zone with a displacement of 12:34 hours. This still maps
--echo # one-to-one to the CEST times 13:56, 14:25, 14:26 and 14:56 on the
--echo # preceding day.

INSERT INTO ts2 VALUES ('2018-10-28 00:30:00+12:34'),
                       ('2018-10-28 00:59:00+12:34'),
                       ('2018-10-28 01:00:00+12:34'),
                       ('2018-10-28 01:30:00+12:34');

SELECT * FROM ts2;

--echo # Repeated for DATETIME.

INSERT INTO dt2 VALUES ('2018-10-28 00:30:00+12:34'),
                       ('2018-10-28 00:59:00+12:34'),
                       ('2018-10-28 01:00:00+12:34'),
                       ('2018-10-28 01:30:00+12:34');

SELECT * FROM dt2;

--echo # Finally, a test with displaced times where the corresponding UTC time
--echo # is right around the daylight savings shift in CET.

INSERT INTO ts3 VALUES ('2018-10-27 23:06:00-01:24'),
                       ('2018-10-27 23:06:00-01:53'),
                       ('2018-10-27 23:06:00-01:54'),
                       ('2018-10-27 23:06:00-02:24');

SELECT * FROM ts3;

--echo # Repeated for DATETIME.

INSERT INTO dt3 VALUES ('2018-10-27 23:06:00-01:24'),
                       ('2018-10-27 23:06:00-01:53'),
                       ('2018-10-27 23:06:00-01:54'),
                       ('2018-10-27 23:06:00-02:24');

SELECT * FROM dt3;

--echo #
--echo # Daylight savings gap, occurs when clocks are set forwards one hour at
--echo # 02:00. After 01:59, the next minute the clock is at 03:00.
--echo #

INSERT INTO ts4 VALUES ('2019-03-31 00:30:00+00:00'),
                       ('2019-03-31 00:59:00+00:00'),
                       ('2019-03-31 01:00:00+00:00'),
                       ('2019-03-31 01:30:00+00:00');

SELECT * FROM ts4;

--echo # Repeated for DATETIME.

INSERT INTO dt4 VALUES ('2019-03-31 00:30:00+00:00'),
                       ('2019-03-31 00:59:00+00:00'),
                       ('2019-03-31 01:00:00+00:00'),
                       ('2019-03-31 01:30:00+00:00');

SELECT * FROM dt4;

DROP TABLE ts1, dt1, ts2, dt2, ts3, dt3, ts4, dt4;

SET time_zone = DEFAULT;

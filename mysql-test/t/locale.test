
--disable_warnings
DROP TABLE IF EXISTS t1;
--enable_warnings

--echo Start of 5.4 tests

--echo #
--echo # WL#4642 Greek locale for DAYNAME, MONTHNAME, DATE_FORMAT
--echo #

SET NAMES utf8mb3;

SET @@lc_time_names=109;
SELECT @@lc_time_names;

CREATE TABLE t1 (a DATE);
INSERT INTO t1 VALUES
('2006-01-01'),('2006-01-02'),('2006-01-03'),
('2006-01-04'),('2006-01-05'),('2006-01-06'),('2006-01-07');
SELECT a, date_format(a,'%a') as abday, dayname(a) as day FROM t1 ORDER BY a;
DROP TABLE t1;

CREATE TABLE t1 (a DATE);
INSERT INTO t1 VALUES
('2006-01-01'),('2006-02-01'),('2006-03-01'),
('2006-04-01'),('2006-05-01'),('2006-06-01'),
('2006-07-01'),('2006-08-01'),('2006-09-01'),
('2006-10-01'),('2006-11-01'),('2006-12-01');
SELECT a, date_format(a,'%b') as abmon, monthname(a) as mon FROM t1 ORDER BY a;

SELECT format(123456.789, 3, 'el_GR');
DROP TABLE t1;

--echo #
--echo # Bug#46633 Obsolete Serbian locale name
--echo #
SET lc_messages=sr_YU;
SHOW VARIABLES LIKE 'lc_messages';
SET lc_messages=sr_RS;
SHOW VARIABLES LIKE 'lc_messages';
SET lc_time_names=sr_RS;
SELECT format(123456.789, 3, 'sr_RS');

--echo #
--echo # Bug#43207 wrong LC_TIME names for romanian locale
--echo #
SET NAMES utf8mb3;
SET lc_time_names=ro_RO;
SELECT DATE_FORMAT('2001-01-01', '%w %a %W');
SELECT DATE_FORMAT('2001-01-02', '%w %a %W');
SELECT DATE_FORMAT('2001-01-03', '%w %a %W');
SELECT DATE_FORMAT('2001-01-04', '%w %a %W');
SELECT DATE_FORMAT('2001-01-05', '%w %a %W');
SELECT DATE_FORMAT('2001-01-06', '%w %a %W');
SELECT DATE_FORMAT('2001-01-07', '%w %a %W');
--echo End of 5.4 tests


--echo #
--echo # Start of 5.6 tests
--echo #

--echo #
--echo # WL#5303 Romansh locale for DAYNAME, MONTHNAME, DATE_FORMAT
--echo #

SET NAMES utf8mb3;
SET @old_50915_lc_time_names := @@lc_time_names;
SET lc_time_names=en_US;
SELECT DATE_FORMAT('2001-01-01', '%w %a %W');
SELECT DATE_FORMAT('2001-03-01', '%c %b %M');
SET lc_time_names=rm_CH;
SELECT DATE_FORMAT('2001-01-01', '%w %a %W');
SELECT DATE_FORMAT('2001-01-02', '%w %a %W');
SELECT DATE_FORMAT('2001-01-03', '%w %a %W');
SELECT DATE_FORMAT('2001-01-04', '%w %a %W');
SELECT DATE_FORMAT('2001-01-05', '%w %a %W');
SELECT DATE_FORMAT('2001-01-06', '%w %a %W');
SELECT DATE_FORMAT('2001-01-07', '%w %a %W');
SELECT DATE_FORMAT('2001-01-01', '%c %b %M');
SELECT DATE_FORMAT('2001-02-01', '%c %b %M');
SELECT DATE_FORMAT('2001-03-01', '%c %b %M');
SELECT DATE_FORMAT('2001-04-01', '%c %b %M');
SELECT DATE_FORMAT('2001-05-01', '%c %b %M');
SELECT DATE_FORMAT('2001-06-01', '%c %b %M');
SELECT DATE_FORMAT('2001-07-01', '%c %b %M');
SELECT DATE_FORMAT('2001-08-01', '%c %b %M');
SELECT DATE_FORMAT('2001-09-01', '%c %b %M');
SELECT DATE_FORMAT('2001-10-01', '%c %b %M');
SELECT DATE_FORMAT('2001-11-01', '%c %b %M');
SELECT DATE_FORMAT('2001-12-01', '%c %b %M');
SET lc_time_names=de_CH;
SELECT DATE_FORMAT('2001-01-06', '%w %a %W');
SELECT DATE_FORMAT('2001-09-01', '%c %b %M');

# Checking AM/PM
SELECT DATE_FORMAT('2010-03-23 11:00:00','%h %p');
SELECT DATE_FORMAT('2010-03-23 13:00:00','%h %p');

# Checking numeric format
SELECT format(123456789,2,'rm_CH');

# Checking that error messages point to en_US.
SET lc_messages=rm_CH;
--error ER_NO_SUCH_TABLE
SELECT * FROM non_existent;

SET lc_time_names=@old_50915_lc_time_names;


--echo #
--echo # End of 5.6 tests
--echo #

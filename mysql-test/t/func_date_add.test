#
# Bug #10627: Invalid date turned to NULL from date_sub/date_add in
# traditional mode
#
set sql_mode='traditional';
create table t1 (d date);
--error S22008
insert into t1 (d) select date_sub('2000-01-01', INTERVAL 2001 YEAR);
--error S22008
insert into t1 (d) select date_add('2000-01-01',interval 8000 year);
# No warnings/errors from the next two
insert into t1 values (date_add(NULL, INTERVAL 1 DAY));
insert into t1 values (date_add('2000-01-04', INTERVAL NULL DAY));
set sql_mode='';
# These will all work now, and we'll end up with some NULL entries in the
# table and some warnings.
insert into t1 (d) select date_sub('2000-01-01', INTERVAL 2001 YEAR);
insert into t1 (d) select date_add('2000-01-01',interval 8000 year);
insert into t1 values (date_add(NULL, INTERVAL 1 DAY));
insert into t1 values (date_add('2000-01-04', INTERVAL NULL DAY));
select * from t1;
drop table t1;

--echo End of 4.1 tests

#
# Bug#21811
#
# Make sure we end up with an appropriate
# date format (DATE) after addition operation
#
SELECT CAST('2006-09-26' AS DATE) + INTERVAL 1 DAY;
SELECT CAST('2006-09-26' AS DATE) + INTERVAL 1 MONTH;
SELECT CAST('2006-09-26' AS DATE) + INTERVAL 1 YEAR;
SELECT CAST('2006-09-26' AS DATE) + INTERVAL 1 WEEK;

#
# Bug#28450: The Item_date_add_interval in select list may fail the field 
#            type assertion.
#
create table t1 (a int, b varchar(10));
insert into t1 values (1, '2001-01-01'),(2, '2002-02-02'); 
select '2007-01-01' + interval a day from t1;
select b + interval a day from t1;
drop table t1;

--echo End of 5.0 tests

--echo #
--echo # Bug #27004806: UBSAN: ITEM_DATE_ADD_INTERVAL - NEGATION OF XYZ AND SIGNED INTEGER OVERFLOW
--echo #
SELECT ADDDATE('8112-06-20', REPEAT('1', 32));

--echo # Bug#32915973: Prepare DATE_ADD/DATE_SUB incorrectly returns DATETIME

# Document resolver actions of various ADDDATE parameters

SELECT ADDDATE(DATE'2021-01-01', INTERVAL 1 DAY);
SELECT ADDDATE(DATE'2021-01-01', INTERVAL 1 HOUR);
SELECT ADDDATE(TIMESTAMP'2021-01-01 00:00:00', INTERVAL 1 DAY);
SELECT ADDDATE(TIMESTAMP'2021-01-01 00:00:00', INTERVAL 1 HOUR);
# Will add current date to the TIME value:
SELECT DATE(ts) = CURRENT_DATE + INTERVAL '1' DAY AS is_tomorrow, TIME(ts)
FROM (SELECT ADDDATE(TIME'00:00:00', INTERVAL 1 DAY) AS ts) AS dt;
SELECT ADDDATE(TIME'00:00:00', INTERVAL 1 HOUR);
SELECT ADDDATE('2021-01-01', INTERVAL 1 DAY);
SELECT ADDDATE('2021-01-01', INTERVAL 1 HOUR);
SELECT ADDDATE('2021-01-01 00:00:00', INTERVAL 1 DAY);
SELECT ADDDATE('2021-01-01 00:00:00', INTERVAL 1 HOUR);
SELECT ADDDATE('00:00:00', INTERVAL 1 DAY);
SELECT ADDDATE('00:00:00', INTERVAL 1 HOUR);

set @d='2021-01-01';
set @t='00:00:00';
set @ts='2021-01-01 00:00:00';

prepare sd from "SELECT ADDDATE(?, INTERVAL 1 DAY)";
execute sd using @d;
prepare st from "SELECT ADDDATE(?, INTERVAL 1 HOUR)";
execute st using @d;
prepare sd from "SELECT ADDDATE(?, INTERVAL 1 DAY)";
execute sd using @ts;
prepare st from "SELECT ADDDATE(?, INTERVAL 1 HOUR)";
execute st using @ts;
prepare sd from "SELECT ADDDATE(?, INTERVAL 1 DAY)";
execute sd using @t;
prepare st from "SELECT ADDDATE(?, INTERVAL 1 HOUR)";
execute st using @t;

# Document resolver actions of various ADDTIME parameters

SELECT ADDTIME(DATE'2021-01-01', '01:01:01');
SELECT ADDTIME(TIMESTAMP'2021-01-01 00:00:00', TIME'01:01:01');
SELECT ADDTIME(TIME'00:00:00', TIME'01:01:01');
SELECT ADDTIME('2021-01-01', '01:01:01');
SELECT ADDTIME('2021-01-01 00:00:00', TIME'01:01:01');
SELECT ADDTIME('00:00:00', TIME'01:01:01');

prepare s from "SELECT ADDTIME(?, TIME'01:01:01')";
execute s using @d;
prepare s from "SELECT ADDTIME(?, TIME'01:01:01')";
execute s using @ts;
prepare s from "SELECT ADDTIME(?, TIME'01:01:01')";
execute s using @t;
#
# Bug#37818573: assertion data_type() != mysql_type_datetime ||
#               data_type() != mysql_type_string failed
CREATE TABLE t1(t TIME, d DATE);
INSERT INTO t1 VALUES(TIME'11:59:59', DATE'2025-04-11');

SELECT COALESCE(ADDTIME(t, DATE(d)), '00:00:00') AS c1 FROM t1;

DROP TABLE t1;

--echo #
--echo # Bug#37864407: Assertion mt.time_type == MYSQL_TIMESTAMP_DATETIME
--echo #

SELECT TIME(ADDTIME(241, 241));

--echo #
--echo # Bug#37946872: Assertion 'null_value' failed at sql/item.cc
--echo #

SELECT ADDTIME(TIME'00:20:08',
               LEAST(DATEDIFF(TIMESTAMP'2004-07-15 19:35:20.060', '2005-07-21'),
                     '0000-00-00 00:00:00', TIME'00:20:08'
                    )
              ) AS c1;

--echo #
--echo # Bug#37953309: Assertion 'date.time_type == time.time.type' failed
--echo #

SELECT ADDTIME(FROM_UNIXTIME(2985951232, "%y-%r-%i-%b-%x"), DATE('0000-00-00'));

--echo #
--echo # Bug#38487675: Assertion failed: is_valid() && iv.year == 0 &&
--echo #               iv.month == 0 && iv.day == 0
--echo #

SET TIMESTAMP = UNIX_TIMESTAMP('2025-10-11 00:00:00');

SELECT INTERVAL '{"a":[1,2,3,4,5]}' DAY_MICROSECOND + TIME'11:22:33';

SET TIMESTAMP = DEFAULT;

--echo #
--echo # Bug#38487373: Assertion failed: time.time_type == MYSQL_TIMESTAMP_TIME
--echo #

SET TIMESTAMP = UNIX_TIMESTAMP('2025-10-11 00:00:00');

DO ADDTIME(TIME_FORMAT('-250:07:38.935647', '1974-10-18 08:57:35.681107'), 1);

SET TIMESTAMP = DEFAULT;

--echo #
--echo # Bug#38177766: ADDDATE with date in year zero yields incorrect result
--echo #

set @d='0000-01-01';

prepare sd from "SELECT ADDDATE(?, INTERVAL 1 DAY)";
execute sd using @d;
deallocate prepare sd;

--echo #
--echo # Bug#39027029: Assert in Date_val::day_number with DATEDIFF
--echo #

SELECT DATEDIFF(CURRENT_TIMESTAMP -
                    INTERVAL(TO_SECONDS(CURRENT_TIMESTAMP)) DAY_SECOND,
                DATE'1000-01-01');

--disable_warnings
SELECT DATEDIFF(SUBDATE(CURRENT_TIMESTAMP, TO_DAYS(CURRENT_TIMESTAMP)),
                DATE'1000-01-01');
--enable_warnings

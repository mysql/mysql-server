
--disable_warnings
drop table if exists t1, test;
--enable_warnings


#
# time functions
#
select extract(DAY_MICROSECOND FROM "1999-01-02 10:11:12.000123");
select extract(HOUR_MICROSECOND FROM "1999-01-02 10:11:12.000123");
select extract(MINUTE_MICROSECOND FROM "1999-01-02 10:11:12.000123");
select extract(SECOND_MICROSECOND FROM "1999-01-02 10:11:12.000123");
select extract(MICROSECOND FROM "1999-01-02 10:11:12.000123");
select date_format("1997-12-31 23:59:59.000002", "%f");

select date_add("1997-12-31 23:59:59.000002",INTERVAL "10000 99:99:99.999999" DAY_MICROSECOND);
select date_add("1997-12-31 23:59:59.000002",INTERVAL "10000:99:99.999999" HOUR_MICROSECOND);
select date_add("1997-12-31 23:59:59.000002",INTERVAL "10000:99.999999" MINUTE_MICROSECOND);
select date_add("1997-12-31 23:59:59.000002",INTERVAL "10000.999999" SECOND_MICROSECOND);
select date_add("1997-12-31 23:59:59.000002",INTERVAL "999999" MICROSECOND);

select date_sub("1998-01-01 00:00:00.000001",INTERVAL "1 1:1:1.000002" DAY_MICROSECOND);
select date_sub("1998-01-01 00:00:00.000001",INTERVAL "1:1:1.000002" HOUR_MICROSECOND);
select date_sub("1998-01-01 00:00:00.000001",INTERVAL "1:1.000002" MINUTE_MICROSECOND);
select date_sub("1998-01-01 00:00:00.000001",INTERVAL "1.000002" SECOND_MICROSECOND);
select date_sub("1998-01-01 00:00:00.000001",INTERVAL "000002" MICROSECOND);

#Date functions
select adddate("1997-12-31 23:59:59.000001", 10);
select subdate("1997-12-31 23:59:59.000001", 10);

select datediff("1997-12-31 23:59:59.000001","1997-12-30");
select datediff("1997-11-30 23:59:59.000001","1997-12-31");
SET @@SQL_MODE="ALLOW_INVALID_DATES";
select datediff("1997-11-31 23:59:59.000001","1997-12-31");
SET @@SQL_MODE="";

# This will give a warning
select datediff("1997-11-31 23:59:59.000001","1997-12-31");
select datediff("1997-11-30 23:59:59.000001",null);

select weekofyear("1997-11-30 23:59:59.000001");

select makedate(03,1);
select makedate('0003',1);
select makedate(1997,1);
select makedate(1997,0);
select makedate(9999,365);
select makedate(9999,366);
select makedate(100,1);

#Time functions

select addtime("1997-12-31 23:59:59.999999", "1 1:1:1.000002");
select subtime("1997-12-31 23:59:59.000001", "1 1:1:1.000002");
select addtime("1997-12-31 23:59:59.999999", "1998-01-01 01:01:01.999999");
select subtime("1997-12-31 23:59:59.999999", "1998-01-01 01:01:01.999999");
select subtime("01:00:00.999999", "02:00:00.999998");
select subtime("02:01:01.999999", "01:01:01.999999");

select timediff("1997-01-01 23:59:59.000001","1995-12-31 23:59:59.000002");
select timediff("1997-12-31 23:59:59.000001","1997-12-30 01:01:01.000002");
select timediff("1997-12-30 23:59:59.000001","1997-12-31 23:59:59.000002");
select timediff("1997-12-31 23:59:59.000001","23:59:59.000001");
select timediff("2000:01:01 00:00:00", "2000:01:01 00:00:00.000001");
select timediff("2005-01-11 15:48:49.999999", "2005-01-11 15:48:50");

select maketime(10,11,12);
select maketime(25,11,12);
select maketime(-25,11,12);

# Extraction functions

select timestamp("2001-12-01", "01:01:01.999999");
select timestamp("2001-13-01", "01:01:01.000001");
select timestamp("2001-12-01", "25:01:01");
select timestamp("2001-12-01 01:01:01.000100");
select timestamp("2001-12-01");
select day("1997-12-31 23:59:59.000001");
select date("1997-12-31 23:59:59.000001");
select date("1997-13-31 23:59:59.000001");
select time("1997-12-31 23:59:59.000001");
select time("1997-12-31 25:59:59.000001");
select microsecond("1997-12-31 23:59:59.000001");

create table t1 
select makedate(1997,1) as f1,
   addtime(cast("1997-12-31 23:59:59.000001" as datetime), "1 1:1:1.000002") as f2,
   addtime(cast("23:59:59.999999" as time) , "1 1:1:1.000002") as f3,
   timediff("1997-12-31 23:59:59.000001","1997-12-30 01:01:01.000002") as f4,
   timediff("1997-12-30 23:59:59.000001","1997-12-31 23:59:59.000002") as f5,
   maketime(10,11,12) as f6,
   timestamp(cast("2001-12-01" as date), "01:01:01") as f7,
   date("1997-12-31 23:59:59.000001") as f8,
   time("1997-12-31 23:59:59.000001") as f9;
describe t1;
select * from t1;

create table test(t1 datetime, t2 time, t3 time, t4 datetime);
insert into test values 
('2001-01-01 01:01:01', '01:01:01', null, '2001-02-01 01:01:01'),
('2001-01-01 01:01:01', '-01:01:01', '-23:59:59', "1997-12-31 23:59:59.000001"),
('1997-12-31 23:59:59.000001', '-23:59:59', '-01:01:01', '2001-01-01 01:01:01'),
('2001-01-01 01:01:01', '01:01:01', '-1 01:01:01', null),
('2001-01-01 01:01:01', '-01:01:01', '1 01:01:01', '2001-01-01 01:01:01'),
('2001-01-01 01:01:01', null, '-1 01:01:01', null),
(null, null, null, null),
('2001-01-01 01:01:01', '01:01:01', '1 01:01:01', '2001-01-01 01:01:01');

SELECT ADDTIME(t1,t2) As ttt, ADDTIME(t2, t3) As qqq from test;
SELECT TIMEDIFF(t1, t4) As ttt, TIMEDIFF(t2, t3) As qqq,
       TIMEDIFF(t3, t2) As eee, TIMEDIFF(t2, t4) As rrr from test;

drop table t1, test;

select addtime("-01:01:01.01", "-23:59:59.1") as a;
select microsecond("1997-12-31 23:59:59.01") as a;
select microsecond(19971231235959.01) as a;
select date_add("1997-12-31",INTERVAL "10.09" SECOND_MICROSECOND) as a;
select str_to_date("2003-01-02 10:11:12.0012", "%Y-%m-%d %H:%i:%S.%f");

# End of 4.1 tests



#
# Bug#37553: MySql Error Compare TimeDiff & Time
#

# calculations involving negative time values ignored sign
select timediff('2008-09-29 20:10:10','2008-09-30 20:10:10'),time('00:00:00');
select timediff('2008-09-29 20:10:10','2008-09-30 20:10:10')>time('00:00:00');
select timediff('2008-09-29 20:10:10','2008-09-30 20:10:10')<time('00:00:00');

# show that conversion to DECIMAL no longer drops sign
SELECT CAST(time('-73:42:12') AS DECIMAL);



#
# Bug#42525 - TIMEDIFF function
#

SELECT TIMEDIFF(TIME('17:00:00'),TIME('17:00:00'))=TIME('00:00:00') AS 1Eq,
       TIMEDIFF(TIME('17:59:00'),TIME('17:00:00'))=TIME('00:00:00') AS 1NEq1,
       TIMEDIFF(TIME('18:00:00'),TIME('17:00:00'))=TIME('00:00:00') AS 1NEq2,
       TIMEDIFF(TIME('17:00:00'),TIME('17:00:00'))=     '00:00:00'  AS 2Eq,
       TIMEDIFF(TIME('17:59:00'),TIME('17:00:00'))=     '00:00:00'  AS 2NEq1,
       TIMEDIFF(TIME('18:00:00'),TIME('17:00:00'))=     '00:00:00'  AS 2NEq2,
       TIMEDIFF(TIME('17:00:00'),TIME('17:00:00'))=TIME(0)          AS 3Eq,
       TIMEDIFF(TIME('17:59:00'),TIME('17:00:00'))=TIME(0)          AS 3NEq1,
       TIMEDIFF(TIME('18:00:00'),TIME('17:00:00'))=TIME(0)          AS 3NEq2,
       TIME(0) AS Time0, TIME('00:00:00') AS Time00, '00:00:00' AS Literal0000,
       TIMEDIFF(TIME('17:59:00'),TIME('17:00:00')),
       TIMEDIFF(TIME('17:00:00'),TIME('17:59:00'));

#
# Bug#42661 - sec_to_time() and signedness
#

SELECT sec_to_time(3020399)=TIME('838:59:59');
SELECT sec_to_time(-3020399)=TIME('-838:59:59');
SELECT sec_to_time(-3020399)='-838:59:59';
SELECT time(sec_to_time(-3020399))=TIME('-838:59:59');
SELECT time(sec_to_time(-3020399))=TIME('-838:59:58');

#
# Bug#42662 - maketime() and signedness
#

# TIME(...) and CAST(... AS TIME) go through the same code-path here,
# but we'll explicitly show show that both work in case the ever changes.
SELECT maketime(-1,0,1)='-01:00:01';
SELECT TIME(maketime(-1,0,1))=CAST('-01:00:01' AS TIME);
SELECT maketime(-1,0,1)=CAST('-01:00:01' AS TIME);
SELECT maketime(1,0,1)=CAST('01:00:01' AS TIME);
SELECT maketime(1,0,1)=CAST('01:00:02' AS TIME);

# End of 5.0 tests

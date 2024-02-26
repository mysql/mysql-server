DROP TABLE IF EXISTS t1;
Start of 5.4 tests
#
# WL#4642 Greek locale for DAYNAME, MONTHNAME, DATE_FORMAT
#
SET NAMES utf8;
Warnings:
Warning	3719	'utf8' is currently an alias for the character set UTF8MB3, but will be an alias for UTF8MB4 in a future release. Please consider using UTF8MB4 in order to be unambiguous.
SET @@lc_time_names=109;
SELECT @@lc_time_names;
@@lc_time_names
el_GR
CREATE TABLE t1 (a DATE);
INSERT INTO t1 VALUES
('2006-01-01'),('2006-01-02'),('2006-01-03'),
('2006-01-04'),('2006-01-05'),('2006-01-06'),('2006-01-07');
SELECT a, date_format(a,'%a') as abday, dayname(a) as day FROM t1 ORDER BY a;
a	abday	day
2006-01-01	Κυρ	Κυριακή
2006-01-02	Δευ	Δευτέρα
2006-01-03	Τρί	Τρίτη
2006-01-04	Τετ	Τετάρτη
2006-01-05	Πέμ	Πέμπτη
2006-01-06	Παρ	Παρασκευή
2006-01-07	Σάβ	Σάββατο
DROP TABLE t1;
CREATE TABLE t1 (a DATE);
INSERT INTO t1 VALUES
('2006-01-01'),('2006-02-01'),('2006-03-01'),
('2006-04-01'),('2006-05-01'),('2006-06-01'),
('2006-07-01'),('2006-08-01'),('2006-09-01'),
('2006-10-01'),('2006-11-01'),('2006-12-01');
SELECT a, date_format(a,'%b') as abmon, monthname(a) as mon FROM t1 ORDER BY a;
a	abmon	mon
2006-01-01	Ιαν	Ιανουάριος
2006-02-01	Φεβ	Φεβρουάριος
2006-03-01	Μάρ	Μάρτιος
2006-04-01	Απρ	Απρίλιος
2006-05-01	Μάι	Μάιος
2006-06-01	Ιούν	Ιούνιος
2006-07-01	Ιούλ	Ιούλιος
2006-08-01	Αύγ	Αύγουστος
2006-09-01	Σεπ	Σεπτέμβριος
2006-10-01	Οκτ	Οκτώβριος
2006-11-01	Νοέ	Νοέμβριος
2006-12-01	Δεκ	Δεκέμβριος
SELECT format(123456.789, 3, 'el_GR');
format(123456.789, 3, 'el_GR')
123456,789
DROP TABLE t1;
#
# Bug#46633 Obsolete Serbian locale name
#
SET lc_messages=sr_YU;
Warnings:
Warning	1287	'sr_YU' is deprecated and will be removed in a future release. Please use sr_RS instead
SHOW VARIABLES LIKE 'lc_messages';
Variable_name	Value
lc_messages	sr_RS
SET lc_messages=sr_RS;
SHOW VARIABLES LIKE 'lc_messages';
Variable_name	Value
lc_messages	sr_RS
SET lc_time_names=sr_RS;
SELECT format(123456.789, 3, 'sr_RS');
format(123456.789, 3, 'sr_RS')
123456.789
#
# Bug#43207 wrong LC_TIME names for romanian locale
#
SET NAMES utf8;
Warnings:
Warning	3719	'utf8' is currently an alias for the character set UTF8MB3, but will be an alias for UTF8MB4 in a future release. Please consider using UTF8MB4 in order to be unambiguous.
SET lc_time_names=ro_RO;
SELECT DATE_FORMAT('2001-01-01', '%w %a %W');
DATE_FORMAT('2001-01-01', '%w %a %W')
1 Lu Luni
SELECT DATE_FORMAT('2001-01-02', '%w %a %W');
DATE_FORMAT('2001-01-02', '%w %a %W')
2 Ma Marţi
SELECT DATE_FORMAT('2001-01-03', '%w %a %W');
DATE_FORMAT('2001-01-03', '%w %a %W')
3 Mi Miercuri
SELECT DATE_FORMAT('2001-01-04', '%w %a %W');
DATE_FORMAT('2001-01-04', '%w %a %W')
4 Jo Joi
SELECT DATE_FORMAT('2001-01-05', '%w %a %W');
DATE_FORMAT('2001-01-05', '%w %a %W')
5 Vi Vineri
SELECT DATE_FORMAT('2001-01-06', '%w %a %W');
DATE_FORMAT('2001-01-06', '%w %a %W')
6 Sâ Sâmbătă
SELECT DATE_FORMAT('2001-01-07', '%w %a %W');
DATE_FORMAT('2001-01-07', '%w %a %W')
0 Du Duminică
End of 5.4 tests
#
# Start of 5.6 tests
#
#
# WL#5303 Romansh locale for DAYNAME, MONTHNAME, DATE_FORMAT
#
SET NAMES utf8;
Warnings:
Warning	3719	'utf8' is currently an alias for the character set UTF8MB3, but will be an alias for UTF8MB4 in a future release. Please consider using UTF8MB4 in order to be unambiguous.
SET @old_50915_lc_time_names := @@lc_time_names;
SET lc_time_names=en_US;
SELECT DATE_FORMAT('2001-01-01', '%w %a %W');
DATE_FORMAT('2001-01-01', '%w %a %W')
1 Mon Monday
SELECT DATE_FORMAT('2001-03-01', '%c %b %M');
DATE_FORMAT('2001-03-01', '%c %b %M')
3 Mar March
SET lc_time_names=rm_CH;
SELECT DATE_FORMAT('2001-01-01', '%w %a %W');
DATE_FORMAT('2001-01-01', '%w %a %W')
1 gli glindesdi
SELECT DATE_FORMAT('2001-01-02', '%w %a %W');
DATE_FORMAT('2001-01-02', '%w %a %W')
2 ma mardi
SELECT DATE_FORMAT('2001-01-03', '%w %a %W');
DATE_FORMAT('2001-01-03', '%w %a %W')
3 me mesemna
SELECT DATE_FORMAT('2001-01-04', '%w %a %W');
DATE_FORMAT('2001-01-04', '%w %a %W')
4 gie gievgia
SELECT DATE_FORMAT('2001-01-05', '%w %a %W');
DATE_FORMAT('2001-01-05', '%w %a %W')
5 ve venderdi
SELECT DATE_FORMAT('2001-01-06', '%w %a %W');
DATE_FORMAT('2001-01-06', '%w %a %W')
6 so sonda
SELECT DATE_FORMAT('2001-01-07', '%w %a %W');
DATE_FORMAT('2001-01-07', '%w %a %W')
0 du dumengia
SELECT DATE_FORMAT('2001-01-01', '%c %b %M');
DATE_FORMAT('2001-01-01', '%c %b %M')
1 schan schaner
SELECT DATE_FORMAT('2001-02-01', '%c %b %M');
DATE_FORMAT('2001-02-01', '%c %b %M')
2 favr favrer
SELECT DATE_FORMAT('2001-03-01', '%c %b %M');
DATE_FORMAT('2001-03-01', '%c %b %M')
3 mars mars
SELECT DATE_FORMAT('2001-04-01', '%c %b %M');
DATE_FORMAT('2001-04-01', '%c %b %M')
4 avr avrigl
SELECT DATE_FORMAT('2001-05-01', '%c %b %M');
DATE_FORMAT('2001-05-01', '%c %b %M')
5 matg matg
SELECT DATE_FORMAT('2001-06-01', '%c %b %M');
DATE_FORMAT('2001-06-01', '%c %b %M')
6 zercl zercladur
SELECT DATE_FORMAT('2001-07-01', '%c %b %M');
DATE_FORMAT('2001-07-01', '%c %b %M')
7 fan fanadur
SELECT DATE_FORMAT('2001-08-01', '%c %b %M');
DATE_FORMAT('2001-08-01', '%c %b %M')
8 avust avust
SELECT DATE_FORMAT('2001-09-01', '%c %b %M');
DATE_FORMAT('2001-09-01', '%c %b %M')
9 sett settember
SELECT DATE_FORMAT('2001-10-01', '%c %b %M');
DATE_FORMAT('2001-10-01', '%c %b %M')
10 oct october
SELECT DATE_FORMAT('2001-11-01', '%c %b %M');
DATE_FORMAT('2001-11-01', '%c %b %M')
11 nov november
SELECT DATE_FORMAT('2001-12-01', '%c %b %M');
DATE_FORMAT('2001-12-01', '%c %b %M')
12 dec december
SET lc_time_names=de_CH;
SELECT DATE_FORMAT('2001-01-06', '%w %a %W');
DATE_FORMAT('2001-01-06', '%w %a %W')
6 Sa Samstag
SELECT DATE_FORMAT('2001-09-01', '%c %b %M');
DATE_FORMAT('2001-09-01', '%c %b %M')
9 Sep September
SELECT DATE_FORMAT('2010-03-23 11:00:00','%h %p');
DATE_FORMAT('2010-03-23 11:00:00','%h %p')
11 AM
SELECT DATE_FORMAT('2010-03-23 13:00:00','%h %p');
DATE_FORMAT('2010-03-23 13:00:00','%h %p')
01 PM
SELECT format(123456789,2,'rm_CH');
format(123456789,2,'rm_CH')
123'456'789,00
SET lc_messages=rm_CH;
SELECT * FROM non_existent;
ERROR 42S02: Table 'test.non_existent' doesn't exist
SET lc_time_names=@old_50915_lc_time_names;
#
# End of 5.6 tests
#

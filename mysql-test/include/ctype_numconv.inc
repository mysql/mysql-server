--echo #
--echo # Start of WL#2649 Number-to-string conversions
--echo #
#
# Basic constants
#
select hex(concat(1));
create table t1 as select concat(1) as c1;
show create table t1;
select hex(c1) from t1;
drop table t1;

select hex(concat(18446744073709551615));
create table t1 as select concat(18446744073709551615) as c1;
show create table t1;
select hex(c1) from t1;
drop table t1;

select hex(concat(1.1));
create table t1 as select concat(1.1) as c1;
show create table t1;
select hex(c1) from t1;
drop table t1;


#
# Arithmetic operators
#

select hex(concat('a', 1+2)), charset(concat(1+2));
create table t1 as select concat(1+2) as c1;
show create table t1;
drop table t1;

select hex(concat(1-2));
create table t1 as select concat(1-2) as c1;
show create table t1;
drop table t1;

select hex(concat(1*2));
create table t1 as select concat(1*2) as c1;
show create table t1;
drop table t1;

select hex(concat(1/2));
create table t1 as select concat(1/2) as c1;
show create table t1;
drop table t1;

select hex(concat(1 div 2));
create table t1 as select concat(1 div 2) as c1;
show create table t1;
drop table t1;

select hex(concat(1 % 2));
create table t1 as select concat(1 % 2) as c1;
show create table t1;
drop table t1;

select hex(concat(-1));
create table t1 as select concat(-1) as c1;
show create table t1;
drop table t1;

select hex(concat(-(1+2)));
create table t1 as select concat(-(1+2)) as c1;
show create table t1;
drop table t1;


#
# Bit functions
#

select hex(concat(1|2));
create table t1 as select concat(1|2) as c1;
show create table t1;
drop table t1;

select hex(concat(1&2));
create table t1 as select concat(1&2) as c1;
show create table t1;
drop table t1;

select hex(concat(bit_count(12)));
create table t1 as select concat(bit_count(12)) as c1;
show create table t1;
drop table t1;

select hex(concat(2<<1));
create table t1 as select concat(2<<1) as c1;
show create table t1;
drop table t1;

select hex(concat(2>>1));
create table t1 as select concat(2>>1) as c1;
show create table t1;
drop table t1;

select hex(concat(~0));
create table t1 as select concat(~0) as c1;
show create table t1;
drop table t1;

select hex(concat(3^2));
create table t1 as select concat(3^2) as c1;
show create table t1;
drop table t1;



#
# Math functions
#
# Note, some tests use LEFT(func(),1) to avoid
# non-deterministic results on various platforms.
#

select hex(concat(abs(-2)));
create table t1 as select concat(abs(-2)) as c1;
show create table t1;
drop table t1;

select hex(left(concat(exp(2)),1));
create table t1 as select concat(exp(2)) as c1;
show create table t1;
drop table t1;

select hex(left(concat(log(2)),1));
create table t1 as select concat(log(2)) as c1;
show create table t1;
drop table t1;

select hex(left(concat(log2(2)),1));
create table t1 as select concat(log2(2)) as c1;
show create table t1;
drop table t1;

select hex(left(concat(log10(2)),1));
create table t1 as select concat(log10(2)) as c1;
show create table t1;
drop table t1;

select hex(left(concat(sqrt(2)),1));
create table t1 as select concat(sqrt(2)) as c1;
show create table t1;
drop table t1;

select hex(left(concat(pow(2,2)),1));
create table t1 as select concat(pow(2,2)) as c1;
show create table t1;
drop table t1;

select hex(left(concat(acos(0.5)),1));
create table t1 as select concat(acos(0.5)) as c1;
show create table t1;
drop table t1;

select hex(left(concat(asin(0.5)),1));
create table t1 as select concat(asin(0.5)) as c1;
show create table t1;
drop table t1;

select hex(left(concat(atan(0.5)),1));
create table t1 as select concat(atan(0.5)) as c1;
show create table t1;
drop table t1;

select hex(left(concat(cos(0.5)),1));
create table t1 as select concat(cos(0.5)) as c1;
show create table t1;
drop table t1;

select hex(left(concat(sin(0.5)),1));
create table t1 as select concat(sin(0.5)) as c1;
show create table t1;
drop table t1;

select hex(left(concat(tan(0.5)),1));
create table t1 as select concat(tan(0.5)) as c1;
show create table t1;
drop table t1;

select hex(concat(degrees(0)));
create table t1 as select concat(degrees(0)) as c1;
show create table t1;
drop table t1;

select hex(concat(radians(0)));
create table t1 as select concat(radians(0)) as c1;
show create table t1;
drop table t1;

select hex(concat(ceiling(0.5)));
create table t1 as select concat(ceiling(0.5)) as c1;
show create table t1;
drop table t1;

select hex(concat(floor(0.5)));
create table t1 as select concat(floor(0.5)) as c1;
show create table t1;
drop table t1;

select hex(concat(round(0.5)));
create table t1 as select concat(round(0.5)) as c1;
show create table t1;
drop table t1;

select hex(concat(sign(0.5)));
create table t1 as select concat(sign(0.5)) as c1;
show create table t1;
drop table t1;

create table t1 as select concat(rand()) as c1;
show create table t1;
drop table t1;


#
# String functions
#

select hex(concat(length('a')));
create table t1 as select concat(length('a')) as c1;
show create table t1;
drop table t1;

select hex(concat(char_length('a')));
create table t1 as select concat(char_length('a')) as c1;
show create table t1;
drop table t1;

select hex(concat(bit_length('a')));
create table t1 as select concat(bit_length('a')) as c1;
show create table t1;
drop table t1;

select hex(concat(coercibility('a')));
create table t1 as select concat(coercibility('a')) as c1;
show create table t1;
drop table t1;

select hex(concat(locate('a','a')));
create table t1 as select concat(locate('a','a')) as c1;
show create table t1;
drop table t1;

select hex(concat(field('c','a','b','c')));
create table t1 as select concat(field('c','a','b','c')) as c1;
show create table t1;
drop table t1;

select hex(concat(ascii(61)));
create table t1 as select concat(ascii(61)) as c1;
show create table t1;
drop table t1;

select hex(concat(ord(61)));
create table t1 as select concat(ord(61)) as c1;
show create table t1;
drop table t1;

select hex(concat(find_in_set('b','a,b,c,d')));
create table t1 as select concat(find_in_set('b','a,b,c,d')) as c1;
show create table t1;
drop table t1;


#
# String hash functions
#

select md5('a'), hex(md5('a'));
create table t1 as select md5('a') as c1;
show create table t1;
drop table t1;

select sha('a'), hex(sha('a'));
create table t1 as select sha('a') as c1;
show create table t1;
drop table t1;

select sha1('a'), hex(sha1('a'));
create table t1 as select sha1('a') as c1;
show create table t1;
drop table t1;

#select sha2('a',224), hex(sha2('a',224));
#create table t1 as select sha2('a',224) as c1;
#show create table t1;
#drop table t1;



#
# CAST
#

select hex(concat(cast('-1' as signed)));
create table t1 as select concat(cast('-1' as signed)) as c1;
show create table t1;
drop table t1;

select hex(concat(cast('1' as unsigned)));
create table t1 as select concat(cast('1' as unsigned)) as c1;
show create table t1;
drop table t1;

select hex(concat(cast(1/2 as decimal(5,5))));
create table t1 as select concat(cast(1/2 as decimal(5,5))) as c1;
show create table t1;
drop table t1;

select hex(concat(cast('2001-01-02 03:04:05' as date)));
create table t1 as select concat(cast('2001-01-02 03:04:05' as date)) as c1;
show create table t1;
select * from t1;
drop table t1;

select hex(concat(cast('2001-01-02 03:04:05' as time)));
create table t1 as select concat(cast('2001-01-02 03:04:05' as time)) as c1;
show create table t1;
select * from t1;
drop table t1;

select hex(concat(cast('2001-01-02' as datetime)));
create table t1 as select concat(cast('2001-01-02' as datetime)) as c1;
show create table t1;
select * from t1;
drop table t1;


#
# Aggregation: LEAST, GREATEST
#
select hex(concat(least(1,2)));
create table t1 as select concat(least(1,2)) as c1;
show create table t1;
drop table t1;

select hex(concat(greatest(1,2)));
create table t1 as select concat(greatest(1,2)) as c1;
show create table t1;
drop table t1;


#
# Aggregation: CASE
#
select hex(concat(case when 11 then 22 else 33 end));
create table t1 as select concat(case when 11 then 22 else 33 end) as c1;
show create table t1;
drop table t1;


#
# Aggregation: COALESCE
#
select hex(concat(coalesce(1,2)));
create table t1 as select concat(coalesce(1,2)) as c1;
show create table t1;
drop table t1;


#
# Aggregation: CONCAT_WS, GROUP_CONCAT
#
select hex(concat_ws(1,2,3));
create table t1 as select concat_ws(1,2,3) as c1;
show create table t1;
drop table t1;

select hex(group_concat(1,2,3));
create table t1 as select group_concat(1,2,3) as c1;
show create table t1;
drop table t1;

#
#  Aggregation: UNION
#
create table t1 as select 1 as c1 union select 'a';
show create table t1;
select hex(c1) from t1 order by c1;
drop table t1;


#
# Miscelaneous functions
#

create table t1 as select concat(last_insert_id()) as c1;
show create table t1;
drop table t1;

select hex(concat(benchmark(0,0)));
create table t1 as select concat(benchmark(0,0)) as c1;
show create table t1;
drop table t1;

select hex(concat(sleep(0)));
create table t1 as select concat(sleep(0)) as c1;
show create table t1;
drop table t1;

# Fails with "mtr --ps-protocol" for some reasons.
#select hex(concat(get_lock('a',0)));
#select hex(concat(release_lock('a')));
#create table t1 as select concat(get_lock('a',0)) as c1;
#show create table t1;
#drop table t1;

select hex(concat(is_free_lock('xxxx')));
create table t1 as select concat(is_free_lock('xxxx')) as c1;
show create table t1;
drop table t1;

create table t1 as select concat(is_used_lock('a')) as c1;
show create table t1;
drop table t1;

create table t1 as select concat(release_lock('a')) as c1;
show create table t1;
drop table t1;

select hex(concat(crc32('')));
create table t1 as select concat(crc32('')) as c1;
show create table t1;
drop table t1;

select hex(concat(uncompressed_length('')));
create table t1 as select concat(uncompressed_length('')) as c1;
show create table t1;
drop table t1;

create table t1 as select concat(connection_id()) as c1;
show create table t1;
drop table t1;

select hex(concat(inet_aton('127.1.1.1')));
create table t1 as select concat(inet_aton('127.1.1.1')) as c1;
show create table t1;
drop table t1;

select hex(concat(inet_ntoa(2130772225)));
create table t1 as select concat(inet_ntoa(2130772225)) as c1;
select * from t1;
show create table t1;
drop table t1;

# Ensure that row_count() value is reset after drop table.
select 1;
select hex(concat(row_count()));
create table t1 as select concat(row_count()) as c1;
show create table t1;
drop table t1;

select hex(concat(found_rows()));
create table t1 as select concat(found_rows()) as c1;
show create table t1;
drop table t1;

create table t1 as select concat(uuid_short()) as c1;
show create table t1;
drop table t1;

create table t1 as select concat(uuid()) as c1;
show create table t1;
drop table t1;

#
# Make sure we can mix uuid() to a latin1 object
# with DERIVATION_IMPLICIT (and higher):
# (DERIVATION_COERCIBLE + MY_REPERTOIRE_ASCII allow to do so)
#
select coercibility(uuid()), coercibility(cast('a' as char character set latin1));
select charset(concat(uuid(), cast('a' as char character set latin1)));
create table t1 as select concat(uuid(), cast('a' as char character set latin1)) as c1;
show create table t1;
drop table t1;

create table t1 as select concat(master_pos_wait('non-existent',0,2)) as c1;
show create table t1;
drop table t1;


#
# User and system variable functions
#

# User variables: INT
select hex(concat(@a1:=1));
create table t1 as select concat(@a2:=2) as c1, @a3:=3 as c2;
select hex(c1) from t1;
show create table t1;
drop table t1;

set @a2=1;
select hex(concat(@a2));
create table t1 as select concat(@a2) as c1, @a2 as c2;
select hex(c1) from t1;
show create table t1;
drop table t1;

# User variables: REAL
select hex(concat(@a1:=sqrt(1)));
create table t1 as select concat(@a2:=sqrt(1)) as c1, @a3:=sqrt(1) as c2;
select hex(c1) from t1;
show create table t1;
drop table t1;

set @a2=sqrt(1);
select hex(concat(@a2));
create table t1 as select concat(@a2) as c1, @a2 as c2;
select hex(c1) from t1;
show create table t1;
drop table t1;

# User variables: DECIMAL
select hex(concat(@a1:=1.1));
create table t1 as select concat(@a2:=1.1) as c1, @a3:=1.1 as c2;
select hex(c1) from t1;
show create table t1;
drop table t1;

set @a2=1.1;
select hex(concat(@a2));
create table t1 as select concat(@a2) as c1, @a2 as c2;
select hex(c1) from t1;
show create table t1;
drop table t1;


select hex(concat(@@ft_max_word_len));
create table t1 as select concat(@@ft_max_word_len) as c1;
select hex(c1) from t1;
show create table t1;
drop table t1;

#
# Comparison functions
#

select hex(concat('a'='a' IS TRUE));
create table t1 as select concat('a'='a' IS TRUE) as c1;
show create table t1;
drop table t1;

select hex(concat('a'='a' IS NOT TRUE));
create table t1 as select concat('a'='a' IS NOT TRUE) as c1;
show create table t1;
drop table t1;

select hex(concat(NOT 'a'='a'));
create table t1 as select concat(NOT 'a'='a') as c1;
show create table t1;
drop table t1;

select hex(concat('a' IS NULL));
create table t1 as select concat('a' IS NULL) as c1;
show create table t1;
drop table t1;

select hex(concat('a' IS NOT NULL));
create table t1 as select concat('a' IS NOT NULL) as c1;
show create table t1;
drop table t1;

select hex(concat('a' rlike 'a'));
create table t1 as select concat('a' IS NOT NULL) as c1;
show create table t1;
drop table t1;

select hex(concat(strcmp('a','b')));
create table t1 as select concat(strcmp('a','b')) as c1;
show create table t1;
drop table t1;

select hex(concat('a' like 'a'));
create table t1 as select concat('a' like 'b') as c1;
show create table t1;
drop table t1;

select hex(concat('a' between 'b' and 'c'));
create table t1 as select concat('a' between 'b' and 'c') as c1;
show create table t1;
drop table t1;

select hex(concat('a' in ('a','b')));
create table t1 as select concat('a' in ('a','b')) as c1;
show create table t1;
drop table t1;

select hex(concat(interval(23, 1, 15, 17, 30, 44, 200)));
create table t1 as select concat(interval(23, 1, 15, 17, 30, 44, 200)) as c1;
show create table t1;
drop table t1;

create table t1 (a varchar(10), fulltext key(a));
insert into t1 values ('a');
select hex(concat(match (a) against ('a'))) from t1;
create table t2 as select concat(match (a) against ('a')) as a from t1;
show create table t2;
drop table t1, t2;

select hex(ifnull(1,'a'));
create table t1 as select ifnull(1,'a') as c1;
show create table t1;
drop table t1;

select hex(concat(ifnull(1,1)));
create table t1 as select concat(ifnull(1,1)) as c1;
show create table t1;
drop table t1;

select hex(concat(ifnull(1.1,1.1)));
create table t1 as select concat(ifnull(1.1,1.1)) as c1;
show create table t1;
drop table t1;

select hex(if(1,'b',1));
create table t1 as select if(1,'b',1) as c1;
show create table t1;
drop table t1;

select hex(if(1,1,'b'));
create table t1 as select if(1,1,'b') as c1;
show create table t1;
drop table t1;

select hex(concat(if(1,1,1)));
create table t1 as select concat(if(1,1,1)) as c1;
show create table t1;
drop table t1;

select hex(concat(nullif(1,2)));
create table t1 as select concat(nullif(1,2)) as c1;
show create table t1;
drop table t1;

#
# GIS functions
#

select hex(concat(ST_Dimension(ST_GeomFromText('LINESTRING(0 0,10 10)'))));
--error ER_GIS_INVALID_DATA
create table t1 as select concat(ST_Dimension(ST_GeomFromText('LINSTRING(0 0,10 10)'))) as c1;

select hex(concat(ST_NumGeometries(ST_MultiPointFromText('MULTIPOINT(0 0,10 10)'))));
create table t1 as select concat(ST_NumGeometries(ST_MultiPointFromText('MULTIPOINT(0 0,10 10)'))) as c1;
show create table t1;
drop table t1;

select hex(concat(ST_NumPoints(ST_LineFromText('LINESTRING(0 0,10 10)'))));
create table t1 as select concat(ST_NumPoints(ST_LineFromText('LINESTRING(0 0,10 10)'))) as c1;
show create table t1;
drop table t1;

select hex(concat(ST_SRID(ST_MultiPointFromText('MULTIPOINT(0 0,10 10)'))));
create table t1 as select concat(ST_SRID(ST_MultiPointFromText('MULTIPOINT(0 0,10 10)'))) as c1;
show create table t1;
drop table t1;

select hex(concat(ST_NumInteriorRings(ST_PolygonFromText('POLYGON((0 0,50 0,50 50,0 50,0 0), (10 10,20 10,20 20,10 20,10 10))'))));
create table t1 as select concat(ST_NumInteriorRings(ST_PolygonFromText('POLYGON((0 0,50 0,50 50,0 50,0 0), (10 10,20 10,20 20,10 20,10 10))'))) as c1;
show create table t1;
drop table t1;

select hex(concat(ST_IsEmpty(ST_GeomFromText('POINT(1 1)'))));
create table t1 as select concat(ST_IsEmpty(ST_GeomFromText('Point(1 1)'))) as c1;
show create table t1;
drop table t1;

select hex(concat(ST_IsSimple(ST_GeomFromText('POINT(1 1)'))));
create table t1 as select concat(ST_IsSimple(ST_GeomFromText('Point(1 1)'))) as c1;
show create table t1;
drop table t1;

select hex(concat(ST_IsClosed(ST_GeomFromText('LineString(1 1,2 2)'))));
create table t1 as select concat(ST_IsClosed(ST_GeomFromText('LineString(1 1,2 2)'))) as c1;
show create table t1;
drop table t1;

select hex(concat(MBREquals(ST_GeomFromText('Point(1 1)'),ST_GeomFromText('Point(1 1)'))));
create table t1 as select concat(MBREquals(ST_GeomFromText('Point(1 1)'),ST_GeomFromText('Point(1 1)'))) as c1;
drop table t1;

select hex(concat(ST_X(ST_GeomFromText('Point(1 2)'))));
create table t1 as select concat(ST_X(ST_GeomFromText('Point(1 2)'))) as c1;
show create table t1;
drop table t1;

select hex(concat(ST_Y(ST_GeomFromText('Point(1 2)'))));
create table t1 as select concat(ST_X(ST_GeomFromText('Point(1 2)'))) as c1;
show create table t1;
drop table t1;

select hex(concat(ST_Length(ST_GeomFromText('LineString(1 2,2 2)'))));
create table t1 as select concat(ST_Length(ST_GeomFromText('LineString(1 2, 2 2)'))) as c1;
show create table t1;
drop table t1;

select hex(concat(ST_Area(ST_GeomFromText('Polygon((0 0,1 0,1 1,0 1,0 0))'))));
create table t1 as select concat(ST_Area(ST_GeomFromText('Polygon((0 0,1 0,1 1,0 1,0 0))'))) as c1;
show create table t1;
drop table t1;

select hex(concat(ST_GeometryType(ST_GeomFromText('Point(1 2)'))));
create table t1 as select concat(ST_GeometryType(ST_GeomFromText('Point(1 2)'))) as c1;
show create table t1;
drop table t1;

select hex(concat(ST_AsText(ST_GeomFromText('Point(1 2)'))));
create table t1 as select concat(ST_AsText(ST_GeomFromText('Point(1 2)'))) as c1;
show create table t1;
drop table t1;



#
# Date/Time functions
#

select hex(concat(period_add(200902, 2)));
create table t1 as select concat(period_add(200902, 2)) as c1;
show create table t1;
drop table t1;
SET sql_mode = '';
select hex(concat(period_diff(200902, 200802)));
create table t1 as select concat(period_add(200902, 200802)) as c1;
show create table t1;
drop table t1;
SET sql_mode = default;
select hex(concat(to_days(20090224)));
create table t1 as select concat(to_days(20090224)) as c1;
show create table t1;
drop table t1;

select hex(concat(dayofmonth(20090224)));
create table t1 as select concat(dayofmonth(20090224)) as c1;
show create table t1;
drop table t1;

select hex(concat(dayofyear(20090224)));
create table t1 as select concat(dayofyear(20090224)) as c1;
show create table t1;
drop table t1;

select hex(concat(hour('10:11:12')));
create table t1 as select concat(hour('10:11:12')) as c1;
show create table t1;
drop table t1;

select hex(concat(minute('10:11:12')));
create table t1 as select concat(minute('10:11:12')) as c1;
show create table t1;
drop table t1;

select hex(concat(second('10:11:12')));
create table t1 as select concat(second('10:11:12')) as c1;
show create table t1;
drop table t1;

select hex(concat(quarter(20090224)));
create table t1 as select concat(quarter(20090224)) as c1;
show create table t1;
drop table t1;

select hex(concat(week(20090224)));
create table t1 as select concat(week(20090224)) as c1;
show create table t1;
drop table t1;

select hex(concat(yearweek(20090224)));
create table t1 as select concat(yearweek(20090224)) as c1;
show create table t1;
drop table t1;

select hex(concat(year(20090224)));
create table t1 as select concat(year(20090224)) as c1;
show create table t1;
drop table t1;

select hex(concat(weekday(20090224)));
create table t1 as select concat(weekday(20090224)) as c1;
show create table t1;
drop table t1;

select hex(concat(dayofweek(20090224)));
create table t1 as select concat(dayofweek(20090224)) as c1;
show create table t1;
drop table t1;

select hex(concat(unix_timestamp(20090224)));
create table t1 as select concat(unix_timestamp(20090224)) as c1;
show create table t1;
drop table t1;

select hex(concat(time_to_sec('10:11:12')));
create table t1 as select concat(time_to_sec('10:11:12')) as c1;
show create table t1;
drop table t1;

select hex(concat(extract(year from 20090702)));
create table t1 as select concat(extract(year from 20090702)) as c1;
show create table t1;
drop table t1;

select hex(concat(microsecond('12:00:00.123456')));
create table t1 as select concat(microsecond('12:00:00.123456')) as c1;
show create table t1;
drop table t1;

select hex(concat(month(20090224)));
create table t1 as select concat(month(20090224)) as c1;
show create table t1;
drop table t1;


create table t1 as select concat(last_day('2003-02-05')) as c1;
show create table t1;
select c1, hex(c1) from t1;
drop table t1;

create table t1 as select concat(from_days(730669)) as c1;
show create table t1;
select c1, hex(c1) from t1;
drop table t1;

create table t1 as select concat(curdate()) as c1;
show create table t1;
drop table t1;

create table t1 as select concat(utc_date()) as c1;
show create table t1;
drop table t1;

create table t1 as select concat(curtime()) as c1;
show create table t1;
drop table t1;

create table t1 as select repeat('a',20) as c1 limit 0;
set timestamp=1216359724;
insert into t1 values (current_date);
insert into t1 values (current_time);
select c1, hex(c1) from t1;
drop table t1;

create table t1 as select concat(utc_time()) as c1;
show create table t1;
drop table t1;

select hex(concat(sec_to_time(2378)));
create table t1 as select concat(sec_to_time(2378)) as c1;
show create table t1;
drop table t1;

select hex(concat(timediff('2001-01-02 00:00:00', '2001-01-01 00:00:00')));
create table t1 as select concat(timediff('2001-01-02 00:00:00', '2001-01-01 00:00:00')) as c1;
show create table t1;
drop table t1;

select hex(concat(maketime(10,11,12)));
create table t1 as select concat(maketime(10,11,12)) as c1;
show create table t1;
drop table t1;

select hex(get_format(DATE,'USA'));
create table t1 as select get_format(DATE,'USA') as c1;
show create table t1;
drop table t1;

select hex(left(concat(from_unixtime(1111885200)),4));
create table t1 as select concat(from_unixtime(1111885200)) as c1;
show create table t1;
drop table t1;

select hex(concat(convert_tz('2004-01-01 12:00:00','+10:00','-6:00')));
create table t1 as select concat(convert_tz('2004-01-01 12:00:00','+10:00','-6:00')) as c1;
show create table t1;
drop table t1;

select hex(concat(date_add('2004-01-01 12:00:00', interval 1 day)));
create table t1 as select concat(date_add('2004-01-01 12:00:00', interval 1 day)) as c1;
show create table t1;
select * from t1;
drop table t1;

select hex(concat(makedate(2009,1)));
create table t1 as select concat(makedate(2009,1)) as c1;
show create table t1;
select * from t1;
drop table t1;

create table t1 as select concat(now()) as c1;
show create table t1;
drop table t1;

create table t1 as select concat(utc_timestamp()) as c1;
show create table t1;
drop table t1;

create table t1 as select concat(sysdate()) as c1;
show create table t1;
drop table t1;

select hex(concat(addtime('00:00:00','11:22:33')));
create table t1 as select concat(addtime('00:00:00','11:22:33')) as c1;
show create table t1;
drop table t1;

select hex(concat(subtime('23:59:59','11:22:33')));
create table t1 as select concat(subtime('23:59:59','11:22:33')) as c1;
show create table t1;
drop table t1;


#
# Other string functions with numeric input
#
select hex(elt(1,2,3));
create table t1 as select elt(1,2,3) as c1;
show create table t1;
drop table t1;

select hex(export_set(1,2,3,4,2));
create table t1 as select export_set(1,2,3,4,2) as c1;
show create table t1;
drop table t1;

select hex(insert(1133,3,0,22));
create table t1 as select insert(1133,3,0,22) as c1;
show create table t1;
drop table t1;

select hex(lcase(123));
create table t1 as select lcase(123) as c1;
show create table t1;
drop table t1;

select hex(left(123,1));
create table t1 as select left(123,1) as c1;
show create table t1;
drop table t1;

select hex(lower(123));
create table t1 as select lower(123) as c1;
show create table t1;
drop table t1;

select hex(lpad(1,2,0));
create table t1 as select lpad(1,2,0) as c1;
show create table t1;
drop table t1;

select hex(ltrim(1));
create table t1 as select ltrim(1) as c1;
show create table t1;
drop table t1;

select hex(mid(1,1,1));
create table t1 as select mid(1,1,1) as c1;
show create table t1;
drop table t1;

select hex(repeat(1,2));
create table t1 as select repeat(1,2) as c1;
show create table t1;
drop table t1;

select hex(replace(1,1,2));
create table t1 as select replace(1,1,2) as c1;
show create table t1;
drop table t1;

select hex(reverse(12));
create table t1 as select reverse(12) as c1;
show create table t1;
drop table t1;

select hex(right(123,1));
create table t1 as select right(123,1) as c1;
show create table t1;
drop table t1;

select hex(rpad(1,2,0));
create table t1 as select rpad(1,2,0) as c1;
show create table t1;
drop table t1;

select hex(rtrim(1));
create table t1 as select rtrim(1) as c1;
show create table t1;
drop table t1;

select hex(soundex(1));
create table t1 as select soundex(1) as c1;
show create table t1;
drop table t1;

select hex(substring(1,1,1));
create table t1 as select substring(1,1,1) as c1;
show create table t1;
drop table t1;

select hex(trim(1));
create table t1 as select trim(1) as c1;
show create table t1;
drop table t1;

select hex(ucase(1));
create table t1 as select ucase(1) as c1;
show create table t1;
drop table t1;

select hex(upper(1));
create table t1 as select upper(1) as c1;
show create table t1;
drop table t1;


#
# Bug#8204
#
create table t1 as select repeat(' ', 64) as a limit 0;
show create table t1;
insert into t1 values ("1.1"), ("2.1");
select a, hex(a) from t1;
update t1 set a= a + 0.1;
select a, hex(a) from t1;
drop table t1;


#
# Columns
#
create table t1 (a tinyint);
insert into t1 values (1);
select hex(concat(a)) from t1;
create table t2 as select concat(a) from t1;
show create table t2;
drop table t1, t2;

create table t1 (a tinyint zerofill);
insert into t1 values (1), (10), (100);
select hex(concat(a)), a from t1;
drop table t1;

create table t1 (a tinyint(4) zerofill);
insert into t1 values (1), (10), (100);
select hex(concat(a)), a from t1;
drop table t1;

create table t1 (a decimal(10,2));
insert into t1 values (123.45);
select hex(concat(a)) from t1;
create table t2 as select concat(a) from t1;
show create table t2;
drop table t1, t2;

create table t1 (a smallint);
insert into t1 values (1);
select hex(concat(a)) from t1;
create table t2 as select concat(a) from t1;
show create table t2;
drop table t1, t2;

create table t1 (a smallint zerofill);
insert into t1 values (1), (10), (100), (1000), (10000);
select hex(concat(a)), a from t1;
drop table t1;

create table t1 (a mediumint);
insert into t1 values (1);
select hex(concat(a)) from t1;
create table t2 as select concat(a) from t1;
show create table t2;
drop table t1, t2;

create table t1 (a mediumint zerofill);
insert into t1 values (1), (10), (100), (1000), (10000);
select hex(concat(a)), a from t1;
drop table t1;

create table t1 (a int);
insert into t1 values (1);
select hex(concat(a)) from t1;
create table t2 as select concat(a) from t1;
show create table t2;
drop table t1, t2;

create table t1 (a int zerofill);
insert into t1 values (1), (10), (100), (1000), (10000);
select hex(concat(a)), a from t1;
drop table t1;

create table t1 (a bigint);
insert into t1 values (1);
select hex(concat(a)) from t1;
create table t2 as select concat(a) from t1;
show create table t2;
drop table t1, t2;

create table t1 (a bigint zerofill);
insert into t1 values (1), (10), (100), (1000), (10000);
select hex(concat(a)), a from t1;
drop table t1;

create table t1 (a float);
insert into t1 values (123.456);
select hex(concat(a)) from t1;
select concat(a) from t1;
create table t2 as select concat(a) from t1;
show create table t2;
drop table t1, t2;

create table t1 (a float zerofill);
insert into t1 values (1.1), (10.1), (100.1), (1000.1), (10000.1);
select hex(concat(a)), a from t1;
drop table t1;

create table t1 (a double);
insert into t1 values (123.456);
select hex(concat(a)) from t1;
select concat(a) from t1;
create table t2 as select concat(a) from t1;
show create table t2;
drop table t1, t2;

create table t1 (a double zerofill);
insert into t1 values (1.1), (10.1), (100.1), (1000.1), (10000.1);
select hex(concat(a)), a from t1;
drop table t1;

create table t1 (a year(4));
insert into t1 values (1);
select hex(concat(a)) from t1;
create table t2 as select concat(a) from t1;
show create table t2;
drop table t1, t2;

create table t1 (a year);
insert into t1 values (1);
select hex(concat(a)) from t1;
create table t2 as select concat(a) from t1;
show create table t2;
drop table t1, t2;

create table t1 (a bit(64));
# BIT is always BINARY
insert into t1 values (1);
select hex(concat(a)) from t1;
create table t2 as select concat(a) from t1;
show create table t2;
drop table t1, t2;

create table t1 (a timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP);
insert ignore into t1 values (0);
insert into t1 values (20010203040506);
insert into t1 values (19800203040506);
insert into t1 values ('2001-02-03 04:05:06');
select hex(concat(a)) from t1;
select concat(a) from t1;
create table t2 as select concat(a) from t1;
show create table t2;
drop table t1, t2;

create table t1 (a date);
insert into t1 values ('2001-02-03');
insert into t1 values (20010203);
select hex(concat(a)) from t1;
create table t2 as select concat(a) from t1;
show create table t2;
drop table t1, t2;

create table t1 (a time);
insert into t1 values (1);
insert into t1 values ('01:02:03');
select hex(concat(a)) from t1;
select concat(a) from t1;
create table t2 as select concat(a) from t1;
show create table t2;
drop table t1, t2;

create table t1 (a datetime);
insert into t1 values ('2001-02-03 04:05:06');
insert into t1 values (20010203040506);
select hex(concat(a)) from t1;
create table t2 as select concat(a) from t1;
show create table t2;
drop table t1, t2;


#
# create view with string functions with numeric input
#
# Switched off in ucs tests due to bug#50716 
if ($not_ucs)
{
create view v1 as select concat(1,2,3) as c1;
show columns from v1;
select hex(c1) from v1;
drop view v1;

create view v1 as select concat_ws(',',1,2,3) as c1;
show columns from v1;
select hex(c1) from v1;
drop view v1;

create view v1 as select elt(1,2,3) as c1;
show columns from v1;
select hex(c1) from v1;
drop view v1;

create view v1 as select export_set(1,2,3,4,2) as c1;
show columns from v1;
select hex(c1) from v1;
drop view v1;

create view v1 as select insert(1133,3,0,22) as c1;
show columns from v1;
select hex(c1) from v1;
drop view v1;

create view v1 as select lcase(123) as c1;
show columns from v1;
select hex(c1) from v1;
drop view v1;

create view v1 as select left(123,1) as c1;
show columns from v1;
select hex(c1) from v1;
drop view v1;

create view v1 as select lower(123) as c1;
show columns from v1;
select hex(c1) from v1;
drop view v1;

create view v1 as select lpad(1,2,0) as c1;
show columns from v1;
select hex(c1) from v1;
drop view v1;

create view v1 as select ltrim(1) as c1;
show columns from v1;
select hex(c1) from v1;
drop view v1;

create view v1 as select mid(1,1,1) as c1;
show columns from v1;
select hex(c1) from v1;
drop view v1;

create view v1 as select repeat(1,2) as c1;
show columns from v1;
select hex(c1) from v1;
drop view v1;

create view v1 as select replace(1,1,2) as c1;
show columns from v1;
select hex(c1) from v1;
drop view v1;

create view v1 as select reverse(12) as c1;
show columns from v1;
select hex(c1) from v1;
drop view v1;

create view v1 as select right(123,1) as c1;
show columns from v1;
select hex(c1) from v1;
drop view v1;

create view v1 as select rpad(1,2,0) as c1;
show columns from v1;
select hex(c1) from v1;
drop view v1;

create view v1 as select rtrim(1) as c1;
show columns from v1;
select hex(c1) from v1;
drop view v1;

create view v1 as select soundex(1) as c1;
show columns from v1;
select hex(c1) from v1;
drop view v1;

create view v1 as select substring(1,1,1) as c1;
show columns from v1;
select hex(c1) from v1;
drop view v1;

create view v1 as select trim(1) as c1;
show columns from v1;
select hex(c1) from v1;
drop view v1;

create view v1 as select ucase(1) as c1;
show columns from v1;
select hex(c1) from v1;
drop view v1;

create view v1 as select upper(1) as c1;
show columns from v1;
select hex(c1) from v1;
drop view v1;
}


#
# Views from tables with numeric columns
#
create table t1 (a tinyint);
insert into t1 values (1);
create view v1(a) as select concat(a) from t1;
show columns from v1;
select hex(a) from v1;
drop table t1;
drop view v1;

create table t1 (a tinyint zerofill);
insert into t1 values (1), (10), (100);
create view v1(a) as select concat(a) from t1;
show columns from v1;
select hex(a) from v1;
drop table t1;
drop view v1;

create table t1 (a tinyint(30) zerofill);
insert into t1 values (1), (10), (100);
create view v1(a) as select concat(a) from t1;
show columns from v1;
select hex(a) from v1;
drop table t1;
drop view v1;

create table t1 (a decimal(10,2));
insert into t1 values (123.45);
create view v1(a) as select concat(a) from t1;
show columns from v1;
select hex(a) from v1;
drop table t1;
drop view v1;

create table t1 (a smallint);
insert into t1 values (1);
create view v1(a) as select concat(a) from t1;
show columns from v1;
select hex(a) from v1;
drop table t1;
drop view v1;

create table t1 (a smallint zerofill);
insert into t1 values (1), (10), (100), (1000), (10000);
create view v1(a) as select concat(a) from t1;
show columns from v1;
select hex(a) from v1;
drop table t1;
drop view v1;

create table t1 (a mediumint);
insert into t1 values (1);
create view v1(a) as select concat(a) from t1;
show columns from v1;
select hex(a) from v1;
drop table t1;
drop view v1;

create table t1 (a mediumint zerofill);
insert into t1 values (1), (10), (100), (1000), (10000);
create view v1(a) as select concat(a) from t1;
show columns from v1;
select hex(a) from v1;
drop table t1;
drop view v1;

create table t1 (a int);
insert into t1 values (1);
create view v1(a) as select concat(a) from t1;
show columns from v1;
select hex(a) from v1;
drop table t1;
drop view v1;

create table t1 (a int zerofill);
insert into t1 values (1), (10), (100), (1000), (10000);
create view v1(a) as select concat(a) from t1;
show columns from v1;
select hex(a) from v1;
drop table t1;
drop view v1;

create table t1 (a bigint);
insert into t1 values (1);
create view v1(a) as select concat(a) from t1;
show columns from v1;
select hex(a) from v1;
drop table t1;
drop view v1;

create table t1 (a bigint zerofill);
insert into t1 values (1), (10), (100), (1000), (10000);
create view v1(a) as select concat(a) from t1;
show columns from v1;
select hex(a) from v1;
drop table t1;
drop view v1;

create table t1 (a float);
insert into t1 values (123.456);
create view v1(a) as select concat(a) from t1;
show columns from v1;
select hex(a) from v1;
drop table t1;
drop view v1;

create table t1 (a float zerofill);
insert into t1 values (1.1), (10.1), (100.1), (1000.1), (10000.1);
create view v1(a) as select concat(a) from t1;
show columns from v1;
select hex(a) from v1;
drop table t1;
drop view v1;

create table t1 (a double);
insert into t1 values (123.456);
select concat(a) from t1;
create view v1(a) as select concat(a) from t1;
show columns from v1;
select hex(a) from v1;
drop table t1;
drop view v1;

create table t1 (a double zerofill);
insert into t1 values (1.1), (10.1), (100.1), (1000.1), (10000.1);
create view v1(a) as select concat(a) from t1;
show columns from v1;
select hex(a) from v1;
drop table t1;
drop view v1;

create table t1 (a year(4));
insert into t1 values (1);
create view v1(a) as select concat(a) from t1;
show columns from v1;
select hex(a) from v1;
drop table t1;
drop view v1;

create table t1 (a year);
insert into t1 values (1);
create view v1(a) as select concat(a) from t1;
show columns from v1;
select hex(a) from v1;
drop table t1;
drop view v1;

create table t1 (a bit(64));
# BIT is always BINARY
insert into t1 values (1);
create view v1(a) as select concat(a) from t1;
show columns from v1;
select hex(a) from v1;
drop table t1;
drop view v1;

create table t1 (a timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP);
insert ignore into t1 values (0);
insert into t1 values (20010203040506);
insert into t1 values (19800203040506);
insert into t1 values ('2001-02-03 04:05:06');
create view v1(a) as select concat(a) from t1;
show columns from v1;
select hex(a) from v1;
drop table t1;
drop view v1;

create table t1 (a date);
insert into t1 values ('2001-02-03');
insert into t1 values (20010203);
create view v1(a) as select concat(a) from t1;
show columns from v1;
select hex(a) from v1;
drop table t1;
drop view v1;

create table t1 (a time);
insert into t1 values (1);
insert into t1 values ('01:02:03');
create view v1(a) as select concat(a) from t1;
show columns from v1;
select hex(a) from v1;
drop table t1;
drop view v1;

create table t1 (a datetime);
insert into t1 values ('2001-02-03 04:05:06');
insert into t1 values (20010203040506);
create view v1(a) as select concat(a) from t1;
show columns from v1;
select hex(a) from v1;
drop table t1;
drop view v1;

#
# User defined function returning numeric result
#
delimiter |;
create function f1 (par1 int) returns int
begin
return concat(par1);
end|
delimiter ;|

set @a= f1(1);
select hex(@a);
select hex(concat(f1(1)));
create table t1 as select f1(1) as c1; 
show create table t1;
drop table t1;
create table t1 as select concat(f1(1)) as c1; 
show create table t1;
create view v1 as select concat(f1(1)) as c1; 
show columns from v1;
drop table t1;
drop view v1;
drop function f1;

delimiter |;
create function f1 (par1 decimal(18,2)) returns decimal(18,2)
begin
return concat(par1);
end|
delimiter ;|

set @a= f1(123.45);
select hex(@a);
select hex(concat(f1(123.45)));
create table t1 as select f1(123.45) as c1; 
show create table t1;
drop table t1;
create table t1 as select concat(f1(123.45)) as c1; 
show create table t1;
create view v1 as select concat(f1(123.45)) as c1; 
show columns from v1;
drop table t1;
drop view v1;
drop function f1;

delimiter |;
create function f1 (par1 float) returns float
begin
return concat(par1);
end|
delimiter ;|

set @a= f1(123.45);
select hex(@a);
select hex(concat(f1(123.45)));
create table t1 as select f1(123.45) as c1; 
show create table t1;
drop table t1;
create table t1 as select concat(f1(123.45)) as c1; 
show create table t1;
create view v1 as select concat(f1(123.45)) as c1; 
show columns from v1;
drop table t1;
drop view v1;
drop function f1;

delimiter |;
create function f1 (par1 date) returns date
begin
return concat(par1);
end|
delimiter ;|

set @a= f1(cast('2001-01-02' as date));
select hex(@a);
select hex(concat(f1(cast('2001-01-02' as date))));
create table t1 as select f1(cast('2001-01-02' as date)) as c1; 
show create table t1;
drop table t1;
create table t1 as select concat(f1(cast('2001-01-02' as date))) as c1; 
show create table t1;
create view v1 as select concat(f1(cast('2001-01-02' as date))) as c1; 
show columns from v1;
drop table t1;
drop view v1;
drop function f1;


--echo #
--echo # End of WL#2649 Number-to-string conversions
--echo #

--echo #
--echo # Bug#54668 User variable assignments get wrong type
--echo #
SET @ST_X=md5('a');
SELECT charset(@ST_X), collation(@ST_X);
SET @ST_X=sha('a');
SELECT charset(@ST_X), collation(@ST_X);
SET @ST_X=sha1('a');
SELECT charset(@ST_X), collation(@ST_X);
SET @ST_X=ST_astext(point(1,2));
SELECT charset(@ST_X), collation(@ST_X);
SET @ST_X=ST_AsWKT(point(1,2));
SELECT charset(@ST_X), collation(@ST_X);


--echo #
--echo # Bug#54916 GROUP_CONCAT + IFNULL truncates output
--echo #
SELECT @@collation_connection;
# ENGINE=MYISAM is very important to make sure "SYSTEM" join type
# is in use, which will create instances of Item_copy.
CREATE TABLE t1 (a MEDIUMINT NULL) ENGINE=MYISAM;
INSERT INTO t1 VALUES (1234567);
SELECT GROUP_CONCAT(IFNULL(a,'')) FROM t1;
SELECT GROUP_CONCAT(IF(a,a,'')) FROM t1;
SELECT GROUP_CONCAT(CASE WHEN a THEN a ELSE '' END) FROM t1;
--enable_metadata
SELECT COALESCE(a,'') FROM t1 GROUP BY 1;
--disable_metadata
--echo # All columns must be VARCHAR(9) with the same length:
--disable_warnings
CREATE TABLE t2 AS
SELECT
  CONCAT(a),
  IFNULL(a,''),
  IF(a,a,''),
  CASE WHEN a THEN a ELSE '' END,
  COALESCE(a,'')
FROM t1;
--enable_warnings
# The above query is expected to send a warning
# in case of ucs2 character set, until Bug#55744 is fixed.
SHOW CREATE TABLE t2;
DROP TABLE t2;

CREATE TABLE t2 AS SELECT CONCAT_WS(1,2,3) FROM t1;
SHOW CREATE TABLE t2;
DROP TABLE t2;

CREATE TABLE t2 AS SELECT INSERT(1133,3,0,22) FROM t1;
SHOW CREATE TABLE t2;
DROP TABLE t2;

CREATE TABLE t2 AS SELECT LCASE(a) FROM t1;
SHOW CREATE TABLE t2;
DROP TABLE t2;

CREATE TABLE t2 AS SELECT UCASE(a) FROM t1;
SHOW CREATE TABLE t2;
DROP TABLE t2;

CREATE TABLE t2 AS SELECT REPEAT(1,2) FROM t1;
SHOW CREATE TABLE t2;
DROP TABLE t2;

CREATE TABLE t2 AS SELECT LEFT(123,2) FROM t1;
SHOW CREATE TABLE t2;
DROP TABLE t2;

CREATE TABLE t2 AS SELECT RIGHT(123,2) FROM t1;
SHOW CREATE TABLE t2;
DROP TABLE t2;

CREATE TABLE t2 AS SELECT LTRIM(123) FROM t1;
SHOW CREATE TABLE t2;
DROP TABLE t2;

CREATE TABLE t2 AS SELECT RTRIM(123) FROM t1;
SHOW CREATE TABLE t2;
DROP TABLE t2;

CREATE TABLE t2 AS SELECT ELT(1,111,222,333) FROM t1;
SHOW CREATE TABLE t2;
DROP TABLE t2;

CREATE TABLE t2 AS SELECT REPLACE(111,2,3) FROM t1;
SHOW CREATE TABLE t2;
DROP TABLE t2;

CREATE TABLE t2 AS SELECT SUBSTRING_INDEX(111,111,1) FROM t1;
SHOW CREATE TABLE t2;
DROP TABLE t2;

CREATE TABLE t2 AS SELECT MAKE_SET(111,222,3) FROM t1;
SHOW CREATE TABLE t2;
DROP TABLE t2;

CREATE TABLE t2 AS SELECT SOUNDEX(1) FROM t1;
SHOW CREATE TABLE t2;
DROP TABLE t2;

CREATE TABLE t2 AS SELECT EXPORT_SET(1,'ST_Y','N','',8);
SHOW CREATE TABLE t2;
DROP TABLE t2;

DROP TABLE t1;

--echo #
--echo # End of Bug#54916
--echo #

--echo #
--echo # WL#5510 Functions to_base64 and from_base64
--echo #
CREATE TABLE t1 AS SELECT TO_BASE64('test') AS to_base64;
SHOW CREATE TABLE t1;
SELECT to_base64, LENGTH(to_base64), HEX(to_base64) FROM t1;
CREATE TABLE t2 AS SELECT FROM_BASE64(to_base64) AS from_base64 FROM t1;
SHOW CREATE TABLE t2;
SELECT CAST(from_base64 AS CHAR), LENGTH(from_base64), HEX(from_base64) FROM t2;
DROP TABLE t2;
DROP TABLE t1;


--echo #
--echo # Bug#58190 BETWEEN no longer uses indexes for date or datetime fields
--echo #
SELECT @@collation_connection;
CREATE TABLE t1 (
  id INT(11) DEFAULT NULL,
  date_column DATE DEFAULT NULL,
  KEY(date_column));
INSERT INTO t1 VALUES (1,'2010-09-01'),(2,'2010-10-01');
ANALYZE TABLE t1;
EXPLAIN SELECT * FROM t1 WHERE date_column BETWEEN '2010-09-01' AND '2010-10-01';
ALTER TABLE t1 MODIFY date_column DATETIME DEFAULT NULL;
ANALYZE TABLE t1;
EXPLAIN SELECT * FROM t1 WHERE date_column BETWEEN '2010-09-01' AND '2010-10-01';
DROP TABLE t1;


--echo #
--echo # Bug #31384 	DATE_ADD() and DATE_SUB() return binary data
--echo #
SELECT @@collation_connection, @@character_set_results;
CREATE TABLE t1 AS
SELECT
  DATE_SUB('2007-08-03', INTERVAL 1 MINUTE) AS field_str1,
  DATE_SUB('2007-08-03 17:33:00', INTERVAL 1 MINUTE) AS field1_str2,
  DATE_SUB(DATE('2007-08-03'), INTERVAL 1 DAY) AS field_date,
  DATE_SUB(CAST('2007-08-03 17:33:00' AS DATETIME), INTERVAL 1 MINUTE) AS field_datetime;
SHOW CREATE TABLE t1;
DROP TABLE t1;
--enable_metadata
# PS protocol gives different "Max length" value for DATETIME.
--disable_ps_protocol
SELECT
  DATE_SUB('2007-08-03', INTERVAL 1 DAY) AS field_str1,
  DATE_SUB('2007-08-03 17:33:00', INTERVAL 1 MINUTE) AS field1_str2,
  DATE_SUB(DATE('2007-08-03'), INTERVAL 1 DAY) AS field_date,
  DATE_SUB(CAST('2007-08-03 17:33:00' AS DATETIME), INTERVAL 1 MINUTE) AS field_datetime;
--disable_metadata
--enable_ps_protocol
SELECT
  HEX(DATE_SUB('2007-08-03', INTERVAL 1 MINUTE)) AS field_str1,
  HEX(DATE_SUB('2007-08-03 17:33:00', INTERVAL 1 MINUTE)) AS field1_str2,
  HEX(DATE_SUB(DATE('2007-08-03'), INTERVAL 1 DAY)) AS field_date,
  HEX(DATE_SUB(CAST('2007-08-03 17:33:00' AS DATETIME), INTERVAL 1 MINUTE)) AS field_datetime;
 
--echo #
--echo # Bug#11926811 / Bug#60625 Illegal mix of collations
--echo #
SELECT @@collation_connection;
DELIMITER //;
CREATE PROCEDURE p1()
BEGIN
  DECLARE v_LastPaymentDate DATETIME DEFAULT NULL;
  SELECT v_LastPaymentDate < NOW();
  EXPLAIN SELECT v_LastPaymentDate < NOW();
  SHOW WARNINGS;
  EXPLAIN SELECT CONCAT(v_LastPaymentDate, NOW());
END//
DELIMITER ;//
CALL p1;
DROP PROCEDURE p1;

--echo #
--echo # Bug#52159 returning time type from function and empty left join causes debug assertion
--echo #
CREATE FUNCTION f1() RETURNS TIME RETURN 1;
CREATE TABLE t1 (b INT);
INSERT INTO t1 VALUES (0);
SELECT f1() FROM t1 LEFT JOIN (SELECT 1 AS a FROM t1 LIMIT 0) AS d ON 1 GROUP BY a;
DROP FUNCTION f1;
DROP TABLE t1;

SET NAMES latin1;
SET sql_mode='';
CREATE TABLE t1(a char(215) CHARACTER SET utf8 NOT NULL DEFAULT '', KEY(a));
INSERT INTO t1 VALUES ();
--disable_warnings
SELECT maketime(`a`,`a`,`a`) FROM t1 GROUP BY 1;
--enable_warnings
DROP TABLE t1;
SET sql_mode=default;

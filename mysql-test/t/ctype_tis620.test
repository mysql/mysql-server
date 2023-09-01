#
# Tests with the big5 character set
#
--disable_warnings
drop table if exists t1;
--enable_warnings

#
# Bug 1552: tis620 <-> unicode conversion crashed
# Check tis620 -> utf8mb3 -> tis620 round trip conversion
#

SET @pl0= _tis620 0x000102030405060708090A0B0C0D0E0F;
SET @pl1= _tis620 0x101112131415161718191A1B1C1D1E1F;
SET @pl2= _tis620 0x202122232425262728292A2B2C2D2E2F;
SET @pl3= _tis620 0x303132333435363738393A3B3C3D3E3F;
SET @pl4= _tis620 0x404142434445464748494A4B4C4D4E4F;
SET @pl5= _tis620 0x505152535455565758595A5B5C5D5E5F;
SET @pl6= _tis620 0x606162636465666768696A6B6C6D6E6F;
SET @pl7= _tis620 0x707172737475767778797A7B7C7D7E7F;
SET @pl8= _tis620 0x808182838485868788898A8B8C8D8E8F;
SET @pl9= _tis620 0x909192939495969798999A9B9C9D9E9F;
SET @plA= _tis620 0xA0A1A2A3A4A5A6A7A8A9AAABACADAEAF;
SET @plB= _tis620 0xB0B1B2B3B4B5B6B7B8B9BABBBCBDBEBF;
SET @plC= _tis620 0xC0C1C2C3C4C5C6C7C8C9CACBCCCDCECF;
SET @plD= _tis620 0xD0D1D2D3D4D5D6D7D8D9DADBDCDDDEDF;
SET @plE= _tis620 0xE0E1E2E3E4E5E6E7E8E9EAEBECEDEEEF;
SET @plF= _tis620 0xF0F1F2F3F4F5F6F7F8F9FAFBFCFDFEFF;

SELECT hex(@u0:=convert(@pl0 using utf8mb3));
SELECT hex(@u1:=convert(@pl1 using utf8mb3));
SELECT hex(@u2:=convert(@pl2 using utf8mb3));
SELECT hex(@u3:=convert(@pl3 using utf8mb3));
SELECT hex(@u4:=convert(@pl4 using utf8mb3));
SELECT hex(@u5:=convert(@pl5 using utf8mb3));
SELECT hex(@u6:=convert(@pl6 using utf8mb3));
SELECT hex(@u7:=convert(@pl7 using utf8mb3));
SELECT hex(@u8:=convert(@pl8 using utf8mb3));
SELECT hex(@u9:=convert(@pl9 using utf8mb3));
SELECT hex(@uA:=convert(@plA using utf8mb3));
SELECT hex(@uB:=convert(@plB using utf8mb3));
SELECT hex(@uC:=convert(@plC using utf8mb3));
SELECT hex(@uD:=convert(@plD using utf8mb3));
SELECT hex(@uE:=convert(@plE using utf8mb3));
SELECT hex(@uF:=convert(@plF using utf8mb3));

SELECT hex(convert(@u0 USING tis620));
SELECT hex(convert(@u1 USING tis620));
SELECT hex(convert(@u2 USING tis620));
SELECT hex(convert(@u3 USING tis620));
SELECT hex(convert(@u4 USING tis620));
SELECT hex(convert(@u5 USING tis620));
SELECT hex(convert(@u6 USING tis620));
SELECT hex(convert(@u7 USING tis620));
SELECT hex(convert(@u8 USING tis620));
SELECT hex(convert(@u9 USING tis620));
SELECT hex(convert(@uA USING tis620));
SELECT hex(convert(@uB USING tis620));
SELECT hex(convert(@uC USING tis620));
SELECT hex(convert(@uD USING tis620));
SELECT hex(convert(@uE USING tis620));
SELECT hex(convert(@uF USING tis620));

SET NAMES tis620;
--character_set tis620

#
# Check the following:
# "a"  == "a "
# "a\0" < "a"
# "a\0" < "a "

SELECT 'a' = 'a ';
SELECT 'a\0' < 'a';
SELECT 'a\0' < 'a ';
SELECT 'a\t' < 'a';
SELECT 'a\t' < 'a ';

CREATE TABLE t1 (a char(10) not null) CHARACTER SET tis620;
INSERT INTO t1 VALUES ('a'),('a\0'),('a\t'),('a ');
SELECT hex(a),STRCMP(a,'a'), STRCMP(a,'a ') FROM t1;
DROP TABLE t1;

#
# Bug#6608
#
SET sql_mode = 'NO_ENGINE_SUBSTITUTION';
CREATE TABLE t1 (
  `id` int(11) NOT NULL auto_increment,
  `url` varchar(200) NOT NULL default '',
  `name` varchar(250) NOT NULL default '',
  `type` int(11) NOT NULL default '0',
  `website` varchar(250) NOT NULL default '',
  `adddate` date NOT NULL default '0000-00-00',
  `size` varchar(20) NOT NULL default '',
  `movieid` int(11) NOT NULL default '0',
  `musicid` int(11) NOT NULL default '0',
  `star` varchar(20) NOT NULL default '',
  `download` int(11) NOT NULL default '0',
  `lastweek` int(11) NOT NULL default '0',
  `thisweek` int(11) NOT NULL default '0',
  `page` varchar(250) NOT NULL default '',
  PRIMARY KEY  (`id`),
  UNIQUE KEY `url` (`url`)
) CHARACTER SET tis620;

INSERT INTO t1 VALUES
(1,'http://www.siamzone.com/download/download/000001-frodo_1024.jpg','The Lord
of the Rings
Wallpapers',1,'http://www.lordoftherings.net','2002-01-22','',448,0,'',3805,0,0,
'');
INSERT INTO t1 VALUES (2,'http://www.othemovie.com/OScreenSaver1.EXE','O
Screensaver',2,'','2002-01-22','',491,0,'',519,0,0,'');
INSERT INTO t1 VALUES
(3,'http://www.siamzone.com/download/download/000003-jasonx2(800x600).jpg','Jaso
n X Wallpapers',1,'','2002-05-31','',579,0,'',1091,0,0,'');
select * from t1 order by id;
DROP TABLE t1;


SET collation_connection='tis620_thai_ci';
-- source include/ctype_filesort.inc
-- source include/ctype_like_escape.inc
--source include/ctype_ascii_order.inc
SET collation_connection='tis620_bin';
-- source include/ctype_filesort.inc
-- source include/ctype_like_escape.inc
SET sql_mode = default;
# End of 4.1 tests


--echo #
--echo # Start of 5.6 tests
--echo #

--echo #
--echo # WL#3664 WEIGHT_STRING
--echo #

set names tis620;
set collation_connection=tis620_thai_ci;
--source include/weight_string.inc
select hex(weight_string(cast(0xE0A1 as char)));
select hex(weight_string(cast(0xE0A1 as char) as char(1)));

set collation_connection=tis620_bin;
--source include/weight_string.inc
select hex(weight_string(cast(0xE0A1 as char)));
select hex(weight_string(cast(0xE0A1 as char) as char(1)));

--echo #
--echo # Bugs#12635232: VALGRIND WARNINGS: IS_IPV6, IS_IPV4, INET6_ATON,
--echo # INET6_NTOA + MULTIBYTE CHARSET.
--echo #

SET NAMES tis620;
--source include/ctype_inet.inc

--echo #
--echo # End of 5.6 tests
--echo #

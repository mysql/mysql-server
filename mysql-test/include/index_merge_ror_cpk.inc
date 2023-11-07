# include/index_merge_ror_cpk.inc
#
# Clustered PK ROR-index_merge tests
#
# The variable
#     $engine_type       -- storage engine to be tested
# has to be set before sourcing this script.
#
# Note: The comments/expectations refer to InnoDB.
#       They might be not valid for other storage engines.
#
# Last update:
# 2006-08-02 ML test refactored
#               old name was t/index_merge_ror_cpk.test
#               main code went into include/index_merge_ror_cpk.inc
#

--echo #---------------- Clustered PK ROR-index_merge tests -----------------------------
--echo # (Start of test file: index_merge_ror_cpk.inc)

eval SET SESSION DEFAULT_STORAGE_ENGINE = $engine_type;
eval SET SESSION EXPLAIN_FORMAT = tree;

--disable_warnings
drop table if exists  t1;
--enable_warnings

create table t1
(
  pk1 int not null,
  pk2 int not null,

  key1 int not null,
  key2 int not null,

  pktail1ok  int not null,
  pktail2ok  int not null,
  pktail3bad int not null,
  pktail4bad int not null,
  pktail5bad int not null,

  pk2copy int not null,
  badkey  int not null,

  filler1 char (200),
  filler2 char (200),
  key (key1),
  key (key2),

  /* keys with tails from CPK members */
  key (pktail1ok, pk1),
  key (pktail2ok, pk1, pk2),
  key (pktail3bad, pk2, pk1),
  key (pktail4bad, pk1, pk2copy),
  key (pktail5bad, pk1, pk2, pk2copy),

  primary key (pk1, pk2)
);

--disable_query_log
set autocommit=0;
let $1=10000;
while ($1)
{
  eval insert into t1 values ($1 div 10,$1 mod 100,   $1/100,$1/100,   $1/100,$1/100,$1/100,$1/100,$1/100, $1 mod 100, $1/1000,'filler-data-$1','filler2');
  dec $1;
}
set autocommit=1;
--enable_query_log

-- disable_query_log
-- disable_result_log
analyze table t1;
-- enable_result_log
-- enable_query_log

--echo # Verify that range scan on CPK is ROR
--echo # (use index_intersection because it is impossible to check that for index union)
--echo # Column 9, rows, can change depending on innodb-page-size.
--replace_regex $elide_costs_and_rows
explain select * from t1 where pk1 = 1 and pk2 < 80  and key1=0;
--echo # CPK scan + 1 ROR range scan is a special case
--sorted_result
select * from t1 where pk1 = 1 and pk2 < 80  and key1=0;

--echo # Verify that CPK fields are considered to be covered by index scans
--replace_regex $elide_costs_and_rows
explain select pk1,pk2 from t1 where key1 = 10 and key2=10 and 2*pk1+1 < 2*96+1;
select pk1,pk2 from t1 where key1 = 10 and key2=10 and 2*pk1+1 < 2*96+1;

--echo # Verify that CPK is always used for index intersection scans
--echo # (this is because it is used as a filter, not for retrieval)
--echo # The expected number of rows can vary depending on page size
--replace_regex $elide_costs
explain select * from t1 where badkey=1 and key1=10;
--echo # The expected number of rows can vary depending on page size
--replace_regex $elide_costs
explain select * from t1 where pk1 < 7500 and key1 = 10;

--echo # Verify that keys with 'tails' of PK members are ok.
--replace_regex $elide_costs_and_rows
explain select * from t1 where pktail1ok=1 and key1=10;
--replace_regex $elide_costs_and_rows
explain select * from t1 where pktail2ok=1 and key1=10;

--echo # Note: The following is actually a deficiency, it uses sort_union currently.
--echo #       This comment refers to InnoDB and is probably not valid for other engines.
# The expected number of rows can vary depending on page size
--replace_regex $elide_costs_and_rows
explain select * from t1 where (pktail2ok=1 and pk1< 50000) or key1=10;

# The expected column used for KEY vary depending on page size
# The expected number of rows can vary depending on page size and platform
--replace_regex $elide_costs_and_rows
explain select * from t1 where pktail3bad=1 and key1=10;
# The expected column used for KEY vary depending on page size
--replace_regex $elide_costs_and_rows
explain select * from t1 where pktail4bad=1 and key1=10;
# The expected column used for KEY vary depending on page size
--replace_regex $elide_costs_and_rows
explain select * from t1 where pktail5bad=1 and key1=10;

--echo # Test for problem with innodb key values prefetch buffer:
--replace_regex $elide_costs_and_rows
explain select pk1,pk2,key1,key2 from t1 where key1 = 10 and key2=10 limit 10;
select pk1,pk2,key1,key2 from t1 where key1 = 10 and key2=10 limit 10;

drop table t1;
--echo # Testcase for BUG#4984
create table t1
(
  RUNID varchar(22),
  SUBMITNR varchar(5),
  ORDERNR char(1),
  PROGRAMM varchar(8),
  TESTID varchar(4),
  UCCHECK char(1),
  ETEXT varchar(80),
  ETEXT_TYPE char(1),
  INFO char(1),
  SEVERITY tinyint(3),
  TADIRFLAG char(1),
  PRIMARY KEY  (RUNID,SUBMITNR,ORDERNR,PROGRAMM,TESTID,UCCHECK),
  KEY `TVERM~KEY`  (PROGRAMM,TESTID,UCCHECK)
) DEFAULT CHARSET=latin1;

update t1 set `ETEXT` = '', `ETEXT_TYPE`='', `INFO`='', `SEVERITY`='', `TADIRFLAG`=''
WHERE
 `RUNID`= '' AND `SUBMITNR`= '' AND `ORDERNR`='' AND `PROGRAMM`='' AND
 `TESTID`='' AND `UCCHECK`='';

drop table t1;

--echo #
--echo # Bug#50402 Optimizer producing wrong results when using Index Merge on InnoDB
--echo #
CREATE TABLE t1 (f1 INT, PRIMARY KEY (f1));
INSERT INTO t1 VALUES (2);
CREATE TABLE t2 (f1 INT, f2 INT, f3 char(1),
                 PRIMARY KEY (f1), KEY (f2), KEY (f3) );
INSERT INTO t2 VALUES (1, 1, 'h'), (2, 3, 'h'), (3, 2, ''), (4, 2, '');

SELECT t1.f1 FROM t1
WHERE (SELECT COUNT(*) FROM t2 WHERE t2.f3 = 'h' AND t2.f2 = t1.f1) = 0 AND t1.f1 = 2;

-- disable_query_log
-- disable_result_log
analyze table t1;
analyze table t2;
-- enable_result_log
-- enable_query_log

--replace_regex $elide_costs
EXPLAIN SELECT t1.f1 FROM t1
WHERE (SELECT COUNT(*) FROM t2 WHERE t2.f3 = 'h' AND t2.f2 = t1.f1) = 0 AND t1.f1 = 2;

DROP TABLE t1,t2;

--echo #
--echo # Bug#19055268 ASSERT `!TABLE || (!TABLE->READ_SET || BITMAP_IS_SET(TABLE->READ_SET,...) FAILED
--echo #

SET sql_mode = '';

CREATE TABLE t1 (
  pk int(11) NOT NULL AUTO_INCREMENT,
  col_int_nokey int(11) NOT NULL,
  col_int_key int(11) NOT NULL,
  col_date_key date NOT NULL,
  col_date_nokey date NOT NULL,
  col_time_key time NOT NULL,
  col_time_nokey time NOT NULL,
  col_datetime_key datetime NOT NULL,
  col_datetime_nokey datetime NOT NULL,
  col_varchar_key varchar(1) NOT NULL,
  col_varchar_nokey varchar(1) NOT NULL,
  PRIMARY KEY (pk),
  KEY col_int_key (col_int_key),
  KEY col_date_key (col_date_key),
  KEY col_time_key (col_time_key),
  KEY col_datetime_key (col_datetime_key),
  KEY col_varchar_key (col_varchar_key,col_int_key)
);

INSERT INTO t1 VALUES
(1,2,1,'0000-00-00','0000-00-00','00:00:07','00:00:25','0000-00-00 00:00:00','0000-00-00 00:00:00','2','2'),
(109,25,1,'0000-00-00','0000-00-00','00:00:07','00:00:25','0000-00-00 00:00:00','0000-00-00 00:00:00','1','7');

CREATE TABLE t2 (
  pk int(11) NOT NULL AUTO_INCREMENT,
  col_int_nokey int(11) NOT NULL,
  col_int_key int(11) NOT NULL,
  col_date_key date NOT NULL,
  col_date_nokey date NOT NULL,
  col_time_key time NOT NULL,
  col_time_nokey time NOT NULL,
  col_datetime_key datetime NOT NULL,
  col_datetime_nokey datetime NOT NULL,
  col_varchar_key varchar(1) NOT NULL,
  col_varchar_nokey varchar(1) NOT NULL,
  PRIMARY KEY (pk),
  KEY col_int_key (col_int_key),
  KEY col_date_key (col_date_key),
  KEY col_time_key (col_time_key),
  KEY col_datetime_key (col_datetime_key),
  KEY col_varchar_key (col_varchar_key,col_int_key)
);


INSERT INTO t2 VALUES
(10,2,4,'2000-04-16','2000-04-16','17:02:34','17:02:34','2009-08-04 02:49:10','2009-08-04 02:49:10','u','u'),
(11,6,5,'1900-01-01','1900-01-01','12:31:32','12:31:32','2004-05-24 22:22:06','2004-05-24 22:22:06','b','b'),
(12,6,0,'2005-05-14','2005-05-14','16:49:31','16:49:31','1900-01-01 00:00:00','1900-01-01 00:00:00','c','c'),
(13,94,140,'2001-05-23','2001-05-23','00:00:00','00:00:00','2003-07-28 07:31:49','2003-07-28 07:31:49','f','f'),
(14,10,230,'2000-06-04','2000-06-04','20:23:27','20:23:27','2000-02-04 21:03:36','2000-02-04 21:03:36','g','g'),
(15,8,2,'1900-01-01','1900-01-01','10:31:09','10:31:09','2005-05-22 11:48:12','2005-05-22 11:48:12','q','q'),
(16,43,110,'2008-03-18','2008-03-18','09:58:35','09:58:35','1900-01-01 00:00:00','1900-01-01 00:00:00','n','n'),
(17,1,8,'2007-02-25','2007-02-25','05:39:28','05:39:28','2005-05-06 00:53:37','2005-05-06 00:53:37','c','c'),
(18,6,5,'2001-11-22','2001-11-22','00:00:00','00:00:00','2002-08-08 23:43:46','2002-08-08 23:43:46','x','x'),
(19,5,8,'0000-00-00','0000-00-00','00:00:00','00:00:00','2009-10-26 04:07:22','2009-10-26 04:07:22','n','n'),
(20,1,5,'2001-08-07','2001-08-07','14:06:09','14:06:09','2005-03-12 22:55:23','2005-03-12 22:55:23','r','r'),
(21,195,70,'2000-08-03','2000-08-03','00:00:00','00:00:00','2008-05-03 22:34:19','2008-05-03 22:34:19','j','j'),
(22,1,0,'2007-04-21','2007-04-21','17:43:27','17:43:27','2001-10-12 13:30:14','2001-10-12 13:30:14','l','l'),
(23,0,9,'2000-06-13','2000-06-13','12:23:15','12:23:15','2007-10-07 03:53:06','2007-10-07 03:53:06','h','h'),
(24,7,3,'0000-00-00','0000-00-00','19:35:06','19:35:06','2005-12-27 20:58:56','2005-12-27 20:58:56','z','z'),
(25,1,9,'2004-07-08','2004-07-08','10:16:08','10:16:08','2009-05-16 18:44:21','2009-05-16 18:44:21','z','z'),
(26,4,3,'2003-06-01','2003-06-01','00:00:00','00:00:00','2008-10-02 00:00:00','2008-10-02 00:00:00','d','d'),
(27,76,22,'0000-00-00','0000-00-00','16:51:15','16:51:15','2001-03-20 00:00:00','2001-03-20 00:00:00','l','l'),
(28,6,1,'1900-01-01','1900-01-01','00:00:00','00:00:00','1900-01-01 00:00:00','1900-01-01 00:00:00','s','s'),
(29,7,3,'2007-01-22','2007-01-22','00:00:00','00:00:00','2000-01-10 23:19:37','2000-01-10 23:19:37','k','k'),
(30,6,3,'2006-10-19','2006-10-19','21:46:20','21:46:20','2008-06-21 16:56:37','2008-06-21 16:56:37','l','l'),
(31,6,8,'2005-02-27','2005-02-27','00:00:00','00:00:00','2002-03-01 00:34:37','2002-03-01 00:34:37','t','t'),
(32,1,4,'2001-09-10','2001-09-10','03:34:16','03:34:16','2003-02-05 18:41:00','2003-02-05 18:41:00','n','n'),
(33,5,1,'2002-09-06','2002-09-06','11:25:31','11:25:31','2004-01-18 13:28:24','2004-01-18 13:28:24','z','z'),
(34,2,3,'2005-01-15','2005-01-15','17:00:53','17:00:53','2002-12-03 01:06:27','2002-12-03 01:06:27','f','f'),
(35,1,3,'1900-01-01','1900-01-01','23:32:47','23:32:47','2006-03-03 02:58:59','2006-03-03 02:58:59','q','q'),
(36,185,63,'2006-12-20','2006-12-20','00:00:00','00:00:00','2002-07-01 10:13:39','2002-07-01 10:13:39','l','l'),
(37,3,4,'2002-04-17','2002-04-17','12:15:05','12:15:05','2009-01-13 20:47:01','2009-01-13 20:47:01','a','a'),
(38,7,8,'2004-01-10','2004-01-10','22:26:38','22:26:38','2008-07-18 13:35:29','2008-07-18 13:35:29','c','c'),
(39,8,6,'2008-03-24','2008-03-24','00:00:00','00:00:00','2001-03-15 02:44:57','2001-03-15 02:44:57','c','c'),
(40,1,4,'2007-08-18','2007-08-18','00:00:00','00:00:00','2000-06-25 14:03:55','2000-06-25 14:03:55','i','i'),
(41,3,9,'2005-07-22','2005-07-22','00:00:00','00:00:00','2005-12-13 21:32:03','2005-12-13 21:32:03','r','r'),
(42,8,2,'2008-07-12','2008-07-12','08:59:37','08:59:37','2009-03-18 19:27:29','2009-03-18 19:27:29','g','g'),
(43,159,118,'2003-08-18','2003-08-18','07:19:50','07:19:50','2004-04-10 02:50:59','2004-04-10 02:50:59','l','l'),
(44,112,10,'2002-02-26','2002-02-26','03:51:46','03:51:46','2008-09-27 06:49:19','2008-09-27 06:49:19','w','w'),
(45,0,9,'2007-07-26','2007-07-26','15:57:32','15:57:32','2006-02-04 15:10:41','2006-02-04 15:10:41','b','b'),
(46,2,9,'1900-01-01','1900-01-01','02:57:08','02:57:08','2004-01-04 18:45:14','2004-01-04 18:45:14','i','i'),
(47,1,1,'2000-07-26','2000-07-26','00:21:15','00:21:15','2009-04-04 02:52:09','2009-04-04 02:52:09','o','o'),
(48,7,6,'2005-03-06','2005-03-06','23:35:02','23:35:02','2001-03-20 12:50:17','2001-03-20 12:50:17','f','f'),
(49,5,2,'2005-04-16','2005-04-16','09:21:51','09:21:51','2001-11-15 10:51:46','2001-11-15 10:51:46','a','a'),
(50,2,2,'2004-04-09','2004-04-09','19:21:02','19:21:02','2007-08-13 00:00:00','2007-08-13 00:00:00','j','j'),
(51,5,3,'2003-05-13','2003-05-13','03:16:22','03:16:22','1900-01-01 00:00:00','1900-01-01 00:00:00','k','k'),
(52,5,0,'0000-00-00','0000-00-00','00:00:00','00:00:00','2008-01-24 08:37:15','2008-01-24 08:37:15','x','x'),
(53,2,8,'2000-05-27','2000-05-27','18:19:51','18:19:51','2003-03-02 10:18:02','2003-03-02 10:18:02','r','r'),
(54,2,5,'2004-07-04','2004-07-04','01:42:17','01:42:17','2003-04-25 07:35:13','2003-04-25 07:35:13','t','t'),
(55,9,0,'2006-03-07','2006-03-07','00:00:00','00:00:00','2006-07-11 23:31:13','2006-07-11 23:31:13','p','p'),
(56,3,1,'2002-08-17','2002-08-17','20:32:51','20:32:51','2009-08-08 16:55:59','2009-08-08 16:55:59','g','g'),
(57,2,9,'2003-06-08','2003-06-08','10:04:19','10:04:19','1900-01-01 00:00:00','1900-01-01 00:00:00','q','q'),
(58,7,4,'1900-01-01','1900-01-01','06:39:26','06:39:26','2000-12-22 10:27:41','2000-12-22 10:27:41','m','m'),
(59,4,7,'2003-10-09','2003-10-09','15:48:26','15:48:26','2002-07-20 00:00:00','2002-07-20 00:00:00','l','l'),
(60,5,6,'2007-08-12','2007-08-12','02:17:23','02:17:23','2007-09-24 00:12:11','2007-09-24 00:12:11','d','d'),
(61,3,3,'2008-12-13','2008-12-13','07:48:47','07:48:47','2009-01-05 06:13:21','2009-01-05 06:13:21','n','n'),
(62,0,8,'0000-00-00','0000-00-00','09:09:14','09:09:14','2008-11-23 18:13:35','2008-11-23 18:13:35','l','l'),
(63,1,2,'2003-07-27','2003-07-27','22:46:23','22:46:23','2002-12-19 00:00:00','2002-12-19 00:00:00','c','c'),
(64,5,5,'2003-04-13','2003-04-13','19:42:42','19:42:42','2003-04-03 16:16:35','2003-04-03 16:16:35','v','v'),
(65,0,3,'2007-04-25','2007-04-25','15:15:32','15:15:32','2009-04-13 13:44:41','2009-04-13 13:44:41','z','z'),
(66,4,4,'2000-03-04','2000-03-04','02:50:26','02:50:26','2007-04-19 20:49:24','2007-04-19 20:49:24','h','h'),
(67,0,9,'2001-08-03','2001-08-03','14:44:31','14:44:31','2002-11-15 23:36:04','2002-11-15 23:36:04','y','y'),
(68,2,0,'2006-04-13','2006-04-13','00:00:00','00:00:00','2000-05-16 00:00:00','2000-05-16 00:00:00','o','o'),
(69,2,1,'2006-04-22','2006-04-22','07:31:27','07:31:27','2002-06-08 06:51:27','2002-06-08 06:51:27','h','h'),
(70,8,5,'2004-08-26','2004-08-26','10:48:57','10:48:57','2006-12-07 01:49:26','2006-12-07 01:49:26','h','h'),
(71,8,6,'2002-11-26','2002-11-26','00:00:00','00:00:00','1900-01-01 00:00:00','1900-01-01 00:00:00','c','c'),
(72,5,2,'2002-01-07','2002-01-07','23:06:00','23:06:00','2000-09-27 16:26:23','2000-09-27 16:26:23','m','m'),
(73,1,0,'2005-05-07','2005-05-07','05:56:02','05:56:02','1900-01-01 00:00:00','1900-01-01 00:00:00','s','s'),
(74,0,0,'2001-04-21','2001-04-21','10:44:22','10:44:22','2008-02-13 04:28:14','2008-02-13 04:28:14','t','t'),
(75,7,9,'2000-09-01','2000-09-01','00:00:00','00:00:00','2000-03-10 06:17:52','2000-03-10 06:17:52','r','r'),
(76,6,8,'2004-07-06','2004-07-06','01:44:26','01:44:26','2003-05-23 00:00:00','2003-05-23 00:00:00','u','u'),
(77,0,6,'0000-00-00','0000-00-00','00:00:00','00:00:00','1900-01-01 00:00:00','1900-01-01 00:00:00','h','h'),
(78,6,0,'2001-02-21','2001-02-21','17:01:49','17:01:49','2003-10-27 12:45:52','2003-10-27 12:45:52','p','p'),
(79,9,0,'2004-01-28','2004-01-28','16:27:14','16:27:14','2004-08-06 00:00:00','2004-08-06 00:00:00','z','z'),
(80,5,3,'2008-10-07','2008-10-07','00:00:00','00:00:00','2001-10-05 05:13:39','2001-10-05 05:13:39','h','h'),
(81,5,5,'0000-00-00','0000-00-00','10:34:22','10:34:22','2001-03-25 04:24:51','2001-03-25 04:24:51','q','q'),
(82,6,0,'2008-03-04','2008-03-04','10:48:51','10:48:51','2006-08-20 11:54:04','2006-08-20 11:54:04','l','l'),
(83,1,0,'2009-05-08','2009-05-08','16:55:06','16:55:06','2003-05-16 18:14:52','2003-05-16 18:14:52','p','p'),
(84,1,1,'2005-03-19','2005-03-19','14:46:32','14:46:32','2009-10-24 13:39:35','2009-10-24 13:39:35','t','t'),
(85,9,8,'2007-09-14','2007-09-14','16:26:50','16:26:50','2001-08-10 00:00:00','2001-08-10 00:00:00','o','o'),
(86,249,190,'2007-02-10','2007-02-10','06:35:32','06:35:32','2001-09-06 10:15:08','2001-09-06 10:15:08','o','o'),
(87,3,0,'2000-06-12','2000-06-12','02:28:38','02:28:38','2006-09-27 00:00:00','2006-09-27 00:00:00','p','p'),
(88,6,4,'2002-07-06','2002-07-06','00:00:00','00:00:00','2002-02-27 20:12:29','2002-02-27 20:12:29','e','e'),
(89,7,2,'2001-02-25','2001-02-25','04:52:29','04:52:29','1900-01-01 00:00:00','1900-01-01 00:00:00','u','u'),
(90,5,8,'2000-04-27','2000-04-27','00:00:00','00:00:00','2005-08-18 00:00:00','2005-08-18 00:00:00','z','z'),
(91,4,0,'2005-08-23','2005-08-23','09:11:04','09:11:04','2000-10-15 15:52:54','2000-10-15 15:52:54','j','j'),
(92,24,137,'2006-01-07','2006-01-07','22:38:18','22:38:18','2007-11-22 13:52:22','2007-11-22 13:52:22','e','e'),
(93,3,7,'2007-07-08','2007-07-08','13:23:13','13:23:13','2005-09-20 09:09:48','2005-09-20 09:09:48','k','k'),
(94,179,153,'2000-01-06','2000-01-06','15:33:10','15:33:10','2008-01-11 17:35:34','2008-01-11 17:35:34','d','d'),
(95,5,7,'0000-00-00','0000-00-00','09:20:10','09:20:10','2002-11-01 00:00:00','2002-11-01 00:00:00','q','q'),
(96,7,9,'2002-02-22','2002-02-22','11:19:10','11:19:10','2009-06-27 00:00:00','2009-06-27 00:00:00','j','j'),
(97,7,2,'2001-04-17','2001-04-17','08:08:41','08:08:41','2000-09-25 14:11:58','2000-09-25 14:11:58','j','j'),
(98,1,0,'2005-06-26','2005-06-26','10:17:36','10:17:36','2004-01-05 04:49:39','2004-01-05 04:49:39','e','e'),
(99,5,6,'2008-09-14','2008-09-14','00:00:00','00:00:00','1900-01-01 00:00:00','1900-01-01 00:00:00','f','f'),
(100,7,6,'0000-00-00','0000-00-00','01:05:02','01:05:02','2001-01-17 23:16:24','2001-01-17 23:16:24','a','a'),
(101,9,2,'2001-12-20','2001-12-20','00:00:00','00:00:00','1900-01-01 00:00:00','1900-01-01 00:00:00','o','o'),
(102,5,9,'2001-11-05','2001-11-05','00:00:00','00:00:00','2008-07-15 00:00:00','2008-07-15 00:00:00','y','y'),
(103,4,4,'1900-01-01','1900-01-01','15:32:04','15:32:04','2001-03-06 10:00:37','2001-03-06 10:00:37','d','d'),
(104,8,7,'2009-02-13','2009-02-13','20:25:36','20:25:36','2004-08-15 00:00:00','2004-08-15 00:00:00','z','z'),
(105,8,6,'2008-03-25','2008-03-25','21:33:53','21:33:53','2006-10-12 06:13:06','2006-10-12 06:13:06','s','s');


-- disable_result_log
analyze table t1;
analyze table t2;
-- enable_result_log

--disable_warnings
--replace_regex $elide_costs
EXPLAIN UPDATE t1 SET col_time_nokey = 25
WHERE (col_varchar_key, col_varchar_nokey) IN (
  SELECT col_int_key, col_int_key
  FROM t2
  WHERE (col_time_key, col_int_key, col_int_key) IN (
    (0, 'a', 'm'), (0, 'n', 7)
  )
);

UPDATE t1 SET col_time_nokey = 25
WHERE (col_varchar_key, col_varchar_nokey) IN (
  SELECT col_int_key, col_int_key
  FROM t2
  WHERE (col_time_key, col_int_key, col_int_key) IN (
    (0, 'a', 'm'), (0, 'n', 7)
  )
);
--enable_warnings

DROP TABLE t1, t2;
SET sql_mode = default;

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

eval SET SESSION DEFAULT_STORAGE_ENGINE = $engine_type;

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

# Verify that range scan on CPK is ROR
# (use index_intersection because it is impossible to check that for index union)
# Column 9, rows, can change depending on innodb-page-size.
--replace_column 9 ROWS
explain select * from t1 where pk1 = 1 and pk2 < 80  and key1=0;
# CPK scan + 1 ROR range scan is a special case
--sorted_result
select * from t1 where pk1 = 1 and pk2 < 80  and key1=0;

# Verify that CPK fields are considered to be covered by index scans
explain select pk1,pk2 from t1 where key1 = 10 and key2=10 and 2*pk1+1 < 2*96+1;
select pk1,pk2 from t1 where key1 = 10 and key2=10 and 2*pk1+1 < 2*96+1;

# Verify that CPK is always used for index intersection scans
# (this is because it is used as a filter, not for retrieval)
# The expected number of rows can vary depending on page size
--replace_column 9 ROWS
explain select * from t1 where badkey=1 and key1=10;
# The expected number of rows can vary depending on page size
--replace_column 9 ROWS
explain select * from t1 where pk1 < 7500 and key1 = 10;

# Verify that keys with 'tails' of PK members are ok.
explain select * from t1 where pktail1ok=1 and key1=10;
explain select * from t1 where pktail2ok=1 and key1=10;

# Note: The following is actually a deficiency, it uses sort_union currently.
#       This comment refers to InnoDB and is probably not valid for other engines.
# The expected number of rows can vary depending on page size
--replace_column 9 ROWS
explain select * from t1 where (pktail2ok=1 and pk1< 50000) or key1=10;

# The expected column used for KEY vary depending on page size
# The expected number of rows can vary depending on page size and platform
--replace_column 6 EITHER_KEY 9 ROWS
explain select * from t1 where pktail3bad=1 and key1=10;
# The expected column used for KEY vary depending on page size
--replace_column 9 ROWS
explain select * from t1 where pktail4bad=1 and key1=10;
# The expected column used for KEY vary depending on page size
--replace_column 9 ROWS
explain select * from t1 where pktail5bad=1 and key1=10;

# Test for problem with innodb key values prefetch buffer:
explain select pk1,pk2,key1,key2 from t1 where key1 = 10 and key2=10 limit 10;
select pk1,pk2,key1,key2 from t1 where key1 = 10 and key2=10 limit 10;

drop table t1;
# Testcase for BUG#4984
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

EXPLAIN SELECT t1.f1 FROM t1
WHERE (SELECT COUNT(*) FROM t2 WHERE t2.f3 = 'h' AND t2.f2 = t1.f1) = 0 AND t1.f1 = 2;

DROP TABLE t1,t2;

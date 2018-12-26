
#try to crash gcc 2.96
--disable_warnings
drop table if exists t1;
--enable_warnings

CREATE TABLE t1 (
  kodoboru varchar(10) default NULL,
  obor tinytext,
  aobor tinytext,
  UNIQUE INDEX kodoboru (kodoboru),
  FULLTEXT KEY obor (obor),
  FULLTEXT KEY aobor (aobor)
);
INSERT INTO t1 VALUES ('0101000000','aaa','AAA');
INSERT INTO t1 VALUES ('0102000000','bbb','BBB');
INSERT INTO t1 VALUES ('0103000000','ccc','CCC');
INSERT INTO t1 VALUES ('0104000000','xxx','XXX');

select * from t1;
drop table t1;

# End of 4.1 tests

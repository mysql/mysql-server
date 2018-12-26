drop table if exists t1;
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
kodoboru	obor	aobor
0101000000	aaa	AAA
0102000000	bbb	BBB
0103000000	ccc	CCC
0104000000	xxx	XXX
drop table t1;

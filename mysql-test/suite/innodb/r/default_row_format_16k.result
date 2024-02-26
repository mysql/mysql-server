#
# BUG#21816041 - ROW_FORMAT=DYNAMIC IS RESTRICTIVE THAN ROW_FORMAT=COMPACT
#
CREATE TABLE t1 (a VARCHAR(300), b VARCHAR(16383) CHARACTER SET UTF8MB3,
PRIMARY KEY(a(255), b(255))) ENGINE=INNODB ROW_FORMAT=DYNAMIC;
Warnings:
Warning	1287	'utf8mb3' is deprecated and will be removed in a future release. Please use utf8mb4 instead
DROP TABLE t1;
CREATE TABLE t1 (a VARCHAR(300), b VARCHAR(16383) CHARACTER SET UTF8MB3,
PRIMARY KEY(a(255))) ENGINE=INNODB ROW_FORMAT=DYNAMIC;
Warnings:
Warning	1287	'utf8mb3' is deprecated and will be removed in a future release. Please use utf8mb4 instead
DROP TABLE t1;
CREATE TABLE t1 (a VARCHAR(300), b VARCHAR(16280) CHARACTER SET UTF8MB4,
PRIMARY KEY(a(255), b(255))) ENGINE=INNODB ROW_FORMAT=DYNAMIC charset latin1;
DROP TABLE t1;
CREATE TABLE t1 (a VARCHAR(16383) CHARACTER SET UTF8MB4) ENGINE=INNODB
ROW_FORMAT=DYNAMIC;
ALTER TABLE t1 ADD  PRIMARY KEY(a(100));
DROP TABLE t1;
CREATE TABLE t1 (a VARCHAR(16383) CHARACTER SET UTF8MB3,
PRIMARY KEY(a(255))) ENGINE=INNODB ROW_FORMAT=DYNAMIC;
Warnings:
Warning	1287	'utf8mb3' is deprecated and will be removed in a future release. Please use utf8mb4 instead
DROP TABLE t1;
CREATE TABLE t1 (a VARCHAR(12383) CHARACTER SET UTF8MB3,
PRIMARY KEY(a(255)), KEY k1(a(1024))) ENGINE=INNODB ROW_FORMAT=DYNAMIC;
Warnings:
Warning	1287	'utf8mb3' is deprecated and will be removed in a future release. Please use utf8mb4 instead
INSERT INTO t1 VALUES(REPEAT('A', 500));
UPDATE t1 SET a = REPEAT('B', 999);
DROP TABLE t1;
CREATE TABLE t1 (a VARCHAR(16383) CHARACTER SET UTF8MB3,
PRIMARY KEY(a(255))) ENGINE=INNODB ROW_FORMAT=DYNAMIC;
Warnings:
Warning	1287	'utf8mb3' is deprecated and will be removed in a future release. Please use utf8mb4 instead
DROP TABLE t1;
CREATE TABLE t1 (a VARCHAR(16383), b VARCHAR(3000), c TEXT,
PRIMARY KEY(a(255), c(10)), KEY k1(a(500)), KEY k2(b(72), c(3000)))
charset latin1 ENGINE=INNODB ROW_FORMAT=DYNAMIC;
DROP TABLE t1;
CREATE TABLE t1 (a VARCHAR(3070) , b TEXT,
PRIMARY KEY(a(400)), KEY k1(a)) ENGINE=INNODB ROW_FORMAT=DYNAMIC charset latin1;
DROP TABLE t1;
CREATE TABLE t1 (a VARCHAR(3000) , b TEXT, c INT, d TEXT,
PRIMARY KEY(a, b(3), c), KEY k1(a, b(25))) charset latin1 ENGINE=INNODB ROW_FORMAT=DYNAMIC;
DROP TABLE t1;
CREATE TABLE t1 (a INT , b TEXT,
PRIMARY KEY(a, b(3)), KEY k1(a)) ENGINE=INNODB ROW_FORMAT=DYNAMIC;
DROP TABLE t1;
CREATE TABLE t1(a CHAR(10), KEY(a(5)));
DROP TABLE t1;
CREATE TABLE t1(a CHAR(255), KEY(a(200)));
DROP TABLE t1;
CREATE TABLE t1(a CHAR(255), KEY(a));
DROP TABLE t1;
CREATE TABLE t1(a VARCHAR(4000), KEY(a(2000))) charset latin1;
DROP TABLE t1;
CREATE TABLE t1(a VARCHAR(2000), PRIMARY KEY (a), KEY k1(a(2000))) charset latin1;
DROP TABLE t1;
CREATE TABLE t1(a VARCHAR(5000), PRIMARY KEY (a(3000)), KEY k1(a(2000))) charset latin1;
DROP TABLE t1;
CREATE TABLE t1 (
c1 INT AUTO_INCREMENT,
c2 GEOMETRY NOT NULL SRID 0,
PRIMARY KEY(c1),
SPATIAL INDEX(c2)
) ENGINE=INNODB;
DROP TABLE t1;
CREATE TABLE t1(a TEXT, b TEXT, c TEXT, FULLTEXT `AB`(a, b), FULLTEXT `C1`(c));
DROP TABLE t1;
CREATE TABLE t1 (
col_1_VARBINARY VARBINARY (4000) ,
col_2_VARCHAR VARCHAR (4000) ,
col_3_TEXT TEXT (4000),
col_4_BLOB BLOB (4000),
col_5_TEXT TEXT (4000),
col_6_TEXT TEXT (4000),
col_7_TEXT TEXT (4000),
col_8_TEXT TEXT (4000),
col_9_TEXT TEXT (4000),
col_10_TEXT TEXT (4000),
col_11_TEXT TEXT (4000),
col_12_TEXT TEXT (4000),
col_13_TEXT TEXT (4000),
col_14_TEXT TEXT (4000),
col_15_TEXT TEXT (4000),
col_16_TEXT TEXT (4000),
col_17_TEXT TEXT (4000),
col_18_TEXT TEXT (4000),
col_19_TEXT TEXT (4000),
col_20_TEXT TEXT (4000),
col_21_TEXT TEXT (4000),
col_22_TEXT TEXT (4000),
col_23_TEXT TEXT (4000),
col_24_BINARY BINARY (255)
) ROW_FORMAT=DYNAMIC, engine = innodb;
INSERT INTO t1 VALUES(
repeat("a", 4000), repeat("o", 4000),
repeat("a", 4000), repeat("o", 4000),
repeat("a", 4000), repeat("o", 4000),
repeat("a", 4000), repeat("o", 4000),
repeat("a", 4000), repeat("o", 4000),
repeat("a", 4000), repeat("o", 4000),
repeat("a", 4000), repeat("o", 4000),
repeat("a", 4000), repeat("o", 4000),
repeat("a", 4000), repeat("o", 4000),
repeat("a", 4000), repeat("o", 4000),
repeat("a", 4000), repeat("o", 4000),
repeat("a", 4000),  repeat("a", 255)
);
CREATE INDEX prefix_idx1 ON t1(col_1_VARBINARY (767));
CREATE INDEX prefix_idx2 ON t1(col_2_VARCHAR (767));
CREATE INDEX prefix_idx3 ON t1(col_3_TEXT (767));
CREATE INDEX prefix_idx4 ON t1(col_4_BLOB (767));
CREATE INDEX prefix_idx5 ON t1(col_5_TEXT (767));
CREATE INDEX prefix_idx6 ON t1(col_6_TEXT (767));
CREATE INDEX prefix_idx7 ON t1(col_7_TEXT (767));
CREATE INDEX prefix_idx8 ON t1(col_8_TEXT (767));
CREATE INDEX prefix_idx9 ON t1(col_9_TEXT (767));
CREATE INDEX prefix_idx10 ON t1(col_10_TEXT (767));
CREATE INDEX prefix_idx11 ON t1(col_11_TEXT (767));
CREATE INDEX prefix_idx12 ON t1(col_12_TEXT (767));
CREATE INDEX prefix_idx13 ON t1(col_13_TEXT (767));
CREATE INDEX prefix_idx14 ON t1(col_14_TEXT (767));
CREATE INDEX prefix_idx15 ON t1(col_15_TEXT (767));
CREATE INDEX prefix_idx16 ON t1(col_16_TEXT (767));
CREATE INDEX prefix_idx17 ON t1(col_17_TEXT (767));
CREATE INDEX prefix_idx18 ON t1(col_18_TEXT (767));
CREATE INDEX prefix_idx19 ON t1(col_19_TEXT (767));
CREATE INDEX prefix_idx24 ON t1(col_24_BINARY  (250));
UPDATE t1 SET col_1_VARBINARY = repeat("x", 4000), col_2_VARCHAR=
repeat("c",
4000) WHERE col_1_VARBINARY = repeat("a", 4000) AND col_2_VARCHAR =
repeat("o", 4000);
ERROR HY000: Undo log record is too big.
DROP TABLE t1;
CREATE TABLE t1(a INT, b FLOAT, c CHAR(10),
v1 VARCHAR(1001),
v2 VARCHAR(1002),
vx_blob BLOB,
vx_text TEXT,
vx_varchar VARCHAR(3005),
v3 VARCHAR(1006),
v4 VARCHAR(1007),
PRIMARY KEY (a,b,c,vx_text(200))) charset latin1 ROW_FORMAT=DYNAMIC;
CREATE INDEX idx1 ON t1(a,b,c);
CREATE INDEX idx2 ON t1(a,v1(101));
CREATE INDEX idx3 ON t1(b,v2(102));
CREATE INDEX idx4 ON t1(c,vx_blob(3003));
CREATE INDEX idx5 ON t1(a,vx_text(1004));
SHOW WARNINGS;
Level	Code	Message
INSERT INTO t1 VALUES(1,1,'111111111',
repeat('a',1001),
repeat('a',1002),
repeat('a',4003),
repeat('a',3004),
repeat('a',3005),
repeat('a',1006),
repeat('a',1007));
UPDATE t1 SET
v1=repeat('b',1001),
v2=repeat('b',1002),
vx_blob=repeat('b',4003),
vx_varchar=repeat('b',3005),
v3=repeat('b',1006),
v4=repeat('b',1007);
DROP TABLE t1;
create table t1(a int, b float, c char(10), d date,
vx_varchar varchar(10000), vx_text text, vx_blob BLOB,
v1 varchar(1000), v2 varchar(1000),
v3 varchar(1000), v4 varchar(3000))
charset latin1 ROW_FORMAT=DYNAMIC;
create index idx1 on t1(a,b,c,d);
create index idx2 on t1(vx_varchar(2000));
create index idx3 on t1(vx_text(2000));
create index idx8 on t1(vx_blob(2000));
create index idx4 on t1(v1);
create index idx5 on t1(v2);
create index idx6 on t1(v3);
create index idx7 on t1(v4);
insert into t1 values(1,1,'111111111', '2011-07-11',
repeat('a',10000),
repeat('a',10000),
repeat('a',10000),
repeat('a',1000), repeat('a',1000),
repeat('a',1000), repeat('a',3000));
update t1 set a=2,b=2,c='2222222222',d='2011-07-12',
vx_varchar=repeat('b',10000),
vx_text=repeat('b',10000),
vx_blob=repeat('b',10000),
v1=repeat('b',1000), v2=repeat('b',1000),
v3=repeat('b',1000), v4=repeat('b',3000);
ERROR HY000: Undo log record is too big.
drop table t1;
CREATE TABLE t1(x VARCHAR(300), y VARCHAR(200), KEY k1(y,x));
ALTER TABLE t1 ADD INDEX k2(x(10));
DROP TABLE t1;
SHOW CREATE TABLE t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `c1` text,
  `c2` text,
  `c3` text,
  `c4` text,
  `c5` text,
  `c6` text,
  `c7` text,
  `c8` text,
  `c9` text,
  `c10` text,
  `c11` text,
  `c12` text,
  `c13` text,
  `c14` text,
  `c15` text,
  `c16` text,
  `c17` text,
  `c18` text,
  `c19` text,
  `c20` text,
  `c21` text,
  `c22` text,
  `c23` text,
  `c24` text,
  `c25` text,
  `c26` text,
  `c27` text,
  `c28` text,
  `c29` text,
  `c30` text,
  `c31` text,
  `c32` text,
  `c33` text,
  `c34` text,
  `c35` text,
  `c36` text,
  `c37` text,
  `c38` text,
  `c39` text,
  `c40` text,
  `c41` text,
  `c42` text,
  `c43` text,
  `c44` text,
  `c45` text,
  `c46` text,
  `c47` text,
  `c48` text,
  `c49` text,
  `c50` text,
  `c51` text,
  `c52` text,
  `c53` text,
  `c54` text,
  `c55` text,
  `c56` text,
  `c57` text,
  `c58` text,
  `c59` text,
  `c60` text,
  `c61` text,
  `c62` text,
  `c63` text,
  `c64` text,
  `c65` text,
  `c66` text,
  `c67` text,
  `c68` text,
  `c69` text,
  `c70` text,
  `c71` text,
  `c72` text,
  `c73` text,
  `c74` text,
  `c75` text,
  `c76` text,
  `c77` text,
  `c78` text,
  `c79` text,
  `c80` text,
  `c81` text,
  `c82` text,
  `c83` text,
  `c84` text,
  `c85` text,
  `c86` text,
  `c87` text,
  `c88` text,
  `c89` text,
  `c90` text,
  `c91` text,
  `c92` text,
  `c93` text,
  `c94` text,
  `c95` text,
  `c96` text,
  `c97` text,
  `c98` text,
  `c99` text,
  `c100` text,
  `c101` text,
  `c102` text,
  `c103` text,
  `c104` text,
  `c105` text,
  `c106` text,
  `c107` text,
  `c108` text,
  `c109` text,
  `c110` text,
  `c111` text,
  `c112` text,
  `c113` text,
  `c114` text,
  `c115` text,
  `c116` text,
  `c117` text,
  `c118` text,
  `c119` text,
  `c120` text,
  `c121` text,
  `c122` text,
  `c123` text,
  `c124` text,
  `c125` text,
  `c126` text,
  `c127` text,
  `c128` text,
  `c129` text,
  `c130` text,
  `c131` text,
  `c132` text,
  `c133` text,
  `c134` text,
  `c135` text,
  `c136` text,
  `c137` text,
  `c138` text,
  `c139` text,
  `c140` text,
  `c141` text,
  `c142` text,
  `c143` text,
  `c144` text,
  `c145` text,
  `c146` text,
  `c147` text,
  `c148` text,
  `c149` text,
  `c150` text,
  `c151` text,
  `c152` text,
  `c153` text,
  `c154` text,
  `c155` text,
  `c156` text,
  `c157` text,
  `c158` text,
  `c159` text,
  `c160` text,
  `c161` text,
  `c162` text,
  `c163` text,
  `c164` text,
  `c165` text,
  `c166` text,
  `c167` text,
  `c168` text,
  `c169` text,
  `c170` text,
  `c171` text,
  `c172` text,
  `c173` text,
  `c174` text,
  `c175` text,
  `c176` text,
  `c177` text,
  `c178` text,
  `c179` text,
  `c180` text,
  `c181` text,
  `c182` text,
  `c183` text,
  `c184` text,
  `c185` text,
  `c186` text,
  `c187` text,
  `c188` text,
  `c189` text,
  `c190` text,
  `c191` text,
  `c192` text,
  `c193` text,
  `c194` text,
  `c195` text,
  `c196` text,
  KEY `k1` (`c1`(767)),
  KEY `k2` (`c2`(767)),
  KEY `k3` (`c3`(767)),
  KEY `k4` (`c4`(767)),
  KEY `k5` (`c5`(767)),
  KEY `k6` (`c6`(767)),
  KEY `k7` (`c7`(767)),
  KEY `k8` (`c8`(767)),
  KEY `k9` (`c9`(767)),
  KEY `k10` (`c10`(767)),
  KEY `k11` (`c11`(767)),
  KEY `k12` (`c12`(767)),
  KEY `k13` (`c13`(767)),
  KEY `k14` (`c14`(767)),
  KEY `k15` (`c15`(767)),
  KEY `k16` (`c16`(767)),
  KEY `k17` (`c17`(767)),
  KEY `k18` (`c18`(767)),
  KEY `k19` (`c19`(767)),
  KEY `k20` (`c20`(767)),
  KEY `k21` (`c21`(767)),
  KEY `k22` (`c22`(767)),
  KEY `k23` (`c23`(767)),
  KEY `k24` (`c24`(767)),
  KEY `k25` (`c25`(767)),
  KEY `k26` (`c26`(767)),
  KEY `k27` (`c27`(767)),
  KEY `k28` (`c28`(767)),
  KEY `k29` (`c29`(767)),
  KEY `k30` (`c30`(767)),
  KEY `k31` (`c31`(767)),
  KEY `k32` (`c32`(767)),
  KEY `k33` (`c33`(767)),
  KEY `k34` (`c34`(767)),
  KEY `k35` (`c35`(767)),
  KEY `k36` (`c36`(767)),
  KEY `k37` (`c37`(767)),
  KEY `k38` (`c38`(767)),
  KEY `k39` (`c39`(767)),
  KEY `k40` (`c40`(767)),
  KEY `k41` (`c41`(767)),
  KEY `k42` (`c42`(767)),
  KEY `k43` (`c43`(767)),
  KEY `k44` (`c44`(767)),
  KEY `k45` (`c45`(767)),
  KEY `k46` (`c46`(767)),
  KEY `k47` (`c47`(767)),
  KEY `k48` (`c48`(767)),
  KEY `k49` (`c49`(767)),
  KEY `k50` (`c50`(767)),
  KEY `k51` (`c51`(767)),
  KEY `k52` (`c52`(767)),
  KEY `k53` (`c53`(767)),
  KEY `k54` (`c54`(767)),
  KEY `k55` (`c55`(767)),
  KEY `k56` (`c56`(767)),
  KEY `k57` (`c57`(767)),
  KEY `k58` (`c58`(767)),
  KEY `k59` (`c59`(767)),
  KEY `k60` (`c60`(767)),
  KEY `k61` (`c61`(767)),
  KEY `k62` (`c62`(767)),
  KEY `k63` (`c63`(767)),
  KEY `k64` (`c64`(767))
) ENGINE=InnoDB DEFAULT CHARSET=latin1 ROW_FORMAT=DYNAMIC
UPDATE t1 SET
c1=REPEAT('a', 65535),
c2=REPEAT('b', 65535),
c3=REPEAT('c', 65535),
c4=REPEAT('d', 65535),
c5=REPEAT('e', 65535),
c6=REPEAT('f', 65535),
c7=REPEAT('g', 65535),
c8=REPEAT('h', 65535),
c9=REPEAT('i', 65535),
c10=REPEAT('j', 65535),
c11=REPEAT('k', 65535),
c12=REPEAT('l', 65535),
c13=REPEAT('m', 65535),
c14=REPEAT('n', 65535),
c15=REPEAT('o', 65535);
UPDATE t1 SET
c14=REPEAT('a', 65535),
c13=REPEAT('b', 65535),
c12=REPEAT('c', 65535),
c11=REPEAT('d', 65535),
c10=REPEAT('e', 65535),
c9=REPEAT('f', 65535),
c8=REPEAT('g', 65535),
c7=REPEAT('h', 65535),
c6=REPEAT('i', 65535),
c5=REPEAT('j', 65535),
c4=REPEAT('k', 65535),
c3=REPEAT('l', 65535),
c2=REPEAT('m', 65535),
c1=REPEAT('n', 65535);
ERROR HY000: Undo log record is too big.
UPDATE t1 SET
c14 = REPEAT('a', 65535);
DROP TABLE t1;
SHOW CREATE TABLE t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `c1` text,
  `c2` text,
  `c3` text,
  `c4` text,
  `c5` text,
  `c6` text,
  `c7` text,
  `c8` text,
  `c9` text,
  `c10` text,
  `c11` text,
  `c12` text,
  `c13` text,
  `c14` text,
  `c15` text,
  `c16` text,
  `c17` text,
  `c18` text,
  `c19` text,
  `c20` text,
  `c21` text,
  `c22` text,
  `c23` text,
  `c24` text,
  `c25` text,
  `c26` text,
  `c27` text,
  `c28` text,
  `c29` text,
  `c30` text,
  `c31` text,
  `c32` text,
  `c33` text,
  `c34` text,
  `c35` text,
  `c36` text,
  `c37` text,
  `c38` text,
  `c39` text,
  `c40` text,
  `c41` text,
  `c42` text,
  `c43` text,
  `c44` text,
  `c45` text,
  `c46` text,
  `c47` text,
  `c48` text,
  `c49` text,
  `c50` text,
  `c51` text,
  `c52` text,
  `c53` text,
  `c54` text,
  `c55` text,
  `c56` text,
  `c57` text,
  `c58` text,
  `c59` text,
  `c60` text,
  `c61` text,
  `c62` text,
  `c63` text,
  `c64` text,
  `c65` text,
  `c66` text,
  `c67` text,
  `c68` text,
  `c69` text,
  `c70` text,
  `c71` text,
  `c72` text,
  `c73` text,
  `c74` text,
  `c75` text,
  `c76` text,
  `c77` text,
  `c78` text,
  `c79` text,
  `c80` text,
  `c81` text,
  `c82` text,
  `c83` text,
  `c84` text,
  `c85` text,
  `c86` text,
  `c87` text,
  `c88` text,
  `c89` text,
  `c90` text,
  `c91` text,
  `c92` text,
  `c93` text,
  `c94` text,
  `c95` text,
  `c96` text,
  `c97` text,
  `c98` text,
  `c99` text,
  `c100` text,
  `c101` text,
  `c102` text,
  `c103` text,
  `c104` text,
  `c105` text,
  `c106` text,
  `c107` text,
  `c108` text,
  `c109` text,
  `c110` text,
  `c111` text,
  `c112` text,
  `c113` text,
  `c114` text,
  `c115` text,
  `c116` text,
  `c117` text,
  `c118` text,
  `c119` text,
  `c120` text,
  `c121` text,
  `c122` text,
  `c123` text,
  `c124` text,
  `c125` text,
  `c126` text,
  `c127` text,
  `c128` text,
  `c129` text,
  `c130` text,
  `c131` text,
  `c132` text,
  `c133` text,
  `c134` text,
  `c135` text,
  `c136` text,
  `c137` text,
  `c138` text,
  `c139` text,
  `c140` text,
  `c141` text,
  `c142` text,
  `c143` text,
  `c144` text,
  `c145` text,
  `c146` text,
  `c147` text,
  `c148` text,
  `c149` text,
  `c150` text,
  `c151` text,
  `c152` text,
  `c153` text,
  `c154` text,
  `c155` text,
  `c156` text,
  `c157` text,
  `c158` text,
  `c159` text,
  `c160` text,
  `c161` text,
  `c162` text,
  `c163` text,
  `c164` text,
  `c165` text,
  `c166` text,
  `c167` text,
  `c168` text,
  `c169` text,
  `c170` text,
  `c171` text,
  `c172` text,
  `c173` text,
  `c174` text,
  `c175` text,
  `c176` text,
  `c177` text,
  `c178` text,
  `c179` text,
  `c180` text,
  `c181` text,
  `c182` text,
  `c183` text,
  `c184` text,
  `c185` text,
  `c186` text,
  `c187` text,
  `c188` text,
  `c189` text,
  `c190` text,
  `c191` text,
  `c192` text,
  `c193` text,
  `c194` text,
  `c195` text,
  `c196` text,
  KEY `k1` (`c1`(3072)),
  KEY `k2` (`c2`(3072)),
  KEY `k3` (`c3`(3072)),
  KEY `k4` (`c4`(3072)),
  KEY `k5` (`c5`(3072)),
  KEY `k6` (`c6`(3072)),
  KEY `k7` (`c7`(3072)),
  KEY `k8` (`c8`(3072)),
  KEY `k9` (`c9`(3072)),
  KEY `k10` (`c10`(3072)),
  KEY `k11` (`c11`(3072)),
  KEY `k12` (`c12`(3072)),
  KEY `k13` (`c13`(3072)),
  KEY `k14` (`c14`(3072)),
  KEY `k15` (`c15`(3072)),
  KEY `k16` (`c16`(3072)),
  KEY `k17` (`c17`(3072)),
  KEY `k18` (`c18`(3072)),
  KEY `k19` (`c19`(3072)),
  KEY `k20` (`c20`(3072)),
  KEY `k21` (`c21`(3072)),
  KEY `k22` (`c22`(3072)),
  KEY `k23` (`c23`(3072)),
  KEY `k24` (`c24`(3072)),
  KEY `k25` (`c25`(3072)),
  KEY `k26` (`c26`(3072)),
  KEY `k27` (`c27`(3072)),
  KEY `k28` (`c28`(3072)),
  KEY `k29` (`c29`(3072)),
  KEY `k30` (`c30`(3072)),
  KEY `k31` (`c31`(3072)),
  KEY `k32` (`c32`(3072)),
  KEY `k33` (`c33`(3072)),
  KEY `k34` (`c34`(3072)),
  KEY `k35` (`c35`(3072)),
  KEY `k36` (`c36`(3072)),
  KEY `k37` (`c37`(3072)),
  KEY `k38` (`c38`(3072)),
  KEY `k39` (`c39`(3072)),
  KEY `k40` (`c40`(3072)),
  KEY `k41` (`c41`(3072)),
  KEY `k42` (`c42`(3072)),
  KEY `k43` (`c43`(3072)),
  KEY `k44` (`c44`(3072)),
  KEY `k45` (`c45`(3072)),
  KEY `k46` (`c46`(3072)),
  KEY `k47` (`c47`(3072)),
  KEY `k48` (`c48`(3072)),
  KEY `k49` (`c49`(3072)),
  KEY `k50` (`c50`(3072)),
  KEY `k51` (`c51`(3072)),
  KEY `k52` (`c52`(3072)),
  KEY `k53` (`c53`(3072)),
  KEY `k54` (`c54`(3072)),
  KEY `k55` (`c55`(3072)),
  KEY `k56` (`c56`(3072)),
  KEY `k57` (`c57`(3072)),
  KEY `k58` (`c58`(3072)),
  KEY `k59` (`c59`(3072)),
  KEY `k60` (`c60`(3072)),
  KEY `k61` (`c61`(3072)),
  KEY `k62` (`c62`(3072)),
  KEY `k63` (`c63`(3072)),
  KEY `k64` (`c64`(3072))
) ENGINE=InnoDB DEFAULT CHARSET=latin1 ROW_FORMAT=DYNAMIC
UPDATE t1 SET
c1=REPEAT('a', 65535),
c2=REPEAT('b', 65535),
c3=REPEAT('c', 65535),
c4=REPEAT('d', 65535),
c5=REPEAT('e', 65535),
c6=REPEAT('f', 65535),
c7=REPEAT('g', 65535),
c8=REPEAT('h', 65535),
c9=REPEAT('i', 65535),
c10=REPEAT('j', 65535),
c11=REPEAT('k', 65535),
c12=REPEAT('l', 65535),
c13=REPEAT('m', 65535),
c14=REPEAT('n', 65535),
c15=REPEAT('o', 65535);
UPDATE t1 SET
c14=REPEAT('a', 65535),
c13=REPEAT('b', 65535),
c12=REPEAT('c', 65535),
c11=REPEAT('d', 65535),
c10=REPEAT('e', 65535),
c9=REPEAT('f', 65535),
c8=REPEAT('g', 65535),
c7=REPEAT('h', 65535),
c6=REPEAT('i', 65535),
c5=REPEAT('j', 65535),
c4=REPEAT('k', 65535),
c3=REPEAT('l', 65535),
c2=REPEAT('m', 65535),
c1=REPEAT('n', 65535);
ERROR HY000: Undo log record is too big.
UPDATE t1 SET
c14 = REPEAT('a', 65535);
ERROR HY000: Undo log record is too big.
DROP TABLE t1;
#
# Bug#21880445 - FAILING ASSERTION: !FIELD->PREFIX_LEN ||
#	        FIELD->FIXED_LEN == FIELD->PREFIX_LEN
#
CREATE TABLE t1(
a CHAR(255) CHARACTER SET UTF32,
KEY k1(a(254))
) ENGINE=INNODB;
DROP TABLE t1;
CREATE TABLE t1(
a VARCHAR(16383) CHARACTER SET UTF32,
KEY k1(a(768))
) ENGINE=INNODB;
DROP TABLE t1;
CREATE TABLE t1(
a TEXT CHARACTER SET UTF32,
KEY k1(a(768))
) ENGINE=INNODB;
DROP TABLE t1;
CREATE TABLE t1(
a CHAR(255) CHARACTER SET UTF32,
KEY k1(a)
)ENGINE=INNODB;
DROP TABLE t1;
CREATE TABLE t1(
a CHAR(20) CHARACTER SET UTF32,
KEY k1(a(5))
) ENGINE=INNODB;
DROP TABLE t1;
CREATE TABLE t1(
a CHAR(255) CHARACTER SET UTF32,
KEY k1(a(195))
) ENGINE=INNODB;
ALTER TABLE t1 ADD COLUMN b CHAR(255) CHARACTER SET UTF32;
ALTER TABLE t1 ADD COLUMN c CHAR(255) CHARACTER SET UTF32;
ALTER TABLE t1 ADD INDEX k2(b);
ALTER TABLE t1 ADD INDEX k3(c);
DROP TABLE t1;
CREATE TABLE t1(a VARCHAR(1000)) charset latin1 ROW_FORMAT=DYNAMIC;
CREATE INDEX idx1 ON t1(a);
ALTER TABLE t1 ROW_FORMAT=COMPACT;
ERROR 42000: Specified key was too long; max key length is 767 bytes
DROP TABLE t1;

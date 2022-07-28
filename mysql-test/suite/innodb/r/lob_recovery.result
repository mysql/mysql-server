#
# Bug #33454557 Innodb asserts due to corruption in the fsp layer while
# running MTR tests
#
set @s:=concat("CREATE TABLE t(a LONGBLOB GENERATED ALWAYS AS
('",REPEAT("a",1024*1024),"') STORED) ENGINE=INNODB;");
PREPARE r FROM @s;
EXECUTE r;
DEALLOCATE PREPARE r;
DROP TABLE t;
set @s:=concat("CREATE TABLE t(w INT, A INT GENERATED ALWAYS
AS('",REPEAT("\\0",1024*1024),"') STORED) ENGINE=INNODB;");
PREPARE r FROM @s;
EXECUTE r;
DEALLOCATE PREPARE r;
DROP TABLE t;
# Kill and restart
CREATE TABLE `ibstd_02` (
`a` int NOT NULL,
`d` int NOT NULL,
`b` varchar(198) NOT NULL,
`c` char(94) DEFAULT NULL,
`nc02614` blob,
`nc07663` decimal(10,0) NOT NULL,
`nc02836` int DEFAULT NULL,
`nc06406` bigint NOT NULL,
`nc08713` mediumtext,
`nc00159` char(9) DEFAULT NULL,
`nc03394` varchar(198) NOT NULL,
`nc09912` int DEFAULT NULL,
`nc04074` mediumtext NOT NULL,
`gc06942` geometry DEFAULT NULL,
`nc01416` decimal(10,0) DEFAULT '-62775914',
`nc02900` mediumtext,
`nc01060` int NOT NULL,
`nc09703` double DEFAULT NULL,
`nc02361` char(19) DEFAULT NULL,
`nc09146` tinytext NOT NULL,
`nc01349` tinytext,
`nc05115` varchar(83) DEFAULT NULL,
`nc01550` blob,
`nc01963` tinyint DEFAULT NULL,
`nc03089` tinyint DEFAULT NULL,
`nc03917` bigint NOT NULL,
`nc00433` tinytext,
`nc00080` tinytext,
`nc03787` decimal(10,0) NOT NULL DEFAULT '-116224180',
`nc04351` int NOT NULL,
`nc05428` longblob,
`nc02756` bigint DEFAULT '-7995081',
`nc01152` double NOT NULL,
`nc07297` decimal(10,0) NOT NULL,
`nc07970` tinytext NOT NULL,
`nc07670` varchar(239) DEFAULT '5dgword0.7621719722386470.227883050724099',
`nc06861` tinyint NOT NULL,
`nc08265` char(23) NOT NULL,
`nc09700` longblob NOT NULL,
`nc06366` decimal(10,0) DEFAULT '1744702',
`nc03517` bigint NOT NULL DEFAULT '-109091878',
`nc06783` double NOT NULL,
`nc09164` char(21) DEFAULT NULL,
`nc00539` blob,
`nc05630` varchar(111) NOT NULL,
`nc01824` varchar(411) NOT NULL DEFAULT '0.2748128049709210.5692422509033510.0247909802167818',
`nc07849` longblob NOT NULL,
`nc02543` tinyint DEFAULT NULL,
`nc06790` double DEFAULT NULL,
`nc08367` varchar(40) DEFAULT '0.678796587967447',
`nc04269` mediumtext,
`nc01684` char(21) NOT NULL,
`nc09725` decimal(10,0) DEFAULT NULL,
`nc06545` bigint DEFAULT NULL,
`nc08187` varchar(232) DEFAULT NULL,
`gc05904` multipoint DEFAULT NULL,
`nc03479` longblob,
`nc06046` tinyint DEFAULT NULL,
`nc01036` decimal(10,0) NOT NULL DEFAULT '-7530867',
`nc00735` tinyint DEFAULT NULL,
`nc09430` varchar(117) DEFAULT NULL,
`nc02844` blob NOT NULL,
`nc00023` tinytext,
`nc03094` tinyint DEFAULT NULL,
`nc03312` tinytext NOT NULL,
`nc03475` tinytext,
`nc02475` mediumtext NOT NULL,
`nc01252` decimal(10,0) NOT NULL DEFAULT '8615274',
`nc01020` longblob NOT NULL,
`nc04131` mediumtext NOT NULL,
`nc05027` mediumtext,
`nc02184` double NOT NULL,
`nc07416` double DEFAULT NULL,
`nc04127` varchar(135) DEFAULT NULL,
`nc06159` longblob,
`nc01122` int DEFAULT NULL,
`nc01409` blob,
`nc01326` decimal(10,0) NOT NULL,
`nc03458` decimal(10,0) DEFAULT NULL,
`nc03327` bigint NOT NULL,
`nc00834` tinytext,
`nc07637` bigint DEFAULT '-125873342',
`nc07289` mediumtext NOT NULL,
`nc00110` int NOT NULL,
`nc09363` varchar(389) DEFAULT NULL,
`nc02046` tinytext,
`nc04790` bigint NOT NULL DEFAULT '81062191',
`nc03499` tinyint NOT NULL DEFAULT '-100',
`nc05566` decimal(10,0) DEFAULT NULL,
`nc01148` tinyint NOT NULL,
`nc03784` mediumtext,
`nc06338` bigint NOT NULL DEFAULT '-92351690',
`nc01068` tinytext,
`nc08596` mediumtext NOT NULL,
`nc02796` tinyint NOT NULL,
`nc04875` decimal(10,0) DEFAULT NULL,
`nc02799` int NOT NULL,
`nc00074` int NOT NULL,
`nc03703` decimal(10,0) NOT NULL,
`nc06965` tinyint NOT NULL DEFAULT '0',
`nc08710` mediumtext NOT NULL,
`nc01675` varchar(200) DEFAULT NULL,
`nc09917` decimal(10,0) NOT NULL,
`nc08148` longblob,
`nc01298` longblob,
`nc01613` decimal(10,0) DEFAULT NULL,
`nc01656` int DEFAULT NULL,
`nc08402` char(19) DEFAULT NULL,
`nc04340` tinytext,
`nc05768` mediumtext,
`nc03616` int DEFAULT NULL,
`gc00680` point DEFAULT NULL,
`nc06802` tinytext,
`nc01681` tinytext NOT NULL,
`nc00004` double NOT NULL DEFAULT '-110206412.196301',
`nc07322` double NOT NULL,
`nc05772` bigint DEFAULT NULL,
`nc02010` varchar(338) DEFAULT NULL,
KEY `d` (`d` DESC),
KEY `a` (`a`),
KEY `b` (`b`(10),`a`,`d` DESC),
KEY `c` (`c`(56),`b`(56) DESC),
KEY `b_2` (`b`(5),`c`(10),`a` DESC)
) /*!50100 TABLESPACE `innodb_file_per_table` */
ENGINE=InnoDB DEFAULT CHARSET=utf8mb4
COLLATE=utf8mb4_0900_ai_ci STATS_PERSISTENT=1 ENCRYPTION='N';
alter table ibstd_02 add column nc09053 blob not null, algorithm=instant;
alter table ibstd_02 add column nc03118 mediumtext, algorithm=instant;
alter table ibstd_02 add column nc08926 tinytext, algorithm=instant, lock=default;
alter table ibstd_02 add column nc09010 double default '73563844.85182579', algorithm=instant;
alter table ibstd_02 add column nc07017 decimal not null, algorithm=instant, lock=default;
alter table ibstd_02 add column nc07007 tinytext not null, algorithm=instant;
alter table ibstd_02 add column nc08086 blob, algorithm=instant;
select * from ibstd_02;
a	d	b	c	nc02614	nc07663	nc02836	nc06406	nc08713	nc00159	nc03394	nc09912	nc04074	gc06942	nc01416	nc02900	nc01060	nc09703	nc02361	nc09146	nc01349	nc05115	nc01550	nc01963	nc03089	nc03917	nc00433	nc00080	nc03787	nc04351	nc05428	nc02756	nc01152	nc07297	nc07970	nc07670	nc06861	nc08265	nc09700	nc06366	nc03517	nc06783	nc09164	nc00539	nc05630	nc01824	nc07849	nc02543	nc06790	nc08367	nc04269	nc01684	nc09725	nc06545	nc08187	gc05904	nc03479	nc06046	nc01036	nc00735	nc09430	nc02844	nc00023	nc03094	nc03312	nc03475	nc02475	nc01252	nc01020	nc04131	nc05027	nc02184	nc07416	nc04127	nc06159	nc01122	nc01409	nc01326	nc03458	nc03327	nc00834	nc07637	nc07289	nc00110	nc09363	nc02046	nc04790	nc03499	nc05566	nc01148	nc03784	nc06338	nc01068	nc08596	nc02796	nc04875	nc02799	nc00074	nc03703	nc06965	nc08710	nc01675	nc09917	nc08148	nc01298	nc01613	nc01656	nc08402	nc04340	nc05768	nc03616	gc00680	nc06802	nc01681	nc00004	nc07322	nc05772	nc02010	nc09053	nc03118	nc08926	nc09010	nc07017	nc07007	nc08086
drop table ibstd_02;
# Kill and restart
set global innodb_file_per_table = off;
CREATE TABLE t1(a INT NOT NULL, PRIMARY KEY(a)) ENGINE=INNODB
PARTITION BY HASH(a) PARTITIONS 1024;
DROP TABLE t1;
set global innodb_file_per_table = off;
CREATE TABLE t1(a INT NOT NULL, PRIMARY KEY(a)) ENGINE=INNODB
PARTITION BY HASH(a) PARTITIONS 1024;
RENAME TABLE t1 TO t2;
DROP TABLE t2;
set global innodb_file_per_table = off;
CREATE TABLE t1(a INT NOT NULL, PRIMARY KEY(a)) ENGINE=INNODB
PARTITION BY HASH(a) PARTITIONS 1024;
TRUNCATE TABLE t1;
DROP TABLE t1;
set global innodb_file_per_table = on;
CREATE TABLE t1(a INT NOT NULL, PRIMARY KEY(a)) ENGINE=INNODB
PARTITION BY HASH(a) PARTITIONS 1024;
DROP TABLE t1;

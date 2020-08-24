DROP TABLE IF EXISTS t1;
CREATE TABLE t1 (
fid INT NOT NULL AUTO_INCREMENT PRIMARY KEY, 
g GEOMETRY NOT NULL SRID 0,
SPATIAL KEY(g)
) ENGINE=MyISAM;
SHOW CREATE TABLE t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `fid` int NOT NULL AUTO_INCREMENT,
  `g` geometry NOT NULL /*!80003 SRID 0 */,
  PRIMARY KEY (`fid`),
  SPATIAL KEY `g` (`g`)
) ENGINE=MyISAM DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
SELECT count(*) FROM t1;
count(*)
150
EXPLAIN SELECT fid, ST_AsText(g) FROM t1 WHERE ST_Within(g, ST_GeomFromText('Polygon((140 140,160 140,160 160,140 140))'));
id	select_type	table	partitions	type	possible_keys	key	key_len	ref	rows	filtered	Extra
1	SIMPLE	t1	NULL	range	g	g	34	NULL	8	100.00	Using where
Warnings:
Note	1003	/* select#1 */ select `test`.`t1`.`fid` AS `fid`,st_astext(`test`.`t1`.`g`) AS `ST_AsText(g)` from `test`.`t1` where st_within(`test`.`t1`.`g`,<cache>(st_geomfromtext('Polygon((140 140,160 140,160 160,140 140))')))
SELECT fid, ST_AsText(g) FROM t1 WHERE ST_Within(g, ST_GeomFromText('Polygon((140 140,160 140,160 160,140 160,140 140))'));
fid	ST_AsText(g)
1	LINESTRING(150 150,150 150)
10	LINESTRING(141 141,159 159)
11	LINESTRING(140 140,160 160)
2	LINESTRING(149 149,151 151)
3	LINESTRING(148 148,152 152)
4	LINESTRING(147 147,153 153)
5	LINESTRING(146 146,154 154)
6	LINESTRING(145 145,155 155)
7	LINESTRING(144 144,156 156)
8	LINESTRING(143 143,157 157)
9	LINESTRING(142 142,158 158)
DROP TABLE t1;
CREATE TABLE t1 (
fid INT NOT NULL AUTO_INCREMENT PRIMARY KEY, 
g GEOMETRY NOT NULL SRID 0
) ENGINE=MyISAM;
ALTER TABLE t1 ADD SPATIAL KEY(g);
SHOW CREATE TABLE t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `fid` int NOT NULL AUTO_INCREMENT,
  `g` geometry NOT NULL /*!80003 SRID 0 */,
  PRIMARY KEY (`fid`),
  SPATIAL KEY `g` (`g`)
) ENGINE=MyISAM AUTO_INCREMENT=101 DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
SELECT count(*) FROM t1;
count(*)
100
EXPLAIN SELECT fid, ST_AsText(g) FROM t1 WHERE ST_Within(g, 
ST_GeomFromText('Polygon((40 40,60 40,60 60,40 40))'));
id	select_type	table	partitions	type	possible_keys	key	key_len	ref	rows	filtered	Extra
1	SIMPLE	t1	NULL	range	g	g	34	NULL	4	100.00	Using where
Warnings:
Note	1003	/* select#1 */ select `test`.`t1`.`fid` AS `fid`,st_astext(`test`.`t1`.`g`) AS `ST_AsText(g)` from `test`.`t1` where st_within(`test`.`t1`.`g`,<cache>(st_geomfromtext('Polygon((40 40,60 40,60 60,40 40))')))
SELECT fid, ST_AsText(g) FROM t1 WHERE ST_Within(g, 
ST_GeomFromText('Polygon((40 40,60 40,60 60,40 40))'));
fid	ST_AsText(g)
46	LINESTRING(51 41,60 50)
DROP TABLE t1;
End of 5.5 tests.

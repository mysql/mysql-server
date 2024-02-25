SET collation_connection=latin2_czech_cs;
CREATE TABLE t1 ENGINE=MYISAM AS SELECT repeat('a', 5) AS s1 LIMIT 0;
INSERT INTO t1 VALUES ('x'),('y'),('z'),('X'),('Y'),('Z');
SELECT * FROM t1 GROUP BY s1;
s1
X
Y
Z
x
y
z
SELECT * FROM t1 ORDER BY s1;
s1
x
X
y
Y
z
Z
CREATE INDEX i1 ON t1 (s1);
SELECT * FROM t1 GROUP BY s1;
s1
X
Y
Z
x
y
z
SELECT * FROM t1 ORDER BY s1;
s1
x
X
y
Y
z
Z
DROP TABLE t1;
CREATE TABLE `t1` (
`ID` smallint(5) unsigned zerofill NOT NULL AUTO_INCREMENT,
`Post` enum('','B','O','Z','U') COLLATE latin2_czech_cs DEFAULT NULL,
PRIMARY KEY (`ID`)
) ENGINE=MyISAM AUTO_INCREMENT=135 DEFAULT CHARSET=latin2;
Warnings:
Warning	1681	The ZEROFILL attribute is deprecated and will be removed in a future release. Use the LPAD function to zero-pad numbers, or store the formatted numbers in a CHAR column.
Warning	1681	Integer display width is deprecated and will be removed in a future release.
SHOW CREATE TABLE t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `ID` smallint(5) unsigned zerofill NOT NULL AUTO_INCREMENT,
  `Post` enum('','B','O','Z','U') CHARACTER SET latin2 COLLATE latin2_czech_cs DEFAULT NULL,
  PRIMARY KEY (`ID`)
) ENGINE=MyISAM AUTO_INCREMENT=135 DEFAULT CHARSET=latin2
INSERT INTO t1 (ID,Post) VALUES (00041,'');
SELECT ID, Post, HEX(WEIGHT_STRING(Post)) FROM t1;
ID	Post	HEX(WEIGHT_STRING(Post))
00041		8000000000000001
DROP TABLE t1;

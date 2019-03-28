CREATE TABLE t (
c char(20) NOT NULL
) ENGINE=MyISAM DEFAULT CHARACTER SET utf16 COLLATE utf16_unicode_ci;
INSERT INTO t VALUES ('a'),('ab'),('aba');
ALTER TABLE t ADD INDEX (c);
SELECT c FROM t WHERE c LIKE 'a%';
c
a
ab
aba
DROP TABLE t;
CREATE TABLE t1 (
c1 text character set utf16 collate utf16_polish_ci NOT NULL
) ENGINE=MyISAM;
insert into t1 values (''),('a');
SELECT COUNT(*), c1 FROM t1 GROUP BY c1;
COUNT(*)	c1
1	
1	a
DROP TABLE IF EXISTS t1;

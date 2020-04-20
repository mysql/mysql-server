DROP TABLE IF EXISTS t1;
CREATE TABLE t1 (
a int(11) NOT NULL auto_increment,
b text,
c varchar(254) default NULL,
PRIMARY KEY (a),
FULLTEXT KEY bb(b),
FULLTEXT KEY cc(c),
FULLTEXT KEY a(b,c)
);
Warnings:
Warning	1681	Integer display width is deprecated and will be removed in a future release.
INSERT INTO t1 VALUES (1,'lala lolo lili','oooo aaaa pppp');
INSERT INTO t1 VALUES (2,'asdf fdsa','lkjh fghj');
INSERT INTO t1 VALUES (3,'qpwoei','zmxnvb');
SELECT a, round(MATCH  b  AGAINST ('lala lkjh'),5) FROM t1;
a	round(MATCH  b  AGAINST ('lala lkjh'),5)
1	0.67003
2	0
3	0
SELECT a, round(MATCH  c  AGAINST ('lala lkjh'),5) FROM t1;
a	round(MATCH  c  AGAINST ('lala lkjh'),5)
1	0
2	0.67756
3	0
SELECT a, round(MATCH b,c AGAINST ('lala lkjh'),5) FROM t1;
a	round(MATCH b,c AGAINST ('lala lkjh'),5)
1	0.64841
2	0.66266
3	0
drop table t1;

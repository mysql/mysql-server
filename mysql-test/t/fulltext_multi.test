# This tests the functionality of the fulltext search of Myisam engine
# The implementation of the fulltext search is different in InnoDB engine
# All tests are required to run with Myisam.
# Hence MTR starts mysqld with MyISAM as default

--source include/force_myisam_default.inc
--source include/have_myisam.inc

# several FULLTEXT indexes in one table test
--disable_warnings
DROP TABLE IF EXISTS t1;
--enable_warnings

CREATE TABLE t1 (
  a int(11) NOT NULL auto_increment,
  b text,
  c varchar(254) default NULL,
  PRIMARY KEY (a),
  FULLTEXT KEY bb(b),
  FULLTEXT KEY cc(c),
  FULLTEXT KEY a(b,c)
);

INSERT INTO t1 VALUES (1,'lala lolo lili','oooo aaaa pppp');
INSERT INTO t1 VALUES (2,'asdf fdsa','lkjh fghj');
INSERT INTO t1 VALUES (3,'qpwoei','zmxnvb');

SELECT a, round(MATCH  b  AGAINST ('lala lkjh'),5) FROM t1;
SELECT a, round(MATCH  c  AGAINST ('lala lkjh'),5) FROM t1;
SELECT a, round(MATCH b,c AGAINST ('lala lkjh'),5) FROM t1;
drop table t1;

# End of 4.1 tests

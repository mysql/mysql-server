# This tests the functionality of the fulltext search of Myisam engine
# The implementation of the fulltext search is different in InnoDB engine
# All tests are required to run with Myisam.
# Hence MTR starts mysqld with MyISAM as default

--source include/force_myisam_default.inc
--source include/have_myisam.inc

#
# Bugreport due to Roy Nasser <roy@vem.ca>
#

--disable_warnings
drop table if exists t1, t2;
--enable_warnings

CREATE TABLE t1 (
  id int(10) unsigned NOT NULL auto_increment,
  q varchar(255) default NULL,
  PRIMARY KEY (id)
);
INSERT INTO t1 VALUES (1,'aaaaaaaaa dsaass de');
INSERT INTO t1 VALUES (2,'ssde df s fsda sad er');
CREATE TABLE t2 (
  id int(10) unsigned NOT NULL auto_increment,
  id2 int(10) unsigned default NULL,
  item varchar(255) default NULL,
  PRIMARY KEY (id),
  FULLTEXT KEY item(item)
);
INSERT INTO t2 VALUES (1,1,'sushi');
INSERT INTO t2 VALUES (2,1,'Bolo de Chocolate');
INSERT INTO t2 VALUES (3,1,'Feijoada');
INSERT INTO t2 VALUES (4,1,'Mousse de Chocolate');
INSERT INTO t2 VALUES (5,2,'um copo de Vodka');
INSERT INTO t2 VALUES (6,2,'um chocolate Snickers');
INSERT INTO t2 VALUES (7,1,'Bife');
INSERT INTO t2 VALUES (8,1,'Pizza de Salmao');

SELECT t1.q, t2.item, t2.id, round(MATCH t2.item AGAINST ('sushi'),6)
as x FROM t1, t2 WHERE (t2.id2 = t1.id) ORDER BY x DESC,t2.id;

SELECT t1.q, t2.item, t2.id, MATCH t2.item AGAINST ('sushi' IN BOOLEAN MODE)
as x FROM t1, t2 WHERE (t2.id2 = t1.id) ORDER BY x DESC,t2.id;

SELECT t1.q, t2.item, t2.id, round(MATCH t2.item AGAINST ('sushi'),6)
as x FROM t2, t1 WHERE (t2.id2 = t1.id) ORDER BY x DESC,t2.id;

SELECT t1.q, t2.item, t2.id, MATCH t2.item AGAINST ('sushi' IN BOOLEAN MODE)
as x FROM t2, t1 WHERE (t2.id2 = t1.id) ORDER BY x DESC,t2.id;

drop table t1, t2;

# End of 4.1 tests

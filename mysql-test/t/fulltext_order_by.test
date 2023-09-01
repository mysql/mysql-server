# This tests the functionality of the fulltext search of Myisam engine
# The implementation of the fulltext search is different in InnoDB engine
# All tests are required to run with Myisam.
# Hence MTR starts mysqld with MyISAM as default

--source include/force_myisam_default.inc
--source include/have_myisam.inc

--disable_warnings
DROP TABLE IF EXISTS t1,t2,t3;
--enable_warnings

CREATE TABLE t1 (
  a INT AUTO_INCREMENT PRIMARY KEY,
  message CHAR(20),
  FULLTEXT(message)
) comment = 'original testcase by sroussey@network54.com';
INSERT INTO t1 (message) VALUES ("Testing"),("table"),("testbug"),
        ("steve"),("is"),("cool"),("steve is cool");
# basic MATCH
SELECT a, FORMAT(MATCH (message) AGAINST ('steve'),6) FROM t1 WHERE MATCH (message) AGAINST ('steve');
SELECT a, MATCH (message) AGAINST ('steve' IN BOOLEAN MODE) FROM t1 WHERE MATCH (message) AGAINST ('steve');
SELECT a, FORMAT(MATCH (message) AGAINST ('steve'),6) FROM t1 WHERE MATCH (message) AGAINST ('steve' IN BOOLEAN MODE);
SELECT a, MATCH (message) AGAINST ('steve' IN BOOLEAN MODE) FROM t1 WHERE MATCH (message) AGAINST ('steve' IN BOOLEAN MODE);

# MATCH + ORDER BY (with ft-ranges)
SELECT a, FORMAT(MATCH (message) AGAINST ('steve'),6) FROM t1 WHERE MATCH (message) AGAINST ('steve') ORDER BY a;
SELECT a, MATCH (message) AGAINST ('steve' IN BOOLEAN MODE) FROM t1 WHERE MATCH (message) AGAINST ('steve' IN BOOLEAN MODE) ORDER BY a;

# MATCH + ORDER BY (with normal ranges) + UNIQUE
SELECT a, FORMAT(MATCH (message) AGAINST ('steve'),6) FROM t1 WHERE a in (2,7,4) and MATCH (message) AGAINST ('steve') ORDER BY a DESC;
SELECT a, MATCH (message) AGAINST ('steve' IN BOOLEAN MODE) FROM t1 WHERE a in (2,7,4) and MATCH (message) AGAINST ('steve' IN BOOLEAN MODE) ORDER BY a DESC;

# MATCH + ORDER BY + UNIQUE (const_table)
SELECT a, FORMAT(MATCH (message) AGAINST ('steve'),6) FROM t1 WHERE a=7 and MATCH (message) AGAINST ('steve') ORDER BY 1;
SELECT a, MATCH (message) AGAINST ('steve' IN BOOLEAN MODE) FROM t1 WHERE a=7 and MATCH (message) AGAINST ('steve' IN BOOLEAN MODE) ORDER BY 1;

# ORDER BY MATCH
SELECT a, FORMAT(MATCH (message) AGAINST ('steve'),6) as rel FROM t1 ORDER BY rel;
SELECT a, MATCH (message) AGAINST ('steve' IN BOOLEAN MODE) as rel FROM t1 ORDER BY rel;

#
# BUG#6635 - test_if_skip_sort_order() thought it can skip filesort
# for fulltext searches too
#
alter table t1 add key m (message);
explain SELECT message FROM t1 WHERE MATCH (message) AGAINST ('steve') ORDER BY message;
SELECT message FROM t1 WHERE MATCH (message) AGAINST ('steve') ORDER BY message desc;

drop table t1;

#
# reused boolean scan bug
#
CREATE TABLE t1 (
  a INT AUTO_INCREMENT PRIMARY KEY,
  message CHAR(20),
  FULLTEXT(message)
);
INSERT INTO t1 (message) VALUES ("testbug"),("testbug foobar");
SELECT a, MATCH (message) AGAINST ('t* f*' IN BOOLEAN MODE) as rel FROM t1;
SELECT a, MATCH (message) AGAINST ('t* f*' IN BOOLEAN MODE) as rel FROM t1 ORDER BY rel,a;
drop table t1;

# BUG#11869
CREATE TABLE t1 (
  id int(11) NOT NULL auto_increment,
  thread int(11) NOT NULL default '0',
  beitrag longtext NOT NULL,
  PRIMARY KEY  (id),
  KEY thread (thread),
  FULLTEXT KEY beitrag (beitrag)
) ENGINE=MyISAM DEFAULT CHARSET=utf8mb3 AUTO_INCREMENT=7923 ;

CREATE TABLE t2 (
  id int(11) NOT NULL auto_increment,
  text varchar(100) NOT NULL default '',
  PRIMARY KEY  (id),
  KEY text (text)
) ENGINE=MyISAM DEFAULT CHARSET=utf8mb3 AUTO_INCREMENT=63 ;

CREATE TABLE t3 (
  id int(11) NOT NULL auto_increment,
  forum int(11) NOT NULL default '0',
  betreff varchar(70) NOT NULL default '',
  PRIMARY KEY  (id),
  KEY forum (forum),
  FULLTEXT KEY betreff (betreff)
) ENGINE=MyISAM DEFAULT CHARSET=utf8mb3 AUTO_INCREMENT=996 ;

--error ER_TABLENAME_NOT_ALLOWED_HERE
select a.text, b.id, b.betreff
from 
  t2 a inner join t3 b on a.id = b.forum inner join
  t1 c on b.id = c.thread
where 
  match(b.betreff) against ('+abc' in boolean mode)
group by a.text, b.id, b.betreff
union
select a.text, b.id, b.betreff
from 
  t2 a inner join t3 b on a.id = b.forum inner join
  t1 c on b.id = c.thread
where 
  match(c.beitrag) against ('+abc' in boolean mode)
group by 
  a.text, b.id, b.betreff
order by 
  match(b.betreff) against ('+abc' in boolean mode) desc;
  
--error ER_TABLENAME_NOT_ALLOWED_HERE
select a.text, b.id, b.betreff
from 
  t2 a inner join t3 b on a.id = b.forum inner join
  t1 c on b.id = c.thread
where 
  match(b.betreff) against ('+abc' in boolean mode)
union
select a.text, b.id, b.betreff
from 
  t2 a inner join t3 b on a.id = b.forum inner join
  t1 c on b.id = c.thread
where 
  match(c.beitrag) against ('+abc' in boolean mode)
order by 
  match(b.betreff) against ('+abc' in boolean mode) desc;

--error ER_TABLE_CANT_HANDLE_FT
select a.text, b.id, b.betreff
from 
  t2 a inner join t3 b on a.id = b.forum inner join
  t1 c on b.id = c.thread
where 
  match(b.betreff) against ('+abc' in boolean mode)
union
select a.text, b.id, b.betreff
from 
  t2 a inner join t3 b on a.id = b.forum inner join
  t1 c on b.id = c.thread
where 
  match(c.beitrag) against ('+abc' in boolean mode)
order by 
  match(betreff) against ('+abc' in boolean mode) desc;

# BUG#11869 part2: used table type doesn't support FULLTEXT indexes error
--error ER_TABLE_CANT_HANDLE_FT
(select b.id, b.betreff from t3 b) union 
(select b.id, b.betreff from t3 b) 
order by match(betreff) against ('+abc' in boolean mode) desc;

--error ER_TABLE_CANT_HANDLE_FT
(select b.id, b.betreff from t3 b) union 
(select b.id, b.betreff from t3 b) 
order by match(betreff) against ('+abc') desc;

select distinct b.id, b.betreff from t3 b 
order by match(betreff) against ('+abc' in boolean mode) desc;

--source include/turn_off_only_full_group_by.inc

select b.id, b.betreff from t3 b group by b.id+1 
order by match(betreff) against ('+abc' in boolean mode) desc;

--source include/restore_sql_mode_after_turn_off_only_full_group_by.inc

drop table t1,t2,t3;

# End of 4.1 tests

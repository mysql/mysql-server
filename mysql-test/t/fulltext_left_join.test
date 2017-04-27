# This tests the functionality of the fulltext search of Myisam engine
# The implementation of the fulltext search is different in InnoDB engine
# All tests are required to run with Myisam.
# Hence MTR starts mysqld with MyISAM as default

--source include/force_myisam_default.inc
--source include/have_myisam.inc

#
# Test for bug from Jean-Cédric COSTA <jean-cedric.costa@ensmp.fr>
#

--disable_warnings
drop table if exists t1, t2;
--enable_warnings

CREATE TABLE t1 (
       id           VARCHAR(80) NOT NULL PRIMARY KEY,
       sujet        VARCHAR(80),
       motsclefs    TEXT,
       texte        MEDIUMTEXT,
       FULLTEXT(sujet, motsclefs, texte)
);
INSERT INTO t1 VALUES('123','toto','essai','test');
INSERT INTO t1 VALUES('456','droit','penal','lawyer');
INSERT INTO t1 VALUES('789','aaaaa','bbbbb','cccccc');
CREATE TABLE t2 (
       id         VARCHAR(255) NOT NULL,
       author     VARCHAR(255) NOT NULL
);
INSERT INTO t2 VALUES('123', 'moi');
INSERT INTO t2 VALUES('123', 'lui');
INSERT INTO t2 VALUES('456', 'lui');

select round(match(t1.texte,t1.sujet,t1.motsclefs) against('droit'),5)
       from t1 left join t2 on t2.id=t1.id;
select match(t1.texte,t1.sujet,t1.motsclefs) against('droit' IN BOOLEAN MODE)
       from t1 left join t2 on t2.id=t1.id;

drop table t1, t2;

#
# BUG#484, reported by Stephen Brandon <stephen@brandonitconsulting.co.uk>
#

create table t1 (venue_id int(11) default null, venue_text varchar(255) default null, dt datetime default null) engine=myisam;
insert into t1 (venue_id, venue_text, dt) values (1, 'a1', '2003-05-23 19:30:00'),(null, 'a2', '2003-05-23 19:30:00');
create table t2 (name varchar(255) not null default '', entity_id int(11) not null auto_increment, primary key  (entity_id), fulltext key name (name)) engine=myisam;
insert into t2 (name, entity_id) values ('aberdeen town hall', 1), ('glasgow royal concert hall', 2), ('queen\'s hall, edinburgh', 3);
select * from t1 left join t2 on venue_id = entity_id where match(name) against('aberdeen' in boolean mode) and dt = '2003-05-23 19:30:00';
select * from t1 left join t2 on venue_id = entity_id where match(name) against('aberdeen') and dt = '2003-05-23 19:30:00';
select * from t1 left join t2 on (venue_id = entity_id and match(name) against('aberdeen' in boolean mode)) where dt = '2003-05-23 19:30:00';
select * from t1 left join t2 on (venue_id = entity_id and match(name) against('aberdeen')) where dt = '2003-05-23 19:30:00';
drop table t1,t2;

#
# BUG#14708
# Inconsistent treatment of NULLs in LEFT JOINed FULLTEXT matching without index
#

create table t1 (id int not null primary key, d char(200) not null, e char(200));
insert into t1 values (1, 'aword', null), (2, 'aword', 'bword'), (3, 'bword', null), (4, 'bword', 'aword'), (5, 'aword and bword', null);
select * from t1 where match(d, e) against ('+aword +bword' in boolean mode);
create table t2 (m_id int not null, f char(200), key (m_id));
insert into t2 values (1, 'bword'), (3, 'aword'), (5, '');
select * from t1 left join t2 on m_id = id where match(d, e, f) against ('+aword +bword' in boolean mode);
drop table t1,t2;

#
# BUG#25637: LEFT JOIN with BOOLEAN FULLTEXT loses left table matches
#            (this is actually the same bug as bug #14708)
#

CREATE TABLE t1 (
  id int(10) NOT NULL auto_increment,
  link int(10) default NULL,
  name mediumtext default NULL,
  PRIMARY KEY (id),
  FULLTEXT (name)
);
INSERT INTO t1 VALUES (1, 1, 'string');
INSERT INTO t1 VALUES (2, 0, 'string');
CREATE TABLE t2 (
    id int(10) NOT NULL auto_increment,
    name mediumtext default NULL,
    PRIMARY KEY (id),
    FULLTEXT (name)
);
INSERT INTO t2 VALUES (1, 'string');

SELECT t1.*, MATCH(t1.name) AGAINST('string') AS relevance 
  FROM t1 LEFT JOIN t2 ON t1.link = t2.id
    WHERE MATCH(t1.name, t2.name) AGAINST('string' IN BOOLEAN MODE);

DROP TABLE t1,t2;

# End of 4.1 tests

#
# BUG#25729 - boolean full text search is confused by NULLs produced by LEFT
#             JOIN
#
CREATE TABLE t1 (a INT);
CREATE TABLE t2 (b INT, c TEXT, KEY(b));
INSERT INTO t1 VALUES(1);
INSERT INTO t2(b,c) VALUES(2,'castle'),(3,'castle');
SELECT * FROM t1 LEFT JOIN t2 ON a=b WHERE MATCH(c) AGAINST('+castle' IN BOOLEAN MODE);
DROP TABLE t1, t2;

#
# Test latin_de character set
#

set names latin1;
--character_set latin1

let $old_collation_schema= `SELECT @@collation_database`;
let $collation_server=`SELECT @@global.collation_server`;
eval ALTER SCHEMA test COLLATE $collation_server;

set @@collation_connection=latin1_german2_ci;

select @@collation_connection;

--disable_warnings
drop table if exists t1;
--enable_warnings

create table t1 (a char (20) not null, b int not null primary key auto_increment, index (a,b));
insert into t1 (a) values ('ä'),('ac'),('ae'),('ad'),('Äc'),('aeb');
insert into t1 (a) values ('üc'),('uc'),('ue'),('ud'),('Ü'),('ueb'),('uf');
insert into t1 (a) values ('ö'),('oc'),('Öa'),('oe'),('od'),('Öc'),('oeb');
insert into t1 (a) values ('s'),('ss'),('ß'),('ßb'),('ssa'),('ssc'),('ßa');
insert into t1 (a) values ('eä'),('uü'),('öo'),('ää'),('ääa'),('aeae');
insert into t1 (a) values ('q'),('a'),('u'),('o'),('é'),('É'),('a');
select a,b from t1 order by a,b;
select a,b from t1 order by upper(a),b;
select a from t1 order by a desc,b;
check table t1;
--sorted_result
select * from t1 where a like "ö%";
--sorted_result
select * from t1 where a like binary "%É%";
--sorted_result
select * from t1 where a like "%Á%";
--sorted_result
select * from t1 where a like "%U%";
--sorted_result
select * from t1 where a like "%ss%";
drop table t1;

# The following should all be true
select strcmp('ä','ae'),strcmp('ae','ä'),strcmp('aeq','äq'),strcmp('äq','aeq');
select strcmp('ss','ß'),strcmp('ß','ss'),strcmp('ßs','sss'),strcmp('ßq','ssq');

# The following should all return -1
select strcmp('ä','af'),strcmp('a','ä'),strcmp('ää','aeq'),strcmp('ää','aeaeq');
select strcmp('ss','ßa'),strcmp('ß','ssa'),strcmp('sßa','sssb'),strcmp('s','ß');
select strcmp('ö','oö'),strcmp('Ü','uü'),strcmp('ö','oeb');

# The following should all return 1
select strcmp('af','ä'),strcmp('ä','a'),strcmp('aeq','ää'),strcmp('aeaeq','ää');
select strcmp('ßa','ss'),strcmp('ssa','ß'),strcmp('sssb','sßa'),strcmp('ß','s');
select strcmp('u','öa'),strcmp('u','ö');

#
# overlapping combo's
#
select strcmp('sä', 'ßa'), strcmp('aä', 'äx');

#
# Test bug report #152 (problem with index on latin1_de)
#

#
# The below checks both binary and character comparisons.
#
create table t1 (word varchar(255) not null, word2 varchar(255) not null default '', index(word));
show create table t1;
insert into t1 (word) values ('ss'),(0xDF),(0xE4),('ae');
update t1 set word2=word;
select word, word=binary 0xdf as t from t1 having t > 0;
select word, word=cast(0xdf AS CHAR) as t from t1 having t > 0;
select * from t1 where word=binary 0xDF;
select * from t1 where word=CAST(0xDF as CHAR);
select * from t1 where word2=binary 0xDF;
select * from t1 where word2=CAST(0xDF as CHAR);
select * from t1 where word='ae';
select * from t1 where word= 0xe4 or word=CAST(0xe4 as CHAR);
select * from t1 where word between binary 0xDF and binary 0xDF;
select * from t1 where word between CAST(0xDF AS CHAR) and CAST(0xDF AS CHAR);
select * from t1 where word like 'ae';
select * from t1 where word like 'AE';
select * from t1 where word like binary 0xDF;
select * from t1 where word like CAST(0xDF as CHAR);
drop table t1;

#
# Bug #5447 Select does not find records
#
CREATE TABLE t1 (
  autor varchar(80) NOT NULL default '',
  PRIMARY KEY  (autor)
);
INSERT INTO t1 VALUES ('Powell, B.'),('Powell, Bud.'),('Powell, L. H.'),('Power, H.'),
('Poynter, M. A. L. Lane'),('Poynting, J. H. und J. J. Thomson.'),('Pozzi, S(amuel-Jean).'),
('Pozzi, Samuel-Jean.'),('Pozzo, A.'),('Pozzoli, Serge.');
SELECT * FROM t1 WHERE autor LIKE 'Poz%' ORDER BY autor;
DROP TABLE t1;

#
# Test of special character in german collation
#

CREATE TABLE t1 (
s1 CHAR(5) CHARACTER SET latin1 COLLATE latin1_german2_ci
);
show create table t1;
INSERT INTO t1 VALUES ('Ü');
INSERT INTO t1 VALUES ('ue');
SELECT DISTINCT s1 FROM t1;
SELECT s1,COUNT(*) FROM t1 GROUP BY s1;
SELECT COUNT(DISTINCT s1) FROM t1;
SELECT FIELD('ue',s1), FIELD('Ü',s1), s1='ue', s1='Ü' FROM t1;
DROP TABLE t1;

-- source include/ctype_filesort.inc
-- source include/ctype_german.inc

# End of 4.1 tests

#
# Bug#9509
#
create table t1 (s1 char(5) character set latin1 collate latin1_german2_ci);
insert into t1 values (0xf6) /* this is o-umlaut */;
select * from t1 where length(s1)=1 and s1='oe';
drop table t1;

--echo End of 5.1 tests


--echo #
--echo # Start of 5.6 tests
--echo #

--echo #
--echo # WL#3664 WEIGHT_STRING
--echo #

set @@collation_connection=latin1_german2_ci;
--source include/weight_string.inc
--source include/weight_string_euro.inc
select hex(weight_string('Ä'));
select hex(weight_string('ä'));
select hex(weight_string('Ö'));
select hex(weight_string('ö'));
select hex(weight_string('Ü'));
select hex(weight_string('ü'));
select hex(weight_string('S'));
select hex(weight_string('s'));
select hex(weight_string('ß'));
select hex(weight_string('ä' as char(1)));
select hex(weight_string('ö' as char(1)));
select hex(weight_string('ü' as char(1)));
select hex(weight_string('ß' as char(1)));
select hex(weight_string('xä' as char(2)));
select hex(weight_string('xö' as char(2)));
select hex(weight_string('xü' as char(2)));
select hex(weight_string('xß' as char(2)));

--echo #
--echo # End of 5.6 tests
--echo #

# Revert the collation of the 'test' schema
eval ALTER SCHEMA test COLLATE $old_collation_schema;

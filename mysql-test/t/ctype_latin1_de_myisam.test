#
# Test latin_de character set
#
--source include/force_myisam_default.inc
--source include/have_myisam.inc

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


#
# Some other simple tests with the current character set
#
create table t1 (a varchar(10), key(a), fulltext (a)) engine=myisam;
insert into t1 values ("a"),("abc"),("abcd"),("hello"),("test");
select * from t1 where a like "abc%"; 
select * from t1 where a like "test%"; 
select * from t1 where a like "te_t"; 
select * from t1 where match a against ("te*" in boolean mode)+0;
drop table t1;

#
# Bug#7878 with utf8_general_ci, equals (=) has problem with
# accent insensitivity.
# Although originally this problem was found with utf8mb3 character set,
# '=' behaved wrong for latin1_german2_ci as well.
# Let's check it does not work incorrect anymore.
# 
SET NAMES latin1;
CREATE TABLE t1 (
  col1 varchar(255) NOT NULL default ''
) ENGINE=MyISAM DEFAULT CHARSET=latin1 collate latin1_german2_ci;
INSERT INTO t1 VALUES ('ß'),('ss'),('ss');
ALTER TABLE t1 ADD KEY ifword(col1);
SELECT * FROM t1 WHERE col1='ß' ORDER BY col1, BINARY col1;
DROP TABLE t1;


# Revert the collation of the 'test' schema
eval ALTER SCHEMA test COLLATE $old_collation_schema;


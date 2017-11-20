# This tests the functionality of the Myisam engine
# like BUG#31277,Bug#40949,Bug#43973,repair
# All tests are required to run with Myisam
# Hence MTR starts mysqld with MyISAM as default

--source include/force_myisam_default.inc
--source include/have_myisam.inc

-- disable_warnings
DROP TABLE IF EXISTS t1,t2,t3;
-- enable_warnings
#
# BUG#31277 - myisamchk --unpack corrupts a table
#
CREATE TABLE t1(c1 DOUBLE, c2 DOUBLE, c3 DOUBLE, c4 DOUBLE, c5 DOUBLE,
  c6 DOUBLE, c7 DOUBLE, c8 DOUBLE, c9 DOUBLE, a INT PRIMARY KEY);
INSERT INTO t1 VALUES
(-3.31168791059336e-06,-3.19054655887874e-06,-1.06528081684847e-05,-1.227278240089e-06,-1.66718069164799e-06,-2.59038972510885e-06,-2.83145227805303e-06,-4.09678491270648e-07,-2.22610091291797e-06,6),
(0.0030743000272545,2.53222044316438e-05,2.78674650061845e-05,1.95914465544536e-05,1.7347572525984e-05,1.87513810069614e-05,1.69882826885005e-05,2.44449336987598e-05,1.89914629921774e-05,9),
(2.85229319423495e-05,3.05970988282259e-05,3.77161100113133e-05,2.3055238978766e-05,2.08241267364615e-05,2.28009504270553e-05,2.12070165658947e-05,2.84350091565409e-05,2.3366822910704e-05,3),
(0,0,0,0,0,0,0,0,0,12),
(3.24544577570754e-05,3.44619021870993e-05,4.37561613201124e-05,2.57556808726748e-05,2.3195354640561e-05,2.58532400758869e-05,2.34934241667179e-05,3.1621640063232e-05,2.58229982746189e-05,19),
(2.53222044316438e-05,0.00445071933455582,2.97447268116016e-05,2.12379514059868e-05,1.86777776502663e-05,2.0170058676712e-05,1.8946030385445e-05,2.66040037173511e-05,2.09161899668946e-05,20),
(3.03462382611645e-05,3.26517930083994e-05,3.5242025468662e-05,2.53219745106391e-05,2.24384532945004e-05,2.4052346047657e-05,2.23865572957053e-05,3.1634313969082e-05,2.48285463481801e-05,21),
(1.95914465544536e-05,2.12379514059868e-05,2.27808649037128e-05,0.000341724375366877,1.4512761275113e-05,1.56475828693953e-05,1.44372366441415e-05,2.07952121981765e-05,1.61488256935919e-05,28),
(1.7347572525984e-05,1.86777776502663e-05,2.04116907052727e-05,1.4512761275113e-05,0.000432162526082388,1.38116514014465e-05,1.2712914948904e-05,1.82503165178506e-05,1.43043075345922e-05,30),
(1.68339762136661e-05,1.77836497166611e-05,2.36328309295222e-05,1.30183423732016e-05,1.18674654241553e-05,1.32467273128652e-05,1.24581739117775e-05,1.55624190959406e-05,1.33010638508213e-05,31),
(1.89643062824415e-05,2.06997140070717e-05,2.29045490159364e-05,1.57918175731019e-05,1.39864987449492e-05,1.50580274578455e-05,1.45908734129609e-05,1.95329296993327e-05,1.5814709481221e-05,32),
(1.69882826885005e-05,1.8946030385445e-05,2.00820439721439e-05,1.44372366441415e-05,1.2712914948904e-05,1.35209686474184e-05,0.00261563314789896,1.78285095864627e-05,1.46699314500019e-05,34),
(2.0278186540684e-05,2.18923409729654e-05,2.39981539939738e-05,1.71774589459438e-05,1.54654355357383e-05,1.62731485707636e-05,1.49253140625051e-05,2.18229800160297e-05,1.71923561673718e-05,35),
(2.44449336987598e-05,2.66040037173511e-05,2.84860148925308e-05,2.07952121981765e-05,1.82503165178506e-05,1.97667730441441e-05,1.78285095864627e-05,0.00166478601822712,2.0299952103232e-05,36),
(1.89914629921774e-05,2.09161899668946e-05,2.26026841007872e-05,1.61488256935919e-05,1.43043075345922e-05,1.52609063290127e-05,1.46699314500019e-05,2.0299952103232e-05,0.00306670170971682,39),
(0,0,0,0,0,0,0,0,0,41),
(0,0,0,0,0,0,0,0,0,17),
(0,0,0,0,0,0,0,0,0,18),
(2.51880677333017e-05,2.63051795435778e-05,2.79874748974906e-05,2.02888886670845e-05,1.8178636318197e-05,1.91308527003585e-05,1.83260023644133e-05,2.4422300558171e-05,1.96411467520551e-05,44),
(2.22402118719591e-05,2.37546284320705e-05,2.58463051055541e-05,1.83391609130854e-05,1.6300720519646e-05,1.74559091886791e-05,1.63733785575587e-05,2.26616253279828e-05,1.79541237435621e-05,45),
(3.01092775359837e-05,3.23865212934412e-05,4.09444584045994e-05,0,2.15470966302776e-05,2.39082636344032e-05,2.28296706429177e-05,2.9007671511595e-05,2.44201138973326e-05,46);
FLUSH TABLES;
let $MYSQLD_DATADIR= `select @@datadir`;
--exec $MYISAMPACK -s $MYSQLD_DATADIR/test/t1
--exec $MYISAMCHK -srq $MYSQLD_DATADIR/test/t1
--exec $MYISAMCHK -s --unpack $MYSQLD_DATADIR/test/t1
CHECK TABLE t1 EXTENDED;
DROP TABLE t1;

#
# Bug#40949 Debug version of MySQL server crashes when run OPTIMIZE on compressed table.
# expanded with testcase for
# BUG#41574 - REPAIR TABLE: crashes for compressed tables
#
--disable_warnings
drop table if exists t1;
--enable_warnings
create table t1(f1 int, f2 char(255));
insert into t1 values(1, 'foo'), (2, 'bar');
insert into t1 select * from t1;
insert into t1 select * from t1;
insert into t1 select * from t1;
insert into t1 select * from t1;
insert into t1 select * from t1;
insert into t1 select * from t1;
insert into t1 select * from t1;
insert into t1 select * from t1;
insert into t1 select * from t1;
insert into t1 select * from t1;
insert into t1 select * from t1;
insert into t1 select * from t1;
flush tables;
let $MYSQLD_DATADIR= `select @@datadir`;
--exec $MYISAMPACK $MYSQLD_DATADIR/test/t1
optimize table t1;
repair table t1;
drop table t1;

--echo #
--echo # BUG#41541 - Valgrind warnings on packed MyISAM table
--echo #
CREATE TABLE  t1(f1 VARCHAR(200), f2 TEXT);
INSERT INTO  t1 VALUES ('foo', 'foo1'), ('bar', 'bar1');
let $i=9;
--disable_query_log
while ($i)
{
 INSERT INTO t1 SELECT * FROM t1; 
 dec $i; 
}
--enable_query_log
FLUSH TABLE t1; 
--echo # Compress the table using MYISAMPACK tool
let $MYSQLD_DATADIR= `select @@datadir`;
--exec $MYISAMPACK $MYSQLD_DATADIR/test/t1
SELECT COUNT(*) FROM t1;
DROP TABLE t1; 

--echo #
--echo # Bug #43973 - backup_myisam.test fails on 6.0-bugteam
--echo #
CREATE DATABASE mysql_db1;
CREATE TABLE mysql_db1.t1 (c1 VARCHAR(5), c2 int);
CREATE INDEX i1 ON mysql_db1.t1 (c1, c2);
INSERT INTO mysql_db1.t1 VALUES ('A',1);
INSERT INTO mysql_db1.t1 SELECT * FROM mysql_db1.t1;
INSERT INTO mysql_db1.t1 SELECT * FROM mysql_db1.t1;
INSERT INTO mysql_db1.t1 SELECT * FROM mysql_db1.t1;
INSERT INTO mysql_db1.t1 SELECT * FROM mysql_db1.t1;
INSERT INTO mysql_db1.t1 SELECT * FROM mysql_db1.t1;
INSERT INTO mysql_db1.t1 SELECT * FROM mysql_db1.t1;
INSERT INTO mysql_db1.t1 SELECT * FROM mysql_db1.t1;
FLUSH TABLE mysql_db1.t1;
#
--echo # Compress the table using MYISAMPACK tool
let $MYSQLD_DATADIR= `select @@datadir`;
--exec $MYISAMPACK -s $MYSQLD_DATADIR/mysql_db1/t1
--echo # Run MYISAMCHK tool on the compressed table
--exec $MYISAMCHK -srq $MYSQLD_DATADIR/mysql_db1/t1
SELECT COUNT(*) FROM mysql_db1.t1 WHERE c2 < 5;
DROP DATABASE mysql_db1;

--echo #
--echo # BUG#11761180 - 53646: MYISAMPACK CORRUPTS TABLES WITH FULLTEXT INDEXES
--echo #
CREATE TABLE t1(a CHAR(4), FULLTEXT(a));
INSERT INTO t1 VALUES('aaaa'),('bbbb'),('cccc');
FLUSH TABLE t1;
--exec $MYISAMPACK -sf $MYSQLD_DATADIR/test/t1
--exec $MYISAMCHK -srq $MYSQLD_DATADIR/test/t1
CHECK TABLE t1;
SELECT * FROM t1 WHERE MATCH(a) AGAINST('aaaa' IN BOOLEAN MODE);
SELECT * FROM t1 WHERE MATCH(a) AGAINST('aaaa');
DROP TABLE t1;

--echo # Test table with key_reflength > rec_reflength
CREATE TABLE t1(a CHAR(30), FULLTEXT(a));
--disable_query_log
--echo # Populating a table, so it's index file exceeds 65K
let $1=1700;
while ($1)
{
  eval INSERT INTO t1 VALUES('$1aaaaaaaaaaaaaaaaaaaaaaaaaa');
  dec $1;
}

--echo # Populating a table, so index file has second level fulltext tree
let $1=60;
while ($1)
{
  eval INSERT INTO t1 VALUES('aaaa'),('aaaa'),('aaaa'),('aaaa'),('aaaa');
  dec $1;
}
--enable_query_log

FLUSH TABLE t1;
--echo # Compressing table
--exec $MYISAMPACK -sf $MYSQLD_DATADIR/test/t1
--echo # Fixing index (repair by sort)
--exec $MYISAMCHK -srnq $MYSQLD_DATADIR/test/t1
CHECK TABLE t1;
FLUSH TABLE t1;
--echo # Fixing index (repair with keycache)
--exec $MYISAMCHK -soq $MYSQLD_DATADIR/test/t1
CHECK TABLE t1;
DROP TABLE t1;

--echo #
--echo # BUG#11751736: DROP DATABASE STATEMENT SHOULD REMOVE .OLD SUFFIX FROM 
--echo # DATABASE DIRECTORY 
--echo #
CREATE DATABASE db1;
CREATE TABLE db1.t1(c1 INT) ENGINE=MyISAM;
## Added -f to force pack db in any case regardless the size of database 
## being packed
let $MYSQLD_DATADIR = `SELECT @@datadir`;
--exec $MYISAMPACK -b -f $MYSQLD_DATADIR/db1/t1
DROP DATABASE db1;


--echo #
--echo # Test coverage for myisampack/myisamchk --unpack interaction with
--echo # the data dictionary.
--echo #
--echo # The first DML statement which accesses table after it was
--echo # compressed/uncompressed triggers data-dictionary update which
--echo # synchronizes ROW_FORMAT for the table stored in the data dictionary
--echo # and in the MyISAM files.
--echo #

--echo #
--echo # This test assumes that the table metadata being shown by
--echo # INFORMATION_SCHEMA.TABLES would internally 'open' the table.
--echo # However with WL#6599, we do not open the under lying table for every
--echo # row fetch for INFORMATION_SCHEMA.TABLES by default. This
--echo # demands following two things,
--echo # 1. Use of configuration variable
--echo #    'information_schema_stats_expiry' to be set to '0',
--echo #    which would make INFORMATION_SCHEMA implementation to access
--echo #    storage engine to open the table and fetch dynamic statistics.
--echo # 2. Use a column of INFORMATION_SCHEMA.TABLES which would fetch
--echo #    dynamic table statistics, which would trigger 1.
--echo #
SET SESSION information_schema_stats_expiry=0;

CREATE TABLE t1(a CHAR(4)) ENGINE=MYISAM;
SELECT table_rows, row_format, table_comment FROM information_schema.tables
  WHERE TABLE_SCHEMA='test' AND TABLE_NAME='t1';
INSERT INTO t1 VALUES('aaaa'),('bbbb'),('cccc');
FLUSH TABLE t1;
--echo # Compressing table t1.
let $MYSQLD_DATADIR= `select @@datadir`;
--exec $MYISAMPACK -sf $MYSQLD_DATADIR/test/t1
--echo # ROW_FORMAT in DD is invalid/outdated here.
SELECT table_rows, row_format, table_comment FROM information_schema.tables
  WHERE TABLE_SCHEMA='test' AND TABLE_NAME='t1';
--echo # The below statement should cause ROW_FORMAT update.
SELECT COUNT(*) FROM t1;
SELECT table_rows, row_format, table_comment FROM information_schema.tables
  WHERE TABLE_SCHEMA='test' AND TABLE_NAME='t1';
FLUSH TABLE t1;
--echo # Uncompressing table t1.
--exec $MYISAMCHK -s --unpack $MYSQLD_DATADIR/test/t1
--echo # ROW_FORMAT in DD is invalid/outdated here.
SELECT table_rows, row_format, table_comment FROM information_schema.tables
  WHERE TABLE_SCHEMA='test' AND TABLE_NAME='t1';
--echo # The below statement should cause ROW_FORMAT update.
SELECT COUNT(*) FROM t1;
SELECT table_rows, row_format, table_comment FROM information_schema.tables
  WHERE TABLE_SCHEMA='test' AND TABLE_NAME='t1';
DROP TABLE t1;

SET SESSION information_schema_stats_expiry=default;

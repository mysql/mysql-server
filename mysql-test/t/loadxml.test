
#
# Tests for "LOAD XML" - a contributed patch from Erik Wetterberg.
#

--disable_warnings
drop table if exists t1, t2;
--enable_warnings

create table t1 (a int, b varchar(64));


--echo -- Load a static XML file
load xml infile '../../std_data/loadxml.dat' into table t1
rows identified by '<row>';
select * from t1 order by a;
delete from t1;


--echo -- Load a static XML file with 'IGNORE num ROWS'
load xml infile '../../std_data/loadxml.dat' into table t1
rows identified by '<row>' ignore 4 rows;
select * from t1 order by a;


--echo -- Check 'mysqldump --xml' + 'LOAD XML' round trip
--exec $MYSQL_DUMP --xml test t1 > "$MYSQLTEST_VARDIR/tmp/loadxml-dump.xml" 2>&1
delete from t1;
--replace_result $MYSQLTEST_VARDIR MYSQLTEST_VARDIR
--eval load xml infile '$MYSQLTEST_VARDIR/tmp/loadxml-dump.xml' into table t1 rows identified by '<row>';
select * from t1 order by a;

--echo --Check that default row tag is '<row>
delete from t1;
--replace_result $MYSQLTEST_VARDIR MYSQLTEST_VARDIR
--eval load xml infile '$MYSQLTEST_VARDIR/tmp/loadxml-dump.xml' into table t1;
select * from t1 order by a;

--echo -- Check that 'xml' is not a keyword
select 1 as xml;


#
# Bug #42520    killing load .. infile Assertion failed: ! is_set(), file .\sql_error.cc, line 8
#

--disable_query_log
delete from t1;
insert into t1 values (1, '12345678900987654321'), (2, 'asdfghjkl;asdfghjkl;');
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
insert into t1 select * from t1;
--exec $MYSQL_DUMP --xml test t1 > "$MYSQLTEST_VARDIR/tmp/loadxml-dump.xml" 2>&1
--enable_query_log

connect (addconroot, localhost, root,,);
connection addconroot;
create table t2(fl text);
--let $PSEUDO_THREAD_ID=`select @@pseudo_thread_id    `

--replace_result $MYSQLTEST_VARDIR MYSQLTEST_VARDIR
--send_eval LOAD XML LOCAL INFILE "$MYSQLTEST_VARDIR/tmp/loadxml-dump.xml"  INTO TABLE t2 ROWS IDENTIFIED BY '<person>';

sleep 3;


connection default;

--disable_query_log
--eval kill $PSEUDO_THREAD_ID
connection addconroot;
# Read response from connection to avoid packets out-of-order when disconnecting
# Note, that connection can already be dead due to previously issued kill
--error 0,2013
--reap
disconnect addconroot;
connection default;
--enable_query_log
#
# Clean up
#
remove_file $MYSQLTEST_VARDIR/tmp/loadxml-dump.xml;
drop table t1;
drop table t2;

#
# Bug #36750    LOAD XML doesn't understand new line (feed) characters in multi line text fields
#

create table t1 (
  id int(11) not null,
  text text,
  primary key (id)
) default charset=latin1;
load xml infile '../../std_data/loadxml2.dat' into table t1;
select * from t1;
drop table t1;

--echo #
--echo # Bug#51571 load xml infile causes server crash
--echo #
CREATE TABLE t1 (a text, b text);
LOAD XML INFILE '../../std_data/loadxml.dat' INTO TABLE t1
ROWS IDENTIFIED BY '<row>' (a,@b) SET b=concat('!',@b);
SELECT * FROM t1 ORDER BY a;
DROP TABLE t1;


--echo #
--echo # Bug#16171518 LOAD XML DOES NOT HANDLE EMPTY ELEMENTS
--echo #
CREATE TABLE t1 (col1 VARCHAR(3), col2 VARCHAR(3), col3 VARCHAR(3), col4 VARCHAR(4));
LOAD XML INFILE '../../std_data/bug16171518_1.dat' INTO TABLE t1;
SELECT * FROM t1 ORDER BY col1, col2, col3, col4;
DROP TABLE t1;

CREATE TABLE t1 (col1 VARCHAR(3), col2 VARCHAR(3), col3 INTEGER);
LOAD XML INFILE '../../std_data/bug16171518_2.dat' INTO TABLE t1;
SELECT * FROM t1 ORDER BY col1, col2, col3;
DROP TABLE t1;

--echo #
--echo # Regression test added in WL#8063
--echo #

CREATE TABLE t1 (col1 VARCHAR(3), col2 VARCHAR(3), col3 INTEGER);
CREATE TABLE t2(i INT NOT NULL);
CREATE TRIGGER t1_ai AFTER INSERT ON t1 FOR EACH ROW INSERT INTO t2 VALUES (NULL);

--error ER_BAD_NULL_ERROR
LOAD XML INFILE '../../std_data/bug16171518_2.dat' INTO TABLE t1;

DROP TABLE t1, t2;


CREATE TABLE t3 (col1 VARCHAR(3), col2 VARCHAR(3), col3 INTEGER);
CREATE VIEW v3 AS SELECT * FROM t3 WHERE col3 < 0 WITH CHECK OPTION;

--error ER_VIEW_CHECK_FAILED
LOAD XML INFILE '../../std_data/bug16171518_2.dat' INTO TABLE v3;

DROP VIEW v3;
DROP TABLE t3;


CREATE TABLE t4 (col1 VARCHAR(3), col2 VARCHAR(3), col3 INTEGER, col4 INT NOT NULL);

--error ER_BAD_NULL_ERROR
LOAD XML INFILE '../../std_data/bug16171518_2.dat' INTO TABLE t4 (col1, col2, col3) SET col4 = NULL;

DROP TABLE t4;


--echo #
--echo # BUG#30753708: LOAD XML: FIELD VALUES LEAK WHEN GOING OUT OF SCOPE
--echo #
CREATE TABLE t1(a INT NOT NULL PRIMARY KEY, p INT NULL);
LOAD XML INFILE '../../std_data/bug30753708.dat' INTO TABLE t1 ROWS IDENTIFIED BY '<address>';
SELECT * FROM t1 ORDER BY a;
DROP TABLE t1;

CREATE TABLE t1(a INT NOT NULL PRIMARY KEY, p INT NULL, s VARCHAR(100));
LOAD XML INFILE '../../std_data/bug30753708.dat' INTO TABLE t1 ROWS IDENTIFIED BY '<address>';
SELECT * FROM t1 ORDER BY a;
DROP TABLE t1;

--echo #

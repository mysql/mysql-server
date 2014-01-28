include/master-slave.inc
Warnings:
Note	####	Sending passwords in plain text without SSL/TLS is extremely insecure.
Note	####	Storing MySQL user name or password information in the master info repository is not secure and is therefore not recommended. Please consider using the USER and PASSWORD connection options for START SLAVE; see the 'START SLAVE Syntax' in the MySQL Manual for more information.
[connection master]
DROP PROCEDURE IF EXISTS test.p1;
DROP TABLE IF EXISTS test.t1;
CREATE TABLE test.t1 (a INT, blob_column LONGBLOB, PRIMARY KEY(a));
INSERT INTO test.t1  VALUES(1,'test');
UPDATE test.t1 SET blob_column=LOAD_FILE('../../std_data/words2.dat') WHERE a=1;
create procedure test.p1()
begin
INSERT INTO test.t1  VALUES(2,'test');
UPDATE test.t1 SET blob_column=LOAD_FILE('../../std_data/words2.dat') WHERE a=2;
end|
CALL test.p1();
SELECT * FROM test.t1 ORDER BY blob_column;
a	blob_column
1	abase
abased
abasement
abasements
abases
abash
abashed
abashes
abashing
abasing
abate
abated
abatement
abatements
abater
abates
abating
Abba
abbe
abbey
abbeys
abbot
abbots
Abbott
abbreviate
abbreviated
abbreviates
abbreviating
abbreviation
abbreviations
Abby
abdomen
abdomens
abdominal
abduct
abducted
abduction
abductions
abductor
abductors
abducts
Abe
abed
Abel
Abelian
Abelson
Aberdeen
Abernathy
aberrant
aberration

2	abase
abased
abasement
abasements
abases
abash
abashed
abashes
abashing
abasing
abate
abated
abatement
abatements
abater
abates
abating
Abba
abbe
abbey
abbeys
abbot
abbots
Abbott
abbreviate
abbreviated
abbreviates
abbreviating
abbreviation
abbreviations
Abby
abdomen
abdomens
abdominal
abduct
abducted
abduction
abductions
abductor
abductors
abducts
Abe
abed
Abel
Abelian
Abelson
Aberdeen
Abernathy
aberrant
aberration

include/sync_slave_sql_with_master.inc
SELECT * FROM test.t1 ORDER BY blob_column;
a	blob_column
1	abase
abased
abasement
abasements
abases
abash
abashed
abashes
abashing
abasing
abate
abated
abatement
abatements
abater
abates
abating
Abba
abbe
abbey
abbeys
abbot
abbots
Abbott
abbreviate
abbreviated
abbreviates
abbreviating
abbreviation
abbreviations
Abby
abdomen
abdomens
abdominal
abduct
abducted
abduction
abductions
abductor
abductors
abducts
Abe
abed
Abel
Abelian
Abelson
Aberdeen
Abernathy
aberrant
aberration

2	abase
abased
abasement
abasements
abases
abash
abashed
abashes
abashing
abasing
abate
abated
abatement
abatements
abater
abates
abating
Abba
abbe
abbey
abbeys
abbot
abbots
Abbott
abbreviate
abbreviated
abbreviates
abbreviating
abbreviation
abbreviations
Abby
abdomen
abdomens
abdominal
abduct
abducted
abduction
abductions
abductor
abductors
abducts
Abe
abed
Abel
Abelian
Abelson
Aberdeen
Abernathy
aberrant
aberration

DROP PROCEDURE IF EXISTS test.p1;
DROP TABLE test.t1;
include/sync_slave_sql_with_master.inc
include/rpl_reset.inc
SELECT repeat('x',20) INTO OUTFILE 'MYSQLTEST_VARDIR/tmp/bug_39701.data';
DROP TABLE IF EXISTS t1;
CREATE TABLE t1 (t text);
CREATE PROCEDURE p(file varchar(4096)) 
BEGIN
INSERT INTO t1 VALUES (LOAD_FILE(file));
END|
include/stop_slave.inc
CALL p('MYSQLTEST_VARDIR/tmp/bug_39701.data');
include/start_slave.inc
include/sync_slave_sql_with_master.inc
include/diff_tables.inc [master:t1, slave:t1]
DROP TABLE t1;
DROP PROCEDURE p;
include/rpl_end.inc

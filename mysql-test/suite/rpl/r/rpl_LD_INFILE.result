include/master-slave.inc
Warnings:
Note	####	Sending passwords in plain text without SSL/TLS is extremely insecure.
Note	####	Storing MySQL user name or password information in the master info repository is not secure and is therefore not recommended. Please consider using the USER and PASSWORD connection options for START SLAVE; see the 'START SLAVE Syntax' in the MySQL Manual for more information.
[connection master]
DROP TABLE IF EXISTS test.t1;
CREATE TABLE test.t1 (a VARCHAR(255), PRIMARY KEY(a));
LOAD DATA INFILE '../../std_data/words2.dat' INTO TABLE test.t1;
DELETE FROM test.t1 WHERE a = 'abashed';
DELETE FROM test.t1;
LOAD DATA INFILE '../../std_data/words2.dat' INTO TABLE test.t1;
SELECT * FROM test.t1 ORDER BY a DESC;
a
aberration
aberrant
Abernathy
Aberdeen
Abelson
Abelian
Abel
abed
Abe
abducts
abductors
abductor
abductions
abduction
abducted
abduct
abdominal
abdomens
abdomen
Abby
abbreviations
abbreviation
abbreviating
abbreviates
abbreviated
abbreviate
Abbott
abbots
abbot
abbeys
abbey
abbe
Abba
abating
abates
abater
abatements
abatement
abated
abate
abasing
abashing
abashes
abashed
abash
abases
abasements
abasement
abased
abase
include/sync_slave_sql_with_master.inc
SELECT * FROM test.t1 ORDER BY a DESC;
a
aberration
aberrant
Abernathy
Aberdeen
Abelson
Abelian
Abel
abed
Abe
abducts
abductors
abductor
abductions
abduction
abducted
abduct
abdominal
abdomens
abdomen
Abby
abbreviations
abbreviation
abbreviating
abbreviates
abbreviated
abbreviate
Abbott
abbots
abbot
abbeys
abbey
abbe
Abba
abating
abates
abater
abatements
abatement
abated
abate
abasing
abashing
abashes
abashed
abash
abases
abasements
abasement
abased
abase
DROP TABLE test.t1;
include/sync_slave_sql_with_master.inc
include/rpl_end.inc

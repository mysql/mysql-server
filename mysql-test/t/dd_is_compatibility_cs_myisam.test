--source include/force_myisam_default.inc
--source include/have_myisam.inc

# Result differences depending on FS case sensitivity.
if (!$require_case_insensitive_file_system)
{
  --source include/have_case_sensitive_file_system.inc
}

#

--echo #########################################################
--echo # WL#6599: New data dictionary and I_S.
--echo # 
--echo # The re-implemntation of I_S as views on top of DD tables,
--echo # together with the modified way of retrieving statistics
--echo # information, introduces some differences when comparing
--echo # with the previous I_S implementation. The purpose of this
--echo # test is to focus on these behavioral differences, both
--echo # for the purpose of regression testing, and to document
--echo # the changes. The issues below refer to the items listed
--echo # in the WL#6599 text (HLS section 6).


--echo #########################################################
--echo # WL#6599 HLS/6m
--echo # MySQL 'DISABLE KEYS' functionality is only applicable to MyISAM
--echo # tables, but looks like its marked for InnoDB table too in trunk.
--echo # This problem is fixed now. It has been discussed with Martin Hansson
--echo # who worked on WL8697 which introduced invisible index. He agreed that
--echo # the behavior on trunk is bad and needed to be fixed. We have fixed
--echo # that now.
--echo #########################################################

CREATE TABLE t1 ( a INT, KEY (a) INVISIBLE );
CREATE TABLE t2 ( a INT, KEY (a) INVISIBLE ) ENGINE=MyISAM;
SHOW KEYS FROM t1;
SHOW KEYS FROM t2;
CREATE TABLE t3 ( a INT, KEY (a));
SHOW KEYS FROM t3;
ALTER TABLE t3 ALTER INDEX a INVISIBLE;
SHOW KEYS FROM t3;
CREATE TABLE t4 ( a INT, KEY (a)) ENGINE=MyISAM;
SHOW KEYS FROM t4;
ALTER TABLE t4 ALTER INDEX a INVISIBLE;
SHOW KEYS FROM t4;
DROP TABLE t1;
DROP TABLE t2;
DROP TABLE t3;
DROP TABLE t4;

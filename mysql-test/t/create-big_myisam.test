# This test takes rather long time so let us run it only in --big-test mode
#--source include/big_test.inc
--source include/not_valgrind.inc
# We need the Debug Sync Facility.
--source include/have_debug_sync.inc
# Some of tests below also use binlog to check that statements are
# executed and logged in correct order
#--source include/rpl/force_binlog_format_statement.inc
# Save the initial number of concurrent sessions.
#--source include/count_sessions.inc

--source include/force_myisam_default.inc
--source include/have_myisam.inc

--echo #
--echo # WL#6390: Use new DD API for handling non-partitioned tables
--echo # Test limits on number of columns. See also comment_column2.test
--echo # and view.test for additional coverage.
--echo #

#
# MyISAM has limits imposed by the server

let $colnum= 4095;
let $str= c text;
while ($colnum)
{
  let $str= c$colnum int, $str;
  dec $colnum;
}
--eval CREATE TABLE t1 ($str) engine= myisam;
--error ER_TOO_MANY_FIELDS
ALTER TABLE t1 ADD COLUMN too_much int;
DROP TABLE t1;

let $str= c4096 int, $str;
--error ER_TOO_MANY_FIELDS
--eval CREATE TABLE t1 ($str) engine= myisam;

--echo #
--echo # Tests for limitations related to ENUMs and SETs
--echo #

--echo #
--echo # 1: Max number of ENUM/SET columns

#
# MyISAM has limits imposed by the server

let $colnum= 4095;
let $str= c4096 ENUM('a');
while ($colnum)
{
  let $str= c$colnum ENUM('a$colnum'), $str;
  dec $colnum;
}
--eval CREATE TABLE t1 ($str) engine= myisam
--error ER_TOO_MANY_FIELDS
ALTER TABLE t1 ADD COLUMN too_much ENUM('a9999');
DROP TABLE t1;

let $str= $str, too_much ENUM('a9999');
--error ER_TOO_MANY_FIELDS
--eval CREATE TABLE t1 ($str) engine= myisam

#
## MyISAM has limits imposed by the server
#

let $colnum= 4095;
let $str= c4096 SET('a');
while ($colnum)
{
  let $str= c$colnum SET('a$colnum'), $str;
  dec $colnum;
}
--eval CREATE TABLE t1 ($str) engine= myisam
--error ER_TOO_MANY_FIELDS
ALTER TABLE t1 ADD COLUMN too_much SET('a9999');
DROP TABLE t1;

let $str= $str, too_much SET('a9999');
--error ER_TOO_MANY_FIELDS
--eval CREATE TABLE t1 ($str) engine= myisam


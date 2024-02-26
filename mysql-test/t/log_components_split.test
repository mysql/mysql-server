--echo #
--echo # WL#9343:  Logging services: log writers
--echo #

#
# this test uses "production" mode where different formats (JSON,
# test, etc.) go to different files, and we'll concatenate them
# one after the other. log_components.test in contrast shows the
# "debug" mode where we interleave output from all sinks to stderr
# for immediate comparison.
#

--source include/have_debug.inc
--source include/have_log_component.inc

call mtr.add_suppression("filter_rules: *");
call mtr.add_suppression(".*No error message, or error message of non-string type. This is almost certainly a bug!");
call mtr.add_suppression(".*using log_message.. with .*");
call mtr.add_suppression(".*System schema directory does not exist.");
call mtr.add_suppression(".* Simulated");
call mtr.add_suppression(".*aaaaaaaaaaaaa");

let GREP_START=`SELECT DATE_FORMAT(CONVERT_TZ(SYSDATE(6),'SYSTEM','UTC'),'%Y%m%d%H%i%s%f');`;

SET @orig_log_error_verbosity= @@GLOBAL.log_error_verbosity;
SET @@global.log_error_verbosity=3;

let $log_error_  = `SELECT @@GLOBAL.log_error`;
let $log_json_0_ = `SELECT REPLACE(@@GLOBAL.log_error, '.err', '.err.00.json')`;
let $log_json_1_ = `SELECT REPLACE(@@GLOBAL.log_error, '.err', '.err.01.json')`;

if($log_error_ == "stderr")
{
  let $log_error_  = $MYSQLTEST_VARDIR/log/mysqld.1.err;
  let $log_json_0_ = $MYSQLTEST_VARDIR/log/mysqld.1.err.00.json;
  let $log_json_1_ = $MYSQLTEST_VARDIR/log/mysqld.1.err.01.json;
}
FLUSH LOGS;

# Send parse-trace to error log; first one with a current timestamp
# to compare against our GREP_START defined above.
SET @@session.debug="+d,parser_stmt_to_error_log";
# Now normalize timestamp and thread_ID on all following lines,
# for less hassle with --regex_replace in test files.  Once
# WL#9651 goes live, we can use that to achieve the same thing.
SET @@session.debug="+d,log_error_normalize";

# Log parser statements with System label too.
SET @@session.debug="+d,parser_stmt_to_error_log_with_system_prio";

SELECT @@global.log_error_services;
--echo

SELECT "*** TRYING TO LOG THINGS FROM EXTERNAL SERVICE ***";
--echo # NB: log_sink_test must self-disable its sink after one line.
INSTALL COMPONENT "file://component_log_sink_json";
INSTALL COMPONENT "file://component_log_sink_test";
SET @@global.log_error_services="log_filter_internal; log_sink_test; log_sink_json; log_sink_internal";
SELECT "logging as traditional MySQL error log and as JSON";
FLUSH ERROR LOGS;
SET @@global.log_error_services="log_sink_json; log_sink_json";
SELECT "double dutch!";
SET @@global.log_error_services="log_sink_json";
SELECT "Escape this: \" \' \\ >< >< > < ⟼λ⮞γζ←⤆↕↑↓A7Uη)R|5怪獣";
SELECT "Too long: ⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓A7Uη)R|5怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼AZ0Uλ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣⟼λ⮞γζ←⤆↕↑↓怪獣";
SELECT "Too long as well: 
                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                ";
SET @@global.log_error_services="log_filter_internal; log_sink_internal";
UNINSTALL COMPONENT "file://component_log_sink_test";
UNINSTALL COMPONENT "file://component_log_sink_json";
--echo


--echo # cleanup
SET @@session.debug="-d,parser_stmt_to_error_log_with_system_prio";
SET @@session.debug="-d,parser_stmt_to_error_log";
SET @@session.debug="-d,log_error_normalize";
SET @@global.log_error_verbosity= @orig_log_error_verbosity;
## WL#9651
# SET @@global.log_error_filter_rules=@save_filters;

FLUSH ERROR LOGS;


--echo
--echo ###
--echo ### error log file
--echo ###
--echo

let GREP_FILE=$log_error_;

perl;
   use strict;
   use File::stat;
   my $file= $ENV{'GREP_FILE'} or die("grep file not set");
   my $pattern="^20";
   my $stime= $ENV{'GREP_START'};

   open(FILE, "$file") or die("Unable to open $file: $!");
   while (<FILE>) {
     my $line = $_;
     my $ts = 0;

     if ($stime == 0) {
       print "$line";
     }
     elsif (($line =~ /$pattern/) and not ($line =~ /redo log/)) {
       $line =~ /([0-9][0-9][0-9][0-9])-([0-9][0-9])-([0-9][0-9])T([0-9][0-9]):([0-9][0-9]):([0-9][0-9])\.([0-9][0-9][0-9][0-9][0-9][0-9])[-+Z][0-9:]* *[0-9]* *?(\[.*)/;
       $ts=$1.$2.$3.$4.$5.$6.$7;
       if ($ts >= $stime) {
         $stime= 0;
       }
     }
   }
   close(FILE);
EOF

--echo
--echo ###
--echo ### error log file (JSON) -- stream 0
--echo ###
--echo

let GREP_FILE=$log_json_0_;

perl;
   use strict;
   use File::stat;
   my $file= $ENV{'GREP_FILE'} or die("grep file not set");

   open(FILE, "$file") or die("Unable to open $file: $!");
   while (<FILE>) {
     my $line = $_;
     $line =~ s/"source_line" : \d+, //;
     $line =~ s/"err_code" : \d+, //;
     print "$line";
   }
   close(FILE);
EOF

--echo
--echo ###
--echo ### error log file (JSON) -- stream 1
--echo ###
--echo

let GREP_FILE=$log_json_1_;

perl;
   use strict;
   use File::stat;
   my $file= $ENV{'GREP_FILE'} or die("grep file not set");

   open(FILE, "$file") or die("Unable to open $file: $!");
   while (<FILE>) {
     my $line = $_;
     $line =~ s/"source_line" : \d+, //;
     $line =~ s/"err_code" : \d+, //;
     print "$line";
   }
   close(FILE);
EOF

--remove_file $log_json_0_
--remove_file $log_json_1_


--echo
--echo #
--echo # Bug#31464539: SET PERSISTED LOG_ERROR_SERVICES WAS RESTORED PREMATURELY
--echo #
--echo

--echo # Set up logging via PERSISTs. Order of the PERSISTs matters!
RESET PERSIST;
SET PERSIST log_error_verbosity=3;
SET PERSIST log_error_services="log_filter_internal; log_sink_internal; log_sink_json";
SET PERSIST innodb_monitor_enable=all;

--echo # Restart server and verify log configuration.
--echo # We should also receive a full JSON log of the start-up.
--source include/restart_mysqld.inc

SELECT @@global.log_error_verbosity;
SELECT @@global.log_error_services;
SELECT @@global.innodb_monitor_enable;

--echo # Reset PERSISTs.
RESET PERSIST;
SET GLOBAL log_error_services=DEFAULT;

--echo # Log file analysis:
let GREP_FILE=$log_json_0_;

perl;
   use strict;
   use File::stat;
   my $file= $ENV{'GREP_FILE'} or die("grep file not set");

   my $second_run=0;
   my $seen_inno=0;
   my $post_inno=0;

   open(FILE, "$file") or die("Unable to open $file: $!");
   while (<FILE>) {
     my $line = $_;

     # Ignore everything pre-shutdown
     if ($line =~ /"ER_SERVER_SHUTDOWN_COMPLETE"/) {
       $second_run=1; }

     if ($second_run > 0) {
       # Count un-buffered InnoDB messages.
       if (!($line =~ /"buffered" : /) && ($line =~ /"subsystem" : "InnoDB"/)) {
         $seen_inno++; }
       #
       if (($line =~ /"buffered" : /) && ($seen_inno > 0)) {
         $post_inno++; }}}
   close(FILE);

   if ($post_inno > 0) {
     print "FAIL: $post_inno buffered lines seen after $seen_inno unbuffered ones.\n"; }
   else {
     print "SUCCESS!\n"; }
EOF

# --remove_file $MYSQLD_DATADIR/mysqld-auto.cnf
--remove_file $log_json_0_

--echo # Restart server with all defaults
--source include/restart_mysqld.inc


--echo
--echo ###
--echo ### done
--echo ###

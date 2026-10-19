# Stops the debug trace that the server was started with, and counts the
# events that read_gtids_from_binlog() read at startup, from the trace file
# $bolt_trace, into $bolt_events_read.
--disable_query_log
SET GLOBAL debug = '';
--enable_query_log
--let $bolt_count_inc = $MYSQLTEST_VARDIR/tmp/bolt_unusable_header_count.inc
--exec grep -c 'read_gtids_from_binlog: info: done=' $bolt_trace | xargs printf -- '--let \044bolt_events_read = %s\n' > $bolt_count_inc
--source $bolt_count_inc
--remove_file $bolt_count_inc
--remove_file $bolt_trace

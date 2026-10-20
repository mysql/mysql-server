# --let $compare_log_number_occurrences=NUMBER
# --let $compare_log_level=LEVEL # optional (defaults to ERROR)
# --let $compare_log_pattern=PATTERN
# --source ../include/cmrs/compare_log.inc

if (!$compare_log_level) {
   --let $compare_log_level=ERROR
}

--let $grep_file=$MYSQLTEST_VARDIR/log/mysqld.1.err
--let $grep_output=none
--let $grep_pattern=\[$compare_log_level] \[.*\] \[Server\] MySQL Rest Service Component: $compare_log_pattern
--source include/grep_pattern.inc

if ($GREP_RESULT != $compare_log_number_occurrences)
{
   --echo '$grep_pattern' found '$GREP_RESULT' times.
   --die "Number of occurences of the message in the log file is different than expected"
}

--let $compare_log_number_occurrences=
--let $compare_log_level=
--let $compare_log_pattern=

# report_stats.inc

--disable_query_log
# SELECT * FROM stats;
--enable_query_log

let $report_file= $MYSQLTEST_VARDIR/log/report_crunch_update.log;

let $sample_period = `SELECT TIMEDIFF('$end_time', '$start_time')`;
let $transactions_applied = `SELECT end-start FROM stats WHERE name = 'Ndb_api_trans_commit_count_replica'`;
let $bytes_sent = `SELECT (end-start) FROM stats WHERE name = 'Ndb_api_bytes_sent_count_replica'`;

--exec $MYSQL_TEST_DIR/suite/ndbcrunch/report_stats.pl --file=$report_file --start="$start_time" --end="$end_time" --time="$sample_period" --tx=$transactions_applied --bytes=$bytes_sent --info="$test_info" --info="$config_info"

#
# Test --log-slow-extra
#
--source include/have_innodb_16k.inc

# We'll be looking at the contents of the slow log later, and PS protocol
# would give us extra lines for the prepare and drop phases.
--source include/no_ps_protocol.inc

# Hypergraph chooses different query plans, so the collected
# statistics are different.
--source include/not_hypergraph.inc

--source include/big_test.inc
--source include/count_sessions.inc

SET @my_slow_logname = @@global.slow_query_log_file;
SET @my_lqt = @@global.long_query_time;

--replace_result $MYSQL_TMP_DIR ...
eval SET GLOBAL slow_query_log_file= '$MYSQL_TMP_DIR/my_extra_big.log';
let BIG_LOG= `SELECT @@global.slow_query_log_file`;

#
# Confirm that per-query stats work.
#
SET SESSION long_query_time = 20;
--disable_warnings
--disable_query_log
DROP TABLE IF EXISTS big_table_slow;
CREATE TABLE big_table_slow (id INT PRIMARY KEY AUTO_INCREMENT, v VARCHAR(100), t TEXT) ENGINE=InnoDB KEY_BLOCK_SIZE=8;

let $x = 200;
while ($x)
{
  eval INSERT INTO big_table_slow VALUES(2 * $x, LPAD("v", $x MOD 100, "b"), LPAD("a", ($x * 100) MOD 9000, "b"));
  dec $x;
}

--enable_query_log
--enable_warnings

SET GLOBAL long_query_time = 0;

connect (con,localhost,root,,);
SELECT COUNT(*) FROM big_table_slow;

connect (con1,localhost,root,,);
SELECT COUNT(*) FROM big_table_slow;

SELECT COUNT(*) FROM big_table_slow WHERE id>100 AND id<200;

SELECT * FROM big_table_slow WHERE id=2;

SELECT COUNT(*) FROM big_table_slow WHERE id>100;

SELECT COUNT(*) FROM big_table_slow WHERE id<100;

--echo # Cleanup

connection default;

# wait for queries to complete
let $wait_condition=
  SELECT COUNT(*)=2 FROM performance_schema.threads WHERE name="thread/sql/one_connection" AND processlist_command="Sleep";
--source include/wait_condition.inc

# make sure we add no more log lines
SET GLOBAL long_query_time=@my_lqt;

# change back to original log file
SET GLOBAL slow_query_log_file = @my_slow_logname;

disconnect con1;
disconnect con;

--source include/wait_until_count_sessions.inc
DROP TABLE big_table_slow;

--echo #
--echo # This is a hack to check the log result.
--echo # We strip off time related fields (non-deterministic) and verify the rest are correct.
--echo #

--perl
open FILE, "$ENV{'BIG_LOG'}" or die;
my @lines = <FILE>;
foreach $line (@lines) {
  if ($line =~ m/^# Query_time/) {
    $line =~ m/(Rows_sent.*) Thread_id.* (Errno.*) Start.*/;
    print "$1 $2\n";
  }
}
EOF

--remove_file $BIG_LOG

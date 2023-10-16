--echo #
--echo # Bug#11747227: --CONNECT_TIMEOUT AND --WAIT KEYS ARE NOT TAKE EFFECT
--echo #

# test --wait flag
let $MYSQLD_DATADIR= `SELECT @@datadir`;
let $MYSQL_LOG= $MYSQLD_DATADIR/mysql_wait_output.log;

# Wait should attempt a single retry on connectivity issues to a resolvable, but unavailable host
# (simulated with unused IPv4 numeric address and port combination)
# Use -v to output "Waiting" line (to assert retry actually happened)
--echo Test --wait with unavailable server host
--error 1
--exec $MYSQL -v --wait --host=0.0.0.0 --port=1 -e "SELECT 1;" 2>$MYSQL_LOG

--let $assert_text= Found Waiting line in mysql client log
--let $assert_select= Waiting
--let $assert_file= $MYSQL_LOG
--let $assert_count= 1
--source include/assert_grep.inc

# Wait should immediately fail on connectivity issues to unresolvable host name (no retries)
--echo Test --wait with unresolvable server host name
--error 1
--exec $MYSQL -v --wait --host=invalid -e "SELECT 1;" 2>$MYSQL_LOG

--let $assert_text= No Waiting line in mysql client log
--let $assert_select= Waiting
--let $assert_file= $MYSQL_LOG
--let $assert_count= 0
--source include/assert_grep.inc

remove_file $MYSQL_LOG;

--echo
--echo End of tests

--echo #WL#12393: Logging: Add new command line option, --log-slow-extra,
--echo #          for richer slow logging
--echo

# We'll be looking at the contents of the slow log later, and PS protocol
# would give us extra lines for the prepare and drop phases.
--source include/no_ps_protocol.inc

#
# Confirm that per-query stats work.
#

SET @save_sqlf=@@global.slow_query_log_file;

--replace_result $MYSQL_TMP_DIR ...
eval SET GLOBAL slow_query_log_file= '$MYSQL_TMP_DIR/my_extra_slow2.log';

let SLOW_LOG2= `SELECT @@global.slow_query_log_file`;

SET GLOBAL long_query_time=0;

connect (con,localhost,root,,);

--disable_warnings
DROP TABLE IF EXISTS islow;
DROP TABLE IF EXISTS mslow;
--enable_warnings

CREATE TABLE islow(i INT) ENGINE=innodb;
INSERT INTO islow VALUES (1), (2), (3), (4), (5), (6), (7), (8);

CREATE TABLE mslow(i INT) ENGINE=myisam;
INSERT INTO mslow VALUES (1), (2), (3), (4), (5), (6), (7), (8);

SELECT * FROM islow;
SELECT * FROM islow;

SELECT * FROM mslow;
SELECT * FROM mslow;

# make sure we don't log disconnect even when running in valgrind
SET GLOBAL slow_query_log=0;

connection default;
disconnect con;

SET GLOBAL long_query_time=1;

DROP TABLE islow;
DROP TABLE mslow;

--echo #
--echo # This is a hack to check the log result.
--echo # We strip off time related fields (non-deterministic) and
--echo # verify the rest are correct.
--echo #

--perl
open FILE, "$ENV{'SLOW_LOG2'}" or die;
my @lines = <FILE>;
foreach $line (@lines) {
  if ($line =~ m/^# Query_time/) {
    $line =~ s/Thread_id: \d* Errno:/Thread_id: 0 Errno:/;
    $line =~ m/(Rows_sent.*) Start.*/;
    print "$1\n";
  }
}
EOF

SET @@global.slow_query_log_file=@save_sqlf;
SET GLOBAL slow_query_log=1;

--remove_file $SLOW_LOG2

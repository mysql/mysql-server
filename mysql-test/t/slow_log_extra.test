--echo #WL#12393: Logging: Add new command line option, --log-slow-extra,
--echo #          for richer slow logging
--echo

# We'll be looking at the contents of the slow log later, and PS protocol
# would give us extra lines for the prepare and drop phases.
--source include/no_ps_protocol.inc
--source include/not_valgrind.inc

SET @save_sqlf=@@global.slow_query_log_file;

--replace_result $MYSQL_TMP_DIR ...
eval SET GLOBAL slow_query_log_file= '$MYSQL_TMP_DIR/my_extra_slow1.log';

SET timestamp=10;
SELECT unix_timestamp(), sleep(2);

let SLOW_LOG1= `SELECT @@global.slow_query_log_file`;

--perl
   use strict;

   my $file= $ENV{'SLOW_LOG1'} or die("slow log not set");
   my $result=0;

   open(FILE, "$file") or die("Unable to open $file: $!");
   while (<FILE>) {
     my $line = $_;
     $result++ if ($line =~ /SET timestamp=10;/);
     $result++ if ($line =~ /Start: 1970-01-01T00:00:10.000000Z /);
   }
   close(FILE);

   if($result != 2) {
     print "[ FAIL ] timestamp not found\n";
   }
   else {
     print "[ PASS ] timestamp found\n";
   }

EOF

SET @@global.slow_query_log_file=@save_sqlf;

--remove_file $SLOW_LOG1

--echo
--echo
--echo

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
--enable_warnings

CREATE TABLE islow(i INT) ENGINE=innodb;
INSERT INTO islow VALUES (1), (2), (3), (4), (5), (6), (7), (8);

SELECT * FROM islow;
SELECT * FROM islow;

# make sure we don't log disconnect even when running in valgrind
SET GLOBAL slow_query_log=0;

connection default;
disconnect con;

SET GLOBAL long_query_time=1;

DROP TABLE islow;

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

--echo #
--echo # Bug#30769965: WITH LOG_SLOW_EXTRA=ON ERRNO: INFO NOT GETTING UPDATED
--echo # CORRECTLY FOR ERROR
--echo #

SET @save_sqlf=@@global.slow_query_log_file;

--replace_result $MYSQL_TMP_DIR ...
eval SET GLOBAL slow_query_log_file= '$MYSQL_TMP_DIR/my_extra_slow3.log';

let SLOW_LOG3= `SELECT @@global.slow_query_log_file`;

# Can also use SET SESSION, but NOT SET GLOBAL, as the check is not done
# against the global variable.
SET long_query_time=0;

--error ER_WRONG_AUTO_KEY
CREATE TABLE b(id INT NOT NULL AUTO_INCREMENT);

--echo # Look for Errnos in slow log. There should be two,
--echo # both SET and CREATE will be logged as slow.
--perl
open FILE, "$ENV{'SLOW_LOG3'}" or die "Could not open tmp slow log file: $!";
while (<FILE>) {
  if (/Errno: (\d+)/)  { print "# Found Errno: $1 in slow log\n"; }
}
close (FILE);
EOF

SET @@global.slow_query_log_file=@save_sqlf;
SET GLOBAL slow_query_log=1;

--remove_file $SLOW_LOG3

--echo #
--echo # WL-13689 Use signal SIGUSR1 to flush logs
--echo #
--echo # Use signal SIGUSR1 to be a light version of SIGHUP.
--echo # The signal is needed to have a reliable
--echo # way to flush logs in mysql. This will include error log,
--echo # general log and slow query log.
--echo #
--echo # This test will cover, that the signal is sent corectly, meaning that:

--echo # When SIGUSR1 is sent:
--echo # Logs are empty afterwards.
--echo # Logs can be written to afterwards.

--echo # Test involves sending SIGUSR1 signal from perl.
--source include/not_windows.inc

--echo #################################################################
--echo ###############     ERROR LOG    ################################

--let MYSQLD_LOG= $MYSQL_TMP_DIR/sigusr1_test.log
--let MYSQLD_LOG_SLOW= $MYSQL_TMP_DIR/sigusr1_test_slow.log
--let MYSQLD_LOG_GENERAL= $MYSQL_TMP_DIR/sigusr1_test_general.log

--let $restart_parameters=restart:--log-error=$MYSQLD_LOG --general_log=0 --general_log_file=$MYSQLD_LOG_GENERAL --log_output=FILE --slow_query_log=0 --long_query_time=0 --slow_query_log_file=$MYSQLD_LOG_SLOW
--replace_result $MYSQLD_LOG MYSQLD_LOG $MYSQLD_LOG_SLOW MYSQLD_LOG_SLOW $MYSQLD_LOG_GENERAL MYSQLD_LOG_GENERAL
--source include/restart_mysqld.inc

let MYSQLD_PIDFILE= `SELECT @@pid_file;`;

--echo #
--echo # Send the log file into a new file that perl can fiddle with.
--echo #
--perl
  use strict;
  use warnings;
  my $filename = $ENV{"MYSQLD_PIDFILE"} or die("pidfile not set");
  my $pid;
  my $wait_cnt=60;
  open(FILE, "$filename") or die "cannot open file $filename";
  while(<FILE>) {
        $pid = $_;
  }
  close(FILE);
  rename $ENV{"MYSQLD_LOG"}, $ENV{"MYSQLD_LOG"}.".1";
  kill 'USR1', $pid or die "could not kill $pid: $!";
  while (! -e $ENV{"MYSQLD_LOG"}) {
        if($wait_cnt==0) { die "timedout waiting for general_log to be flushed"; }
        sleep 1;
        $wait_cnt--;
  }
EOF

--echo # Check that both files still exists
--file_exists $MYSQLD_LOG
--file_exists $MYSQLD_LOG.1

--echo # CLEAN UP
--remove_file $MYSQLD_LOG.1

--echo #################################################################
--echo ###############     SLOW LOG     ################################
SET @@global.slow_query_log=1;
let MYSQLD_PIDFILE= `SELECT @@pid_file;`;

SELECT 1;

--echo #
--echo # Send the log file into a new file that perl can fiddle with.
--echo #
--perl
  use strict;
  use warnings;
  my $filename = $ENV{"MYSQLD_PIDFILE"} or die("pidfile not set");
  my $pid;
  my $wait_cnt=60;
  open(FILE, "$filename") or die "cannot open file $filename";
  while(<FILE>) {
        $pid = $_;
  }
  close(FILE);
  rename $ENV{"MYSQLD_LOG_SLOW"}, $ENV{"MYSQLD_LOG_SLOW"}.".1";
  kill 'USR1', $pid or die "could not kill $pid: $!";
  while (! -e $ENV{"MYSQLD_LOG_SLOW"}) {
        if($wait_cnt==0) { die "timedout waiting for general_log to be flushed"; }
        sleep 1;
        $wait_cnt--;
  }
EOF

--echo # Check that both files still exists
--file_exists $MYSQLD_LOG_SLOW
--file_exists $MYSQLD_LOG_SLOW.1

--echo #
--echo # Set the log output to a table.
--echo # The server must not fail when SIGUSR1 is sent, even though slow log output
--echo # is set to a table (log_output).
--echo #
let MYSQLD_PIDFILE= `SELECT @@pid_file;`;
SET @@global.log_output='TABLE';

SELECT 1;
--perl
  use strict;
  use warnings;
  my $filename = $ENV{"MYSQLD_PIDFILE"} or die("pidfile not set");
  my $pid;
  open(FILE, "$filename") or die "cannot open file $filename";
  while(<FILE>) {
        $pid = $_;
  }
  close(FILE);
  kill 'USR1', $pid or die "could not kill $pid: $!";
EOF

--echo #
--echo # SET LOG OUTPUT TO 'NONE'
--echo #
let MYSQLD_PIDFILE= `SELECT @@pid_file;`;
SET @@global.log_output='NONE';

SELECT 1;
--perl
  use strict;
  use warnings;
  my $filename = $ENV{"MYSQLD_PIDFILE"} or die("pidfile not set");
  my $pid;
  open(FILE, "$filename") or die "cannot open file $filename";
  while(<FILE>) {
        $pid = $_;
  }
  close(FILE);
  kill 'USR1', $pid or die "could not kill $pid: $!";
EOF


--echo # CLEAN UP
SET @@global.slow_query_log=0;
--remove_file $MYSQLD_LOG_SLOW
--remove_file $MYSQLD_LOG_SLOW.1

--echo #################################################################
--echo ###############   GENERAL LOG    ################################
SET @@global.general_log=1;
SET @@global.log_output='FILE';
let MYSQLD_PIDFILE= `SELECT @@pid_file;`;

SELECT 1;

--echo #
--echo # Send the log file into a new file that perl can fiddle with.
--echo #
--perl
  use strict;
  use warnings;
  my $filename = $ENV{"MYSQLD_PIDFILE"} or die("pidfile not set");
  my $pid;
  my $wait_cnt=60;
  open(FILE, "$filename") or die "cannot open file $filename";
  while(<FILE>) {
        $pid = $_;
  }
  close(FILE);
  rename $ENV{"MYSQLD_LOG_GENERAL"}, $ENV{"MYSQLD_LOG_GENERAL"}.".1";
  kill 'USR1', $pid or die "could not kill $pid: $!";
  while (! -e $ENV{"MYSQLD_LOG_GENERAL"}) {
        if($wait_cnt==0) { die "timedout waiting for general_log to be flushed"; }
        sleep 1;
        $wait_cnt--;
  }
EOF

--echo # Check that both files still exists
--file_exists $MYSQLD_LOG_GENERAL
--file_exists $MYSQLD_LOG_GENERAL.1

--echo #
--echo # Set the log output to a table.
--echo # The server must not fail when SIGUSR1 is sent, even though slow log output 
--echo # is set to a table (log_output).
--echo # let MYSQLD_PIDFILE= `SELECT @@pid_file;`;
SET @@global.log_output='TABLE';

SELECT 1;
--perl
  use strict;
  use warnings;
  my $filename = $ENV{"MYSQLD_PIDFILE"} or die("pidfile not set");
  my $pid;
  open(FILE, "$filename") or die "cannot open file $filename";
  while(<FILE>) {
        $pid = $_;
  }
  close(FILE);
  kill 'USR1', $pid or die "could not kill $pid: $!";
EOF

--echo #
--echo # SET LOG OUTPUT TO 'NONE'
--echo #
let MYSQLD_PIDFILE= `SELECT @@pid_file;`;
SET @@global.log_output='NONE';

SELECT 1;
--perl
  use strict;
  use warnings;
  my $filename = $ENV{"MYSQLD_PIDFILE"} or die("pidfile not set");
  my $pid;
  open(FILE, "$filename") or die "cannot open file $filename";
  while(<FILE>) {
        $pid = $_;
  }
  close(FILE);
  kill 'USR1', $pid or die "could not kill $pid: $!";
EOF

--echo # CLEAN UP
SET @@global.general_log=0;
TRUNCATE TABLE mysql.general_log;
TRUNCATE TABLE mysql.slow_log;

--echo # Restore default settings
--let $restart_parameters = restart:
--source include/restart_mysqld.inc

--remove_file $MYSQLD_LOG
--remove_file $MYSQLD_LOG_GENERAL
--remove_file $MYSQLD_LOG_GENERAL.1


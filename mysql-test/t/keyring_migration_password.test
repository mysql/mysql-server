
# Checking if perl Expect module is installed on the system.
# If not, the test will be skipped.
--source include/have_expect.inc
--source include/not_windows.inc

--echo #
--echo # Bug#33619511: MySQL server exits in debug build when -p parameter used
--echo #

# No $ sign before the name to make it visible in Perl code below
--let MYSQLD_ARGS = --datadir=$MYSQLD_DATADIR --secure-file-priv="" -p
--let MYSQLD_LOG = $MYSQL_TMP_DIR/bug33619511_log.txt

# Start a custom mysqld instance and interactively fill up the dummy password.
# Server should exit with "[Server] Aborting"
--perl

use strict;
require Expect;

# 1. Start the server
# The server should enter password prompt, we'll type a password 'a'.
# Use "log_stdout(0)" to avoid leaking output to record file because it contains timestamps and custom paths.
my $texp = new Expect();
$texp->raw_pty(1);
$texp->log_stdout(0);
$texp->log_file("$ENV{MYSQLD_LOG}", "w");
$texp->spawn("$ENV{MYSQLD} $ENV{MYSQLD_ARGS}");
$texp->expect(15,' -re ',[ 'Enter password:' => sub {
    $texp->send("a\n");}]) or die "Error sending the password";

$texp->soft_close();

EOF

--echo # Expect log that proves the server clean exit
--let SEARCH_FILE = $MYSQLD_LOG
--let SEARCH_PATTERN=\[ERROR\] \[MY-[0-9]+] \[Server\] Aborting
--source include/search_pattern.inc

# Cleanup
--remove_file $MYSQLD_LOG

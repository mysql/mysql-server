# ==== Purpose ====
#
# Wait until the MTR `expect` file exists and contains "wait".
#
# This is used to either:
# a) assert that this is aleady so, by using $wait_timeout=0
# b) await the transition from restart_abort to wait performed by
#    the test runner when it notices restart_abort in the expect file.
#
# ==== Usage ====
#
# 1. ensure $_expect_file_name is initialized, by using one of the scripts
# a) --source include/shutdown_mysqld.inc
# b) --source include/kill_mysqld.inc
# c) --source include/expect_crash.inc
#    execute something which causes a crash
# 2. optionally set the timeout (default is 0)
# --let $wait_timeout = SECONDS
# 3. include the script:
# --source include/wait_for_wait_in_expect_file.inc
#
# Parameters:
#
#   $_expect_file_name
#     The MTR expect file expected to contain "wait".
#     You shouldn't have to set it up manually. See point 1. above.
#
#   $wait_timeout
#     If instantiated, the wait period in seconds to wait for the expect
#     file to be removed. If not instantiated, it will give up after one try.
#
if ($_expect_file_name == '') {
  --die ERROR: $expect_file_name parameter needed for `wait_for_wait_in_expect_file.inc`
}
--let _WFWIEF_EXPECT_FILE_NAME = $_expect_file_name
--let _WFWIEF_WAIT_TIMEOUT = $wait_timeout
--perl
  use strict;
  my $expect_file= $ENV{'_WFWIEF_EXPECT_FILE_NAME'} or die "_WFWIEF_EXPECT_FILE_NAME is not set";
  my $retries_left = $ENV{'_WFWIEF_WAIT_TIMEOUT'};
  while (1) {
    if (open(FILE, "< $expect_file")) {
      my $line = <FILE>;
      close(FILE);
      if ($line) {
        if ($line =~ /^wait\b/) {
          last;
        }
        die "$expect_file exists but contains $line" unless $retries_left;
      }
      die "$expect_file is empty" unless $retries_left;
    }
    die "$expect_file is missing" unless $retries_left;
    --$retries_left;
    sleep 1;
  }
EOF

# ==== Purpose ====
#
# Wait until the MTR `expect` file is removed.
#
# The general usage for this script is to synchronize the sequential write
# of different states to the expect file, by waiting for MTR to process the
# current expect file, after which the file is deleted. One example of a
# possible usage is to expect a crash during server start:
#
#      ....
#      --exec echo "restart" > $expect_file_name
#      --source include/wait_for_removed_expect_file.inc
#      --exec echo "wait" > $expect_file_name
#      ....
#
# ==== Usage ====
#
# --let $expect_file_name = FILENAME
# [--let $wait_timeout = SECONDS]
# --source include/wait_for_removed_expect_file.inc
#
# Parameters:
#
#   $expect_file_name
#     The MTR expect file to be removed.
#
#   $wait_timeout
#     If instantiated, the wait period in seconds to wait for the expect
#     file to be removed. If not instantiated, it will wait indefinitely.
#
if ($expect_file_name == '') {
  --die ERROR: $expect_file_name parameter needed for `wait_for_removed_expect_file.inc`
}

--echo include/wait_for_removed_expect_file.inc

--let _WFREF_EXPECT_FILE_NAME = $expect_file_name
--let _WFREF_WAIT_TIMEOUT = $wait_timeout

--perl
  use strict;
  my $expect_file= $ENV{'_WFREF_EXPECT_FILE_NAME'} or die "expect_file_name is not set";
  my $wait_timeout = $ENV{'_WFREF_WAIT_TIMEOUT'};
  my $timeout = $wait_timeout * 10;

  while (-e $expect_file) {
    if ($wait_timeout != 0) {
      if ($timeout == 0) {
        die "Wait timeout reached in `wait_for_removed_expect_file.inc`";
      }
      --$timeout;
    }
    sleep 0.1;
  }
EOF

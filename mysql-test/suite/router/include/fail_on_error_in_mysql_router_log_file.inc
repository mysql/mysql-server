# ==== Purpose ====
#
# Scan router logs and check for error entries.
#
# NOTE: The search output is reset whenever the MTR beginning-of-test
# tag is found (CURRENT_TEST: ...).
#
# ==== Usage ====
#
# --let $grep_pattern = PERL_REGEX
# --let $grep_file = FILENAME
# [--let $wait_timeout = SECONDS]
# --source ../include/fail_on_error_in_mysql_router_log_file.inc
#
# Parameters:
#
#   $grep_pattern
#     The pattern to search for. This can be a perl regex.
#
#   $grep_file
#     The file to search in.
#
#   $wait_timeout
#     If instantiated, the wait period in seconds to wait for the pattern
#     to appear in the file. If not instantiated, it will wait
#     indefinitely.
#

--perl
  use strict;
  my $allowed_error="MRS metadata refresh failed";
  my $file = $ENV{'MYSQLROUTER_LOGFILE'} or die "grep_file is not set";
  my $pattern = "[0-9][0-9][0-9][0-9]-[0-9][0-9]-[0-9][0-9] [0-9][0-9]:[0-9][0-9]:[0-9][0-9] [a-zA-Z0-9_]+ ERROR ";
  my $found = 0;
  my $line = '';
  my $last_found = '';
  open(my $fd, "<", $file) or die("Unable to open $file: $!\n");
  
  while (<$fd>) {
    $line = $line . $_;
    if (substr($line, length($line) - 1, 1) eq "\n" || substr($line, length($line) - 2, 2) eq "\r\f") {
      if (($line =~ /^CURRENT_TEST: /)) {
        $found = 0;
      }
      if ($found == 0 && $line =~ /$pattern/) {
        if ($line !~ /$allowed_error/) {
          print($line);
          $found = 1;
          $last_found=$line;
        }
      }
      $line = '';
    }
  }

  if ($found > 0) {
    die "Error log entry was found in router log file:\n  $last_found";
  }

  close($fd) or die "Error closing $file: $!";
EOF

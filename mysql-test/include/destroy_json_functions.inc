# ==== Purpose ====
#
# Remove files created by create_json_*.inc
#
# ==== Usage ====
#
# --source include/destroy_json_functions.inc
#
# This removes all files created by earlier invocations of
# create_json_iterator.inc, create_json_unpacker.inc, and
# create_json_unpacking_iterator.inc
#
# Note: only source this file once during a test, after you are done
# with all json iterators and unpackers.


--let DJF_FILES = $json_function_files
perl END_OF_PERL;
  my $files = $ENV{'DJF_FILES'};
  my $vardir = $ENV{'MYSQLTEST_VARDIR'};
  $files =~ s/^ +//;
  for my $file (split(/ +/, $files)) {
    $file =~ m{^$vardir/tmp/}
      or die "Error: cannot remove file <$file> outside MYSQLTEST_VARDIR/tmp";
    unlink($file)
      or die "Error $? removing file <$file>: $!";
  }
END_OF_PERL
--let $json_function_files =


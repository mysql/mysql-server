# ==== Purpose ====
#
# Grep for a pattern in a file in a portable manner.
#
# WARNING: Use include/assert_grep.inc instead, if at all possible.
# That allows you to assert the property being tested in a much more
# precise way.  It also does not depend on the result file, so your
# test becomes more readable/maintainable. It also produces better
# debug output if the test fails.
#
# ==== Usage ====
#
# --let $grep_pattern= PERL_REGEX
# --let $grep_file= FILENAME
# [--let $grep_output= print_each | print_count | boolean | none]
#
# Parameters:
#
#   $grep_pattern
#     The pattern to search for. This can be a perl regex.
#
#   $grep_file
#     The file to search in.
#
#   $grep_output
#     The format of the output. Can be one of:
#     - print_each: prints each line, and a count.
#     - print_count: print just a count.
#     - boolean: print only whether something was found or not.
#     If this is not specified, print_each is used.
#     - none: doesn't print anything but result is $GREP_RESULT
#             0 - no match, and non-zero as number of matches

#--let $include_filename= grep_pattern.inc
#--source include/begin_include_file.inc


if ($grep_output)
{
  --let _GP_GREP_OUTPUT= $grep_output
}
if (!$grep_output)
{
  --let _GP_GREP_OUTPUT= print_each
}
--let _GP_GREP_PATTERN= $grep_pattern
--let _GP_GREP_FILE= $grep_file

perl;
  use strict;
  my $file= $ENV{'_GP_GREP_FILE'} or die "grep_file is not set";
  my $pattern= $ENV{'_GP_GREP_PATTERN'} or die "grep_pattern is not set";
  open(FILE, "$file") or die("Unable to open $file: $!\n");
  my $count = 0;
  my $result = "";
  my $output = $ENV{'_GP_GREP_OUTPUT'};
  if ($output eq 'print_each') {
    print "Matching lines are:\n";
  }

  open(OUT_FILE, ">$ENV{MYSQL_TMP_DIR}/grep_result.txt") or 
      die("Unable to open out file for writing: $!\n");

  my $finished = 0;
  while (<FILE>) {
    my $line = $_;
    if (($line =~ /^CURRENT_TEST: /)) {
      $result = "";
      $finished = 0;
      $count = 0;
    }
    if ($finished == 0 && $line =~ /$pattern/) {
      if ($output eq 'print_each') {
        $result = $result . $line;
      }
      $count++;
      if ($output eq 'boolean') {
        $finished = 1;
      }
    }
  }
  print $result;
  if ($count == 0 && $output eq 'print_each') {
    print "None\n";
  }
  if ($output eq 'boolean') {
    print $count ? "Pattern found.\n" : "Pattern not found.\n";
  } elsif ($output eq 'none') {
    print OUT_FILE "--let \$GREP_RESULT=$count\n";
  } else {
    print "Occurrences of '$pattern' in the input file: $count\n";
  }
  close(OUT_FILE) or die "Error closing $file: $!";
  close(FILE) or die "Error closing $file: $!";
EOF

--source $MYSQL_TMP_DIR/grep_result.txt
--remove_file $MYSQL_TMP_DIR/grep_result.txt


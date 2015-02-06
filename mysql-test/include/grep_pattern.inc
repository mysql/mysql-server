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
# [--let $grep_output= print_each | print_count | boolean]
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

--perl
  use strict;
  my $file= $ENV{'_GP_GREP_FILE'} or die "grep_file is not set";
  my $pattern= $ENV{'_GP_GREP_PATTERN'} or die "grep_pattern is not set";
  open(FILE, "$file") or die("Unable to open $file: $!\n");
  my $count = 0;
  my $output = $ENV{'_GP_GREP_OUTPUT'};
  if ($output eq 'print_each') {
    print "Matching lines are:\n";
  }
  while (<FILE>) {
    my $line = $_;
    if ($line =~ /$pattern/) {
      if ($output eq 'print_each') {
        print $line;
      }
      $count++;
      if ($output eq 'boolean') {
        last;
      }
    }
  }
  if ($count == 0 && $output eq 'print_each') {
    print "None\n";
  }
  if ($output eq 'boolean') {
    print $count ? "Pattern found.\n" : "Pattern not found.\n";
  }
  else {
    print "Occurrences of '$pattern' in the input file: $count\n";
  }
  close(FILE) or die "Error closing $file: $!";
EOF


#--source include/end_include_file.inc

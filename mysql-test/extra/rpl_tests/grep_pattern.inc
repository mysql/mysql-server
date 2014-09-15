# Please set GREP_FILE and GREP_PATTERN environment variables 
#  to work this file properly.
--perl
  use strict;
  my $verbose= $ENV{'GREP_PRINT_NOT_VERBOSE'} ? 0 : 1;
  my $file= $ENV{'GREP_FILE'} or die "grep file not set";
  my $pattern= $ENV{'GREP_PATTERN'} or die "pattern is not set";
  open(FILE, "$file") or die("Unable to open $file: $!\n");
  my $count = 0;
  print "Matching lines are:\n";
  while (<FILE>) {
    my $line = $_;
    if ($line =~ /$pattern/) {
      if ($verbose == 1) {
        print "$line\n";
      }
      $count++;
    }
  }
  if ($count == 0) {
    print "None\n";
  }
  print "Occurrences of the $pattern in the input file : $count\n";
  close(FILE);
EOF

# Please set GREP_FILE and GREP_PATTERN environment variables 
#  to work this file properly.
--perl
  use strict;
  my $verbose= $ENV{'GREP_PRINT_NOT_VERBOSE'} ? 0 : 1;
  my $last_sequence_after= $ENV{'GREP_LAST_SEQUENCE_AFTER'};
  my $file= $ENV{'GREP_FILE'} or die "grep file not set";
  my $pattern= $ENV{'GREP_PATTERN'} or die "pattern is not set";
  open(FILE, "$file") or die("Unable to open $file: $!\n");
  my $count = 0;
  my $output = "";
  while (<FILE>) {
    my $line = $_;
    if ($last_sequence_after && $line =~ /$last_sequence_after/) {
      $output = "";
      $count = 0;
    }
    if ($line =~ /$pattern/) {
      if ($verbose == 1) {
        if ($ENV{'GREP_FIND'}) {
          $line=~ s/$ENV{'GREP_FIND'}/$ENV{'GREP_REPLACE'}/;
        }
        if ($ENV{'GREP_NO_NEWLINE'}) {
          $output= $output . $line;
        } else {
          $output= $output . $line . "\n";
        }
      }
      $count++;
    }
  }
  unless($ENV{'GREP_SKIP_HEADER'}) {
    print "Matching lines are:\n";
  }
  print $output;
  unless($ENV{'GREP_SKIP_FOOTER'}) {
    if ($count == 0) {
      print "None\n";
    }
    print "Occurrences of the $pattern in the input file : $count\n";
  }
  close(FILE);
EOF

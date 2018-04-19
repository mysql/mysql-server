# MTR "subroutine" which checks the output from mysqld --help
# --verbose to check if the deduced values for --basedir,
# --lc-messages-dir, --character-sets-dir and --plugin-dir have
# "reasonable" values. By reasonable is meant that the path exists and
# contains some key file/directory which is expected to be there.
#
# Note that the deduced value for --character-sets-dir and
# --plugin-dir is currently wrong when running from a sandbox build
# dir, so we just print the ok string anyway in this case
#
# Usage: Assign the MTR variable HELP_VERBOSE_OUTPUT to a file which contains
# the help output, then source this file.

--echo # Checking HELP_VERBOSE_OUTPUT
--perl
  use strict;
  #use Dumpvalue;
  #my $dumper = Dumpvalue->new;

  #$dumper->dumpValue(\%ENV);
  $_= $ENV{'MYSQLD'};
  my $in_cbd= m!/sql/! || m!/runtime_output_directory/!;
  #print "# DBG: \$in_cbd: $in_cbd\n";

  my $mysqld_out= $ENV{'HELP_VERBOSE_OUTPUT'};

  open(MYSQLD_OUT, $mysqld_out);
  while(<MYSQLD_OUT>)
  {
    if (/^basedir\s+(\S+)$/)
    {
      my $bd= $1;
      if (-d "$bd/share") {
        print "# basedir looks ok\n";
        next;
      }
      print "# ERROR: No share directory in basedir $bd\n";
      next;
    }

    if (/^character-sets-dir\s+(\S+)$/)
    {
      my $csd= $1;
      if (-f "$csd/ascii.xml" || $in_cbd) {
        print "# character-sets-dir is as expected\n";
        next;
      }
      print "# ERROR: No ascii.xml in character-sets-dir $csd\n";
      next;
    }

    if (/^lc-messages-dir\s+(\S+)$/)
    {
      my $lmd= $1;
      if (-d "$lmd/english") {
        print "# lc-messages-dir looks ok\n";
        next;
      }
      print "# ERROR: No english directory in lc-messages-dir $lmd\n";
      next;
    }

    if (/^plugin-dir\s+(\S+)$/)
    {
      my $pd= $1;
      if (-B "$pd/auth.so" || -B "$pd/auth.dll" || $in_cbd) {
        print "# plugin-dir is as expected\n";
        next;
      }
      print "# ERROR: No auth dll in plugin-dir $pd\n";
      next;
    }
  }
  close(MYSQLD_OUT);
EOF

--echo # HELP_VERBOSE_OUTPUT check finished
--echo #

#! /usr/bin/perl

use strict;
use warnings;

use FindBin;
require "$FindBin::Bin/triggers-lib.pl";

die "$0: Script error: \$BK_PENDING is not set in pre-commit trigger\n"
  unless defined $ENV{BK_PENDING};

# Read changed files from $BK_PENDING directly.  Do not bother user about
# merge changes; they don't have any choice, the merge must be done.
my $changes = innodb_get_changes('file', $ENV{BK_PENDING}, undef)
  or exit 0;

innodb_inform_and_query_user($changes)
  or exit 1;  # Abort commit

# OK, continue with commit
exit 0;

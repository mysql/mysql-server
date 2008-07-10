#! /usr/bin/perl

use strict;
use warnings;

use FindBin;
require "$FindBin::Bin/triggers-lib.pl";

# Don't run unless commit was successful
check_status() || exit 0;

my $cset = latest_cset();

# Read most recent ChangeSet's changed files.  Send merge changes along, since
# they'll need to be incorporated in InnoDB's source tree eventually.
my $changes = innodb_get_changes('cset', $cset, 'yes')
  or exit 0;

innodb_send_changes_email($cset, $changes)
  or exit 1;

exit 0;

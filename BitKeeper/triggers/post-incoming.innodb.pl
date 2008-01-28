#! /usr/bin/perl

use strict;
use warnings;

use FindBin;
require "$FindBin::Bin/triggers-lib.pl";

# Don't run unless push/pull was successful
check_status() or exit 0;

# Don't run if push/pull is in local clones
exit 0 if repository_type() eq 'local';

# For each pushed ChangeSet, check it for InnoDB files and send
# diff of entire ChangeSet to InnoDB developers if such changes
# exist.

my $error = 0;

foreach my $cset (read_bk_csetlist())
{
  my $changes = innodb_get_changes('cset', $cset, 'yes')
    or next;

  innodb_send_changes_email($cset, $changes)
    or $error = 1;
}

exit ($error == 0 ? 0 : 1);

#! /usr/bin/perl

use strict;
use warnings;

my $event= $ENV{BK_EVENT};
unless($event eq 'outgoing pull' || $event eq 'outgoing push' ||
       $event eq 'resolve') {
  exit 0;
}

print "Checking for bad changesets from old crashed 5.1 tree...\n";

my @bad_csets=
    ( 'monty@mysql.com|ChangeSet|20060418090255|16983',
      'monty@mysql.com|ChangeSet|20060418090458|02628',
      'monty@mysql.com|ChangeSet|20060419084236|49576',
      'monty@mysql.com|ChangeSet|20060503164655|51444',
      'monty@mysql.com|ChangeSet|20060503225814|60133',
      'monty@mysql.com|ChangeSet|20060504033006|54878',
      'monty@mysql.com|ChangeSet|20060504130520|48660',
      'monty@mysql.com|ChangeSet|20060504164102|03511',
      'monty@mysql.com|ChangeSet|20060504193112|04109',
      'monty@mysql.com|ChangeSet|20060505015314|02799',
      'monty@mysql.com|ChangeSet|20060505084007|16704',
      'monty@mysql.com|ChangeSet|20060505104008|16695',
      'monty@mysql.com|ChangeSet|20060505171041|13924',
      'monty@mysql.com|ChangeSet|20060508121933|13866',
      'monty@mysql.com|ChangeSet|20060508160902|15029',
      'monty@mysql.com|ChangeSet|20060509145448|38636',
      'monty@mysql.com|ChangeSet|20060509224111|40037',
      'monty@mysql.com|ChangeSet|20060510090758|40678',
      'monty@mysql.com|ChangeSet|20060515164104|46760',
      'monty@mysql.com|ChangeSet|20060530114549|35852',
      'monty@mysql.com|ChangeSet|20060605032828|23579',
      'monty@mysql.com|ChangeSet|20060605033011|10641',
      'monty@mysql.com|ChangeSet|20060605060652|09843',
      'msvensson@neptunus.(none)|ChangeSet|20060605094744|10838',
      'msvensson@neptunus.(none)|ChangeSet|20060605105746|11800',
      'msvensson@neptunus.(none)|ChangeSet|20060605122345|12772',
      'jmiller@mysql.com|ChangeSet|20060531210831|36442',
      'jmiller@mysql.com|ChangeSet|20060602151941|36118',
      'jmiller@mysql.com|ChangeSet|20060602152136|27762',
      'jmiller@mysql.com|ChangeSet|20060605121748|12864',
      'jmiller@mysql.com|ChangeSet|20060605160304|14798',
      'jimw@mysql.com|ChangeSet|20060605210201|14667',
      'igor@rurik.mysql.com|ChangeSet|20060605220727|15265',
      'igor@rurik.mysql.com|ChangeSet|20060605221206|15134',
      'stewart@mysql.com|ChangeSet|20060525073521|11169',
      'stewart@mysql.com|ChangeSet|20060605154220|12975',
      'stewart@mysql.com|ChangeSet|20060606040001|15337',
      );

# Read the list of changesets.
my $csetlist = $ENV{BK_CSETLIST};
if(!defined($csetlist) || !open(FH, '<', $csetlist)) {
  die "Failed to open list of incoming changesets '$csetlist': $!.\n";
}
my @csets = <FH>;
close FH;

# Reject any attempt to push/pull a bad changeset.
for my $cs (@csets) {
  # Do this the raw way, don't want to be bitten by different EOL conventions
  # on server and client (Unix/Windows/Mac).
  $cs =~ s/\x0d?\x0a?$//s;
  if(grep($_ eq $cs, @bad_csets)) {
    print <<END;
BAD CHANGESET DETECTED! $event REJECTED!

The changeset with key '$cs' was detected in the attempted push or pull.
This changeset is from the corrupt part of the crashed mysql-5.1-new tree.
Pushing or pulling this changeset would result in corruption of the new tree,
and therefore the operation has been rejected.

Contact Kristian Nielsen (knielsen\@mysql.com, IRC knielsen) if you have any
questions regarding this.
END
    exit 1;
  }
}

print "No bad changesets found, proceeding.\n";

exit 0;

#
# ndb_check_unique_index.inc - check that all unique index use same fragment count as main table
#
# Usage:
# let ndb_database= <table database>;
# let ndb_table= <table name>;
# --source suite/ndb/include/ndb_check_unique_index.inc
#

--perl
use strict;

my $db = $ENV{ndb_database} or die "Missing ndb_database in environment\n";
my $tbl = $ENV{ndb_table} or die "Missing ndb_table in environment\n";
my $cmd = "$ENV{NDB_DESC} -d$db $tbl";

my $fragcnt;
my %idxfragcnts;
open MAIN, "$cmd|";
print "Table: $db.$tbl\n";
while (<MAIN>)
{
  if (/FragmentCount: (\S+).*/ || /PartitionCount: (\S+).*/)
  {
    # print previous tables indices fragment counts
    foreach my $i (sort keys %idxfragcnts)
    {
      print $i;
      print $idxfragcnts{$i};
    }
    # clear current tables indices fragment counts
    %idxfragcnts = undef;
    # print current tables fragment count
    print;
    $fragcnt = $1;
  }
  if (/([^(]+unique)\S+ - UniqueHashIndex.*/)
  {
    my $row = $_;
    $idxfragcnts{$row} = '';
    my $index = $1;
    open IDX, "$ENV{NDB_DESC} -d$db -t$tbl $index|"
      or die "FAILED: $ENV{NDB_DESC} -d$db -t$tbl $index|";
    while (<IDX>)
    {
      if (/FragmentCount: (\S+).*/)
      {
        $idxfragcnts{$row} = $_;
      }
    }
    close IDX;
  }
}
# print last tables indices fragment counts
foreach my $i (sort keys %idxfragcnts)
{
  print $i;
  print $idxfragcnts{$i};
}
close MAIN;
EOF

let ndb_database=;
let ndb_table=;

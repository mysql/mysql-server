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
open MAIN, "$cmd|";
print "Table: $db.$tbl\n";
while (<MAIN>)
{
  if (/FragmentCount: (\S+).*/)
  {
    print;
    $fragcnt = $1;
  }
  if (/([^(]+unique)\S+ - UniqueHashIndex.*/)
  {
    print;
    my $index = $1;
    open IDX, "$ENV{NDB_DESC} -d$db -t$tbl $index|"
      or die "FAILED: $ENV{NDB_DESC} -d$db -t$tbl $index|";
    while (<IDX>)
    {
      if (/FragmentCount: (\S+).*/)
      {
        print;
      }
    }
    close IDX;
  }
}
close MAIN;
EOF

let ndb_database=;
let ndb_table=;

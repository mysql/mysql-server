#
# ndb_check_blob_tables.inc - check that all blob tables use same hashmap as main table
#
# Usage:
# let ndb_die_on_error= 1; # 1 if it should fail on hashmap mismatch
# let ndb_database= <table database>;
# let ndb_table= <table name>;
# --source suite/ndb/include/ndb_check_blob_tables.inc
#

--perl
use strict;

my $die_on_error = $ENV{ndb_die_on_error};
my $db = $ENV{ndb_database} or die "Missing ndb_database in environment";
my $tbl = $ENV{ndb_table} or die "Mising ndb_table in environment";
my $cmd = "$ENV{NDB_DESC} -d$db $tbl";

my $hashmap;
open MAIN, "$cmd|";
print "Table: $db.$tbl\n";
while (<MAIN>)
{
  if (/HashMap: (\S+).*/)
  {
    print;
    $hashmap = $1;
  }
  if (/(\w+) .* BT=(NDB.BLOB_\d+_\d+).*/)
  {
    print;
    my $column = $1;
    my $blob_table = $2;
    my $bt_hashmap;
    $blob_table =~ s/_/\"\"_/;
    open BLOB, "$ENV{NDB_DESC} -d$db $blob_table|"
      or die "FAILED: $ENV{NDB_DESC} -d$db $blob_table|";
    while (<BLOB>)
    {
      if (/HashMap: (\S+).*/)
      {
        print;
        $bt_hashmap = $1;
      }
    }
    close BLOB;
    if ($die_on_error and ($bt_hashmap ne $hashmap))
    {
      die "Blob table $db.$blob_table for $db.$tbl.$column have wrong hashmap. ($bt_hashmap is not $hashmap)"
    }
  }
}
close MAIN;
EOF

let ndb_die_on_error=;
let ndb_database=;
let ndb_table=;

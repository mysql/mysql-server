#
# ndb_get_blob_tables.inc - get blob tables names for all blob columns
#
# Usage:
# let ndb_database= <table database>;
# let ndb_table= <table name>;
# --source suite/ndb/include/ndb_get_blob_tables.inc
#
# For each blob column where will be a variable
#   $bt_<table database>_<table_name>_<column_name>
# with the blob table name (with escaped $)
#

let ndb_get_blob_tables_result_file= $MYSQLTEST_VARDIR/tmp/ndb_get_blob_tables_result.inc;
--perl
my $db = $ENV{ndb_database} or die "Missing ndb_database in environment";
my $tbl = $ENV{ndb_table} or die "Missing ndb_table in environment";
my $cmd = "$ENV{NDB_DESC} -d$db $tbl";

open RUN, "$cmd|";
open RES, ">$ENV{ndb_get_blob_tables_result_file}";
while (<RUN>)
{
  if (/(\w+) .* BT=(NDB.BLOB_\d+_\d+).*/)
  {
    my $column = $1;
    my $blob_table = $2;
    $blob_table =~ s/\$/\\\$/g;
    print RES "let \$bt_${db}_${tbl}_${column} = ".$blob_table.";\n";
  }
}
close RES;
close RUN;
EOF

let ndb_database=;
let ndb_table=;

--source $ndb_get_blob_tables_result_file
--remove_file $ndb_get_blob_tables_result_file

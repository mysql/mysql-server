--perl
use strict;

use File::Basename;
use IO::File;
use lib "lib/";
use My::Find;

#
# Include file that sets the following environment variables:
# MTR_NDB_NO_OF_NODES - Number of ndb[mt]d data nodes
# MTR_NDB_NO_OF_REPLICAS - Number of replicas
# MTR_NDB_MAX_NO_OF_TABLES - Max number of tables
# MTR_NDB_MAX_NO_OF_ORDERED_INDEXES - Max number of ordered indexes
# MTR_NDB_DATA_MEMORY - Data memory per node
# MTR_NDB_INDEX_MEMORY - Index memory per node
# MTR_NDB_USABLE_DATA_MEMORY - Data memory available for user (#nodes * data_memory / #replicas)
# MTR_NDB_USABLE_INDEX_MEMORY - Index memory available for user (#nodes * index_memory / #replicas)
#

#
# Set up paths
#
my $vardir = $ENV{MYSQLTEST_VARDIR} or die "Need MYSQLTEST_VARDIR";
my $mysql_test_dir = $ENV{MYSQL_TEST_DIR} or die "Need MYSQL_TEST_DIR";
my $basedir = dirname($mysql_test_dir);
my $ndb_connectstring = $ENV{NDB_CONNECTSTRING} or die "Need NDB_CONNECTSTRING";

#
# Check if the needed jars are available
#
my $ndb_config = my_find_file($basedir,
                              ["storage/ndb/tools", "bin"],
                              ["ndb_config", "ndb_config.exe"], NOT_REQUIRED);

my $fields = "NoOfReplicas,MaxNoOfTables,MaxNoOfOrderedIndexes,DataMemory,IndexMemory";
my $cmd = "$ndb_config --ndb-connectstring='$ndb_connectstring' -q '$fields' -f ',' -r '\\n'";
print "Calling ndb_config: '$cmd'\n";
my $output = `$cmd`;

my $no_of_nodes = 0;
my $no_of_replicas = 0;
my $max_no_of_tables = 0;
my $max_no_of_ordered_indexes = 0;
my $data_memory = 0;
my $index_memory = 0;

foreach my $line (split("\n", $output)) {
    # Skip empty lines
    next if ($line =~ m/^,+$/g);

    # One line per node
    $no_of_nodes++;

    # Same on each line (node)
    ($no_of_replicas, $max_no_of_tables, $max_no_of_ordered_indexes, $data_memory, $index_memory) = split(',', $line);
}

my $usable_data_memory = 0;
my $usable_index_memory = 0;

if ($no_of_replicas > 0) {
    $usable_data_memory = $no_of_nodes * $data_memory / $no_of_replicas;
    $usable_index_memory = $no_of_nodes * $index_memory / $no_of_replicas;
} else {
    die "Failed to parse ndb_config output - could not get number of replicas";
}

sub get_first_last {
  my $type = shift;

  my $cmd = "$ndb_config --ndb-connectstring='$ndb_connectstring' -q 'NodeId' -f '' -r ',' --type=$type";
  my $output = `$cmd`;
  chomp($output);
  my @nums = split(',', $output);
  my @sorted = sort(@nums);
  return ($sorted[0], $sorted[scalar(@sorted) - 1]);
}

my ($first_ndbd_nodeid, $last_ndbd_nodeid) = get_first_last('ndbd');
my ($first_mgmd_nodeid, $last_mgmd_nodeid) = get_first_last('ndb_mgmd');

my $file_name = "$vardir/tmp/ndb_info_result.inc";
my $F = IO::File->new($file_name, 'w') or die "Could not open '$file_name' for writing";
print $F "--let \$MTR_NDB_NO_OF_NODES= $no_of_nodes\n";
print $F "--let \$MTR_NDB_NO_OF_REPLICAS= $no_of_replicas\n";
print $F "--let \$MTR_NDB_MAX_NO_OF_TABLES= $max_no_of_tables\n";
print $F "--let \$MTR_NDB_MAX_NO_OF_ORDERED_INDEXES= $max_no_of_ordered_indexes\n";
print $F "--let \$MTR_NDB_DATA_MEMORY= $data_memory\n";
print $F "--let \$MTR_NDB_INDEX_MEMORY= $index_memory\n";
print $F "--let \$MTR_NDB_USABLE_DATA_MEMORY= $usable_data_memory\n";
print $F "--let \$MTR_NDB_USABLE_INDEX_MEMORY= $usable_index_memory\n";
print $F "--let \$MTR_NDB_FIRST_NDBD_NODEID= $first_ndbd_nodeid\n";
print $F "--let \$MTR_NDB_LAST_NDBD_NODEID= $last_ndbd_nodeid\n";
print $F "--let \$MTR_NDB_FIRST_MGMD_NODEID= $first_mgmd_nodeid\n";
print $F "--let \$MTR_NDB_LAST_MGMD_NODEID= $last_mgmd_nodeid\n";
$F->close();

EOF

--source $MYSQLTEST_VARDIR/tmp/ndb_info_result.inc

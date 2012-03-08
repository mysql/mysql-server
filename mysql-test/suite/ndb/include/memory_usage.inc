--perl
##
## NDB Memory Usage
##
## Include script that sets two variables that can be used to evaluate actual memory
## usage in NDB data nodes
##
## Parameters:
##   $ndb_node_id - The node ID to get the memory usage for, can be a number of the node, or 'ALL',
##                  which returns the average useage over all nodes. ALL is the default value.
##
## Returned variables:
##   $MTR_NDB_DATA_USAGE_PERCENTAGE  - The percentage of data memory used on all nodes
##   $MTR_NDB_INDEX_USAGE_PERCENTAGE - The percentage of index memory used on all nodes
##
## Values are read from ndb_mgm REPORT MemoryUsage
##
## Example output from ALL REPORT MemoryUsage
## ndb_mgm -e "ALL REPORT MemoryUsage"
## Connected to Management Server at: localhost:16660
## Node 1: Data usage is 85%(548 32K pages of total 640)
## Node 1: Index usage is 86%(138 8K pages of total 160)
## Node 2: Data usage is 85%(548 32K pages of total 640)
## Node 2: Index usage is 86%(138 8K pages of total 160)

use strict;

use IO::File;

#
# Set up paths
#
my $vardir = $ENV{MYSQLTEST_VARDIR} or die "Need MYSQLTEST_VARDIR";
my $ndb_connectstring = $ENV{NDB_CONNECTSTRING} or die "Need NDB_CONNECTSTRING";
my $ndb_mgm = $ENV{NDB_MGM} or die "Need NDB_MGM";

# Input parameters
my $ndb_node_id = $ENV{ndb_node_id} || 'ALL';

my $cmd = "$ndb_mgm --ndb-connectstring='$ndb_connectstring' -e \"$ndb_node_id REPORT MemoryUsage\"";
# print "Calling ndb_mgm: '$cmd'\n";
my $output = `$cmd`;

my $memory = {};
foreach my $line (split("\n", $output)) {
    # Skip empty lines
    next if ($line =~ m/^,+$/g);

    if ($line =~ m/Node (\d+): (Index|Data) usage is \d+%\((\d+) (\d+)K pages of total (\d+)\)/g) {
        my $node_id = $1;
        my $type = lc($2);
        my $used = $3;
        my $size = $4;
        my $total = $5;

        if (!defined $memory->{$type}) {
            $memory->{$type} = {}
        }
        if (!defined $memory->{$type}->{$node_id}) {
            $memory->{$type}->{$node_id} = {};
        }
        $memory->{$type}->{$node_id}->{'used'} = $used;
        $memory->{$type}->{$node_id}->{'total'} = $total;
        $memory->{$type}->{$node_id}->{'size'} = $size;
    }
}

sub usage_percentage {
    my $type = lc(shift);
    my $mem  = shift;

    my $used = 0;
    my $total = 0;

    foreach my $node (keys %{$mem->{$type}}) {
        $used  += $mem->{$type}->{$node}->{'used'};
        $total += $mem->{$type}->{$node}->{'total'};
    }
    die "Parsing error - not able to detect total number of pages, output: '$output'" if ($total == 0);
    return sprintf("%.2f", $used * 100 / $total);
}

my $num_nodes = scalar(keys %{$memory->{Data}});
my $data_usage_percentage = usage_percentage('Data', $memory);
my $index_usage_percentage = usage_percentage('Index', $memory);

my $file_name = "$vardir/tmp/ndb_memory_usage_result.inc";
my $F = IO::File->new($file_name, 'w') or die "Could not open '$file_name' for writing";
print $F "--let \$MTR_NDB_DATA_USAGE_PERCENTAGE= $data_usage_percentage\n";
print $F "--let \$MTR_NDB_INDEX_USAGE_PERCENTAGE= $index_usage_percentage\n";
$F->close();

EOF

--source $MYSQLTEST_VARDIR/tmp/ndb_memory_usage_result.inc

#!/usr/bin/perl

use strict;
use warnings;
use Getopt::Long;
use File::Basename;

my $report_file;
my $duration_str;
my $start_timestamp;
my $end_timestamp;
my @info;
my $tx_count = 0;
my $bytes = 0;

GetOptions(
    "file=s" => \$report_file,
    "time=s" => \$duration_str,
    "start=s" => \$start_timestamp,
    "end=s" => \$end_timestamp,
    "tx=i"   => \$tx_count,
    "bytes=i" => \$bytes,
    "info=s@" => \@info
) or die "Error in command line arguments\n";

if (!$report_file) {
    die "Usage: $0 --file=<path> --time=<duration> --tx=<count> --bytes=<bytes> [--info=<string>]...\n";
}

# Ensure directory exists
my $dir = dirname($report_file);
if (!-d $dir) {
    # Simple check, assuming var/log structure usually exists in MTR
    warn "Directory $dir does not exist, attempting to write anyway...\n";
}

open(my $fh, '>>', $report_file) or die "Could not open report file '$report_file' $!";

# Parse time string HH:MM:SS.micros to seconds
my $duration_sec = 0;
if ($duration_str && $duration_str =~ /(\d+):(\d+):(\d+)(\.\d+)?/) {
    $duration_sec = $1 * 3600 + $2 * 60 + $3 + ($4 || 0);
}

my $mb = $bytes / (1024 * 1024);
my $tps = ($duration_sec > 0) ? $tx_count / $duration_sec : 0;
my $mbps = ($duration_sec > 0) ? $mb / $duration_sec : 0;

print $fh "================================================================================\n";
print $fh "                   Replication Benchmark Report                                 \n";
print $fh "================================================================================\n";
foreach my $i (@info) {
    print $fh sprintf("  %-30s : %s\n", "Info", $i);
}
print $fh sprintf("  %-30s : %s\n", "Start Time", $start_timestamp || "N/A");
print $fh sprintf("  %-30s : %s\n", "End Time", $end_timestamp || "N/A");
print $fh sprintf("  %-30s : %s\n", "Duration", $duration_str || "N/A");
print $fh sprintf("  %-30s : %.4f s\n", "Duration (seconds)", $duration_sec);
print $fh "--------------------------------------------------------------------------------\n";
print $fh sprintf("  %-30s : %d\n", "Transactions Applied", $tx_count);
print $fh sprintf("  %-30s : %.2f MB\n", "Data Replicated", $mb);
print $fh "--------------------------------------------------------------------------------\n";
print $fh sprintf("  %-30s : %.2f TPS\n", "Throughput (Transactions)", $tps);
print $fh sprintf("  %-30s : %.2f MB/s\n", "Throughput (Data)", $mbps);
print $fh "================================================================================\n\n";

close $fh;

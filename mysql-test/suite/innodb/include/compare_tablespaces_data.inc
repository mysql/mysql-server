# This utility compares data of two tablespace files.
# Typical use case is when we need to compare the snapshots of the same
# tablespaces in different states.
# For instance : Tablespace snapshots, after clean shutdown and post recovery
#
# By default, this utility compares data in all page. It can compare data in
# the specific page range also, if you set the needed optional values.
#
# Required Values :
#
# IBD_FILE_1_PATH   first tablespace file path
# IBD_FILE_2_PATH   second tablespace file path
# PAGE_SIZE         page size
#
# Optional Values :
#
# BEGIN_PAGE_NUM    Starting page number of the range in tablespace
# END_PAGE_NUM      Last page number of the range in tablespace
#

perl;

use strict;
use warnings;

my $ibd_file_1 = $ENV{'IBD_FILE_1_PATH'} or die "IBD_FILE_1_PATH is required";
my $ibd_file_2 = $ENV{'IBD_FILE_2_PATH'} or die "IBD_FILE_2_PATH is required";
my $page_size  = $ENV{'PAGE_SIZE'}       or die "PAGE_SIZE is required";

my $ibd_1_size = -s $ibd_file_1;
defined $ibd_1_size or
  die "File '$ibd_file_1' not found or couldn't determine its size.\n";

my $ibd_2_size = -s $ibd_file_2;
defined $ibd_2_size or
  die "File '$ibd_file_2' not found or couldn't determine its size.\n";

if ($ibd_1_size != $ibd_2_size) {
  die "File '$ibd_file_1' size $ibd_1_size is not same " .
    "as File '$ibd_file_2' size $ibd_2_size\n";
}

my $begin_page = $ENV{'BEGIN_PAGE_NUM'} || 0;
my $end_page   = $ENV{'END_PAGE_NUM'}   || $ibd_1_size / $page_size - 1;

my $end_page_offset = $end_page * $page_size;
if ($ibd_1_size <= $end_page_offset) {
  die "Offset $end_page_offset of Page number: $end_page " .
    "must be less than the file size $ibd_1_size\n";
}

my $page_header_len  = 38;
my $page_trailer_len = 8;

sub compare_page_data {
  my ($file_handle1, $file_handle2, $page_num) = @_;

  # Exclude the page, header and trailer as they may differ
  my $start_offset = $page_num * $page_size + $page_header_len;
  my $length       = $page_size - $page_header_len - $page_trailer_len;

  # Move file handles to the specified start offset
  seek($file_handle1, $start_offset, 0);
  seek($file_handle2, $start_offset, 0);

  my $buffer1;
  my $buffer2;

  my $bytes_read1 = read($file_handle1, $buffer1, $length);
  my $bytes_read2 = read($file_handle2, $buffer2, $length);

  if (defined($bytes_read1) &&
      defined($bytes_read2) &&
      $bytes_read1 == $bytes_read2) {
    return $buffer1 eq $buffer2;
  }

  return 0;    # Files are not identical
}

open my $file_handle1, '<', $ibd_file_1 or die "Can't open $ibd_file_1: $!";
open my $file_handle2, '<', $ibd_file_2 or die "Can't open $ibd_file_2: $!";

binmode $file_handle1;
binmode $file_handle2;

for my $page_num ($begin_page .. $end_page) {
  if (!compare_page_data($file_handle1, $file_handle2, $page_num)) {
    print "The page#$page_num data is not identical\n";
  }
}

close $file_handle1;
close $file_handle2;

EOF

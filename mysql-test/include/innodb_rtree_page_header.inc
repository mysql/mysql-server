# Print FIL_PAGE_TYPE, PAGE_DIRECTION and PAGE_N_DIRECTION of the root
# page of an InnoDB R-tree index, read directly from the .ibd file.
#
# The caller must make sure the page has been written to the file, for
# example with FLUSH TABLES ... FOR EXPORT.
#
# Usage:
# --let $rtree_table = t1
#    Table name in schema test; the file read is test/t1.ibd.
# --let $rtree_index = sg
#    Name of the spatial index.
# --let $rtree_page_size = 16384
#    Page stride in the file: innodb_page_size, or ZIP_PAGE_SIZE for a
#    ROW_FORMAT=COMPRESSED table.
# --source include/innodb_rtree_page_header.inc
#
# For a compressed table the .ibd file holds the compressed frame.
# page_zip_compress() copies the page header (FIL_PAGE_DATA .. PAGE_DATA)
# uncompressed to the start of the frame and page_zip_write_header()
# keeps it there at the same offsets, so the same offsets are read.
#
# Output, for example:
# t1.ibd: FIL_PAGE_TYPE=17854 PAGE_DIRECTION=5 PAGE_N_DIRECTION=0
#
# FIL_PAGE_TYPE should be FIL_PAGE_RTREE (17854); any other value means
# that the wrong page was read (wrong page number or page stride).

if (!$rtree_table)
{
  --die innodb_rtree_page_header.inc: rtree_table is not set
}
if (!$rtree_index)
{
  --die innodb_rtree_page_header.inc: rtree_index is not set
}
if (!$rtree_page_size)
{
  --die innodb_rtree_page_header.inc: rtree_page_size is not set
}

--let RTREE_IBD_FILE = `SELECT CONCAT(@@datadir, 'test/', '$rtree_table', '.ibd')`
--let RTREE_IBD_NAME = $rtree_table.ibd
# Byte offset of the root page in the file.
--let RTREE_PAGE_OFFSET = `SELECT i.PAGE_NO * $rtree_page_size FROM INFORMATION_SCHEMA.INNODB_INDEXES i JOIN INFORMATION_SCHEMA.INNODB_TABLES t ON i.TABLE_ID = t.TABLE_ID WHERE t.NAME = 'test/$rtree_table' AND i.NAME = '$rtree_index'`

--perl
use strict;
my $file = $ENV{RTREE_IBD_FILE};
my $offset = $ENV{RTREE_PAGE_OFFSET};
$offset =~ /^\d+$/ or die "no page offset for $file";
sysopen(my $fh, $file, 0) or die "cannot open $file: $!";
# FIL_PAGE_TYPE is at offset 24.
sysseek($fh, $offset + 24, 0) or die "cannot seek $file: $!";
sysread($fh, my $type_buf, 2) == 2 or die "cannot read $file: $!";
my $type = unpack("n", $type_buf);
# PAGE_HEADER (38) + PAGE_DIRECTION (12) = 50, PAGE_N_DIRECTION follows.
sysseek($fh, $offset + 50, 0) or die "cannot seek $file: $!";
sysread($fh, my $buf, 4) == 4 or die "cannot read $file: $!";
close($fh);
my ($direction, $n_direction) = unpack("nn", $buf);
print "$ENV{RTREE_IBD_NAME}: FIL_PAGE_TYPE=$type PAGE_DIRECTION=$direction PAGE_N_DIRECTION=$n_direction\n";
EOF

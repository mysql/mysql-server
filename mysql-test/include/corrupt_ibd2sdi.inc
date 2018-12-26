# The following variables should be set before invoking this inc file
# IBD_FILE      - file name (including absolute path)
# IBD_PAGE_SIZE - page size
# IBD_PAGE_NO   - page number in file
# IBD_OFFSET    - offset within the page  (optional: default to 0th byte
#                 in page)
# IBD_CHAR      - character to write into page (optional: default char
#		  written is 0)
# IBD_CHAR_LEN  - write IBD_CHARs of this length.

perl;
use strict;
use warnings;
use Fcntl qw(SEEK_SET);
my $ibd_file = $ENV{'IBD_FILE'};
my $ibd_page_size = $ENV{'IBD_PAGE_SIZE'};
my $ibd_page_no = $ENV{'IBD_PAGE_NO'};
my $ibd_offset = $ENV{'IBD_OFFSET'};
my $ibd_char = $ENV{'IBD_CHAR'};
my $ibd_char_len = $ENV{'IBD_CHAR_LEN'};

if (!$ibd_file) {
  print "ENV variable IBD_FILE is not set. Use --let IBD_FILE=some_filename_here in test\n";
  exit;
}

if (!$ibd_page_size) {
  print "ENV variable IBD_PAGE_SIZE is not set. Use --let IBD_PAGE_SIZE=some_size_here in test\n";
  exit;
}

if (!$ibd_page_no) {
  print "ENV variable IBD_PAGE_NO is not set. Use --let IBD_PAGE_NO=some_number_here in test\n";
  exit;
}

if (!$ibd_offset) {
	$ibd_offset = 0;
}

my $file_offset = ($ibd_page_size * $ibd_page_no) + $ibd_offset;

open(FILE, "+<", $ibd_file) or die "could not open $ibd_file: $!";
binmode FILE;
seek(FILE, $file_offset, SEEK_SET) or die "could not seek: $!";

my $packed;

if ($ibd_char eq "0") {

  $packed = chr($ibd_char);
  print FILE $packed x $ibd_char_len;

} elsif ($ibd_char_len == 2) {

  # Write 2 bytes in Big-Endian format
  $packed = pack "n", $ibd_char;
  print FILE $packed;

} elsif($ibd_char_len == 4) {

  # Write 4 bytes in  Big-Endian format
  $packed = pack "N", $ibd_char;
  print FILE $packed;
}

close FILE;
EOF

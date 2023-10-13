# Requires three parameters
# 1. IBD_FILE :- the file to corrupt a page in it
# 2. INNODB_PAGE_SIZE :- page_size of IBD
# 3. PAGE_NUM :- the page to corrupt
# 4. ALL_ZEROES :- write the entire page as all-zeros (optional parameter)
#                (innodb doesn't treat all-zero as corrupted page)
perl;
use IO::Handle;
my $file        = $ENV{'IBD_FILE'}         or die;
my $page_size   = $ENV{'INNODB_PAGE_SIZE'} or die;
my $page_num    = $ENV{'PAGE_NUM'}         or die;
my $all_zeroes  = $ENV{'ALL_ZEROES'} || 0;
my $file_offset = $page_size * $page_num;
my $bytes_to_corrupt;
open(FILE, "+<", $file) or die;
FILE->autoflush(1);
binmode FILE;

if ($all_zeroes) {
  $bytes_to_corrupt = $page_size;
} else {
  $bytes_to_corrupt = $page_size / 2;
  $file_offset      = $file_offset + $bytes_to_corrupt;
}
seek(FILE, $file_offset, SEEK_SET);
print FILE chr(0) x $bytes_to_corrupt;
close FILE;
EOF

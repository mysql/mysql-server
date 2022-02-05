# Requires three parameters
# 1. IBD_FILE :- the file to corrupt a page in it
# 2. INNODB_PAGE_SIZE :- page_size of IBD
# 3. PAGE_NUM :- the page to corrupt
# 4. ALL_ZEROES :- write the entire page as all-zeros (optional parameter)
#                (innodb doesn't treat all-zero as corrupted page)
perl;
use IO::Handle;
my $file = $ENV{'IBD_FILE'} or die;
my $page_size = $ENV{'INNODB_PAGE_SIZE'} or die;
my $page_num = $ENV{'PAGE_NUM'} or die;
my $all_zeroes = $ENV{'ALL_ZEROES'};
open(FILE, "+<", $file) or die;
FILE->autoflush(1);
binmode FILE;
seek(FILE, $page_size * $page_num, SEEK_SET);
if ($all_zeroes) {
 print FILE chr(0) x $page_size;
} else {
 print FILE chr(0) x ($page_size/2);
}
close FILE;
EOF

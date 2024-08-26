# Requires three parameters
# 1. IBD_FILE :- the file to corrupt a page in it
# 2. INNODB_PAGE_SIZE :- page_size of IBD
# 3. PAGE_NUM :- the page to corrupt
# 4. ALL_ZEROES :- write the entire page as all-zeros (optional parameter)
#                (innodb doesn't treat all-zero as corrupted page)

# Wait for the server to be fully closed. Otherwise we could try to open
# a file that is still used leading to an error on Windows. It should be
# caller's responsibility to do that, but all current usages allow to have it
# just once, here.
--source include/wait_until_disconnected.inc

perl;
use IO::Handle;
my $file        = $ENV{'IBD_FILE'}         or die "IBD_FILE is required";
my $page_size   = $ENV{'INNODB_PAGE_SIZE'} or die "INNODB_PAGE_SIZE is required";
my $page_num    = $ENV{'PAGE_NUM'}         or die "PAGE_NUM is required";
my $all_zeroes  = $ENV{'ALL_ZEROES'} || 0;
my $file_offset = $page_size * $page_num;
my $bytes_to_corrupt;

# On Windows something prevents opening files for up to several minutes
# (perhaps an antivirus). At any rate, even the mysqld process might
# still be going through termination logic while keeping files open.
my $attempts = 0;
while (!open(FILE, "+<", $file)) {
 if($! == 13 && ++$attempts < 600){
  sleep 1;
 }else{
  die "Could not open the $file due to error $!"
 }
}

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

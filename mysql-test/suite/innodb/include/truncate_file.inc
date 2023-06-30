# Requires two parameters
# 1. FILE_PATH :- File path from which bytes to be truncated
# 2. NUM_OF_BYTES :- Number of bytes to be truncated in the end of file
#
perl;
use IO::Handle;
use File::stat;
my $fname = $ENV{'FILE_PATH'};
my $bytes = $ENV{'NUM_OF_BYTES'};
my $file_size = stat($fname)->size;
if($bytes >= $file_size)
{
  die "Number of bytes $bytes to truncate cannot exceed the file '$fname' size $file_size";
}
open(FILE_HANDLE, "+<:raw", $fname) or die "Could not open file '$fname' in bin mode";
FILE_HANDLE->autoflush(1);

# Remove $bytes from the end of the file
truncate(FILE_HANDLE, $file_size - $bytes) or die "Could not truncate $bytes bytes from file '$fname'";
close FILE_HANDLE;
EOF

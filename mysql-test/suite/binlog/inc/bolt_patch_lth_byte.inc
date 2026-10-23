# Overwrites one byte of the body of the Large_transaction_header event in a
# stopped server's binlog file, and recomputes the event's CRC32 checksum.
#
# Parameters:
#   $bolt_patch_file    path of the binlog file
#   $bolt_patch_pos     start offset of the Large_transaction_header event
#   $bolt_patch_offset  offset of the byte in the event body
#   $bolt_patch_value   new value of the byte
#
--let $bolt_patch_script = $MYSQLTEST_VARDIR/tmp/bolt_patch_lth_byte.pl
--write_file $bolt_patch_script
use strict;
use Compress::Zlib qw(crc32);
my ($file, $pos, $offset, $value) = @ARGV;
# The body starts after the 19-byte common header.
my $byte_pos = $pos + 19 + $offset;
open(my $fh, '+<:raw', $file) or die "Cannot open $file: $!";
my $header;
seek($fh, $pos, 0);
read($fh, $header, 19) == 19 or die "Cannot read the event header";
my $event_length = unpack('V', substr($header, 9, 4));
seek($fh, $byte_pos, 0);
print $fh pack('C', $value);
my $event;
seek($fh, $pos, 0);
read($fh, $event, $event_length - 4) == $event_length - 4
  or die "Cannot read the event";
seek($fh, $pos + $event_length - 4, 0);
print $fh pack('V', crc32($event));
close($fh);
EOF
--exec perl $bolt_patch_script $bolt_patch_file $bolt_patch_pos $bolt_patch_offset $bolt_patch_value
--remove_file $bolt_patch_script

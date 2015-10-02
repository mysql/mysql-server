perl;
  use strict;
  my $path = "$ENV{'PATH_TO_TABLE'}";
  if ($path eq "") { die "No path given!"; }
  my @found_files = glob($path . "*.MYD");
  if ($#found_files >= 0) {
    print "found " . (1 + $#found_files) . " *.MYD file(s)\n";
  }
  @found_files = glob($path . "*.ibd");
  if ($#found_files >= 0) {
    print "found " . (1 + $#found_files) . " *.ibd file(s)\n";
  }
  if (-e $path . ".par") {
    print ".par file exists\n";
  }
  my $frm_file = $path . ".frm";
  if (not -e $frm_file) {
    die $frm_file . " does not file exists!";
  }
  open(FH, "<", $frm_file) or die "Failed to open '$frm_file'";
  binmode FH;
  my $bin_val;
  seek(FH, 0x33, 0) or die "Failed to seek";
  my $res= read FH, $bin_val, 4;
  die "Failed to read bin_val! res: $res" unless ($res == 4);
  my $frm_ver = unpack("V", $bin_val);
  if ($frm_ver >= 50708) {
    print ".frm created by version >= 5.7.8\n";
  } else {
    print ".frm created by version: $frm_ver\n";
  }
  seek(FH, 3, 0) or die "Failed to seek";
  my $res= read FH, $bin_val, 1;
  die "Failed to read bin_val! res: $res" unless ($res == 1);
  print "DB_TYPE (byte 3): " . unpack("C", $bin_val) . "\n";
  seek(FH, 61, 0) or die "Failed to seek";
  $res= read FH, $bin_val, 1;
  die "Failed to read bin_val! res: $res" unless ($res == 1);
  print "DEFAULT_PART_DB_TYPE (byte 61): " . unpack("C", $bin_val) . "\n";
  # Also check the engine name
  seek(FH, 55, 0) or die "Failed to seek";
  $res= read FH, $bin_val, 4;
  my $length = unpack("V", $bin_val);
  if ($length != 0)
  {
    # see from table.cc (reading .frm file)
    seek(FH, 6, 0) or die "Failed to seek";
    $res= read FH, $bin_val, 2;
    my $record_offset= unpack("v", $bin_val);
    seek(FH, 14, 0) or die "Failed to seek";
    $res= read FH, $bin_val, 2;
    my $tmp = unpack("v", $bin_val);
    if ($tmp == 0xffff)
    {
      seek(FH, 47, 0) or die "Failed to seek";
      $res= read FH, $bin_val, 4;
      $tmp= unpack("V", $bin_val);
    }
    $record_offset+= $tmp;
    seek(FH, 16, 0) or die "Failed to seek";
    $res= read FH, $bin_val, 2;
    $tmp = unpack("v", $bin_val);
    $record_offset+= $tmp;
    # connect string length + string
    seek(FH, $record_offset, 0) or die "Failed to seek";
    $res= read FH, $bin_val, 2;
    die "Failed to read bin_val! res: $res" unless ($res == 2);
    $length= unpack("v", $bin_val);
    seek(FH, $record_offset + 2 + $length, 0) or die "Failed to seek";
    $res= read FH, $bin_val, 2;
    die "Failed to read bin_val! res: $res" unless ($res == 2);
    $length= unpack("v", $bin_val);
    $res= read FH, $bin_val, $length;
    die "Failed to read bin_val! res: $res" unless ($res == $length);
    print "DB_TYPE string: '" . $bin_val . "'\n";
  }
  close(FH);
EOF

perl;
use Digest::file qw(digest_file_hex);
use File::Basename;
my $fh;
my $out_file_used = 0;
if (defined $ENV{'LIST_OUT_FILE'} && $ENV{'LIST_OUT_FILE'} ne "") {
  open($fh, ">", $ENV{'LIST_OUT_FILE'}) or die "Could not open file: $!";
  $out_file_used = 1;
} else {
  $fh = STDOUT;
}
for my $file_path (sort (glob "$ENV{'FILE_LIST_PATTERN'}")) {
  my $size = (stat($file_path))[7];
  #my $modify_time = (stat($file_path))[9];
  #my $create_time = (stat($file_path))[10];
  my $filename = basename($file_path);
  $filename =~ s/#p#/#P#/;
  $filename =~ s/#sp#/#SP#/;
  #print $fh "$filename\t$modify_time\t$create_time\t$size\t";
  print $fh "$filename\t$size\t";
  print $fh digest_file_hex($file_path, "MD5") . "\n";
}
if ($out_file_used) {
  close($fh);
}
EOF

# truncate a giving file, all contents of the file are be cleared

if (!$file)
{
  --echo Please assign a file name to $file!!
  exit;
}

let TRUNCATE_FILE= $file;

perl;
my $filename = $ENV{'TRUNCATE_FILE'}; 
$filename =~ s/\/\//\//;
open FILE, '>', $filename || die "Can not open file $filename";
close FILE;
EOF

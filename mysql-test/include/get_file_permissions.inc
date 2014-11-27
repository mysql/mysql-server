--perl
use strict;
my $input_file= $ENV{'_INPUT_FILE_'} or die "Input file is not set";
my $mode = (stat($input_file))[2];
open(FILE, ">", "$ENV{MYSQL_TMP_DIR}/permissions.inc") or die;
my $perm= $mode & 07777;
print FILE "let \$FILE_PERMISSIONS= $perm;\n";
close FILE;
EOF

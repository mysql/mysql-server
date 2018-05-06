# this .inc file needs JSON_FILE_PATH set
perl;
use Cwd;
use strict;
use warnings;
my $srcdir = getcwd;
require "$srcdir/include/ibd2sdi.pl";

ibd2sdi_replace_mysql();
EOF

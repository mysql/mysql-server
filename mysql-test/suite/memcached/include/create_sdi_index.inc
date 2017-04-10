perl;
use strict;
use warnings;
use Cwd;

my $srcdir = getcwd;
require "$srcdir/include/sdi_perl_func.pl";

sdi_create_index();

sdi_disconnect();
EOF

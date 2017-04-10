perl;
use Cwd;
use strict;
use warnings;

my $srcdir = getcwd;
require "$srcdir/include/sdi_perl_func.pl";
sdi_drop_index();
sdi_disconnect();
EOF

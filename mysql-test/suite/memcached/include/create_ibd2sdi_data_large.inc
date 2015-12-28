perl;
use strict;
use warnings;
use Cwd;

my $srcdir = getcwd;
require "$srcdir/include/sdi_perl_func.pl";

my $input = "abcdefghijklmnopqrstuvwxyz";
for (my $i=0; $i <= 800; $i++) {
	sdi_set_get($i, $i, $input x ($i+1));
}

sdi_disconnect();
EOF

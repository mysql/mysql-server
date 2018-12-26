perl;
use Cwd;
use strict;
use warnings;

my $srcdir = getcwd;
require "$srcdir/include/sdi_perl_func.pl";

my $memcached_table=$ENV{'MEMCACHED_TABLE_MAPPING'} or die;
sdi_switch_table($memcached_table);

sdi_create_index();
sdi_disconnect();
EOF

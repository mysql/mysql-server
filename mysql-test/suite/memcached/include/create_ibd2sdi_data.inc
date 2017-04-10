perl;
use strict;
use warnings;
use Cwd;

my $srcdir = getcwd;
require "$srcdir/include/sdi_perl_func.pl";

my $memcached_table=$ENV{'MEMCACHED_TABLE_MAPPING'} or die;
sdi_switch_table($memcached_table);

sdi_set_get(0, 0, "Test00");
sdi_set_get(1, 0, "Test10");
sdi_set_get(2, 0, "Test20");
sdi_set_get(3, 0, "Test30");
sdi_set_get(3, 1, "Test31");
sdi_set_get(3, 2, "Test32");
sdi_set_get(3, 3, "Test33");
sdi_set_get(3, 4, "Test34");
sdi_set_get(3, 5, "Test35");
sdi_set_get(4, 4, "Test44");
sdi_set_get(5, 0, "Test50");

sdi_disconnect();
EOF

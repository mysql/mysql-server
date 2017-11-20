perl;
use strict;
use warnings;
use Cwd;

my $srcdir = getcwd;
require "$srcdir/include/sdi_perl_func.pl";

my $memcached_table=$ENV{'MEMCACHED_TABLE_MAPPING'} or die;
my $input= $ENV{'TABLE_METADATA'} or die;
my $sdi_id = $ENV{'SDI_ID'} or die;
my $sdi_type = $ENV{'SDI_TYPE'} or die;

sdi_switch_table($memcached_table);

sdi_set_get($sdi_type, $sdi_id, $input);
sdi_disconnect();

EOF

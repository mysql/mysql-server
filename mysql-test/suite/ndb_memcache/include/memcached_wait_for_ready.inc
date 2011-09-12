#
# Wait for the memcached server to be ready
#

-- perl 
use lib "lib/";
use My::Memcache;

my $mc = My::Memcache->new();
my $port = $ENV{MTR_BUILD_THREAD} * 10 + 10000 + 8;
my $r = $mc->connect("localhost",$port);
die unless($r);

$mc->get("dummy");
EOF

#
# Wait for the memcached server to finish online reconfiguration
#

--perl RECONF_SCRIPT

  use strict;
  use lib "lib/";
  use My::Memcache;
  my $mc = My::Memcache->new();
  my $port = $ENV{MTR_BUILD_THREAD} * 10 + 10000 + 8;
  my $r = $mc->connect("localhost",$port);
  $mc->wait_for_reconf() if ($r);

RECONF_SCRIPT
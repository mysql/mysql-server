#
# Wait for the memcached server to finish online reconfiguration
#

--perl RECONF_SCRIPT

  use strict;
  use lib "lib/";
  use My::Memcache;
  
  my $port = $ENV{NDB_MEMCACHED_1_PORT} or die "Need NDB_MEMCACHED_1_PORT";

  my $mc = My::Memcache->new();
  my $r = $mc->connect("localhost",$port);
  $mc->wait_for_reconf() if ($r);

RECONF_SCRIPT
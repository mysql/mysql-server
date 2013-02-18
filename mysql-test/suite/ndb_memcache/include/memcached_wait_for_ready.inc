#
# Wait for the memcached server to be ready
#

-- perl WAIT_READY

  use lib "lib/";
  use IO::File;
  use My::Memcache;

  my $port = $ENV{NDB_MEMCACHED_1_PORT} or die "Need NDB_MEMCACHED_1_PORT";
  
  my $mc = My::Memcache->new();
  my $r = $mc->connect("localhost",$port);
  die unless($r);

  $mc->get("dummy");
  $mc->note_config_version();

WAIT_READY
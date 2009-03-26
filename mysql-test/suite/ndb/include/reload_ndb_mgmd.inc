--perl

use strict;
use IO::Socket::INET;

use lib "lib/";
use My::Config;

my $vardir = $ENV{MYSQLTEST_VARDIR} or die "Need MYSQLTEST_VARDIR";
my $config= My::Config->new("$vardir/my.cnf");
my $mgmd = $config->group("cluster_config.ndb_mgmd.1.1");
my $server_port = $mgmd->value("PortNumber");
#print "server_port: $server_port\n";

my $server = new IO::Socket::INET
(
 PeerAddr => 'localhost',
 PeerPort => $server_port,
 Proto    => 'tcp'
);

print $server "reload config\n";
print $server "mycnf: 1\n";
print $server "\n";

my $result = "unkown error";
while(my $line= <$server>){
  if ($line =~ /result: (.*)/)
  {
    $result = $1;
  }
  last if ($line eq "\n");
}
die "reload failed, result: '$result'"
    unless $result eq "Ok";

EOF
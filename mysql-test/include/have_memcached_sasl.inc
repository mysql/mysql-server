--disable_query_log
--disable_warnings
if (!$memcached_port) {
  --die You need to specify $memcached_port variable
}
if ($DAEMON_MEMCACHED_DIR == '') {
  --skip Memcached not available
}

--let ENV_MEMCACHED_PORT= $memcached_port
source include/memcache_config.inc;

INSERT INTO cache_policies VALUES("cache_policy", "innodb_only",
"innodb_only", "innodb_only", "innodb_only");
INSERT INTO config_options VALUES("separator", "0");
INSERT INTO containers VALUES ("desc_t1", "innodb_memcache", "t1", "c1",
"c2", "0", "0", "0", "PRIMARY");
CREATE TABLE t1 (c1 VARCHAR(32), c2 VARCHAR(1024), primary key(c1))
ENGINE = INNODB;

INSTALL PLUGIN daemon_memcached SONAME 'libmemcached.so';

perl;
use strict;
use File::Basename;
use IO::File;
use lib "lib";
use My::Memcache;

use constant BIN_CMD_SASL_AUTH => 0x21;
use constant ERR_AUTH_FAILURE  => 0x20;

sub authenticate {
  my ($mc, $user, $pass, $mech)= @_;
  $mech ||= 'PLAIN';
  my $buf = sprintf("%c%s%c%s", 0, $user, 0, $pass);
  $mc->send_binary_request(BIN_CMD_SASL_AUTH, $mech, $buf, '');
  my ($status, $result) = $mc->get_binary_response();
  if ($status == ERR_AUTH_FAILURE) {
    $mc->{error} = "AUTH_FAILURE";
  };
  return ($status == 0) ? 1 : 0;
}

my $vardir = $ENV{MYSQLTEST_VARDIR} or die "Need MYSQLTEST_VARDIR";
my $F = IO::File->new("$vardir/tmp/have_sasl_result.inc", "w") or die;

my $mc = My::Memcache::Binary->new();
my $r = $mc->connect("127.0.0.1", $ENV{'ENV_MEMCACHED_PORT'});

if (!authenticate($mc, 'testuser', 'testpass')) {
  #print $mc->{error} . "\n";
  if ($mc->{error} eq 'UNKNOWN_COMMAND') {
    print $F "--let \$skip_test=1\n";
  };
};

$F->close();
EOF

--source $MYSQLTEST_VARDIR/tmp/have_sasl_result.inc
--remove_file $MYSQLTEST_VARDIR/tmp/have_sasl_result.inc

UNINSTALL PLUGIN daemon_memcached;
DROP DATABASE innodb_memcache;

if ($skip_test) {
  --skip SASL is not supported by this build
}
--enable_warnings
--enable_query_log

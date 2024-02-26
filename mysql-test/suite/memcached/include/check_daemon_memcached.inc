# We need to wait until the plugin becomes responsive.
# We achieve that by using memcached command "stats"
# which returns an hash containing many fields, among them
# "hosts", but only if the plugin is up and running.
#
# Example of usage:
#   INSTALL PLUGIN daemon_memcached SONAME 'libmemcached.so';
#   # host and port on which the memcached should operate
#   # ususally host is 127.0.0.1 and
#   # port is in -master.opt file as --loose-daemon_memcached_option
#   --let $memcached_address=127.0.0.1:11227
#   # each attempt consists of reconnecting and trying to do 'stats'
#   # which may take up to 20 seconds, but actually takes more like 1s
#   # because determining that local port is closed or that server is
#   # not responding as it should is quick, and we do 1s sleep in between.
#   --let $memcached_check_attempts=20
#   # Either you expect 'success' or 'error'
#   --let $memcached_expect=success
#   --source ../include/check_daemon_memcached.inc
#
# ..or even better use:
#   --let $memcached_address=127.0.0.1:11227
#   --source ../include/load_daemon_memcached_expecting_success.inc
# ..or..
#   --let $memcached_address=127.0.0.1:11227
#   --source ../include/load_daemon_memcached_expecting_error.inc

if(!$memcached_address){
  --die You need to specify $memcached_address variable
}
--let $memcached_expect_is_valid=0
if($memcached_expect=="error"){
  --let $memcached_expect_is_valid=1
}
if($memcached_expect=="success"){
  --let $memcached_expect_is_valid=1
}
if(!$memcached_expect_is_valid){
  --die You need to specify $memcached_expect either 'success' or 'error'
}
if(!$memcached_check_attempts){
  --die You need to specify a non-zero number of $memcached_check_attempts
}
# For some reason the ENV variable can not be named the same
# as the regular variable even if case of letters is different,
# hance we use ENV_ prefix.
--let ENV_MEMCACHED_ADDRESS= $memcached_address
--let ENV_MEMCACHED_CHECK_ATTEMPTS= $memcached_check_attempts
--let ENV_MEMCACHED_EXPECT= $memcached_expect

perl;
use DBI;
use Cache::Memcached;
my $memd = new Cache::Memcached {
  'servers' => [ $ENV{'ENV_MEMCACHED_ADDRESS'} ],
  'connect_timeout' => 20,
  'select_timeout' => 20
};
my $max_wait = $ENV{'ENV_MEMCACHED_CHECK_ATTEMPTS'};
my $wait_sec = $max_wait;
my $success = 0;
while (1) {
  # The undocumented ->forget_dead_hosts() clears a global %host_dead
  # which is crucial, as otherwise failed attempts of ->stats()
  # combined with later attempt to reconnect, will mark the
  # server as dead for 20+rand(10) seconds.
  # By the way there is no connection between all occurences of
  # number 20 in this file.
  $memd->forget_dead_hosts();
  my $hash = $memd->stats();
  if(exists $hash->{'hosts'}){
    $success = 1;
    last;
  }
  if ($wait_sec-->0) {
    # Sleep 1 second and try again
    sleep 1;
  } else {
    last;
  }
}
$memd->disconnect_all();
if (($ENV{'ENV_MEMCACHED_EXPECT'} eq 'success') && !$success) {
  die "Waited for " . $max_wait . " sec. but " . $ENV{'ENV_MEMCACHED_ADDRESS'} . " did not respond with stats\n";
}
if (($ENV{'ENV_MEMCACHED_EXPECT'} eq 'error') && $success) {
  die "The memcached plugin at " . $ENV{'ENV_MEMCACHED_ADDRESS'} . " was supposed to fail, but it works";
}
EOF

source include/not_valgrind.inc;
source include/have_memcached_plugin.inc;
source include/not_windows.inc;

--disable_query_log
CALL mtr.add_suppression("daemon-memcached-w-batch-size': unsigned");
CALL mtr.add_suppression("Could not obtain server's UPN to be used as target service name");
CALL mtr.add_suppression("Warning: MySQL is trying to drop");
--enable_query_log

--enable_connect_log
SET @transaction_isolation= @@global.transaction_isolation;
SET GLOBAL TRANSACTION ISOLATION LEVEL READ UNCOMMITTED;

# Create the memcached tables
--disable_query_log
source include/memcache_config.inc;
--enable_query_log

INSERT INTO cache_policies VALUES("cache_policy", "innodb_only",
				  "innodb_only", "innodb_only", "innodb_only");

INSERT INTO config_options VALUES("separator", "|");

# describe table for memcache
INSERT INTO containers VALUES ("desc_t1", "test", "t1",
			       "c1", "c2",  "c3", "c4", "c5", "PRIMARY");

USE test;

--disable_warnings
DROP TABLE IF EXISTS t1;
--enable_warnings
CREATE TABLE t1        (c1 VARCHAR(32),
			c2 VARCHAR(1024),
			c3 INT, c4 BIGINT UNSIGNED, c5 INT, primary key(c1))
ENGINE = INNODB;

INSERT INTO t1 VALUES ('D', 'Darmstadt', 0, 0, 0);
INSERT INTO t1 VALUES ('B', 'Berlin', 0, 0, 0);
INSERT INTO t1 VALUES ('C', 'Cottbus', 0, 0 ,0);
INSERT INTO t1 VALUES ('H', 'Hamburg', 0, 0, 0);

# Tables must exist before plugin can be started!
--let $memcached_address=$ENV_MEMCACHED_ADDRESS
--source ../include/load_daemon_memcached_expecting_success.inc

# Spawn 3 clients which bombard the server in parallel


perl;

# Set to 1 if you want to test empty keys (strings of length 0)
# which usually leads to server noticing syntax error and causing
# handling of which causes a lot of bugs around dangling conn_data
# at the price of slowdown of the whole test due to waiting for timeouts
# in client after each such failure.
my $TEST_EMPTY_KEYS = $ENV{'ENV_TEST_EMPTY_KEYS'};

# Each test needs a separate server instance
my $MEMCACHED_ADDRESS = $ENV{'ENV_MEMCACHED_ADDRESS'};

# You might want to run bisect on the set of commands used during this test.
# Note that some bugs are actually easier to reproduce with the set of
# operations restricted to just (1,9,12,16) or (1,5,9,12,16).
my $STEPS_SUBSET = $ENV{'ENV_STEPS_SUBSET'};
print "TEST EMPTY KEYS: $TEST_EMPTY_KEYS\n";
print "MEMCACHED ADDRESS: $MEMCACHED_ADDRESS\n";
print "STEPS SUBSET: $STEPS_SUBSET\n";

sub mt_rand
{
  my $a = shift;
  my $b = shift;
  my $c = $b + 1 - $a;
  return $a + int(rand($c));
}
sub rand_str
{
  my $maxlengt = shift;
  my $k=mt_rand(($TEST_EMPTY_KEYS?0:1),$maxlengt);

  my @Chars = ('1'..'9');
  my $Number = '';

  for (1..$k) {
    $Number .= $Chars[int rand @Chars];
  }
  return $Number;
}
use DBI;
use Cache::Memcached;
use Time::HiRes qw(time);
use POSIX qw(strftime);


my @steps = $STEPS_SUBSET =~ /\d+/g;

sub run_single_client
{
  my $worker_id=shift;

  # For debug you might want to change to 1
  my $VERBOSE = 0;
  if ($VERBOSE) {
    open(RESULT_FILE, ">/tmp/$worker_id.worker_log");
    select(RESULT_FILE);
    $| = 1;
  }
  sub log_print
  {
    if ($VERBOSE) {
      my $msg = shift;
      my $t = time;
      my $date = strftime "%F %T", localtime $t;
      $date .= sprintf ".%06d", ($t-int($t))*1e6;

      print "At $date worker $worker_id: $msg\n";
    }
  }

  my $memd = new Cache::Memcached {
    'servers' => [ $MEMCACHED_ADDRESS ],
    'connect_timeout' => 20,
    'select_timeout' => 20
  };



  my $key=mt_rand(0,100);
  my $rounds = 40000 / @steps;
  log_print "Running $rounds rounds...";
  for my $i (0..$rounds)
  {
    if(mt_rand(0,100)==0 && (1 ~~ @steps)){
      log_print "Flush all";
      my $res = $memd->flush_all;
      log_print "Flush all returned $res";
    }

    if(mt_rand(0,1)==0 && (2 ~~ @steps)){
      $key=mt_rand(0,100);
    }

    if(mt_rand(0,100)==0&& (3 ~~ @steps)){
      $key=rand_str(3);
    }

    my $new_val="";

    if(mt_rand(0,1)==0 && (4 ~~ @steps)){
      $new_val=rand_str(67000);
    } else {
      $new_val=mt_rand(0,67000);
    }

    if(mt_rand(0,2)==0 && (5 ~~ @steps)){
      log_print "Set $key to $new_val..";
      my $res=$memd->set($key,$new_val);
      log_print "Set $key to $new_val returned $res";
    }

    if(mt_rand(0,1)==0 && (6 ~~ @steps)){
      $key=mt_rand(0,100);
    }

    if(mt_rand(0,2)==0 && (7 ~~ @steps)){
      log_print "Replace $key with $new_val..";
      my $res=$memd->replace($key,$new_val);
      log_print "Replace $key with $new_val returned $res";
    }

    if(mt_rand(0,1)==0 && (8 ~~ @steps)){
      $key=mt_rand(0,100);
    }

    if(mt_rand(0,2)==0 && (9 ~~ @steps)){
      log_print "Get $key ...";
      my $val=$memd->get($key);
      log_print "Get $key returned $val";
    }

    if(mt_rand(0,1)==0 && (10 ~~ @steps)){
      $key=mt_rand(0,100);
    }

    if(mt_rand(0,2)==0 && (11 ~~ @steps)){
      log_print "Increment $key ...";
      my $val=$memd->incr($key);
      log_print "Increment $key returned $val";
    }

    if(mt_rand(0,1)==0 && (12 ~~ @steps)){
      $key=mt_rand(0,1000);
    }

    if(mt_rand(0,2)==0 && (13 ~~ @steps)){
      log_print "Decrement $key ...";
      my $val=$memd->decr($key);
      log_print "Decrement $key returned $val";
    }

    if(mt_rand(0,1)==0 && (14 ~~ @steps)){
      $key=mt_rand(0,10000000);
    }

    if(mt_rand(0,50)==0 && (15 ~~ @steps)){
      log_print "Delete $key...";
      my $res=$memd->delete($key);
      log_print "Delete $key returned $res";
    }

    if($i%1000 == 0 && (16 ~~ @steps))
    {
      log_print "Get stats()...";
      my %hash = $memd->stats();
      log_print %hash;
    }

    $i++;
  }
  $memd->disconnect_all;
}

use threads;
for my $worker_id (1..3)
{
  print "Spawning $worker_id\n";
  threads->create('run_single_client', $worker_id);
}

foreach my $thr ( threads->list() ) {
  print "joining...\n";
  $thr->join();
}

EOF



# Stop plugin before innodb_memcached configuration
UNINSTALL PLUGIN daemon_memcached;
DROP TABLE t1;
DROP DATABASE innodb_memcache;

SET @@global.transaction_isolation= @transaction_isolation;

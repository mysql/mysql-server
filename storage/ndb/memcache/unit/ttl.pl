#!/usr/bin/perl

use strict;
use lib "../../../../mysql-test/lib/";
use My::Memcache;
use Getopt::Long;
use Carp;
use DBI;

my $do_flush = '';  
my $do_test = '';
my $bin = '';
my $do_all = '';
my $create = '';
my $drop = '';
my $reconf = '';

GetOptions("flush"  => \$do_flush, 
           "all" => \$do_all, 
           "test=s" => \$do_test,  
           "bin" => \$bin,
           "create" => \$create,  "drop" => \$drop,  "reconf" => \$reconf
          );


####### CREATE TABLE for test 3.
if($create || $drop || $reconf) {
  my $dsn = "DBI:mysql:database=ndbmemcache;host=localhost;port=3306";
  my $dbh = DBI->connect($dsn, "root", "");

  if($drop) {
    $dbh->do("DROP TABLE test_long_ttl");
    $dbh->do("DELETE FROM containers where db_table = 'test_long_ttl'");
    $dbh->do("DELETE FROM key_prefixes where key_prefix like 'ttl%'");
  }
  
  if($create) {
    $dbh->do("CREATE TABLE test_long_ttl ( " .
      "      mkey varchar(40) PRIMARY KEY NOT NULL, " .
      "      value varchar(200),  " .
      "      expires timestamp NULL, " .
      "      flags int unsigned NULL, " .
      "      ext_id int unsigned NULL, " .
      "      ext_size int unsigned NULL ) ENGINE=ndbcluster");
    
    $dbh->do("INSERT INTO containers SET " . 
      "        name = 'ttl1', db_schema = 'ndbmemcache', " . 
      "        db_table = 'test_long_ttl', ".
      "        key_columns = 'mkey', value_columns = 'value', " .
      "        flags = 'flags', expire_time_column = 'expires', " .
      "        large_values_table = 'ndbmemcache.external_values'");

    $dbh->do("INSERT INTO containers SET " .
      "       name = 'ttl2', db_schema = 'ndbmemcache', " . 
      "       db_table = 'test_long_ttl', ".
      "       key_columns = 'mkey', value_columns = 'value', " .
      "       large_values_table = 'ndbmemcache.external_values', " .
      "       flags = '1280'");
    
    $dbh->do("INSERT INTO key_prefixes " .
      "       VALUES(0, 'ttl:', 0, 'ndb-test', 'ttl1'), " .
      "             (0, 'ttls:', 0, 'ndb-test', 'ttl2')");
  }

  if($reconf) {
    $dbh->do("UPDATE memcache_server_roles " .
     "        SET update_timestamp = now() where role_id = 0");
  }

  exit;
}



my $mc;
if($bin) {
  $mc = My::Memcache::Binary->new();
}
else {
  $mc = My::Memcache->new();
}
my $port = 11211;

my $r = $mc->connect("localhost",$port);

if($r == 0) {
  print STDERR "DID NOT CONNECT TO MEMCACHE AT PORT $port \n";
}

# Some values 
my $val_short = "Melville Tolstoy Austen Balzac ";
my $val_long  = ($val_short . " ... ") x 1500;

# Flush all
if($do_flush || $do_all) {
  $mc->flush();
}

if($do_all || $do_test == '1') {
  $mc->set("t:test", "Yello");   print $mc->{error} . "\n";
}

###### TEST 2:  Expire times with inline (SHORT) values, demo_table_tabs
if($do_all || $do_test == '2') {
  $mc->set_expires(0);    $mc->set("t:1", "Groen");  ## Will not expire
  $mc->set_expires(5);    $mc->set("t:2", "Yello");  ## Expire in 5 seconds

  sleep(2);
  ($mc->get("t:1") == "Groen")  || Carp::confess "Expected result";
  ($mc->get("t:2") == "Yello")  || Carp::confess "Expected result";
  
  sleep(4); 
  ($mc->get("t:1") == "Groen")  || Carp::confess "Expected result";
  $mc->get("t:2")               && Carp::confess "Item should have expired";  
  ($mc->{error} == "NOT_FOUND") || Carp::confess "Expected NOT_FOUND";
}

###### TEST 3: Expire times with SHORT & LONG values in a long-val table
if($do_all || $do_test == '3') {
  # Set large and small values with no expiration 
  $mc->set_expires(0);
  $mc->set("ttl:one",  $val_short);
  $mc->set("ttl:two",  $val_long);

  # Set large and small values with 5 second expiration 
  $mc->set_expires(5);
  $mc->set("ttl:three",  $val_short);
  $mc->set("ttl:four",   $val_long);
  
  sleep(2);
  my $r1 = $mc->get("ttl:one");   ($r1 == $val_short)  || Carp::confess();
  my $r2 = $mc->get("ttl:two");   ($r2 == $val_long)  || Carp::confess();
  my $r3 = $mc->get("ttl:three");   ($r3 == $val_short)  || Carp::confess();
  my $r4 = $mc->get("ttl:four");   ($r4 == $val_long)  || Carp::confess();

  sleep(4);
  my $r1 = $mc->get("ttl:one");   ($r1 == $val_short)  || Carp::confess();
  my $r2 = $mc->get("ttl:two");   ($r2 == $val_long)  || Carp::confess();
  my $r3 = $mc->get("ttl:three");   ($r3 == $val_short)  || Carp::confess();
  my $r4 = $mc->get("ttl:four");   ($r4 == $val_long)  || Carp::confess();
}


##### TEST 4:  Set flags 
if($do_all || $do_test == '4') {
  $mc->set_expires(0);

  # FLAGS STORED IN DATABASE
  $mc->set_flags(100);  
  $mc->set("ttl:five", $val_short);
  $mc->get("ttl:five");
  ($mc->{flags} == 100)  || Carp::confess("Expected flags = 100");
  
  # STATIC FLAGS
  $mc->set("ttls:six", $val_short) || Carp::confess("SET failed: " . $mc->{error});
  $mc->get("ttls:six");
  ($mc->{flags} == 1280)  || Carp::confess("Expected flags = 1280");

  $mc->delete("ttl:five");
  $mc->delete("ttls:six");

  # FLAGS STORED IN DATABASE
  $mc->set_flags(100);  
  $mc->set("ttl:five", $val_long);
  $mc->get("ttl:five");
  ($mc->{flags} == 100)  || Carp::confess("Expected flags = 100");
  
  # STATIC FLAGS
  $mc->set("ttls:six", $val_long)  || Carp::confess("SET failed");
  $mc->get("ttls:six");
  ($mc->{flags} == 1280)  || Carp::confess("Expected flags = 1280");

}


##### TEST 5.  FLAGS in demo_table_tabs
if($do_all || $do_test == '5') {
  $mc->set_flags(100);
  $mc->set("t:12", "Con\tBrio");
  $mc->set_flags(200);
  $mc->set("t:13", "Sul ponticello");
  
  $mc->set_flags(0);

  $mc->get("t:12");
  ($mc->{flags} == 100)  || Carp::confess("Expected flags = 100");
  
  $mc->get("t:13");
  ($mc->{flags} == 200)  || Carp::confess("Expected flags = 200");
}



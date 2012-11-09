#!/usr/bin/perl

use strict;
use lib "../../../../mysql-test/lib/";
use My::Memcache;
use Getopt::Long;
use Carp;
use DBI;

my $do_flush = '';  
my $do_test = '';
my $do_all = '';
my $create = '';
my $drop = '';
my $reconf = '';

GetOptions("flush"  => \$do_flush, 
"all" => \$do_all, 
"test=s" => \$do_test,  
"create" => \$create,  "drop" => \$drop,  "reconf" => \$reconf
);


if($create || $drop || $reconf) {
  my $dsn = "DBI:mysql:database=ndbmemcache;host=localhost;port=3306";
  my $dbh = DBI->connect($dsn, "root", "");
  
  if($drop) {
    $dbh->do("DROP TABLE test_sequence");
    $dbh->do("DELETE FROM containers where db_table = 'test_sequence'");
    $dbh->do("DELETE FROM key_prefixes where key_prefix = 'seq:'");
  }
  
  if($create) {
    $dbh->do("CREATE TABLE test_sequence ( " .
      "      name varchar(12) PRIMARY KEY NOT NULL, " .
      "      value bigint unsigned ) ENGINE=ndbcluster");
    
    $dbh->do("INSERT INTO containers SET " . 
      "        name = 'seq_tab', db_schema = 'ndbmemcache', " . 
      "        db_table = 'test_sequence', ".
      "        key_columns = 'name', increment_column = 'value' " );
        
    $dbh->do("INSERT INTO key_prefixes " .
      "       VALUES(0, 'seq:', 0, 'ndb-test', 'seq_tab') ");
  }
  
  if($reconf) {
    $dbh->do("UPDATE memcache_server_roles " .
      "       SET update_timestamp = now() where role_id = 0");
  }
}

if ($do_all || $do_test || $do_flush) {

  my $mc = My::Memcache::Binary->new();
  my $port = 11211;

  my $r = $mc->connect("localhost",$port);

  if($r == 0) {
    print STDERR "DID NOT CONNECT TO MEMCACHE AT PORT $port \n";
  }


  # Flush all
  if($do_flush) {
    $mc->flush();
  }


  ###### TEST 1: INCR + CREATE
  if($do_all || $do_test == '1') {
    my $a = $mc->incr("seq:a", 1, 0);
    defined($a) || Carp::croak("no return from incr");
    print "$a \n";
  }


  ###### TEST 2: GET
  if($do_all || $do_test == '2') {
    my $a = $mc->get("seq:a") . "\n";
    print "$a \n";
  }
  
  ###### TEST 3: INCR + CREATE
  if($do_all || $do_test == '3') {
    my $a = $mc->incr("seq:b", 1, 0);
    defined($a) || Carp::croak("no return from incr");
    print "$a \n";
  }

  ###### TEST 4: INCR without create -- should be undefined
  if($do_all || $do_test == '4') {
    my $a = $mc->incr("seq:c", 1);
    defined($a) && Carp::croak("should be undefined!");
  }
}

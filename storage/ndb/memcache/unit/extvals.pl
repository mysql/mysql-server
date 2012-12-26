
use strict;
use lib "../../../../mysql-test/lib/";
use My::Memcache;
use Getopt::Long;

my $do_flush = '';  
my $do_test = '';
my $bin = '';
my $do_all = '';

GetOptions("flush"  => \$do_flush, 
           "all" => \$do_all, 
           "test=s" => \$do_test,  
           "bin" => \$bin
          );
if($do_test == '') { $do_all = 1; }

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

# Here are some values
my $val_50 = "ABCDEFGHIJKLMNOPQRSTabcdefghijklmnopqrst123456789_";
my $val_40 = "Riga,Stockholm,Helsinki,Oslo,Copenhagen_";
my $val_100k = $val_50 x 2000;
my $val_60k  = $val_40 x 1500;
my $val_13949 = "." x 13949;

# Flush all
if($do_flush || $do_all) {
  $mc->flush();
}

# Do two inserts using ADD.  The first should succeed, the second should fail.
if($do_test=='1' || $do_all) {
  $mc->add("b:long_val_1", $val_100k)  ||  Carp::confess("Failed insert: t1");
  $mc->add("b:long_val_1", $val_100k)  &&  Carp::confess("Insert should fail: t1");
}

# Do two DELETES.  The first should succeed, the second should fail.
if($do_test=='2' || $do_all) {
  $mc->delete("b:long_val_1")  || Carp::confess("Failed delete: t2");
  $mc->delete("b:long_val_1")  && Carp::confess("Delete should fail: t2");
}

# Do an insert, then a read, then a delete, then a read
if($do_test=='3' || $do_all) {
  $mc->add("b:long_val_2", $val_100k)     || Carp::confess("Failed insert: t3");
  ($mc->get("b:long_val_2") == $val_100k) || Carp::confess("GET results unexpected: t3");
  $mc->delete("b:long_val_2")             || Carp::confess("Failed delete: t3");
  $mc->get("b:long_val_2")                && Carp::confess("GET should fail: t3");
}

# Insert using SET.  Then read.  Then attempt an ADD, which should fail.
if($do_test=='4' || $do_all) {
  # Short values
  $mc->set("b:test_short_set", "Mikrokosmos");
  ($mc->get("b:test_short_set") == "Mikrokosmos" ) || Carp::confess("GET fail");
  $mc->add("b:test_short_set", "!!") && Carp::confess("ADD should fail");

  # Long values
  $mc->set("b:test_set", $val_60k)     || Carp::confess("Failed SET as insert: t4");
  ($mc->get("b:test_set") == $val_60k) || Carp::confess("GET results unexpected: t4");
  $mc->add("b:test_set", $val_100k)    && Carp::confess("ADD should fail");
}

# UPDATE via SET
if($do_test=='5' || $do_all) {
  $mc->set("b:test_set", $val_100k)     || Carp::confess("Failed SET as update:t5");
  ($mc->get("b:test_set") == $val_100k) || Carp::confess("GET results unexpected:t5");
}

# UPDATE via SET from a long value to a short value
if($do_test=='6' || $do_all) {
  $mc->set("b:test_set", $val_40)     || Carp::confess("Failed SET as update:t6");
  ($mc->get("b:test_set") == $val_40) || Carp::confess("GET results unexpected:t6");

  # UPDATE via REPLACE 
  $mc->replace("b:test_set", $val_13949) || Carp::confess("failed REPLACE");
  ($mc->get("b:test_set") == $val_13949) || Carp::confess("results unexpected");
  
  # REPLACE of non-existing value should fail
  $mc->replace("b:test_626", $val_60k) && Carp::confess("REPLACE should fail");
}

# UPDATE via SET from a short value to a long value 
if($do_test=='7' || $do_all) {
  $mc->add("b:test7", $val_40)        || Carp::confess("Failed insert: t7");
  ($mc->get("b:test7") == $val_40)    || Carp::confess("GET results unexpected: t7");
  $mc->set("b:test7", $val_100k)      || Carp::confess("Failed update: t7");
  ($mc->get("b:test7") == $val_100k)  || Carp::confess("GET results unexpected: t7");
}

# APPEND and PREPEND tests 

if($do_test=='8a' || $do_test=='8' || $do_all) {
  # Inline values
  $mc->set("test8", $val_50);
  $mc->prepend("test8", $val_40);
  $mc->append("test8", $val_40);
  my $r = $mc->get("test8");
  ($r == $val_40 . $val_50 . $val_40)  || Carp::confess("results unexpected");
  # APPEND/PREPEND to empty value should fail
  $mc->append("b:empty", $val_40)  && Carp::confess("append should fail");
  $mc->append("b:empty", $val_60k)  && Carp::confess("append should fail");
  $mc->prepend("b:empty", $val_40)  && Carp::confess("prepend should fail");
  $mc->prepend("b:empty", $val_60k)  && Carp::confess("prepend should fail");
}

if($do_test=='8b' || $do_test=='8' || $do_all) {
  # APPEND/PREPEND to empty value should fail
  $mc->append("b:empty", $val_40)  && Carp::confess("append should fail");
  $mc->prepend("b:empty", $val_40)  && Carp::confess("prepend should fail");
  
  # Externalizable (but short) values 
  $mc->set("b:test8", $val_50);
  $mc->prepend("b:test8", $val_40);
  $mc->append("b:test8", $val_40);
  my $r1 = $mc->get("b:test8");
  ($r1 == $val_40 . $val_50 . $val_40)  || Carp::confess("results unexpected");

  # Now make it long
  $mc->append("b:test8", $val_60k) || Carp::confess("append failed");
  my $r2 = $mc->get("b:test8");
  ($r2 == $1 . $val_60k) || Carp::confess("results unexpected");

  # Prepend to a long value 
  my $saying = "Elephants have trunks. ";
  $mc->prepend("b:test8", $saying) || Carp::confess("prepend failed");
  my $r3 = $mc->get("b:test8");
  ($r3 == $saying . $r2) || Carp::confess("results unexpected");

  # Now append a very large value to it 
  $mc->append("b:test8", $val_100k) || Carp::confess("append failed");
  my $r4 = $mc->get("b:test8");
  ($r4 == $r3 . $val_100k) || Carp::confess("results unexpected"); 
}

if($do_test=='8c' || $do_test=='8' || $do_all) {
  # Take a value 1 less than a complete part, and append one character to it
  # This tests the "update-only" optimization in append
  $mc->set("b:test8c", $val_13949) || Carp::confess("set failed");
  $mc->append("b:test8c", "!");
  my $r1 = $mc->get("b:test8c");
  ($r1 == $val_13949 . "!")    ||  Carp::confess("results unexpected"); 
}

if($do_test=='8d' || $do_test=='8' || $do_all) {
  # Now append one more character.  This tests the "insert-only" optimization.
  $mc->append("b:test8c", "?");
  my $r2 = $mc->get("b:test8c");
  ($r2 == $val_13949 . "!?")    ||  Carp::confess("results unexpected"); 
}

if($do_test=='9' || $do_all) {
  # APPEND stress test
  $mc->add("b:t9", $val_50);
  for my $i (2 .. 300) {
    $mc->append("b:t9", $val_50);
    my $r = $mc->get("b:t9");
    ($r == $val_50 x $i) || Carp::confess("results unexpected");
  } 
}
    
if($do_test=='10' || $do_all) {
  # PREPEND stress test
  $mc->add("b:t10", $val_50);
  for my $i (2 .. 300) {
    $mc->prepend("b:t10", $val_50);
    my $r = $mc->get("b:t10");
    ($r == $val_50 x $i) || Carp::confess("results unexpected");
  } 
}

if($do_test=='11' || $do_all) {
  $mc->set("b:test11", $val_100k);
  $mc->replace("b:test11", "__.__");
  $mc->replace("b:test11", $val_60k);

  my $val_too_big = "x" x ((1024 * 1024) + 1);
  $mc->set("b:testtoobig", $val_too_big);
  $mc->{error} =~ "VALUE_TOO_LARGE" || Carp::confess "Expected TOO_LARGE";
}

if($do_test=='12' || $do_all) {
  # Test SET with flags
  $mc->set_flags(0);
  $mc->set("b:test12", "Mikrokosmos");
}


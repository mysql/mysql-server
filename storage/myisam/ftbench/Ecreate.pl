#!/usr/bin/perl

$test=shift || die "Usage $0 testname [option]";
$option=shift;

open(D, "<data/$test.d") || die "Cannot open(<data/$test.d): $!";
open(Q, "<data/$test.q") || die "Cannot open(<data/$test.q): $!";

$N=0;

print <<__HEADER__;
DROP TABLE IF EXISTS $test;
CREATE TABLE $test (
  id int(10) unsigned NOT NULL,
  text text NOT NULL,
  FULLTEXT KEY text (text)
) TYPE=MyISAM CHARSET=latin1;

ALTER TABLE $test DISABLE KEYS;
__HEADER__

while (<D>) { chomp;
  s/'/\\'/g; ++$N;
  print "INSERT $test VALUES ($N, '$_');\n";
}

print <<__PREP__;
ALTER TABLE $test ENABLE KEYS;
SELECT $N;
__PREP__

$N=0;

while (<Q>) { chomp;
  s/'/\\'/g; ++$N;
  $_="MATCH text AGAINST ('$_' $option)";
  print "SELECT $N, id, $_ FROM $test WHERE $_;\n";
}

print <<__FOOTER__;
DROP TABLE $test;
__FOOTER__



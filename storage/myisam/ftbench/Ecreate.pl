#!/usr/bin/perl

# Copyright (C) 2003 MySQL AB
# 
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; version 2 of the License.
# 
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
# 
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

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



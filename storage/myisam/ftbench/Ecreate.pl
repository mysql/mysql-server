#!/usr/bin/perl

# Copyright (c) 2003, 2022, Oracle and/or its affiliates.
# Use is subject to license terms
# 
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License, version 2.0,
# as published by the Free Software Foundation.
#
# This program is also distributed with certain software (including
# but not limited to OpenSSL) that is licensed under separate terms,
# as designated in a particular file or component or in included license
# documentation.  The authors of MySQL hereby grant you an additional
# permission to link the program and your derivative works with the
# separately licensed software that they have included with MySQL.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License, version 2.0, for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA

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



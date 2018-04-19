#!/usr/bin/perl
# -*- cperl -*-

# Copyright (c) 2007 MySQL AB, 2009 Sun Microsystems, Inc.
# Use is subject to license terms.
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

use strict;
use warnings;

use File::Temp qw / tempdir /;
my $dir = tempdir( CLEANUP => 1 );

use Test::More qw(no_plan);

BEGIN { use_ok ( "My::ConfigFactory" ) };

my $gen1_cnf= "$dir/gen1.cnf";
open(OUT, ">", $gen1_cnf) or die;

print OUT <<EOF
[mysqld.master]
# Comment
option1=value1
basedir=abasedir

[mysqld.1]
# Comment
option1=value1
option2=value2

[ENV]
MASTER_MY_PORT=\@mysqld.master.port

EOF
;
close OUT;

my $basedir= "../..";

my $config= My::ConfigFactory->new_config
(
 {
  basedir => $basedir,
  template_path => $gen1_cnf,
  vardir => "/path/to/var",
  baseport => 10987,
  #hosts => [ 'host1', 'host2' ],
 }
);

print $config;

ok ( $config->group("mysqld.master"), "group mysqld.master exists");
ok ( $config->group("mysqld.1"), "group mysqld.1 exists");
ok ( $config->group("client"), "group client exists");
ok ( !$config->group("mysqld.3"), "group mysqld.3 does not exist");

ok ( $config->first_like("mysqld"), "group like 'mysqld' exists");

is( $config->value('mysqld.1', '#host'), 'localhost',
    "mysqld.1.#host has been generated");

is( $config->value('client', 'host'), 'localhost',
    "client.host has been generated");

is( $config->value('client', 'host'),
    $config->value('mysqld.master', '#host'),
    "client.host is same as mysqld.master.host");

ok ( $config->value("mysqld.1", 'character-sets-dir') =~ /$basedir.*charsets$/,
     "'character-sets-dir' generated");

ok ( $config->value("mysqld.1", 'lc-messages-dir') =~ /$basedir.*share$/,
     "'lc-messages-dir' generated");

ok ( $config->value("ENV", 'MASTER_MY_PORT') =~ /\d/,
     "'lc-messages-dir' generated");

my $gen2_cnf= "$dir/gen2.cnf";
open(OUT, ">", $gen2_cnf) or die;

print OUT <<EOF
[mysqld.master]
EOF
;
close OUT;

my $config2= My::ConfigFactory->new_config
(
 {
  basedir => $basedir,
  template_path => $gen2_cnf,
  vardir => "/path/to/var",
  baseport => 10987,
  #hosts => [ 'host1', 'host2' ],
 }
);

print $config2;

ok ( $config2->first_like("mysqld"), "group like 'mysqld' exists");

#!/usr/bin/perl
# -*- cperl -*-

# Copyright (c) 2007, 2022, Oracle and/or its affiliates.
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
use warnings 'FATAL';
use lib "lib";

use File::Temp qw / tempdir /;
my $dir = tempdir( CLEANUP => 1 );

use Test::More qw(no_plan);

BEGIN { use_ok ( "My::Config" ) };

my $test_cnf= "$dir/test.cnf";

# Write test config file
open(OUT, ">", $test_cnf) or die;

print OUT <<EOF
[mysqld]
# Comment
option1=values2
option2= value4
option4
basedir=thebasedir
[mysqld_1]
[mysqld_2]
[mysqld.9]
[client]
socket  =\tasocketpath
EOF
;
close OUT;

my $config= My::Config->new($test_cnf);
# We don't use isa_ok, as it's console output in case of true varies between platforms.
ok(defined $config && $config->isa("My::Config"), "config is a My::Config");

print $config;

ok ( $config->group("mysqld_2"), "group mysqld_2 exists");
ok ( $config->group("mysqld_1"), "group mysqld_1 exists");
ok ( $config->group("mysqld.9"), "group mysqld.9 exists");
ok ( $config->group("mysqld.9")->suffix() eq ".9", "group mysqld.9 has suffix .9");

ok ( $config->group("mysqld"), "group mysqld exists");
ok ( $config->group("client"), "group client exists");
ok ( !$config->group("mysqld_3"), "group mysqld_3 does not exist");

ok ( $config->options_in_group("mysqld") == 4, "options in [mysqld] is 4");
ok ( !defined $config->options_in_group("nonexist") , "group [nonexist] is not defined");

{
  my @groups= $config->groups();
  ok(@groups == 5, "5 groups");
  my $idx= 0;
  foreach my $name ('mysqld', 'mysqld_1', 'mysqld_2', 'mysqld.9', 'client') {
    is($groups[$idx++]->name(), $name, "checking groups $idx");
  }
}

{
  my @groups= $config->like("mysqld");
  ok(@groups == 4, "4 groups like mysqld");
  my $idx= 0;
  foreach my $name ('mysqld', 'mysqld_1', 'mysqld_2', 'mysqld.9') {
    is($groups[$idx++]->name(), $name, "checking like(\"mysqld\") $idx");
  }
}

{
  my @groups= $config->like("not");
  ok(@groups == 0, "checking like(\"not\")");
}

is($config->first_like("mysqld_")->name(), "mysqld_1", "first_like");

is( $config->value('mysqld', 'option4'), undef,
    "mysqld_option4 exists, does not have a value");

ok( $config->exists('mysqld', 'option4'),
    "mysqld_option4 exists");
ok( $config->exists('mysqld', 'option2'),
    "mysqld_option2 exists");
ok( !$config->exists('mysqld', 'option5'),
    "mysqld_option5 does not exists");

# Save the config to file
my $test2_cnf= "$dir/test2.cnf";
$config->save($test2_cnf);

# read it back and check it's the same
my $config2= My::Config->new($test2_cnf);
# We don't use isa_ok, as it's console output in case of true varies between platforms.
ok(defined $config2 && $config2->isa("My::Config"), "config2 is a My::Config");
is_deeply( \$config, \$config2, "test.cnf is equal to test2.cnf");


my $test_include_cnf= "$dir/test_include.cnf";
# Write test config file that includes test.cnf
open(OUT, ">", $test_include_cnf) or die;

print OUT <<EOF
[mysqld]
!include test.cnf
# Comment
option1=values3
basedir=anotherbasedir
EOF
;
close OUT;

# Read the config file
my $config3= My::Config->new($test_include_cnf);
# We don't use isa_ok, as it's console output in case of true varies between platforms.
ok(defined $config3 && $config3->isa("My::Config"), "config3 is a My::Config");
print $config3;
is( $config3->value('mysqld', 'basedir'), 'anotherbasedir',
    "mysqld_basedir has been overriden by value in test_include.cnf");

is( $config3->value('mysqld', 'option1'), 'values3',
    "mysqld_option1 has been overriden by value in test_include.cnf");

is( $config3->value('mysqld', 'option2'), 'value4',
    "mysqld_option2 is from included file");

is( $config3->value('client', 'socket'), 'asocketpath',
    "client.socket is from included file");

is( $config3->value('mysqld', 'option4'), undef,
    "mysqld_option4 exists, does not have a value");

print "$config3\n";


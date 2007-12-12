# -*- cperl -*-
use Test::More qw(no_plan);
use strict;

use_ok ("My::Find");
my $basedir= "../..";

print "=" x 40, "\n";
my $mysqld_exe= my_find_bin($basedir,
			    ["sql", "bin"],
                            ["mysqld", "mysqld-debug"]);
print "mysqld_exe: $mysqld_exe\n";
print "=" x 40, "\n";
my $mysql_exe= my_find_bin($basedir,
			   ["client", "bin"],
                           "mysql");
print "mysql_exe: $mysql_exe\n";
print "=" x 40, "\n";

my $mtr_build_dir= $ENV{MTR_BUILD_DIR};
$ENV{MTR_BUILD_DIR}= "debug";
my $mysql_exe= my_find_bin($basedir,
			   ["client", "bin"],
                           "mysql");
print "mysql_exe: $mysql_exe\n";
$ENV{MTR_BUILD_DIR}= $mtr_build_dir;
print "=" x 40, "\n";

my $charset_dir= my_find_dir($basedir,
			     ["share/mysql", "sql/share", "share"],
			     "charsets");
print "charset_dir: $charset_dir\n";
print "=" x 40, "\n";

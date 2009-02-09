# -*- cperl -*-
use Test::More qw(no_plan);
use strict;

use_ok ("My::Platform");
use My::Platform;

use File::Temp qw / tempdir /;
my $dir = tempdir( CLEANUP => 1 );

print "Running on Windows\n" if (IS_WINDOWS);
print "Using ActiveState perl\n" if (IS_WIN32PERL);
print "Using cygwin perl\n" if (IS_CYGWIN);

print "dir: '$dir'\n";
print "native: '".native_path($dir)."'\n";
print "mixed: '".mixed_path($dir)."'\n";
print "posix: '".posix_path($dir)."'\n";

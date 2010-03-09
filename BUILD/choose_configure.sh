#!/bin/sh
# Choose whether to use autoconf created configure
# of perl script that calls cmake.

# Ensure cmake and perl are there
cmake -P cmake/check_minimal_version.cmake >/dev/null 2>&1 || HAVE_CMAKE=no
perl --version >/dev/null 2>&1 || HAVE_CMAKE=no
scriptdir=`dirname $0`
if test "$HAVE_CMAKE" = "no"
then
  sh $scriptdir/configure.am "$@"
else
  perl $scriptdir/cmake/configure.pl "$@"
fi


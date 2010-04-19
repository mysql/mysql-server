#!/bin/sh
# Choose whether to use autoconf created configure
# of perl script that calls cmake.

# # This is a temporary hack to build 5.5.3-m3:
# # The "cmake" way still shows issues in our release build environment.
# # Block it temporarily, but in a way that can easily be undone.
# HAVE_CMAKE=no

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


#!/bin/sh
# Choose whether to use autoconf created configure
# of perl script that calls cmake.

# Ensure cmake and perl are there
cmake -P cmake/check_minimal_version.cmake >/dev/null 2>&1 || HAVE_CMAKE=no
perl --version >/dev/null 2>&1 || HAVE_CMAKE=no
if test "$HAVE_CMAKE" = "no"
then
  sh ./configure.am $@
else
  perl ./cmake/configure.pl $@
fi


#!/bin/sh
# Create MySQL autotools infrastructure

die() { echo "$@"; exit 1; }

aclocal || die "Can't execute aclocal" 
autoheader || die "Can't execute autoheader"
# --force means overwrite ltmain.sh script if it already exists 
# Added glibtoolize reference to make native OSX autotools work
if test -f /usr/bin/glibtoolize  ; then
  glibtoolize --automake --force || die "Can't execute glibtoolize"
else
  libtoolize --automake --force || die "Can't execute libtoolize"
fi
  
# --add-missing instructs automake to install missing auxiliary files
# and --force to overwrite them if they already exist
automake --add-missing --force || die "Can't execute automake"
autoconf || die "Can't execute autoconf"
(cd storage/bdb/dist && sh s_all)
(cd storage/innobase && aclocal && autoheader && aclocal && automake && autoconf)

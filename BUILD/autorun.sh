#!/bin/sh
# Create MySQL autotools infrastructure

die() { echo "$@"; exit 1; }

# Added glibtoolize reference to make native OSX autotools work
if [ -f /usr/bin/glibtoolize ]
  then
    LIBTOOLIZE=glibtoolize
  else
    LIBTOOLIZE=libtoolize
fi

(cd storage/innobase && aclocal && autoheader && \
    $LIBTOOLIZE --automake --force --copy && \
    automake --force --add-missing --copy && autoconf)

aclocal || die "Can't execute aclocal" 
autoheader || die "Can't execute autoheader"
# --force means overwrite ltmain.sh script if it already exists 
$LIBTOOLIZE --automake --force || die "Can't execute libtoolize"
  
# --add-missing instructs automake to install missing auxiliary files
# and --force to overwrite them if they already exist
automake --add-missing --force || die "Can't execute automake"
autoconf || die "Can't execute autoconf"

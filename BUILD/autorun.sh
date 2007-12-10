#!/bin/sh
# Create MySQL autotools infrastructure

die() { echo "$@"; exit 1; }

# Handle "glibtoolize" (e.g., for native OS X autotools) as another
# name for "libtoolize". Use the first one, either name, found in PATH.
LIBTOOLIZE=libtoolize  # Default
IFS="${IFS=   }"; save_ifs="$IFS"; IFS=':'
for dir in $PATH
do
  if test -x $dir/glibtoolize
  then
    LIBTOOLIZE=glibtoolize
    break
  elif test -x $dir/libtoolize
  then
    break
  fi
done
IFS="$save_ifs"

aclocal || die "Can't execute aclocal" 
autoheader || die "Can't execute autoheader"
# --force means overwrite ltmain.sh script if it already exists 
$LIBTOOLIZE --automake --force --copy || die "Can't execute libtoolize"
  
# --add-missing instructs automake to install missing auxiliary files
# and --force to overwrite them if they already exist
automake --add-missing --force  --copy || die "Can't execute automake"
autoconf || die "Can't execute autoconf"

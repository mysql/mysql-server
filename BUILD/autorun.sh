#!/bin/sh
# Create MySQL autotools infrastructure

aclocal    || (echo "Can't execute aclocal" && exit 1)
autoheader || (echo "Can't execute autoheader" && exit 1)
# --force means overwrite ltmain.sh script if it already exists 
libtoolize --automake --force \
           || (echo "Can't execute libtoolize" && exit 1)
# --add-missing instructs automake to install missing auxiliary files
# and --force to overwrite them if they already exist
automake --add-missing --force \
           || (echo "Can't execute automake" && exit 1)
autoconf   || (echo "Can't execute autoconf" && exit 1)
(cd bdb/dist && sh s_all)
(cd innobase && aclocal && autoheader && aclocal && automake && autoconf)

#!/bin/sh

CHAR="$1"
case "$1" in 
 [uU]*) 
   CHAR=ujis 
   ;; 
 [sS]*) 
   CHAR=sjis 
   ;; 
esac 

cp -r Docs/* PKG/tmp-${CHAR}/usr/local/share/doc/mysql/
cp INSTALL-SOURCE* COPYING* MIRRORS README* PKG/tmp-${CHAR}/usr/local/share/doc/mysql/

cd PKG/tmp-${CHAR}/usr/local/share/doc/mysql/
gzip *.txt *.texi *.info *.pdf

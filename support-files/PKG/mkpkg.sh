#!/bin/sh

if [ "x$1" = "x" ]; then
  echo " please set charset"
  exit
fi

CHAR="$1"
case "$1" in
 [uU]*)
   CHAR=ujis ; CH=uj
   ;;
 [sS]*)
   CHAR=sjis ; CH=sj
   ;;
esac

#-------------------
DIR=`pwd`

VERSION=`cat version`

T=`uname -p`

sed -e "s/@CHAR1@/${CH}/" \
	 -e "s/@CHAR2@/${CHAR}/" \
	-e "s/@VERSION@/${VERSION}/" \
	-e "s/@TYPE@/${T}/" \
	pkginfo.ini > pkginfo.${CHAR}

sed -e "s,@DIR@,${DIR},g" \
	-e "s,@PKGINFO@,${DIR}/pkginfo.${CHAR}," \
	prototype.ini > prototype.${CHAR}

INIT="tmp-${CHAR}/etc/init.d/mysql"
cp ../support-files/mysql.server $INIT
chmod 755 $INIT

(cd tmp-${CHAR}; \
chown root etc usr ; \
chgrp sys etc usr ;\
chmod 755 usr etc; \
chgrp sys etc/init.d ; \
chmod 755 etc/init.d ; \
find . -print|pkgproto >> ../prototype.${CHAR})

pkgmk -o -f prototype.${CHAR} -r ${DIR}/tmp-${CHAR}

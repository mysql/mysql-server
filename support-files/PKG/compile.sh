#!/bin/sh
# GNU ncurses

# search & check GNU make
GMAKE="gmake"
$GMAKE --version || GMAKE="make"
$GMAKE --version || exit

MAKE=$GMAKE
export MAKE

CC=gcc
CFLAGS="-O6 -fomit-frame-pointer"
CXX=gcc
CXXFLAGS="-O6 -fomit-frame-pointer -felide-constructors  -fno-exceptions -fno-rtti"
# LDFLAGS="-static"
LD=gcc

export CC
export CXX
export LD
export CFLAGS
export CXXFLAGS
# export LDFLAGS
# Solaris don't have libpthread.a.

if [ "x$1" = "x" ]; then
  echo " please set character set"
  exit
fi

CHAR="$1"
case "$1" in
 [uU]*)
   CHAR=ujis
   ;;
 [sS]*)
   CHAR=sjis
   ;;
esac
 
#---------------
P=`pwd`

if [ -f Makefile ] ; then
    ${GMAKE} distclean
fi

for i in bin sbin include man share/doc/mysql mysql-data
do
  /usr/bin/mkdir -p PKG/tmp-${CHAR}/usr/local/${i}
done
/usr/bin/mkdir -p PKG/tmp-${CHAR}/etc/init.d

#----------------------------
./configure \
   --prefix=/usr/local \
   --libexecdir=/usr/local/sbin \
   --sbindir=/usr/local/sbin \
   --localstatedir=/usr/local/mysql-data \
   --with-charset=${CHAR} \
   --with-extra-charsets=all \
   --with-raid \
   --without-docs \
   --without-bench \
   --without-perl \
   --with-gcc \
   --with-mysqld-ldflags="-static" \
   --with-client-ldflags="-static" \
   --with-named-curses-libs=/usr/local/lib/libncurses.a \
   --with-mysqld-user=mysql

#   --with-berkeley-db-includes=/usr/local/include/db3 \
#   --with-berkeley-db-libs=/usr/local/lib/libdb3.a \
#   --with-low-memory

${GMAKE}
${GMAKE} install DESTDIR=${P}/PKG/tmp-${CHAR}

v=`grep '^SHARED_LIB_VERSION' configure.in | sed 's@SHARED_LIB_VERSION@@' | sed -e 's@=@@' -e 's@:@ @g' | awk '{print $1}'`
km="libmysqlclient.so.$v"
export km

(cd ${P}/PKG/tmp-${CHAR}/usr/local/lib/mysql/ ; \
  for i in libmysqlclient* ; do \
    if /usr/bin/test ! -L $i ; then \
        mv $i ../ ; ln -sf ../$i ; \
    fi ; \
  done ; \
  k=`ls libmysqlclient.so.*.*.*` ; \
  cd .. ; \
  if /usr/bin/test ! -z libmysqlclient.so ; then \
      ln -sf $k libmysqlclient.so ;
  fi ; \
  if /usr/bin/test ! -z $km ; then \
      ln -sf $k $km ;
  fi ; \
)

#
(cd ${P}/PKG/tmp-${CHAR}/usr/local/bin ; strip * )
(cd ${P}/PKG/tmp-${CHAR}/usr/local/sbin ; strip * )

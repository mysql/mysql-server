#!/bin/sh

# search & check GNU patch
PATCH="gpatch"
$PATCH --version || PATCH="patch"
$PATCH --version || exit


if /usr/bin/test ! -e PKG/stamp-pre ; then
  grep VERSION configure | head -1 | sed 's/VERSION=//' > ./PKG/version
  touch PKG/stamp-pre
fi

if /usr/bin/test ! -e PKG/stamp-patch ; then
 ${PATCH} -p0 < ./PKG/patch
 touch PKG/stamp-patch
fi

if /usr/bin/test ! -e PKG/stamp-compile ; then
sh ./PKG/compile.sh ujis
touch PKG/stamp-compile

sh ./PKG/doc.sh ujis

fi


cd PKG
sh mkpkg.sh ujis

#!/bin/sh -e
#
# This script is for Innobase internal use.
#
# Create InnoDB source archive that is going to be shipped to users.
#
# This constitutes of:
# (It would have been nice if we can archive branches/zip and ship it,
# but we need to create Makefile.in, so it is a bit more complex. Makefile.in
# is required in order not to require users to have autotools to generate it.
# Users should replace storage/innobase and compile mysql as usual -
# ./configure ; make)
#
# 1. Get the MySQL sources
# 2. Replace storage/innobase from the SVN
# 3. Remove files that need not be shipped
# 4. Create storage/innobase/Makefile.in
# 5. Archive storage/innobase
#

MYSQL_BRANCH="5.1"
MYSQL_VER="${MYSQL_BRANCH}.23-rc"

INNODB_VER="1.0.0"

# end of changeable variables

MYSQL_DIR="mysql-${MYSQL_VER}"
MYSQL_ARCHIVE="${MYSQL_DIR}.tar.gz"

INNODB_DIR="innodb_plugin-${INNODB_VER}-${MYSQL_BRANCH}"

INNODB_SVN_REPO="https://svn.innodb.com/svn/innodb"

REMOVE_FILES="
revert_gen.sh
scripts/dynconfig
scripts/export.sh
scripts/make_binary_release.sh
scripts/make_source_release.sh
"
REMOVE_DIRS="scripts"

# get MySQL sources
# pick one mirror from http://dev.mysql.com/downloads/mirrors.html
#wget ftp://ftp.easynet.be/mysql/Downloads/MySQL-${MYSQL_BRANCH}/${MYSQL_ARCHIVE}
wget ftp://mysql.online.bg/mysql/Downloads/MySQL-${MYSQL_BRANCH}/${MYSQL_ARCHIVE}
# bkf clone -rmysql-${MYSQL_VER%-rc} \
# 	bk://mysql.bkbits.net/mysql-${MYSQL_BRANCH} ./mysql-${MYSQL_VER}
tar -zxf ${MYSQL_ARCHIVE}

# get InnoDB sources
cd ${MYSQL_DIR}/storage
mv innobase innobase.orig
svn export ${INNODB_SVN_REPO}/tags/${INNODB_DIR} innobase
# "steal" MySQL's ./COPYING (GPLv2)
cp -v ../COPYING innobase/
# Hack the autotools files so users can compile by using ./configure and
# make without having autotools installed. This has the drawback that if
# users build dynamic (ha_innodb.so) on i386 it will (probably) be compiled
# with PIC and thus it will be slower. If we do not do this, MySQL's configure
# will not parse @INNODB_CFLAGS@ and @INNODB_DYNAMIC_CFLAGS@ when creating
# innobase/Makefile from innobase/Makefile.in and subsequently compilation
# will fail on the user's machine with an error like (if he does not have
# autotools):
# gcc: @INNODB_CFLAGS@: No such file or directory
# In the long run we could inject INNODB_DYNAMIC_CFLAGS="-prefer-non-pic"
# into branches/5.1/plug.in so it will be included in standard MySQL
# distributions.
cp innobase.orig/plug.in innobase/plug.in
sed -i '' \
	-e 's/\$(INNODB_CFLAGS)//g' \
	-e 's/\$(INNODB_DYNAMIC_CFLAGS)/-DMYSQL_DYNAMIC_PLUGIN/g' \
	innobase/Makefile.am

rm -fr innobase.orig

# remove unnecessary files and directories
# avoid rm -fr, too dangerous
cd innobase
rm ${REMOVE_FILES}
rmdir ${REMOVE_DIRS}

# create InnoDB's Makefile.in
cd ../..
./BUILD/autorun.sh

# create archive
cd storage
mv innobase ${INNODB_DIR}
tar -cf - ${INNODB_DIR} |gzip -9 > ../../${INNODB_DIR}.tar.gz

# EOF

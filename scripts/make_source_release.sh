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

INNODB_VER="1.0.a1"

# end of changeable variables

MYSQL_DIR="mysql-${MYSQL_VER}"
MYSQL_ARCHIVE="${MYSQL_DIR}.tar.gz"

INNODB_DIR="innodb-${MYSQL_BRANCH}-${INNODB_VER}"

INNODB_SVN_REPO="https://svn.innodb.com/svn/innodb"

REMOVE_FILES="
revert_gen.sh
scripts/build-plugin.sh
scripts/dynconfig
scripts/export.sh
scripts/make_source_release.sh
"

MD5_CMD="md5"

# get MySQL sources
# pick one mirror from http://dev.mysql.com/downloads/mirrors.html
#wget ftp://ftp.easynet.be/mysql/Downloads/MySQL-${MYSQL_BRANCH}/${MYSQL_ARCHIVE}
wget ftp://mysql.online.bg/mysql/Downloads/MySQL-${MYSQL_BRANCH}/${MYSQL_ARCHIVE}
# bkf clone -rmysql-${MYSQL_VER%-rc} \
# 	bk://mysql.bkbits.net/mysql-${MYSQL_BRANCH} ./mysql-${MYSQL_VER}
tar -zxf ${MYSQL_ARCHIVE}

# get InnoDB sources
cd ${MYSQL_DIR}/storage
rm -fr innobase
# XXX this must export a specific tag, not the latest revision, but
# we do not yet have tags.
svn export ${INNODB_SVN_REPO}/branches/zip innobase
# "steal" MySQL's ./COPYING (GPLv2)
cp -v ../COPYING innobase/

# remove unnecessary files
cd innobase
rm ${REMOVE_FILES}

# create InnoDB's Makefile.in
cd ../..
./BUILD/autorun.sh

# create archives
cd storage
mv innobase ${INNODB_DIR}
tar -cf - ${INNODB_DIR} |gzip -9 > ../../${INNODB_DIR}.tar.gz
tar -cf - ${INNODB_DIR} |bzip2   > ../../${INNODB_DIR}.tar.bz2

cd ../..

${MD5_CMD} ${INNODB_DIR}.tar.gz
${MD5_CMD} ${INNODB_DIR}.tar.bz2

# EOF

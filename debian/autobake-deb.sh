#!/bin/bash

# Build MariaDB .deb packages.
# Based on OurDelta .deb packaging scripts, which are in turn based on Debian
# MySQL packages.

# Exit immediately on any error
set -e

# Debug script and command lines
#set -x

# Don't run the mysql-test-run test suite as part of build.
# It takes a lot of time, and we will do a better test anyway in
# Buildbot, running the test suite from installed .debs on a clean VM.
export DEB_BUILD_OPTIONS="nocheck"

# Find major.minor version.
#
source ./VERSION
UPSTREAM="${MYSQL_VERSION_MAJOR}.${MYSQL_VERSION_MINOR}.${MYSQL_VERSION_PATCH}${MYSQL_VERSION_EXTRA}"
RELEASE_EXTRA=""

RELEASE_NAME=mariadb
PATCHLEVEL=""
LOGSTRING="MariaDB build"

# Look up distro-version specific stuff.
#
# Libreadline changed to GPLv3. Old GPLv2 version is available, but it
# is called different things on different versions.
CODENAME="$(lsb_release -sc)"
case "${CODENAME}" in
  etch)  LIBREADLINE_DEV=libreadline-dev ;;
  lenny|hardy|intrepid|jaunty|karmic|lucid)  LIBREADLINE_DEV='libreadline5-dev | libreadline-dev' ;;
  squeeze|maverick|natty)  LIBREADLINE_DEV=libreadline5-dev ;;
  *)  LIBREADLINE_DEV=libreadline-gplv2-dev ;;
esac

case "${CODENAME}" in
  etch|lenny|hardy|intrepid|jaunty|karmic) CMAKE_DEP='' ;;
  *) CMAKE_DEP='cmake (>= 2.7), ' ;;
esac

# Clean up build file symlinks that are distro-specific. First remove all, then set
# new links.
DISTRODIRS="$(ls ./debian/dist)"
for distrodir in ${DISTRODIRS}; do
  DISTROFILES="$(ls ./debian/dist/${distrodir})"
  for distrofile in ${DISTROFILES}; do
    rm -f "./debian/${distrofile}";
  done;
done;

# Set no symlinks for build files in the debian dir, so we avoid adding AppArmor on Debian.
DISTRO="$(lsb_release -si)"
echo "Copying distribution specific build files for ${DISTRO}"
DISTROFILES="$(ls ./debian/dist/${DISTRO})"
for distrofile in ${DISTROFILES}; do
  rm -f "./debian/${distrofile}"
  sed -e "s/\\\${LIBREADLINE_DEV}/${LIBREADLINE_DEV}/g" \
      -e "s/\\\${CMAKE_DEP}/${CMAKE_DEP}/g"             \
    < "./debian/dist/${DISTRO}/${distrofile}" > "./debian/${distrofile}"
  chmod --reference="./debian/dist/${DISTRO}/${distrofile}" "./debian/${distrofile}"
done;

# Adjust changelog, add new version.
#
echo "Incrementing changelog and starting build scripts"

dch -b -D ${CODENAME} -v "${UPSTREAM}${PATCHLEVEL}-${RELEASE_NAME}${RELEASE_EXTRA:+-${RELEASE_EXTRA}}1~${CODENAME}" "Automatic build with ${LOGSTRING}."

echo "Creating package version ${UPSTREAM}${PATCHLEVEL}-${RELEASE_NAME}${RELEASE_EXTRA:+-${RELEASE_EXTRA}}1~${CODENAME} ... "

# Build the package.
#
fakeroot dpkg-buildpackage -us -uc

echo "Build complete"

# end of autobake script

#!/bin/bash

# Arg 1: Build type: release or debug. Default is release
# Arg 3: What to do. If set "extract_only", then the source code is downloaded and extracted, but the build is not launched.
#       If set to "do_not_build", the build process will go one step further. It will download the conan packages and execute cmake.
#       If not set ot left empty, the build process will execute completely. That's the default.

# Required environmeent variables
# REPO_USER: User name to use to clone repo where MySQL / Sparrow source is stored
# REPO_PSSWD: Password / token to use to clone repo where MySQL / Sparrow source is stored
# REPO_BRANCH: Branch to check out

# if [[ -z "$REPO_USER" || -z "$REPO_PSSWD" ]] ; then
#     echo "Env variable REPO_USER and REPO_PSSWD need to be define and give the credentials to the opensource repository."
#     exit 1
# fi

# if [ -z "$REPO_BRANCH" ] ; then
#     REPO_BRANCH=8.0
# fi
# echo `date +"%x %X"` Building branch $REPO_BRANCH

BUILD_MODE=release
if [ "$1" == "debug" ] ; then
	BUILD_MODE=debug
fi
echo `date +"%x %X"` Build mode is $BUILD_MODE

OPTIONS=$2
# EXTRACT_ONLY=false
# if [ "$OPTIONS" == "extract_only" ] ; then
# 	EXTRACT_ONLY=true
# fi
# echo "EXTRACT_ONLY is $EXTRACT_ONLY"

if [ -z "$CI_COMMIT_TAG" ]; then
    echo "Env variable CI_COMMIT_TAG is empty or not defined. It must be set to a valid tag values, such as 4.2.123 or 4.2.123-spw-287."
    exit 1
fi

SCRIPT_NAME=$(basename "$0")
SCRIPT_DIR=$( cd -- "$( dirname -- "$0" )" &> /dev/null && pwd )
echo `date +"%x %X"`  "$SCRIPT_NAME directory: $SCRIPT_DIR"

# Setting execution rights on scripts
echo `date +"%x %X"`  Set file rights to additional build scripts
chmod +x $SCRIPT_DIR/*.sh  $SCRIPT_DIR/../misc/*.sh  $SCRIPT_DIR/../conan/lnx_64/*.sh  

# Importing helper functions
# source $SCRIPT_DIR/../misc/misc_functions.sh

# Set the source root folder as working folder.
cd $SCRIPT_DIR/../..;
SOURCE_ROOT_FOLDER=`pwd`
echo `date +"%x %X"` "Source root folder is $SOURCE_ROOT_FOLDER"

# Tag must look like 4.2.123 (without patch), 4.2.123-spw-287-b (with patch)
# Full version (i.e. major.minor.build)
FULL_VERSION=`echo ${CI_COMMIT_TAG} | sed -n 's/\([0-9.]*\).*/\1/p'`
PATCH_VERSION=`echo ${CI_COMMIT_TAG} | sed -n 's/[0-9.]*-\(.*\)/\1/p'`

echo "FULL_VERSION is $FULL_VERSION"

SPW_BUILD_VERSION=`echo ${FULL_VERSION} | cut -d '.' -f 1,2`
SPW_BUILD_VERSION_MAJOR=`echo ${FULL_VERSION} | cut -d '.' -f 1`
SPW_BUILD_VERSION_MINOR=`echo ${FULL_VERSION} | cut -d '.' -f 2`
SPW_BUILD_VERSION_BUILD=`echo ${FULL_VERSION} | cut -d '.' -f 3`
SPW_BUILD_VERSION_FULL=${FULL_VERSION}
SPW_BUILD_VERSION_PATCH=${PATCH_VERSION}
echo "SPW_BUILD_VERSION is $SPW_BUILD_VERSION ($SPW_BUILD_VERSION_MAJOR.$SPW_BUILD_VERSION_MINOR.$SPW_BUILD_VERSION_BUILD)"
echo "SPW_BUILD_VERSION_PATCH is $SPW_BUILD_VERSION_PATCH"

export SPW_BUILD_VERSION  SPW_BUILD_VERSION_MAJOR  SPW_BUILD_VERSION_MINOR  SPW_BUILD_VERSION_BUILD  SPW_BUILD_VERSION_FULL  SPW_BUILD_VERSION_PATCH

if [ -z "${SPW_BUILD_VERSION_PATCH}" ]; then
    echo `date +"%x %X"`  Building sparrow $SPW_BUILD_VERSION_FULL, build mode $BUILD_MODE
else
    echo `date +"%x %X"`  Building sparrow $SPW_BUILD_VERSION_FULL, patch $SPW_BUILD_VERSION_PATCH, build mode $BUILD_MODE
fi

# git clone -b $REPO_BRANCH  https://${REPO_USER}:${REPO_PSSWD}@github.com/infovista-opensource/mysql-server-timeseries.git  .

# echo `date +"%x %X"`  Build and distribution folders
# mkdir -p _build
# mkdir -p _distrib

# if [ "$EXTRACT_ONLY" = true ]; then
#     echo "Code has been extracted."
#     exit 0
# fi

echo `date +"%x %X"`  Executing script to compile source code and make packages, $SOURCE_ROOT_FOLDER/build/mysql/compile-package-linux.sh
# $SCRIPT_DIR/compile-package-linux.sh  $SOURCE_ROOT_FOLDER  $CI_COMMIT_TAG  $BUILD_MODE  $OPTIONS
$SCRIPT_DIR/compile-package-linux.sh  $CI_COMMIT_TAG  $BUILD_MODE  $OPTIONS

echo `date +"%x %X"`  Done

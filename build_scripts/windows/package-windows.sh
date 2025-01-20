#!/bin/bash
#Arg 1 : Root directory where the source code is built.
#Arg 2 : mysql tag (version). Example: 5.6.46
#Arg 3 : Sparrow build number. Example: 4.2.123, or 4.2.123-SPW-387
#Arg 4 : debug or release build.
#Arg 5 : Options. If set to "do_not_build", the script will setup the build env, but won't start the build.

echo `date +"%x %X"` "Source directory $1"
echo `date +"%x %X"` "Mysql version $2"
echo `date +"%x %X"` "Build number $3"
echo `date +"%x %X"` "Build mode $4"

SRC_DIR=$1
MYSQL_TAG=$2
SPARROW_BUILD_NUM=$3
BUILD_MODE=$4
OPTIONS=$5

SCRIPT_NAME=$(basename "$0")
SCRIPT_DIR=$( cd -- "$( dirname -- "$0" )" &> /dev/null && pwd )
echo "$SCRIPT_NAME directory: $SCRIPT_DIR"

BUILD_SCRIPTS_FOLDER=$SCRIPT_DIR/../..

if [ -z "$SRC_DIR" ]; then
    SRC_DIR="$SCRIPT_DIR/../../../mysql_src"
    echo `date +"%x %X"` "Missing source directory. Using $SRC_DIR as default."
fi

if [ -z "$MYSQL_TAG" ]; then
    MYSQL_TAG="5.7.26"
    echo `date +"%x %X"` "Missing mysql tag. No default. Aborting."
    exit 1
fi

if [ -z "$SPARROW_BUILD_NUM" ]; then
    echo `date +"%x %X"` "Missing Sparrow build number. No default. Aborting."
    exit 1
fi

if [ -z "$BUILD_MODE" ]; then
    BUILD_MODE="release"
    echo `date +"%x %X"` "Missing build mode. Using $BUILD_MODE as default."
fi

if [ -n "$OPTIONS" ]; then
	echo `date +"%x %X"` "Build options $OPTIONS"
fi

DO_NOT_BUILD=false
if [ "$OPTIONS" == "do_not_build" ] ; then
	DO_NOT_BUILD=true
fi
echo "DO_NOT_BUILD is $DO_NOT_BUILD"

generate() {
	# First delete any previous zip file left behind
	rm -f $2 $3 $4 > /dev/null 2>&1
	
	pushd $1
	rm -rf share/Makefile* share/*.sql share/*.txt
	zip -r -9 $2 bin share
	popd
	pushd storage/sparrow/udf
	rm -rf lib
	mkdir lib
	mkdir lib/plugin
	cp libsparrowudf.so lib/plugin/
	zip -r -9 $2 lib
	rm -rf lib
	popd
	
	pushd ../../storage/sparrow/api
	zip -r -9 $3 include
	popd
	pushd storage/sparrow
	zip -r -9 $3 libsparrowapi.so
	popd
	
	pushd $1
	zip -r -9 $4 include
	popd
	pushd $1/lib
	zip -r -9 $4 libmysqlclient*.*
	popd
}

generate_distrib_pack() {
	# First delete any previous zip file left behind in the _distrib folder
	rm -f $3/$4 > /dev/null 2>&1

	pushd $2
	rm -r _distrib_tmp

	bin_folder=_distrib_tmp/mysql_${BUILD_MODE}
	mkdir -p $bin_folder

	cp -r share bin $bin_folder

	mkdir $bin_folder/lib
	cp lib/libmysqlclient.a  lib/libmysqlclient.so  $bin_folder/lib

	mkdir -p $bin_folder/lib/plugin
	cp  $1/storage/sparrow/udf/libsparrowudf.so  $bin_folder/lib/plugin

#	pushd $1
	data_folder=_distrib_tmp/mysql_data
	mkdir -p $data_folder
	cp -r data/mysql  data/performance_schema  $data_folder

	sparrow_folder=_distrib_tmp/sparrowapi_${BUILD_MODE}
	mkdir -p $sparrow_folder
#	popd
	cp lib/libsparrowapi.so  $sparrow_folder
	# cp -r ../../storage/sparrow/api/include  $1/$sparrow_folder
#	pushd $1

	cd _distrib_tmp
	zip -r -3  $3/$4  *
	cd ..

	popd
}


generate_mysqlapi_conan_pack() {
	pushd $1

	rm  -Rf  _conan/mysqlapi
	mkdir -p  _conan/mysqlapi
	cd _conan/mysqlapi

	cp  $BUILD_SCRIPTS_FOLDER/conan/lnx_64/profile.txt  profile.txt
	cp  $BUILD_SCRIPTS_FOLDER/conan/conanfile.mysqlapi.py  conanfile.py
	sed -E "s/version[ \t]*=[ \t]*\".*\"/version = \"$SPARROW_BUILD_NUM\"/" conanfile.py > conanfile.new.py
	mv -f conanfile.new.py conanfile.py
	rm -f conanfile.new.py

	if [ $BUILD_MODE = "debug"  ]; then
		sed -E "s/BUILD_MODE[ \t]*=[ \t]*.*/BUILD_MODE=Debug/" profile.txt > profile.new.txt
	else
		sed -E "s/BUILD_MODE[ \t]*=[ \t]*.*/BUILD_MODE=Release/" profile.txt > profile.new.txt
	fi
	mv -f profile.new.txt  profile.txt
	rm -f profile.new.txt

	mkdir lib include
	cp -r ../../include/*  include
	cp ../../lib/libmysqlclient.so  ../../lib/libmysqlclient.a  lib

	echo `date +"%x %X"` "Exporting the mysqlapi conan package." 
	conan export-pkg  .  mysqlapi/${SPARROW_BUILD_NUM}@ativanet-poller/stable  -pr profile.txt  --force

	echo `date +"%x %X"` "Uploading the mysqlapi conan package to JFrog." 
	conan upload  -r jfrog  mysqlapi/${SPARROW_BUILD_NUM}@ativanet-poller/stable  --all  --no-overwrite recipe 

	popd
}


generate_sparrowapi_conan_pack() {
	pushd $1

	rm  -Rf  _conan/sparrowapi
	mkdir -p _conan/sparrowapi
	cd _conan/sparrowapi

	cp  $BUILD_SCRIPTS_FOLDER/conan/lnx_64/profile.txt  profile.txt
	cp  $BUILD_SCRIPTS_FOLDER/conan/conanfile.sparrowapi.py  conanfile.py
	sed -E "s/version[ \t]*=[ \t]*\".*\"/version = \"$SPARROW_BUILD_NUM\"/" conanfile.py > conanfile.new.py
	mv -f conanfile.new.py conanfile.py
	rm -f conanfile.new.py

	if [ $BUILD_MODE = "debug"  ]; then
		sed -E "s/BUILD_MODE[ \t]*=[ \t]*.*/BUILD_MODE=Debug/" profile.txt > profile.new.txt
	else
		sed -E "s/BUILD_MODE[ \t]*=[ \t]*.*/BUILD_MODE=Release/" profile.txt > profile.new.txt
	fi
	mv -f profile.new.txt  profile.txt
	rm -f profile.new.txt

	mkdir lib include
	cp  ../../lib/libsparrowapi.so  lib
	cp  -r $SRC_DIR/storage/sparrow/api/include/* include

	echo `date +"%x %X"` "Exporting the sparrowapi conan package." 
	conan export-pkg  .  sparrowapi/${SPARROW_BUILD_NUM}@ativanet-poller/stable  -pr profile.txt  --force

	echo `date +"%x %X"` "Uploading the sparrowapi conan package to JFrog." 
	conan upload  -r jfrog  sparrowapi/${SPARROW_BUILD_NUM}@ativanet-poller/stable  --all  --no-overwrite recipe

	popd
}

# ---------------- STARTING HERE ---------------------
# env | sort;

build_dir=$SRC_DIR/_build/win64
distrib_dir=$SRC_DIR/_distrib/win64
build_dir_arch=$build_dir/$BUILD_MODE



# This has been deprecated and removed in MySQL 8.0. See https://dev.mysql.com/doc/relnotes/mysql/8.0/en/news-8-0-0.html#:~:text=The%20deprecated%20mysql_install_db
# Generate initial database.
# echo `date +"%x %X"` "Generating initial database"
# pushd _CPack_Packages/Linux/TGZ/mysql-$MYSQL_TAG-linux-x86_64
# scripts/mysql_install_db --datadir=./data
# popd
# pushd _CPack_Packages/Linux/TGZ/mysql-$MYSQL_TAG-linux-x86_64/data	
# rm -rf test

#	echo `date +"%x %X"` "Zipping initial database files from `pwd` into $distrib_dir/mysql_data.zip"
#	rm -f $distrib_dir/mysql_data.zip
#	zip -r -9 $distrib_dir/mysql_data.zip *
#	popd
		
#	echo `date +"%x %X"` "Zipping distribution packages, format for ivserver 6.1."
#	generate  _CPack_Packages/Linux/TGZ/mysql-$MYSQL_TAG-linux-x86_64 $distrib_dir/mysql_${BUILD_MODE}.zip $distrib_dir/sparrowapi_${BUILD_MODE}.zip $distrib_dir/mysqlapi_${BUILD_MODE}.zip

echo `date +"%x %X"` "Zipping distribution package containing binaries and configuration files to be deployed."
generate_distrib_pack  $build_dir_arch  $build_dir_arch/_CPack_Packages/Linux/TGZ/mysql-$MYSQL_TAG-linux-x86_64  $distrib_dir  sparrow-$SPARROW_BUILD_NUM-x64-${BUILD_MODE}.zip
if [ $? -ne 0 ]; then
	echo Failed to generate distribution package.
	exit 1
fi

echo `date +"%x %X"` "Uploading distribution package $distrib_dir/sparrow-$SPARROW_BUILD_NUM.zip to JFrog generic package repository."
# curl --header "Authorization: Bearer $CI_JOB_TOKEN" --upload-file $distrib_dir/sparrow-$SPARROW_BUILD_NUM-x64-${BUILD_MODE}.zip  $CI_API_V4_URL/projects/$CI_PROJECT_ID/packages/generic/sparrow/$SPARROW_BUILD_NUM/sparrow-$SPARROW_BUILD_NUM-x64-${BUILD_MODE}.zip?select=package_file
curl -u ${RELEASE_GENERIC_USER}:${RELEASE_GENERIC_PASSWORD}  --upload-file $distrib_dir/sparrow-$SPARROW_BUILD_NUM-x64-${BUILD_MODE}.zip  ${RELEASE_GENERIC_REPO}/sparrow/$SPARROW_BUILD_NUM/sparrow-$SPARROW_BUILD_NUM-x64-${BUILD_MODE}.zip
if [ $? -ne 0 ]; then
	echo Failed to upload distribution package.
	exit 1
fi

echo `date +"%x %X"` "Packaging the mysql api for conan"
generate_mysqlapi_conan_pack  $build_dir_arch/_CPack_Packages/Linux/TGZ/mysql-$MYSQL_TAG-linux-x86_64
if [ $? -ne 0 ]; then
	echo Failed to make or upload conan package for the mysql api.
	exit 1
fi

echo `date +"%x %X"` "Packaging the sparrow api for conan"
generate_sparrowapi_conan_pack  $build_dir_arch/_CPack_Packages/Linux/TGZ/mysql-$MYSQL_TAG-linux-x86_64
if [ $? -ne 0 ]; then
	echo Failed to make or upload conan package for the sparrow api.
	exit 1
fi

# echo `date +"%x %X"` Cleaning up
# rm -rf _CPack_Packages
# make clean

echo `date +"%x %X"` $BUILD_MODE build finished

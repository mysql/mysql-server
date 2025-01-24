#!/bin/bash
#Arg 1 : Sparrow build number. Example: 4.2.123, or 4.2.123-SPW-387
#Arg 2 : debug or release build.
#Arg 3 : Options. If set to "do_not_build", the script will setup the build env, but won't start the build.
#		 If set to "do_not_pack", the script will setup the build env, compiles everything but does not generate the packages.


# ---------------- Checking argument ---------------------
#  and initializing some global variables

SPARROW_BUILD_NUM=$1
echo `date +"%x %X"` "Build number $SPARROW_BUILD_NUM"

BUILD_MODE=$2
if [ -z "$BUILD_MODE" ] ; then
	BUILD_MODE=release
fi
echo `date +"%x %X"` "Build mode $2"

OPTIONS=$3
if [ -n "$OPTIONS" ]; then
	echo `date +"%x %X"` "Build options $OPTIONS"
fi

# Used only for dev purposes
DO_NOT_BUILD=false
if [ "$OPTIONS" == "do_not_build" ] ; then
	DO_NOT_BUILD=true
fi
echo "DO_NOT_BUILD is $DO_NOT_BUILD"

# Used only for dev purposes
DO_NOT_PACK=false
if [ "$OPTIONS" == "do_not_pack" ] ; then
	DO_NOT_PACK=true
fi
echo "DO_NOT_PACK is $DO_NOT_PACK"

SCRIPT_NAME=$(basename "$0")
SCRIPT_DIR=$( cd -- "$( dirname -- "$0" )" &> /dev/null && pwd )
echo `date +"%x %X"`  "$SCRIPT_NAME directory: $SCRIPT_DIR"

cd $SCRIPT_DIR/../..;
SOURCE_ROOT_FOLDER=`pwd`
echo `date +"%x %X"` "Source root folder is $SOURCE_ROOT_FOLDER"


# Make a package containing everything: the mysql server files and tools, the libmysqlclient API and the sparrow API.
# This generic package will then be used to create the docker images of the dbsrv and poller runtime. 

generate_distrib_pack() {

	echo `date +"%x %X"` "Packaging all binaries and dependencies into a single package $4." 

	# First delete any previous zip file left behind in the _distrib folder
	rm -f $3/$4 > /dev/null 2>&1

	pushd $2
	rm -rf _distrib_tmp > /dev/null 2>&1

	echo `date +"%x %X"` "Gathering all required files for a DB server installation." 
	distrib_folder=_distrib_tmp/mysql_${BUILD_MODE}
	mkdir -p $distrib_folder
	cp -ra share bin lib $distrib_folder

	echo `date +"%x %X"` "Gathering files for libmysqlclient API." 
	cp -a lib/libmysqlclient.so*  $distrib_folder/lib

	echo `date +"%x %X"` "Gathering files for Sparrow UDF plugin." 
	mkdir -p $distrib_folder/lib/plugin
	cp  $1/storage/sparrow/udf/libsparrowudf.so  $distrib_folder/lib/plugin

	echo `date +"%x %X"` "Packaging everything into the compressed file $3/$4." 
	cd $distrib_folder
	tar -czvf  $3/$4.tar.gz  *
	res=$?
	if [ $? -ne 0 ]; then
		echo `date +"%x %X"` "Tar gzip all files into a package failed." 
		return $res
	fi

	popd
}

generate_mysqlapi_pack() {

	echo `date +"%x %X"` "Packaging MySQL API library and headers, version $SPARROW_BUILD_NUM, $BUILD_MODE, into a single package $4." 

	# First delete any previous zip file left behind in the _distrib folder
	rm -f $3/$4 > /dev/null 2>&1

	pushd $2

	echo `date +"%x %X"` "Gathering all required files for the MySQL client API." 
	distrib_folder=_distrib_tmp/mysqlapi_${BUILD_MODE}
	rm -rf $distrib_folder > /dev/null 2>&1
	mkdir -p $distrib_folder
	cd $distrib_folder

	mkdir lib include
	cp -r ../../include/*  include
	cp -a ../../lib/libmysqlclient.so*  ../../lib/libmysqlclient.a  lib

	echo `date +"%x %X"` "Packaging everything into the compressed file $3/$4." 
	tar -czvf  $3/$4.tar.gz  *
	res=$?
	if [ $? -ne 0 ]; then
		echo `date +"%x %X"` "Tar gzip all files into a package failed." 
		return $res
	fi

	popd
}


generate_sparrowapi_pack() {

	echo `date +"%x %X"` "Packaging Sparrow API library and headers, version $SPARROW_BUILD_NUM, $BUILD_MODE, into a single package $4." 

	# First delete any previous zip file left behind in the _distrib folder
	rm -f $3/$4 > /dev/null 2>&1

	pushd $2

	echo `date +"%x %X"` "Gathering all required files for the MySQL client API." 
	distrib_folder=_distrib_tmp/sparrowapi_${BUILD_MODE}
	rm -rf $distrib_folder > /dev/null 2>&1
	mkdir -p $distrib_folder
	cd $distrib_folder

	mkdir lib include
	cp  -r $SOURCE_ROOT_FOLDER/storage/sparrow/api/include/* include
	cp  -a ../../lib/libsparrowapi.so*  lib

	echo `date +"%x %X"` "Packaging everything into the compressed file $3/$4." 
	tar -czvf  $3/$4.tar.gz  *
	res=$?
	if [ $? -ne 0 ]; then
		echo `date +"%x %X"` "Tar gzip all files into a package failed." 
		return $res
	fi

	popd
}



# Packages the libmysqlclient API into a conan package and uploads it to the conan repository on jfrog.
generate_mysqlapi_conan_pack() {
	pushd $1

	echo `date +"%x %X"` "Packaging the MySQL libmysqlclient API into a conan package, version $SPARROW_BUILD_NUM, $BUILD_MODE." 
	rm  -Rf  _conan/mysqlapi
	mkdir -p  _conan/mysqlapi
	cd _conan/mysqlapi

	cp  $SOURCE_ROOT_FOLDER/conan/lnx_64/profile.txt  profile.txt
	cp  $SOURCE_ROOT_FOLDER/conan/conanfile.mysqlapi.py  conanfile.py
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
	conan export-pkg  .  mysqlapi/${SPARROW_BUILD_NUM}@ativanet-poller/stable  -pr profile.txt  --force  -s compiler.version="$GCC_VERSION"
	res=$?
	if [ $? -ne 0 ]; then
		echo `date +"%x %X"` "conan export-pkg failed with error code $res." 
		return $res
	fi

	echo `date +"%x %X"` "Uploading the mysqlapi conan package to JFrog." 
	conan upload  -r jfrog  mysqlapi/${SPARROW_BUILD_NUM}@ativanet-poller/stable  --all  --no-overwrite recipe 
	res=$?
	if [ $? -ne 0 ]; then
		echo `date +"%x %X"` "conan upload failed with error code $res." 
		return $res
	fi

	popd
}

# Packages the Sparrow API into a conan package and uploads it to the conan repository on jfrog.

generate_sparrowapi_conan_pack() {
	pushd $1

	echo `date +"%x %X"` "Packaging the Sparrow API into a conan package, version $SPARROW_BUILD_NUM, $BUILD_MODE." 
	rm  -Rf  _conan/sparrowapi
	mkdir -p _conan/sparrowapi
	cd _conan/sparrowapi

	cp  $SOURCE_ROOT_FOLDER/conan/lnx_64/profile.txt  profile.txt
	cp  $SOURCE_ROOT_FOLDER/conan/conanfile.sparrowapi.py  conanfile.py
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
	cp  -r $SOURCE_ROOT_FOLDER/storage/sparrow/api/include/* include

	echo `date +"%x %X"` "Exporting the sparrowapi conan package." 
	conan export-pkg  .  sparrowapi/${SPARROW_BUILD_NUM}@ativanet-poller/stable  -pr profile.txt  --force  -s compiler.version="$GCC_VERSION"
	res=$?
	if [ $? -ne 0 ]; then
		echo `date +"%x %X"` "conan export-pkg failed with error code $res." 
		return $res
	fi

	echo `date +"%x %X"` "Uploading the sparrowapi conan package to JFrog." 
	conan upload  -r jfrog  sparrowapi/${SPARROW_BUILD_NUM}@ativanet-poller/stable  --all  --no-overwrite recipe
	res=$?
	if [ $? -ne 0 ]; then
		echo `date +"%x %X"` "conan upload failed with error code $res." 
		return $res
	fi

	popd
}

# ---------------- Script actually starts here  ---------------------

# env | sort;

REDHAT_VERSION=`sed -e 's/.*release \([0-9]*\).*/\1/' /etc/redhat-release`
echo `date +"%x %X"` "Running on RedHat version $REDHAT_VERSION"

GCC_VERSION=`gcc --version | head -n1 | sed -e 's/.*(GCC) \([0-9].[0-9]*\).*/\1/'`
echo `date +"%x %X"` "gcc version is $GCC_VERSION"


# Execute the conan script to get the openssl third party lib
echo `date +"%x %X"` "Executing conan script"
cd $SOURCE_ROOT_FOLDER/build_scripts/conan/lnx_64

. ./conan_download_pckgs.sh

if [ -z "$SSLDIR" ]; then
	echo `date +"%x %X"` Missing SSLDIR
	exit 1
fi

if [ -z "$BOOSTDIR" ]; then
	# Checks the MySQL source code includes the boost library it requires.
	# If so, set the BOOSTDIR accordingly
	cd $SOURCE_ROOT_FOLDER
	if [ ! -d boost ]; then
		echo `date +"%x %X"` Missing boost library.
		exit 1
	fi

	cd boost
	BOOSTDIR_VER=`ls`
	BOOSTDIR=$SOURCE_ROOT_FOLDER/boost/$BOOSTDIR_VER
else
	BOOSTDIR=$BOOSTDIR/include
fi
echo `date +"%x %X"` Boost dir is $BOOSTDIR

# Prepare the build folder which will contain the CMake resulting files and the compilation files
#  and prepare the distrib build folder which will the subset of files we package and distribute.
export LD_LIBRARY_PATH=/usr/local/lib64:/usr/local/lib:$LD_LIBRARY_PATH

build_dir=$SOURCE_ROOT_FOLDER/_build/lnx${REDHAT_VERSION}_64
mkdir -p  $build_dir

distrib_dir=$SOURCE_ROOT_FOLDER/_distrib/lnx${REDHAT_VERSION}_64
mkdir -p  $distrib_dir

cd $build_dir

echo `date +"%x %X"` "Creating $BUILD_MODE sub-dir in _build. Removing previous $BUILD_MODE sub-dir if it existed."
rm -Rf  $BUILD_MODE
mkdir  $BUILD_MODE
cd  $BUILD_MODE
build_dir_arch=$build_dir/$BUILD_MODE

# Build source code. Try to build and embed only the required modules. So remove from build all module that are not needed.
echo `date +"%x %X"` "Starting $BUILD_MODE build"
CMAKE_OPTIONS="-DWITH_UNIT_TESTS=0 -DWITHOUT_GROUP_REPLICATION=1 -DWITHOUT_HEAP_STORAGE_ENGINE=1 -DWITHOUT_CSV_STORAGE_ENGINE=1 -DWITHOUT_ARCHIVE_STORAGE_ENGINE=1 -DWITHOUT_BLACKHOLE_STORAGE_ENGINE=1 -DWITHOUT_EXAMPLE_STORAGE_ENGINE=1 -DWITHOUT_FEDERATED_STORAGE_ENGINE=1 -DBUILD_CONFIG=mysql_${BUILD_MODE} -DWITH_SSL=$SSLDIR -DWITH_BOOST=$BOOSTDIR"
echo "CMAKE_OPTIONS is " $CMAKE_OPTIONS

if [ $BUILD_MODE = "debug"  ]; then
	cmake ../../.. $CMAKE_OPTIONS -DCOMPILATION_COMMENT="build: $SPARROW_BUILD_NUM" -DCMAKE_BUILD_MODE=Debug  
else
	cmake ../../.. $CMAKE_OPTIONS -DCOMPILATION_COMMENT="build: $SPARROW_BUILD_NUM" -DCMAKE_BUILD_MODE=RelWithDebInfo 
fi
res=$?
if [ $res -ne 0 ]; then
	echo `date +"%x %X"` "Cmake for $BUILD_MODE build failed." 
	exit $res
fi

if [ "$DO_NOT_BUILD" = true ]; then
    echo `date +"%x %X"` "Build setup and CMake are done."
    exit 0
fi

# Compile everything
echo `date +"%x %X"` "Compiling source code..."
make package
echo `date +"%x %X"` "Compiling source code finished."

export PACKAGE_DIR=`ls -l $build_dir_arch/_CPack_Packages/Linux/TGZ | grep mysql- | head -n1 | awk '{print $NF}'`

export MYSQL_TAG=`echo $PACKAGE_DIR | sed -e 's/mysql-\([0-9.]*\)-.*/\1/'`
echo `date +"%x %X"` "MySQL tag is $MYSQL_TAG"

if [ "$DO_NOT_PACK" = true ]; then
    echo `date +"%x %X"` "Source compilation is done. Packaging is skipped."
    exit 0
fi

echo `date +"%x %X"` "Generating the distribution package which contains the binaries and configuration files to be deployed."
generate_distrib_pack  $build_dir_arch  $build_dir_arch/_CPack_Packages/Linux/TGZ/mysql-$MYSQL_TAG-linux-x86_64  $distrib_dir  sparrow-distrib-$SPARROW_BUILD_NUM-x64-${BUILD_MODE}
if [ $? -ne 0 ]; then
	echo Failed to generate distribution package.
	exit 1
fi

echo `date +"%x %X"` "Generating the MySQL API package which includes the header files and the library."
generate_mysqlapi_pack  $build_dir_arch  $build_dir_arch/_CPack_Packages/Linux/TGZ/mysql-$MYSQL_TAG-linux-x86_64  $distrib_dir  mysqlapi-$SPARROW_BUILD_NUM-x64-${BUILD_MODE}
if [ $? -ne 0 ]; then
	echo Failed to generate MySQL API package.
	exit 1
fi

echo `date +"%x %X"` "Generating the Sparrow API package which includes the header files and the library."
generate_sparrowapi_pack  $build_dir_arch  $build_dir_arch/_CPack_Packages/Linux/TGZ/mysql-$MYSQL_TAG-linux-x86_64  $distrib_dir  sparrowapi-$SPARROW_BUILD_NUM-x64-${BUILD_MODE}
if [ $? -ne 0 ]; then
	echo Failed to generate Sparrow API package.
	exit 1
fi

# This needs to be changed to point to sourceforge or something
# echo `date +"%x %X"` "Uploading distribution package $distrib_dir/sparrow-$SPARROW_BUILD_NUM.zip to JFrog generic package repository."
# # curl --header "Authorization: Bearer $CI_JOB_TOKEN" --upload-file $distrib_dir/sparrow-$SPARROW_BUILD_NUM-x64-${BUILD_MODE}.zip  $CI_API_V4_URL/projects/$CI_PROJECT_ID/packages/generic/sparrow/$SPARROW_BUILD_NUM/sparrow-$SPARROW_BUILD_NUM-x64-${BUILD_MODE}.zip?select=package_file
# curl -u ${RELEASE_GENERIC_USER}:${RELEASE_GENERIC_PASSWORD}  --upload-file $distrib_dir/sparrow-$SPARROW_BUILD_NUM-x64-${BUILD_MODE}.zip  ${RELEASE_GENERIC_REPO}/sparrow/$SPARROW_BUILD_NUM/sparrow-$SPARROW_BUILD_NUM-x64-${BUILD_MODE}.zip
# if [ $? -ne 0 ]; then
# 	echo Failed to upload distribution package.
# 	exit 1
# fi

# It will probably not work to upload packages to our private conan repo from infovista-opensource. Packahes will have to be uploaded to sourceforge or something and then
#  downloaded from there in the Net Poller's docker pre-build scripts. 
# echo `date +"%x %X"` "Packaging the mysql api for conan"
# generate_mysqlapi_conan_pack  $build_dir_arch/_CPack_Packages/Linux/TGZ/mysql-$MYSQL_TAG-linux-x86_64
# if [ $? -ne 0 ]; then
# 	echo Failed to make or upload conan package for the mysql api.
# 	exit 1
# fi

# echo `date +"%x %X"` "Packaging the sparrow api for conan"
# generate_sparrowapi_conan_pack  $build_dir_arch/_CPack_Packages/Linux/TGZ/mysql-$MYSQL_TAG-linux-x86_64
# if [ $? -ne 0 ]; then
# 	echo Failed to make or upload conan package for the sparrow api.
# 	exit 1
# fi

# echo `date +"%x %X"` Cleaning up
# rm -rf _CPack_Packages
# make clean

echo `date +"%x %X"` $BUILD_MODE build finished

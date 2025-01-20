#!/bin/bash
#Arg 1 : Build directory. Example: C:\temp\builds\mysql-4.2\5.6.46-42010-SPW-370.
#Arg 2 : mysql tag (version). Example: 5.6.46
#Arg 3 : Sparrow build number. Example: 42010-SPW-370
#Arg 4 : win64 or win32.

echo "Source directory $1"
echo "Mysql version $2"
echo "Build number $3"
echo "Build type $4"

generate() {
	# First delete any previous zip file left behind
	rm -f $2 $3 $4
	
	pushd $1
	rm -rf share/Makefile* share/*.sql share/*.txt
	zip -r -9 $2 bin share
	popd
	pushd storage/sparrow/udf/$5
	rm -rf lib
	mkdir lib
	mkdir lib/plugin
	cp sparrowudf.dll lib/plugin/
	cp sparrowudf.lib lib/plugin/
	cp sparrowudf.pdb lib/plugin/
	zip -r -9 $2 lib
	rm -rf lib
	popd
	
	pushd ../../storage/sparrow/api
	zip -r -9 $3 include
	popd
	pushd storage/sparrow/$5
	zip -r -9 $3 sparrowapi.dll sparrowapi.lib sparrowapi.pdb
	popd
	
	pushd $1
	zip -r -9 $4 include
	popd
	pushd libmysql/$5
	zip -r -9 $4 libmysql.dll libmysql.lib libmysql.pdb
	popd
}

build_dir=$1/_build
cd $build_dir


if [ $4 = "win32"  ]; then

	# Create win32 build dir, where cmake will output all its files and where the build is going to take place. Example: C:\temp\builds\mysql-4.2\5.6.46-42010-SPW-370\_build\win32
	echo `date` "Creating win32 sub-dir of _build. Removing previous win32 sub-dir if it existed."
	rm -rf win32
	mkdir win32
	cd win32
	
	echo `date` "Starting win32 cmake."
	cmake ../.. -DCOMPILATION_COMMENT="build: $3" -G "Visual Studio 16 2019" -A x86 -DWITH_EMBEDDED_SERVER=0 -DWITHOUT_BLACKHOLE_STORAGE_ENGINE=1 -DWITHOUT_EXAMPLE_STORAGE_ENGINE=1 -DWITHOUT_FEDERATED_STORAGE_ENGINE=1 -DWITH_SSL=T:\\core\\22.2\\openssl\\1.1.1s\\win32
	res=$?
	if [ $res -ne 0 ]; then
		echo `date` "Win32 cmake of source code failed." 
		exit $res
	fi
	
	echo `date` "Starting win32 debug build"
	devenv mysql.sln /build Debug /project ALL_BUILD /out build_win32_debug.log
	res=$?
	if [ $res -ne 0 ]; then
		echo `date` "Debug build of source code failed." 
		exit $res
	fi
	
	echo `date` "Generating debug package"
	devenv mysql.sln /build Debug /project package /out build_win32_debug.log
	res=$?
	if [ $res -ne 0 ]; then
		echo `date` "Packaging failed."
		exit $res
	fi
	
	echo `date` "Packaging done. Zipping debug distribution packages"
	mkdir -p $1/_distrib/win32
	distrib_dir=$1/_distrib/win32
	generate _CPack_Packages/win32/ZIP/mysql-$2-winx32 $distrib_dir/mysql_debug.zip $distrib_dir/sparrowapi_debug.zip $distrib_dir/mysqlapi_debug.zip Debug 2>&1
	

	echo `date` "Starting win32 release build"
	devenv mysql.sln /build RelWithDebInfo /project ALL_BUILD /out build_win32_release.log
	res=$?
	if [ $res -ne 0 ]; then
		echo `date` "Release build of source code failed." 
		exit $res
	fi
	
	echo `date` "Generating release package"
	devenv mysql.sln /build RelWithDebInfo /project package /out build_win32_release.log
	res=$?
	if [ $res -ne 0 ]; then
		echo `date` "Packaging failed."
		exit $res
	fi
	
	echo `date` "Packaging done. Zipping debug distribution packages"
	generate _CPack_Packages/win32/ZIP/mysql-$2-winx32 $distrib_dir/mysql_release.zip $distrib_dir/sparrowapi_release.zip $distrib_dir/mysqlapi_release.zip RelWithDebInfo 2>&1


	echo `date` "Generating initial database"
	devenv mysql.sln /build RelWithDebInfo /project initial_database /out build_win32_release.log
	res=$?
	if [ $res -ne 0 ]; then
		echo `date` "Initial database build failed." 
		exit $res
	fi
	
	echo `date` "Cleaning up debug build"
	rm -rf _CPack_Packages
	devenv mysql.sln /clean Debug /out build_win32_debug.log
	
	echo `date` "Cleaning up release build"
	rm -rf _CPack_Packages
	devenv mysql.sln /clean RelWithDebInfo /out build_win32_release.log
	
	echo `date` "Win32 build finished"
	
else

	# Create win64 build dir, where cmake will output all its files and where the build is going to take place. Example: R:\Sparrow\4.0\5.5.27-40061\_build\win64
	echo `date` "Creating win64 sub-dir of _build. Removing previous win64 sub-dir if it existed."
	rm -rf win64
	mkdir win64
	cd win64
	
	echo `date` "Starting win64 cmake."
	cmake ../.. -DCOMPILATION_COMMENT="build: $3" -G "Visual Studio 16 2019" -A x64 -DWITH_EMBEDDED_SERVER=0 -DWITHOUT_BLACKHOLE_STORAGE_ENGINE=1 -DWITHOUT_EXAMPLE_STORAGE_ENGINE=1 -DWITHOUT_FEDERATED_STORAGE_ENGINE=1 -DWITH_SSL=T:\\core\\22.2\\openssl\\1.1.1s\\win64
	res=$?
	if [ $res -ne 0 ]; then
		echo `date` "Win64 cmake of source code failed." 
		exit $res
	fi
	
	echo `date` "Starting win64 debug build"
	devenv mysql.sln /build Debug /project ALL_BUILD /out build_win64_debug.log
	res=$?
	if [ $res -ne 0 ]; then
		echo `date` "Debug build of source code failed." 
		exit $res
	fi
	
	echo `date` "Generating debug package"
	devenv mysql.sln /build Debug /project package /out build_win64_debug.log
	res=$?
	if [ $res -ne 0 ]; then
		echo `date` "Packaging failed."
		exit $res
	fi
	
	echo `date` "Packaging done. Zipping debug distribution packages"
	mkdir -p $1/_distrib/win64
	distrib_dir=$1/_distrib/win64
	generate _CPack_Packages/win64/ZIP/mysql-$2-winx64 $distrib_dir/mysql_debug.zip $distrib_dir/sparrowapi_debug.zip $distrib_dir/mysqlapi_debug.zip Debug 2>&1
	

	echo `date` "Starting win64 release build"
	devenv mysql.sln /build RelWithDebInfo /project ALL_BUILD /out build_win64_release.log
	res=$?
	if [ $res -ne 0 ]; then
		echo `date` "Release build of source code failed." 
		exit $res
	fi
	
	echo `date` "Generating release package"
	devenv mysql.sln /build RelWithDebInfo /project package /out build_win64_release.log
	res=$?
	if [ $res -ne 0 ]; then
		echo `date` "Packaging failed."
		exit $res
	fi
	
	echo `date` "Packaging done. Zipping debug distribution packages"
	generate _CPack_Packages/win64/ZIP/mysql-$2-winx64 $distrib_dir/mysql_release.zip $distrib_dir/sparrowapi_release.zip $distrib_dir/mysqlapi_release.zip RelWithDebInfo 2>&1


	echo `date` "Generating initial database"
	devenv mysql.sln /build RelWithDebInfo /project initial_database /out build_win64_release.log
	res=$?
	if [ $res -ne 0 ]; then
		echo `date` "Initial database build failed." 
		exit $res
	fi

	#echo `date` "Cleaning up debug build"
	#rm -rf _CPack_Packages
	#devenv mysql.sln /clean Debug /out build_win64_debug.log
	
	#echo `date` "Cleaning up release build"
	#rm -rf _CPack_Packages
	#devenv mysql.sln /clean RelWithDebInfo /out build_win64_release.log
	
	echo `date` "Win64 build finished"
fi

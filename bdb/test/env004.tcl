# See the file LICENSE for redistribution information.
#
# Copyright (c) 1996, 1997, 1998, 1999, 2000
#	Sleepycat Software.  All rights reserved.
#
#	$Id: env004.tcl,v 11.14 2000/08/25 14:21:50 sue Exp $
#
# Env Test 4
# Test multiple data directories.  Do a bunch of different opens
# to make sure that the files are detected in different directories.
proc env004 { } {
	source ./include.tcl

	set method "hash"
	set omethod [convert_method $method]
	set args [convert_args $method ""]

	puts "Env004: Multiple data directory test."

	env_cleanup $testdir
	file mkdir $testdir/data1
	file mkdir $testdir/data2
	file mkdir $testdir/data3

	puts "\tEnv004.a: Multiple data directories in DB_CONFIG file"

	# Create a config file
	set cid [open $testdir/DB_CONFIG w]
	puts $cid "set_data_dir ."
	puts $cid "set_data_dir data1"
	puts $cid "set_data_dir data2"
	puts $cid "set_data_dir data3"
	close $cid

	# Now get pathnames
	set curdir [pwd]
	cd $testdir
	set fulldir [pwd]
	cd $curdir

	set e [berkdb env -create -private -home $testdir]
	error_check_good dbenv [is_valid_env $e] TRUE
	ddir_test $fulldir $method $e $args
	error_check_good env_close [$e close] 0

	puts "\tEnv004.b: Multiple data directories in berkdb env call."
	env_cleanup $testdir
	file mkdir $testdir/data1
	file mkdir $testdir/data2
	file mkdir $testdir/data3

	# Now call dbenv with config specified
	set e [berkdb env -create -private \
	    -data_dir . -data_dir data1 -data_dir data2 \
	    -data_dir data3 -home $testdir]
	error_check_good dbenv [is_valid_env $e] TRUE
	ddir_test $fulldir $method $e $args
	error_check_good env_close [$e close] 0

	env_cleanup $testdir
}

proc ddir_test { fulldir method e args } {
	source ./include.tcl

	set args [convert_args $args]
	set omethod [convert_method $method]

	# Now create one file in each directory
	set db1 [eval {berkdb_open -create \
	    -truncate -mode 0644 $omethod -env $e} $args {data1/datafile1.db}]
	error_check_good dbopen1 [is_valid_db $db1] TRUE

	set db2 [eval {berkdb_open -create \
	    -truncate -mode 0644 $omethod -env $e} $args {data2/datafile2.db}]
	error_check_good dbopen2 [is_valid_db $db2] TRUE

	set db3 [eval {berkdb_open -create \
	    -truncate -mode 0644 $omethod -env $e} $args {data3/datafile3.db}]
	error_check_good dbopen3 [is_valid_db $db3] TRUE

	# Close the files
	error_check_good db_close1 [$db1 close] 0
	error_check_good db_close2 [$db2 close] 0
	error_check_good db_close3 [$db3 close] 0

	# Now, reopen the files without complete pathnames and make
	# sure that we find them.

	set db1 [berkdb_open -env $e $fulldir/data1/datafile1.db]
	error_check_good dbopen1 [is_valid_db $db1] TRUE

	set db2 [berkdb_open -env $e $fulldir/data2/datafile2.db]
	error_check_good dbopen2 [is_valid_db $db2] TRUE

	set db3 [berkdb_open -env $e $fulldir/data3/datafile3.db]
	error_check_good dbopen3 [is_valid_db $db3] TRUE

	# Finally close all the files
	error_check_good db_close1 [$db1 close] 0
	error_check_good db_close2 [$db2 close] 0
	error_check_good db_close3 [$db3 close] 0
}

# See the file LICENSE for redistribution information.
#
# Copyright (c) 2000
#	Sleepycat Software.  All rights reserved.
#
#	$Id: sdb009.tcl,v 11.4 2000/08/25 14:21:53 sue Exp $
#
# Subdatabase Test 9 (replacement)
# Test the DB->rename method.
proc subdb009 { method args } {
	global errorCode
	source ./include.tcl
	set omethod [convert_method $method]
	set args [convert_args $method $args]

	puts "Subdb009: $method ($args): Test of DB->rename()"

	if { [is_queue $method] == 1 } {
		puts "\tSubdb009: Skipping for method $method."
		return
	}

	set file $testdir/subdb009.db
	set oldsdb OLDDB
	set newsdb NEWDB

	# Make sure we're starting from a clean slate.
	cleanup $testdir NULL
	error_check_bad "$file exists" [file exists $file] 1

	puts "\tSubdb009.a: Create/rename file"
	puts "\t\tSubdb009.a.1: create"
	set db [eval {berkdb_open -create -mode 0644}\
	    $omethod $args $file $oldsdb]
	error_check_good dbopen [is_valid_db $db] TRUE

	# The nature of the key and data are unimportant; use numeric key
	# so record-based methods don't need special treatment.
	set key 1
	set data [pad_data $method data]

	error_check_good dbput [$db put $key $data] 0
	error_check_good dbclose [$db close] 0

	puts "\t\tSubdb009.a.2: rename"
	error_check_good rename_file [eval {berkdb dbrename} $file \
	    $oldsdb $newsdb] 0

	puts "\t\tSubdb009.a.3: check"
	# Open again with create to make sure we've really completely
	# disassociated the subdb from the old name.
	set odb [eval {berkdb_open -create -mode 0644}\
	    $omethod $args $file $oldsdb]
	error_check_good odb_open [is_valid_db $odb] TRUE
	set odbt [$odb get $key]
	error_check_good odb_close [$odb close] 0

	set ndb [eval {berkdb_open -create -mode 0644}\
	    $omethod $args $file $newsdb]
	error_check_good ndb_open [is_valid_db $ndb] TRUE
	set ndbt [$ndb get $key]
	error_check_good ndb_close [$ndb close] 0

	# The DBT from the "old" database should be empty, not the "new" one.
	error_check_good odbt_empty [llength $odbt] 0
	error_check_bad ndbt_empty [llength $ndbt] 0
	error_check_good ndbt [lindex [lindex $ndbt 0] 1] $data

	# Now there's both an old and a new.  Rename the "new" to the "old"
	# and make sure that fails.
	puts "\tSubdb009.b: Make sure rename fails instead of overwriting"
	set ret [catch {eval {berkdb dbrename} $file $oldsdb $newsdb} res]
	error_check_bad rename_overwrite $ret 0
	error_check_good rename_overwrite_ret [is_substr $errorCode EEXIST] 1

	puts "\tSubdb009 succeeded."
}

# See the file LICENSE for redistribution information.
#
# Copyright (c) 1996-2002
#	Sleepycat Software.  All rights reserved.
#
# $Id: test089.tcl,v 11.2 2002/08/08 15:38:12 bostic Exp $
#
# TEST	test089
# TEST	Concurrent Data Store test (CDB)
# TEST
# TEST	Enhanced CDB testing to test off-page dups, cursor dups and
# TEST	cursor operations like c_del then c_get.
proc test089 { method {nentries 1000} args } {
	global datastr
	global encrypt
	source ./include.tcl

	#
	# If we are using an env, then skip this test.  It needs its own.
	set eindex [lsearch -exact $args "-env"]
	if { $eindex != -1 } {
		incr eindex
		set env [lindex $args $eindex]
		puts "Test089 skipping for env $env"
		return
	}
	set encargs ""
	set args [convert_args $method $args]
	set oargs [split_encargs $args encargs]
	set omethod [convert_method $method]

	puts "Test089: ($oargs) $method CDB Test cursor/dup operations"

	# Process arguments
	# Create the database and open the dictionary
	set testfile test089.db
	set testfile1 test089a.db

	env_cleanup $testdir

	set env [eval {berkdb_env -create -cdb} $encargs -home $testdir]
	error_check_good dbenv [is_valid_env $env] TRUE

	set db [eval {berkdb_open -env $env -create \
	    -mode 0644 $omethod} $oargs {$testfile}]
	error_check_good dbopen [is_valid_db $db] TRUE

	set db1 [eval {berkdb_open -env $env -create \
	    -mode 0644 $omethod} $oargs {$testfile1}]
	error_check_good dbopen [is_valid_db $db1] TRUE

	set pflags ""
	set gflags ""
	set txn ""
	set count 0

	# Here is the loop where we put each key/data pair
	puts "\tTest089.a: put loop"
	set did [open $dict]
	while { [gets $did str] != -1 && $count < $nentries } {
		if { [is_record_based $method] == 1 } {
			set key [expr $count + 1]
		} else {
			set key $str
		}
		set ret [eval {$db put} \
		    $txn $pflags {$key [chop_data $method $datastr]}]
		error_check_good put:$db $ret 0
		set ret [eval {$db1 put} \
		    $txn $pflags {$key [chop_data $method $datastr]}]
		error_check_good put:$db1 $ret 0
		incr count
	}
	close $did
	error_check_good close:$db [$db close] 0
	error_check_good close:$db1 [$db1 close] 0

	# Database is created, now set up environment

	# Remove old mpools and Open/create the lock and mpool regions
	error_check_good env:close:$env [$env close] 0
	set ret [eval {berkdb envremove} $encargs -home $testdir]
	error_check_good env_remove $ret 0

	set env [eval {berkdb_env_noerr -create -cdb} $encargs -home $testdir]
	error_check_good dbenv [is_valid_widget $env env] TRUE

	# This tests the failure found in #1923
	puts "\tTest089.b: test delete then get"

	set db1 [eval {berkdb_open_noerr -env $env -create \
	    -mode 0644 $omethod} $oargs {$testfile1}]
	error_check_good dbopen [is_valid_db $db1] TRUE

	set dbc [$db1 cursor -update]
	error_check_good dbcursor [is_valid_cursor $dbc $db1] TRUE

	for {set kd [$dbc get -first] } { [llength $kd] != 0 } \
	    {set kd [$dbc get -next] } {
		error_check_good dbcdel [$dbc del] 0
	}
	error_check_good dbc_close [$dbc close] 0

	puts "\tTest089.c: CDB cursor dups"
	set dbc [$db1 cursor -update]
	error_check_good dbcursor [is_valid_cursor $dbc $db1] TRUE
	set stat [catch {$dbc dup} ret]
	error_check_bad wr_cdup_stat $stat 0
	error_check_good wr_cdup [is_substr $ret \
	    "Cannot duplicate writeable cursor"] 1

	set dbc_ro [$db1 cursor]
	error_check_good dbcursor [is_valid_cursor $dbc_ro $db1] TRUE
	set dup_dbc [$dbc_ro dup]
	error_check_good rd_cdup [is_valid_cursor $dup_dbc $db1] TRUE

	error_check_good dbc_close [$dbc close] 0
	error_check_good dbc_close [$dbc_ro close] 0
	error_check_good dbc_close [$dup_dbc close] 0
	error_check_good db_close [$db1 close] 0
	error_check_good env_close [$env close] 0

	if { [is_btree $method] != 1 } {
		puts "Skipping rest of test089 for $method method."
		return
	}
	set pgindex [lsearch -exact $args "-pagesize"]
	if { $pgindex != -1 } {
		puts "Skipping rest of test089 for specific pagesizes"
		return
	}
	append oargs " -dup "
	test089_dup $testdir $encargs $oargs $omethod $nentries
	append oargs " -dupsort "
	test089_dup $testdir $encargs $oargs $omethod $nentries
}

proc test089_dup { testdir encargs oargs method nentries } {

	env_cleanup $testdir
	set env [eval {berkdb_env -create -cdb} $encargs -home $testdir]
	error_check_good dbenv [is_valid_env $env] TRUE

	#
	# Set pagesize small to generate lots of off-page dups
	#
	set page 512
	set nkeys 5
	set data "data"
	set key "test089_key"
	set testfile test089.db
	puts "\tTest089.d: CDB ($oargs) off-page dups"
	set oflags "-env $env -create -mode 0644 $oargs $method"
	set db [eval {berkdb_open} -pagesize $page $oflags $testfile]
	error_check_good dbopen [is_valid_db $db] TRUE

	puts "\tTest089.e: Fill page with $nkeys keys, with $nentries dups"
	for { set k 0 } { $k < $nkeys } { incr k } {
		for { set i 0 } { $i < $nentries } { incr i } {
			set ret [$db put $key $i$data$k]
			error_check_good dbput $ret 0
		}
	}

	# Verify we have off-page duplicates
	set stat [$db stat]
	error_check_bad stat:offpage [is_substr $stat "{{Internal pages} 0}"] 1

	set dbc [$db cursor -update]
	error_check_good dbcursor [is_valid_cursor $dbc $db] TRUE

	puts "\tTest089.f: test delete then get of off-page dups"
	for {set kd [$dbc get -first] } { [llength $kd] != 0 } \
	    {set kd [$dbc get -next] } {
		error_check_good dbcdel [$dbc del] 0
	}
	error_check_good dbc_close [$dbc close] 0
	error_check_good db_close [$db close] 0
	error_check_good env_close [$env close] 0
}

# See the file LICENSE for redistribution information.
#
# Copyright (c) 1999-2002
#	Sleepycat Software.  All rights reserved.
#
# $Id: recd016.tcl,v 11.8 2002/09/05 17:23:07 sandstro Exp $
#
# TEST	recd016
# TEST	This is a recovery test for testing running recovery while
# TEST	recovery is already running.  While bad things may or may not
# TEST	happen, if recovery is then run properly, things should be correct.
proc recd016 { method args } {
	source ./include.tcl

	set args [convert_args $method $args]
	set omethod [convert_method $method]

	puts "Recd016: $method ($args) simultaneous recovery test"
	puts "Recd016: Skipping; waiting on SR #6277" 
	return

	# Create the database and environment.
	set testfile recd016.db

	#
	# For this test we create our database ahead of time so that we
	# don't need to send methods and args to the script.
	#
	cleanup $testdir NULL

	#
	# Use a smaller log to make more files and slow down recovery.
	#
	set gflags ""
	set pflags ""
	set log_max [expr 256 * 1024]
	set nentries 10000
	set nrec 6
	set t1 $testdir/t1
	set t2 $testdir/t2
	set t3 $testdir/t3
	set t4 $testdir/t4
	set t5 $testdir/t5
	# Since we are using txns, we need at least 1 lock per
	# record (for queue).  So set lock_max accordingly.
	set lkmax [expr $nentries * 2]

	puts "\tRecd016.a: Create environment and database"
	set env_cmd "berkdb_env -create -log_max $log_max \
	    -lock_max $lkmax -txn -home $testdir"
	set env [eval $env_cmd]
	error_check_good dbenv [is_valid_env $env] TRUE
	set db [eval {berkdb_open -create} \
	    $omethod -auto_commit -env $env $args $testfile]
	error_check_good dbopen [is_valid_db $db] TRUE
	set did [open $dict]
	set abid [open $t4 w]

	if { [is_record_based $method] == 1 } {
		set checkfunc recd016_recno.check
		append gflags " -recno"
	} else {
		set checkfunc recd016.check
	}
	puts "\tRecd016.b: put/get loop"
	# Here is the loop where we put and get each key/data pair
	set count 0
	while { [gets $did str] != -1 && $count < $nentries } {
		if { [is_record_based $method] == 1 } {
			global kvals

			set key [expr $count + 1]
			if { 0xffffffff > 0 && $key > 0xffffffff } {
				set key [expr $key - 0x100000000]
			}
			if { $key == 0 || $key - 0xffffffff == 1 } {
				incr key
				incr count
			}
			set kvals($key) [pad_data $method $str]
		} else {
			set key $str
			set str [reverse $str]
		}
		#
		# Start a transaction.  Alternately abort and commit them.
		# This will create a bigger log for recovery to collide.
		#
		set txn [$env txn]
		set ret [eval \
		    {$db put} -txn $txn $pflags {$key [chop_data $method $str]}]
		error_check_good put $ret 0

		if {[expr $count % 2] == 0} {
			set ret [$txn commit]
			error_check_good txn_commit $ret 0
			set ret [eval {$db get} $gflags {$key}]
			error_check_good commit_get \
			    $ret [list [list $key [pad_data $method $str]]]
		} else {
			set ret [$txn abort]
			error_check_good txn_abort $ret 0
			set ret [eval {$db get} $gflags {$key}]
			error_check_good abort_get [llength $ret] 0
			puts $abid $key
		}
		incr count
	}
	close $did
	close $abid
	error_check_good dbclose [$db close] 0
	error_check_good envclose [$env close] 0

	set pidlist {}
	puts "\tRecd016.c: Start up $nrec recovery processes at once"
	for {set i 0} {$i < $nrec} {incr i} {
		set p [exec $util_path/db_recover -h $testdir -c &]
		lappend pidlist $p
	}
	watch_procs $pidlist 5
	#
	# Now that they are all done run recovery correctly
	puts "\tRecd016.d: Run recovery process"
	set stat [catch {exec $util_path/db_recover -h $testdir -c} result]
	if { $stat == 1 } {
		error "FAIL: Recovery error: $result."
	}

	puts "\tRecd016.e: Open, dump and check database"
	# Now compare the keys to see if they match the dictionary (or ints)
	if { [is_record_based $method] == 1 } {
		set oid [open $t2 w]
		for {set i 1} {$i <= $nentries} {incr i} {
			set j $i
			if { 0xffffffff > 0 && $j > 0xffffffff } {
				set j [expr $j - 0x100000000]
			}
			if { $j == 0 } {
				incr i
				incr j
			}
			puts $oid $j
		}
		close $oid
	} else {
		set q q
		filehead $nentries $dict $t2
	}
	filesort $t2 $t3
	file rename -force $t3 $t2
	filesort $t4 $t3
	file rename -force $t3 $t4
	fileextract $t2 $t4 $t3
	file rename -force $t3 $t5

	set env [eval $env_cmd]
	error_check_good dbenv [is_valid_env $env] TRUE

	open_and_dump_file $testfile $env $t1 $checkfunc \
	    dump_file_direction "-first" "-next"
	filesort $t1 $t3
	error_check_good envclose [$env close] 0

	error_check_good Recd016:diff($t5,$t3) \
	    [filecmp $t5 $t3] 0

	set stat [catch {exec $util_path/db_printlog -h $testdir \
	    > $testdir/LOG } ret]
	error_check_good db_printlog $stat 0
	fileremove $testdir/LOG
}

# Check function for recd016; keys and data are identical
proc recd016.check { key data } {
	error_check_good "key/data mismatch" $data [reverse $key]
}

proc recd016_recno.check { key data } {
	global kvals

	error_check_good key"$key"_exists [info exists kvals($key)] 1
	error_check_good "key/data mismatch, key $key" $data $kvals($key)
}

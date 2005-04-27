# See the file LICENSE for redistribution information.
#
# Copyright (c) 1996-2002
#	Sleepycat Software.  All rights reserved.
#
# $Id: test094.tcl,v 11.16 2002/06/20 19:01:02 sue Exp $
#
# TEST	test094
# TEST	Test using set_dup_compare.
# TEST
# TEST	Use the first 10,000 entries from the dictionary.
# TEST	Insert each with self as key and data; retrieve each.
# TEST	After all are entered, retrieve all; compare output to original.
# TEST	Close file, reopen, do retrieve and re-verify.
proc test094 { method {nentries 10000} {ndups 10} {tnum "94"} args} {
	source ./include.tcl
	global errorInfo

	set dbargs [convert_args $method $args]
	set omethod [convert_method $method]

	if { [is_btree $method] != 1 && [is_hash $method] != 1 } {
		puts "Test0$tnum: skipping for method $method."
		return
	}

	set txnenv 0
	set eindex [lsearch -exact $dbargs "-env"]
	# Create the database and open the dictionary
	#
	# If we are using an env, then testfile should just be the db name.
	# Otherwise it is the test directory and the name.
	if { $eindex == -1 } {
		set testfile $testdir/test0$tnum-a.db
		set env NULL
	} else {
		set testfile test0$tnum-a.db
		incr eindex
		set env [lindex $dbargs $eindex]
		set rpcenv [is_rpcenv $env]
		if { $rpcenv == 1 } {
			puts "Test0$tnum: skipping for RPC"
			return
		}
		set txnenv [is_txnenv $env]
		if { $txnenv == 1 } {
			append dbargs " -auto_commit "
			if { $nentries == 10000 } {
				set nentries 100
			}
			reduce_dups nentries ndups
		}
		set testdir [get_home $env]
	}
	puts "Test0$tnum: $method ($args) $nentries \
	    with $ndups dups using dupcompare"

	cleanup $testdir $env

	set db [eval {berkdb_open_noerr -dupcompare test094_cmp \
	    -dup -dupsort -create -mode 0644} $omethod $dbargs {$testfile}]
	error_check_good dbopen [is_valid_db $db] TRUE

	set did [open $dict]
	set t1 $testdir/t1
	set pflags ""
	set gflags ""
	set txn ""
	puts "\tTest0$tnum.a: $nentries put/get duplicates loop"
	# Here is the loop where we put and get each key/data pair
	set count 0
	set dlist {}
	for {set i 0} {$i < $ndups} {incr i} {
		set dlist [linsert $dlist 0 $i]
	}
	while { [gets $did str] != -1 && $count < $nentries } {
		set key $str
		for {set i 0} {$i < $ndups} {incr i} {
			set data $i:$str
			if { $txnenv == 1 } {
				set t [$env txn]
				error_check_good txn [is_valid_txn $t $env] TRUE
				set txn "-txn $t"
			}
			set ret [eval {$db put} \
			    $txn $pflags {$key [chop_data $omethod $data]}]
			error_check_good put $ret 0
			if { $txnenv == 1 } {
				error_check_good txn [$t commit] 0
			}
		}

		set ret [eval {$db get} $gflags {$key}]
		error_check_good get [llength $ret] $ndups
		incr count
	}
	close $did
	# Now we will get each key from the DB and compare the results
	# to the original.
	puts "\tTest0$tnum.b: traverse checking duplicates before close"
	if { $txnenv == 1 } {
		set t [$env txn]
		error_check_good txn [is_valid_txn $t $env] TRUE
		set txn "-txn $t"
	}
	dup_check $db $txn $t1 $dlist
	if { $txnenv == 1 } {
		error_check_good txn [$t commit] 0
	}
	error_check_good db_close [$db close] 0

	# Set up second testfile so truncate flag is not needed.
	# If we are using an env, then testfile should just be the db name.
	# Otherwise it is the test directory and the name.
	if { $eindex == -1 } {
		set testfile $testdir/test0$tnum-b.db
		set env NULL
	} else {
		set testfile test0$tnum-b.db
		set env [lindex $dbargs $eindex]
		set testdir [get_home $env]
	}
	cleanup $testdir $env

	#
	# Test dupcompare with data items big enough to force offpage dups.
	#
	puts "\tTest0$tnum.c: big key put/get dup loop key=filename data=filecontents"
	set db [eval {berkdb_open -dupcompare test094_cmp -dup -dupsort \
	     -create -mode 0644} $omethod $dbargs $testfile]
	error_check_good dbopen [is_valid_db $db] TRUE

	# Here is the loop where we put and get each key/data pair
	set file_list [get_file_list 1]
	if { [llength $file_list] > $nentries } {
		set file_list [lrange $file_list 1 $nentries]
	}

	set count 0
	foreach f $file_list {
		set fid [open $f r]
		fconfigure $fid -translation binary
		set cont [read $fid]
		close $fid

		set key $f
		for {set i 0} {$i < $ndups} {incr i} {
			set data $i:$cont
			if { $txnenv == 1 } {
				set t [$env txn]
				error_check_good txn [is_valid_txn $t $env] TRUE
				set txn "-txn $t"
			}
			set ret [eval {$db put} \
			    $txn $pflags {$key [chop_data $omethod $data]}]
			error_check_good put $ret 0
			if { $txnenv == 1 } {
				error_check_good txn [$t commit] 0
			}
		}

		set ret [eval {$db get} $gflags {$key}]
		error_check_good get [llength $ret] $ndups
		incr count
	}

	puts "\tTest0$tnum.d: traverse checking duplicates before close"
	if { $txnenv == 1 } {
		set t [$env txn]
		error_check_good txn [is_valid_txn $t $env] TRUE
		set txn "-txn $t"
	}
	dup_file_check $db $txn $t1 $dlist
	if { $txnenv == 1 } {
		error_check_good txn [$t commit] 0
		set testdir [get_home $env]
	}
	error_check_good db_close [$db close] 0

	# Clean up the test directory, since there's currently
	# no way to specify a dup_compare function to berkdb dbverify
	# and without one it will fail.
	cleanup $testdir $env
}

# Simple dup comparison.
proc test094_cmp { a b } {
	return [string compare $b $a]
}

# Check if each key appears exactly [llength dlist] times in the file with
# the duplicate tags matching those that appear in dlist.
proc test094_dup_big { db txn tmpfile dlist {extra 0}} {
	source ./include.tcl

	set outf [open $tmpfile w]
	# Now we will get each key from the DB and dump to outfile
	set c [eval {$db cursor} $txn]
	set lastkey ""
	set done 0
	while { $done != 1} {
		foreach did $dlist {
			set rec [$c get "-next"]
			if { [string length $rec] == 0 } {
				set done 1
				break
			}
			set key [lindex [lindex $rec 0] 0]
			set fulldata [lindex [lindex $rec 0] 1]
			set id [id_of $fulldata]
			set d [data_of $fulldata]
			if { [string compare $key $lastkey] != 0 && \
			    $id != [lindex $dlist 0] } {
				set e [lindex $dlist 0]
				error "FAIL: \tKey \
				    $key, expected dup id $e, got $id"
			}
			error_check_good dupget.data $d $key
			error_check_good dupget.id $id $did
			set lastkey $key
		}
		#
		# Some tests add an extra dup (like overflow entries)
		# Check id if it exists.
		if { $extra != 0} {
			set okey $key
			set rec [$c get "-next"]
			if { [string length $rec] != 0 } {
				set key [lindex [lindex $rec 0] 0]
				#
				# If this key has no extras, go back for
				# next iteration.
				if { [string compare $key $lastkey] != 0 } {
					set key $okey
					set rec [$c get "-prev"]
				} else {
					set fulldata [lindex [lindex $rec 0] 1]
					set id [id_of $fulldata]
					set d [data_of $fulldata]
					error_check_bad dupget.data1 $d $key
					error_check_good dupget.id1 $id $extra
				}
			}
		}
		if { $done != 1 } {
			puts $outf $key
		}
	}
	close $outf
	error_check_good curs_close [$c close] 0
}

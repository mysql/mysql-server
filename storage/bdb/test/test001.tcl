# See the file LICENSE for redistribution information.
#
# Copyright (c) 1996-2002
#	Sleepycat Software.  All rights reserved.
#
# $Id: test001.tcl,v 11.28 2002/08/08 15:38:11 bostic Exp $
#
# TEST	test001
# TEST	Small keys/data
# TEST		Put/get per key
# TEST		Dump file
# TEST		Close, reopen
# TEST		Dump file
# TEST
# TEST	Use the first 10,000 entries from the dictionary.
# TEST	Insert each with self as key and data; retrieve each.
# TEST	After all are entered, retrieve all; compare output to original.
# TEST	Close file, reopen, do retrieve and re-verify.
proc test001 { method {nentries 10000} {start 0} {tnum "01"} {noclean 0} args } {
	source ./include.tcl

	set args [convert_args $method $args]
	set omethod [convert_method $method]

	# Create the database and open the dictionary
	set eindex [lsearch -exact $args "-env"]
	#
	# If we are using an env, then testfile should just be the db name.
	# Otherwise it is the test directory and the name.
	# If we are not using an external env, then test setting
	# the database cache size and using multiple caches.
	set txnenv 0
	if { $eindex == -1 } {
		set testfile $testdir/test0$tnum.db
		append args " -cachesize {0 1048576 3} "
		set env NULL
	} else {
		set testfile test0$tnum.db
		incr eindex
		set env [lindex $args $eindex]
		set txnenv [is_txnenv $env]
		if { $txnenv == 1 } {
			append args " -auto_commit "
			#
			# If we are using txns and running with the
			# default, set the default down a bit.
			#
			if { $nentries == 10000 } {
				set nentries 100
			}
		}
		set testdir [get_home $env]
	}
	puts "Test0$tnum: $method ($args) $nentries equal key/data pairs"
	if { $start != 0 } {
		# Sadly enough, we are using start in two different ways.
		# In test090, it is used to test really big records numbers
		# in queue.  In replication, it is used to be able to run
		# different iterations of this test using different key/data
		# pairs.  We try to hide all that magic here.
		puts "\tStarting at $start"

		if { $tnum != 90 } {
			set did [open $dict]
			for { set nlines 0 } { [gets $did str] != -1 } \
			    { incr nlines} {
			}
			close $did
			if { $start + $nentries > $nlines } {
				set start [expr $nlines - $nentries]
			}
		}
	}

	set t1 $testdir/t1
	set t2 $testdir/t2
	set t3 $testdir/t3
	if { $noclean == 0 } {
		cleanup $testdir $env
	}
	set db [eval {berkdb_open \
	     -create -mode 0644} $args $omethod $testfile]
	error_check_good dbopen [is_valid_db $db] TRUE
	set did [open $dict]

	set pflags ""
	set gflags ""
	set txn ""

	if { [is_record_based $method] == 1 } {
		set checkfunc test001_recno.check
		append gflags " -recno"
	} else {
		set checkfunc test001.check
	}
	puts "\tTest0$tnum.a: put/get loop"
	# Here is the loop where we put and get each key/data pair
	set count 0
	if { $start != 0 && $tnum != 90 } {
		# Skip over "start" entries
		for { set count 0 } { $count < $start } { incr count } {
			gets $did str
		}
		set count 0
	}
	while { [gets $did str] != -1 && $count < $nentries } {
		if { [is_record_based $method] == 1 } {
			global kvals

			set key [expr $count + 1 + $start]
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
		if { $txnenv == 1 } {
			set t [$env txn]
			error_check_good txn [is_valid_txn $t $env] TRUE
			set txn "-txn $t"
		}
		set ret [eval \
		    {$db put} $txn $pflags {$key [chop_data $method $str]}]
		error_check_good put $ret 0
		if { $txnenv == 1 } {
			error_check_good txn [$t commit] 0
			if { $count % 50 == 0 } {
				error_check_good txn_checkpoint($count) \
				    [$env txn_checkpoint] 0
			}
		}

		set ret [eval {$db get} $gflags {$key}]
		error_check_good \
		    get $ret [list [list $key [pad_data $method $str]]]

		# Test DB_GET_BOTH for success
		set ret [$db get -get_both $key [pad_data $method $str]]
		error_check_good \
		    getboth $ret [list [list $key [pad_data $method $str]]]

		# Test DB_GET_BOTH for failure
		set ret [$db get -get_both $key [pad_data $method BAD$str]]
		error_check_good getbothBAD [llength $ret] 0

		incr count
	}
	close $did
	if { $txnenv == 1 } {
		set t [$env txn]
		error_check_good txn [is_valid_txn $t $env] TRUE
		set txn "-txn $t"
	}
	# Now we will get each key from the DB and compare the results
	# to the original.
	puts "\tTest0$tnum.b: dump file"
	dump_file $db $txn $t1 $checkfunc
	#
	# dump_file should just have been "get" calls, so
	# aborting a get should really be a no-op.  Abort
	# just for the fun of it.
	if { $txnenv == 1 } {
		error_check_good txn [$t abort] 0
	}
	error_check_good db_close [$db close] 0

	# Now compare the keys to see if they match the dictionary (or ints)
	if { [is_record_based $method] == 1 } {
		set oid [open $t2 w]
		# If this is test 90, we're checking wrap and we really
		# only added nentries number of items starting at start.
		# However, if this isn't 90, then we started at start and
		# added an addition nentries number of items.
		if { $tnum == 90 } {
			for {set i 1} {$i <= $nentries} {incr i} {
				set j [expr $i + $start]
				if { 0xffffffff > 0 && $j > 0xffffffff } {
					set j [expr $j - 0x100000000]
				}
				if { $j == 0 } {
					incr i
					incr j
				}
				puts $oid $j
			}
		} else {
			for { set i 1 } { $i <= $nentries + $start } {incr i} {
				puts $oid $i
			}
		}
		close $oid
	} else {
		set q q
		# We assume that when this is used with start != 0, the
		# test database accumulates data
		filehead [expr $nentries + $start] $dict $t2
	}
	filesort $t2 $t3
	file rename -force $t3 $t2
	filesort $t1 $t3

	error_check_good Test0$tnum:diff($t3,$t2) \
	    [filecmp $t3 $t2] 0

	puts "\tTest0$tnum.c: close, open, and dump file"
	# Now, reopen the file and run the last test again.
	open_and_dump_file $testfile $env $t1 $checkfunc \
	    dump_file_direction "-first" "-next"
	if { [string compare $omethod "-recno"] != 0 } {
		filesort $t1 $t3
	}

	error_check_good Test0$tnum:diff($t2,$t3) \
	    [filecmp $t2 $t3] 0

	# Now, reopen the file and run the last test again in the
	# reverse direction.
	puts "\tTest0$tnum.d: close, open, and dump file in reverse direction"
	open_and_dump_file $testfile $env $t1 $checkfunc \
	    dump_file_direction "-last" "-prev"

	if { [string compare $omethod "-recno"] != 0 } {
		filesort $t1 $t3
	}

	error_check_good Test0$tnum:diff($t3,$t2) \
	    [filecmp $t3 $t2] 0
}

# Check function for test001; keys and data are identical
proc test001.check { key data } {
	error_check_good "key/data mismatch" $data [reverse $key]
}

proc test001_recno.check { key data } {
	global dict
	global kvals

	error_check_good key"$key"_exists [info exists kvals($key)] 1
	error_check_good "key/data mismatch, key $key" $data $kvals($key)
}

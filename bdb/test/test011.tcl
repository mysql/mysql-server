# See the file LICENSE for redistribution information.
#
# Copyright (c) 1996, 1997, 1998, 1999, 2000
#	Sleepycat Software.  All rights reserved.
#
#	$Id: test011.tcl,v 11.20 2000/08/25 14:21:54 sue Exp $
#
# DB Test 11 {access method}
# Use the first 10,000 entries from the dictionary.
# Insert each with self as key and data; add duplicate
# records for each.
# Then do some key_first/key_last add_before, add_after operations.
# This does not work for recno
# To test if dups work when they fall off the main page, run this with
# a very tiny page size.
proc test011 { method {nentries 10000} {ndups 5} {tnum 11} args } {
	global dlist
	global rand_init
	source ./include.tcl

	set dlist ""

	if { [is_rbtree $method] == 1 } {
		puts "Test0$tnum skipping for method $method"
		return
	}
	if { [is_record_based $method] == 1 } {
		test011_recno $method $nentries $tnum $args
		return
	} else {
		puts -nonewline "Test0$tnum: $method $nentries small dup "
		puts "key/data pairs, cursor ops"
	}
	if {$ndups < 5} {
		set ndups 5
	}

	set args [convert_args $method $args]
	set omethod [convert_method $method]

	berkdb srand $rand_init

	# Create the database and open the dictionary
	set eindex [lsearch -exact $args "-env"]
	#
	# If we are using an env, then testfile should just be the db name.
	# Otherwise it is the test directory and the name.
	if { $eindex == -1 } {
		set testfile $testdir/test0$tnum.db
		set env NULL
	} else {
		set testfile test0$tnum.db
		incr eindex
		set env [lindex $args $eindex]
	}
	set t1 $testdir/t1
	set t2 $testdir/t2
	set t3 $testdir/t3
	cleanup $testdir $env

	set db [eval {berkdb_open -create -truncate \
	    -mode 0644} [concat $args "-dup"] {$omethod $testfile}]
	error_check_good dbopen [is_valid_db $db] TRUE

	set did [open $dict]

	set pflags ""
	set gflags ""
	set txn ""
	set count 0

	# Here is the loop where we put and get each key/data pair
	# We will add dups with values 1, 3, ... $ndups.  Then we'll add
	# 0 and $ndups+1 using keyfirst/keylast.  We'll add 2 and 4 using
	# add before and add after.
	puts "\tTest0$tnum.a: put and get duplicate keys."
	set dbc [eval {$db cursor} $txn]
	set i ""
	for { set i 1 } { $i <= $ndups } { incr i 2 } {
		lappend dlist $i
	}
	set maxodd $i
	while { [gets $did str] != -1 && $count < $nentries } {
		for { set i 1 } { $i <= $ndups } { incr i 2 } {
			set datastr $i:$str
			set ret [eval {$db put} $txn $pflags {$str $datastr}]
			error_check_good put $ret 0
		}

		# Now retrieve all the keys matching this key
		set x 1
		for {set ret [$dbc get "-set" $str ]} \
		    {[llength $ret] != 0} \
		    {set ret [$dbc get "-next"] } {
			if {[llength $ret] == 0} {
				break
			}
			set k [lindex [lindex $ret 0] 0]
			if { [string compare $k $str] != 0 } {
				break
			}
			set datastr [lindex [lindex $ret 0] 1]
			set d [data_of $datastr]

			error_check_good Test0$tnum:put $d $str
			set id [ id_of $datastr ]
			error_check_good Test0$tnum:dup# $id $x
			incr x 2
		}
		error_check_good Test0$tnum:numdups $x $maxodd
		incr count
	}
	error_check_good curs_close [$dbc close] 0
	close $did

	# Now we will get each key from the DB and compare the results
	# to the original.
	puts "\tTest0$tnum.b: \
	    traverse entire file checking duplicates before close."
	dup_check $db $txn $t1 $dlist

	# Now compare the keys to see if they match the dictionary entries
	set q q
	filehead $nentries $dict $t3
	filesort $t3 $t2
	filesort $t1 $t3

	error_check_good Test0$tnum:diff($t3,$t2) \
	    [filecmp $t3 $t2] 0

	error_check_good db_close [$db close] 0

	set db [eval {berkdb_open} $args $testfile]
	error_check_good dbopen [is_valid_db $db] TRUE

	puts "\tTest0$tnum.c: \
	    traverse entire file checking duplicates after close."
	dup_check $db $txn $t1 $dlist

	# Now compare the keys to see if they match the dictionary entries
	filesort $t1 $t3
	error_check_good Test0$tnum:diff($t3,$t2) \
	    [filecmp $t3 $t2] 0

	puts "\tTest0$tnum.d: Testing key_first functionality"
	add_dup $db $txn $nentries "-keyfirst" 0 0
	set dlist [linsert $dlist 0 0]
	dup_check $db $txn $t1 $dlist

	puts "\tTest0$tnum.e: Testing key_last functionality"
	add_dup $db $txn $nentries "-keylast" [expr $maxodd - 1] 0
	lappend dlist [expr $maxodd - 1]
	dup_check $db $txn $t1 $dlist

	puts "\tTest0$tnum.f: Testing add_before functionality"
	add_dup $db $txn $nentries "-before" 2 3
	set dlist [linsert $dlist 2 2]
	dup_check $db $txn $t1 $dlist

	puts "\tTest0$tnum.g: Testing add_after functionality"
	add_dup $db $txn $nentries "-after" 4 4
	set dlist [linsert $dlist 4 4]
	dup_check $db $txn $t1 $dlist

	error_check_good db_close [$db close] 0
}

proc add_dup {db txn nentries flag dataval iter} {
	source ./include.tcl

	set dbc [eval {$db cursor} $txn]
	set did [open $dict]
	set count 0
	while { [gets $did str] != -1 && $count < $nentries } {
		set datastr $dataval:$str
		set ret [$dbc get "-set" $str]
		error_check_bad "cget(SET)" [is_substr $ret Error] 1
		for { set i 1 } { $i < $iter } { incr i } {
			set ret [$dbc get "-next"]
			error_check_bad "cget(NEXT)" [is_substr $ret Error] 1
		}

		if { [string compare $flag "-before"] == 0 ||
		    [string compare $flag "-after"] == 0 } {
			set ret [$dbc put $flag $datastr]
		} else {
			set ret [$dbc put $flag $str $datastr]
		}
		error_check_good "$dbc put $flag" $ret 0
		incr count
	}
	close $did
	$dbc close
}

proc test011_recno { method {nentries 10000} {tnum 11} largs } {
	global dlist
	source ./include.tcl

	set largs [convert_args $method $largs]
	set omethod [convert_method $method]
	set renum [is_rrecno $method]

	puts "Test0$tnum: \
	    $method ($largs) $nentries test cursor insert functionality"

	# Create the database and open the dictionary
	set eindex [lsearch -exact $largs "-env"]
	#
	# If we are using an env, then testfile should just be the db name.
	# Otherwise it is the test directory and the name.
	if { $eindex == -1 } {
		set testfile $testdir/test0$tnum.db
		set env NULL
	} else {
		set testfile test0$tnum.db
		incr eindex
		set env [lindex $largs $eindex]
	}
	set t1 $testdir/t1
	set t2 $testdir/t2
	set t3 $testdir/t3
	cleanup $testdir $env

	if {$renum == 1} {
		append largs " -renumber"
	}
	set db [eval {berkdb_open \
	     -create -truncate -mode 0644} $largs {$omethod $testfile}]
	error_check_good dbopen [is_valid_db $db] TRUE

	set did [open $dict]

	set pflags ""
	set gflags ""
	set txn ""
	set count 0

	# The basic structure of the test is that we pick a random key
	# in the database and then add items before, after, ?? it.  The
	# trickiness is that with RECNO, these are not duplicates, they
	# are creating new keys.  Therefore, every time we do this, the
	# keys assigned to other values change.  For this reason, we'll
	# keep the database in tcl as a list and insert properly into
	# it to verify that the right thing is happening.  If we do not
	# have renumber set, then the BEFORE and AFTER calls should fail.

	# Seed the database with an initial record
	gets $did str
	set ret [eval {$db put} $txn {1 [chop_data $method $str]}]
	error_check_good put $ret 0
	set count 1

	set dlist "NULL $str"

	# Open a cursor
	set dbc [eval {$db cursor} $txn]
	puts "\tTest0$tnum.a: put and get entries"
	while { [gets $did str] != -1 && $count < $nentries } {
		# Pick a random key
		set key [berkdb random_int 1 $count]
		set ret [$dbc get -set $key]
		set k [lindex [lindex $ret 0] 0]
		set d [lindex [lindex $ret 0] 1]
		error_check_good cget:SET:key $k $key
		error_check_good \
		    cget:SET $d [pad_data $method [lindex $dlist $key]]

		# Current
		set ret [$dbc put -current [chop_data $method $str]]
		error_check_good cput:$key $ret 0
		set dlist [lreplace $dlist $key $key [pad_data $method $str]]

		# Before
		if { [gets $did str] == -1 } {
			continue;
		}

		if { $renum == 1 } {
			set ret [$dbc put \
			    -before [chop_data $method $str]]
			error_check_good cput:$key:BEFORE $ret $key
			set dlist [linsert $dlist $key $str]
			incr count

			# After
			if { [gets $did str] == -1 } {
				continue;
			}
			set ret [$dbc put \
			    -after [chop_data $method $str]]
			error_check_good cput:$key:AFTER $ret [expr $key + 1]
			set dlist [linsert $dlist [expr $key + 1] $str]
			incr count
		}

		# Now verify that the keys are in the right place
		set i 0
		for {set ret [$dbc get "-set" $key]} \
		    {[string length $ret] != 0 && $i < 3} \
		    {set ret [$dbc get "-next"] } {
			set check_key [expr $key + $i]

			set k [lindex [lindex $ret 0] 0]
			error_check_good cget:$key:loop $k $check_key

			set d [lindex [lindex $ret 0] 1]
			error_check_good cget:data $d \
			    [pad_data $method [lindex $dlist $check_key]]
			incr i
		}
	}
	close $did
	error_check_good cclose [$dbc close] 0

	# Create  check key file.
	set oid [open $t2 w]
	for {set i 1} {$i <= $count} {incr i} {
		puts $oid $i
	}
	close $oid

	puts "\tTest0$tnum.b: dump file"
	dump_file $db $txn $t1 test011_check
	error_check_good Test0$tnum:diff($t2,$t1) \
	    [filecmp $t2 $t1] 0

	error_check_good db_close [$db close] 0

	puts "\tTest0$tnum.c: close, open, and dump file"
	open_and_dump_file $testfile $env $txn $t1 test011_check \
	    dump_file_direction "-first" "-next"
	error_check_good Test0$tnum:diff($t2,$t1) \
	    [filecmp $t2 $t1] 0

	puts "\tTest0$tnum.d: close, open, and dump file in reverse direction"
	open_and_dump_file $testfile $env $txn $t1 test011_check \
	    dump_file_direction "-last" "-prev"

	filesort $t1 $t3 -n
	error_check_good Test0$tnum:diff($t2,$t3) \
	    [filecmp $t2 $t3] 0
}

proc test011_check { key data } {
	global dlist

	error_check_good "get key $key" $data [lindex $dlist $key]
}

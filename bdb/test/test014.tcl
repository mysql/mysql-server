# See the file LICENSE for redistribution information.
#
# Copyright (c) 1996, 1997, 1998, 1999, 2000
#	Sleepycat Software.  All rights reserved.
#
#	$Id: test014.tcl,v 11.19 2000/08/25 14:21:54 sue Exp $
#
# DB Test 14 {access method}
#
# Partial put test, small data, replacing with same size.  The data set
# consists of the first nentries of the dictionary.  We will insert them
# (and retrieve them) as we do in test 1 (equal key/data pairs).  Then
# we'll try to perform partial puts of some characters at the beginning,
# some at the end, and some at the middle.
proc test014 { method {nentries 10000} args } {
	set fixed 0
	set args [convert_args $method $args]

	if { [is_fixed_length $method] == 1 } {
		set fixed 1
	}

	puts "Test014: $method ($args) $nentries equal key/data pairs, put test"

	# flagp indicates whether this is a postpend or a
	# normal partial put
	set flagp 0

	eval {test014_body $method $flagp 1 1 $nentries} $args
	eval {test014_body $method $flagp 1 4 $nentries} $args
	eval {test014_body $method $flagp 2 4 $nentries} $args
	eval {test014_body $method $flagp 1 128 $nentries} $args
	eval {test014_body $method $flagp 2 16 $nentries} $args
	if { $fixed == 0 } {
		eval {test014_body $method $flagp 0 1 $nentries} $args
		eval {test014_body $method $flagp 0 4 $nentries} $args
		eval {test014_body $method $flagp 0 128 $nentries} $args

		# POST-PENDS :
		# partial put data after the end of the existent record
		# chars: number of empty spaces that will be padded with null
		# increase: is the length of the str to be appended (after pad)
		#
		set flagp 1
		eval {test014_body $method $flagp 1 1 $nentries} $args
		eval {test014_body $method $flagp 4 1 $nentries} $args
		eval {test014_body $method $flagp 128 1 $nentries} $args
		eval {test014_body $method $flagp 1 4 $nentries} $args
		eval {test014_body $method $flagp 1 128 $nentries} $args
	}
	puts "Test014 complete."
}

proc test014_body { method flagp chars increase {nentries 10000} args } {
	source ./include.tcl

	set omethod [convert_method $method]

	if { [is_fixed_length $method] == 1 && $chars != $increase } {
		puts "Test014: $method: skipping replace\
		    $chars chars with string $increase times larger."
		return
	}

	if { $flagp == 1} {
		puts "Test014: Postpending string of len $increase with \
		    gap $chars."
	} else {
		puts "Test014: Replace $chars chars with string \
		    $increase times larger"
	}

	# Create the database and open the dictionary
	set eindex [lsearch -exact $args "-env"]
	#
	# If we are using an env, then testfile should just be the db name.
	# Otherwise it is the test directory and the name.
	if { $eindex == -1 } {
		set testfile $testdir/test014.db
		set env NULL
	} else {
		set testfile test014.db
		incr eindex
		set env [lindex $args $eindex]
	}
	set t1 $testdir/t1
	set t2 $testdir/t2
	set t3 $testdir/t3
	cleanup $testdir $env

	set db [eval {berkdb_open \
	     -create -truncate -mode 0644} $args {$omethod $testfile}]
	error_check_good dbopen [is_valid_db $db] TRUE

	set gflags ""
	set pflags ""
	set txn ""
	set count 0

	if { [is_record_based $method] == 1 } {
		append gflags " -recno"
	}

	puts "\tTest014.a: put/get loop"
	# Here is the loop where we put and get each key/data pair
	# We will do the initial put and then three Partial Puts
	# for the beginning, middle and end of the string.
	set did [open $dict]
	while { [gets $did str] != -1 && $count < $nentries } {
		if { [is_record_based $method] == 1 } {
			set key [expr $count + 1]
		} else {
			set key $str
		}
		if { $flagp == 1 } {
			# this is for postpend only
			global dvals

			# initial put
			set ret [$db put $key $str]
			error_check_good dbput $ret 0

			set offset [string length $str]

			# increase is the actual number of new bytes
			# to be postpended (besides the null padding)
			set data [repeat "P" $increase]

			# chars is the amount of padding in between
			# the old data and the new
			set len [expr $offset + $chars + $increase]
			set dvals($key) [binary format \
			    a[set offset]x[set chars]a[set increase] \
			    $str $data]
			set offset [expr $offset + $chars]
			set ret [$db put -partial [list $offset 0] $key $data]
			error_check_good dbput:post $ret 0
		} else {
			partial_put $method $db $txn \
			    $gflags $key $str $chars $increase
		}
		incr count
	}
	close $did

	# Now make sure that everything looks OK
	puts "\tTest014.b: check entire file contents"
	dump_file $db $txn $t1 test014.check
	error_check_good db_close [$db close] 0

	# Now compare the keys to see if they match the dictionary (or ints)
	if { [is_record_based $method] == 1 } {
		set oid [open $t2 w]
		for {set i 1} {$i <= $nentries} {set i [incr i]} {
			puts $oid $i
		}
		close $oid
		file rename -force $t1 $t3
	} else {
		set q q
		filehead $nentries $dict $t3
		filesort $t3 $t2
		filesort $t1 $t3
	}

	error_check_good \
	    Test014:diff($t3,$t2) [filecmp $t3 $t2] 0

	puts "\tTest014.c: close, open, and dump file"
	# Now, reopen the file and run the last test again.
	open_and_dump_file $testfile $env $txn \
	    $t1 test014.check dump_file_direction "-first" "-next"

	if { [string compare $omethod "-recno"] != 0 } {
		filesort $t2 $t3
		file rename -force $t3 $t2
		filesort $t1 $t3
	}

	error_check_good \
	    Test014:diff($t3,$t2) [filecmp $t3 $t2] 0
	# Now, reopen the file and run the last test again in the
	# reverse direction.
	puts "\tTest014.d: close, open, and dump file in reverse direction"
	open_and_dump_file $testfile $env $txn $t1 \
	    test014.check dump_file_direction "-last" "-prev"

	if { [string compare $omethod "-recno"] != 0 } {
		filesort $t2 $t3
		file rename -force $t3 $t2
		filesort $t1 $t3
	}

	error_check_good \
	    Test014:diff($t3,$t2) [filecmp $t3 $t2] 0
}

# Check function for test014; keys and data are identical
proc test014.check { key data } {
	global dvals

	error_check_good key"$key"_exists [info exists dvals($key)] 1
	error_check_good "data mismatch for key $key" $data $dvals($key)
}

# See the file LICENSE for redistribution information.
#
# Copyright (c) 2000-2002
#	Sleepycat Software.  All rights reserved.
#
# $Id: test095.tcl,v 11.16 2002/08/08 15:38:12 bostic Exp $
#
# TEST	test095
# TEST	Bulk get test. [#2934]
proc test095 { method {nsets 1000} {noverflows 25} {tnum 95} args } {
	source ./include.tcl
	set args [convert_args $method $args]
	set omethod [convert_method $method]

	set txnenv 0
	set eindex [lsearch -exact $args "-env"]
	#
	# If we are using an env, then testfile should just be the db name.
	# Otherwise it is the test directory and the name.
	if { $eindex == -1 } {
		set basename $testdir/test0$tnum
		set env NULL
		# If we've our own env, no reason to swap--this isn't
		# an mpool test.
		set carg { -cachesize {0 25000000 0} }
	} else {
		set basename test0$tnum
		incr eindex
		set env [lindex $args $eindex]
		set txnenv [is_txnenv $env]
		if { $txnenv == 1 } {
			puts "Skipping for environment with txns"
			return
		}
		set testdir [get_home $env]
		set carg {}
	}
	cleanup $testdir $env

	puts "Test0$tnum: $method ($args) Bulk get test"

	if { [is_record_based $method] == 1 || [is_rbtree $method] == 1 } {
		puts "Test0$tnum skipping for method $method"
		return
	}

	# We run the meat of the test twice: once with unsorted dups,
	# once with sorted dups.
	for { set dflag "-dup"; set sort "unsorted"; set diter 0 } \
	    { $diter < 2 } \
	    { set dflag "-dup -dupsort"; set sort "sorted"; incr diter } {
		set testfile $basename-$sort.db
		set did [open $dict]

		# Open and populate the database with $nsets sets of dups.
		# Each set contains as many dups as its number
		puts "\tTest0$tnum.a:\
		    Creating database with $nsets sets of $sort dups."
		set dargs "$dflag $carg $args"
		set db [eval {berkdb_open -create} $omethod $dargs $testfile]
		error_check_good db_open [is_valid_db $db] TRUE
		t95_populate $db $did $nsets 0

		# Run basic get tests.
		t95_gettest $db $tnum b [expr 8192] 1
		t95_gettest $db $tnum c [expr 10 * 8192] 0

		# Run cursor get tests.
		t95_cgettest $db $tnum d [expr 100] 1
		t95_cgettest $db $tnum e [expr 10 * 8192] 0

		# Run invalid flag combination tests
		# Sync and reopen test file so errors won't be sent to stderr
		error_check_good db_sync [$db sync] 0
		set noerrdb [eval berkdb_open_noerr $dargs $testfile]
		t95_flagtest $noerrdb $tnum f [expr 8192]
		t95_cflagtest $noerrdb $tnum g [expr 100]
		error_check_good noerrdb_close [$noerrdb close] 0

		# Set up for overflow tests
		set max [expr 4000 * $noverflows]
		puts "\tTest0$tnum.h: Growing\
	    database with $noverflows overflow sets (max item size $max)"
		t95_populate $db $did $noverflows 4000

		# Run overflow get tests.
		t95_gettest $db $tnum i [expr 10 * 8192] 1
		t95_gettest $db $tnum j [expr $max * 2] 1
		t95_gettest $db $tnum k [expr $max * $noverflows * 2] 0

		# Run overflow cursor get tests.
		t95_cgettest $db $tnum l [expr 10 * 8192] 1
		t95_cgettest $db $tnum m [expr $max * 2] 0

		error_check_good db_close [$db close] 0
		close $did
	}
}

proc t95_gettest { db tnum letter bufsize expectfail } {
	t95_gettest_body $db $tnum $letter $bufsize $expectfail 0
}
proc t95_cgettest { db tnum letter bufsize expectfail } {
	t95_gettest_body $db $tnum $letter $bufsize $expectfail 1
}
proc t95_flagtest { db tnum letter bufsize } {
	t95_flagtest_body $db $tnum $letter $bufsize 0
}
proc t95_cflagtest { db tnum letter bufsize } {
	t95_flagtest_body $db $tnum $letter $bufsize 1
}

# Basic get test
proc t95_gettest_body { db tnum letter bufsize expectfail usecursor } {
	global errorCode

	if { $usecursor == 0 } {
		set action "db get -multi"
	} else {
		set action "dbc get -multi -set/-next"
	}
	puts "\tTest0$tnum.$letter: $action with bufsize $bufsize"

	set allpassed TRUE
	set saved_err ""

	# Cursor for $usecursor.
	if { $usecursor != 0 } {
		set getcurs [$db cursor]
		error_check_good getcurs [is_valid_cursor $getcurs $db] TRUE
	}

	# Traverse DB with cursor;  do get/c_get(DB_MULTIPLE) on each item.
	set dbc [$db cursor]
	error_check_good is_valid_dbc [is_valid_cursor $dbc $db] TRUE
	for { set dbt [$dbc get -first] } { [llength $dbt] != 0 } \
	    { set dbt [$dbc get -nextnodup] } {
		set key [lindex [lindex $dbt 0] 0]
		set datum [lindex [lindex $dbt 0] 1]

		if { $usecursor == 0 } {
			set ret [catch {eval $db get -multi $bufsize $key} res]
		} else {
			set res {}
			for { set ret [catch {eval $getcurs get -multi $bufsize\
			    -set $key} tres] } \
			    { $ret == 0 && [llength $tres] != 0 } \
			    { set ret [catch {eval $getcurs get -multi $bufsize\
			    -nextdup} tres]} {
				eval lappend res $tres
			}
		}

		# If we expect a failure, be more tolerant if the above fails;
		# just make sure it's an ENOMEM, mark it, and move along.
		if { $expectfail != 0 && $ret != 0 } {
			error_check_good multi_failure_errcode \
			    [is_substr $errorCode ENOMEM] 1
			set allpassed FALSE
			continue
		}
		error_check_good get_multi($key) $ret 0
		t95_verify $res FALSE
	}

	set ret [catch {eval $db get -multi $bufsize} res]

	if { $expectfail == 1 } {
		error_check_good allpassed $allpassed FALSE
		puts "\t\tTest0$tnum.$letter:\
		    returned at least one ENOMEM (as expected)"
	} else {
		error_check_good allpassed $allpassed TRUE
		puts "\t\tTest0$tnum.$letter: succeeded (as expected)"
	}

	error_check_good dbc_close [$dbc close] 0
	if { $usecursor != 0 } {
		error_check_good getcurs_close [$getcurs close] 0
	}
}

# Test of invalid flag combinations for -multi
proc t95_flagtest_body { db tnum letter bufsize usecursor } {
	global errorCode

	if { $usecursor == 0 } {
		set action "db get -multi "
	} else {
		set action "dbc get -multi "
	}
	puts "\tTest0$tnum.$letter: $action with invalid flag combinations"

	# Cursor for $usecursor.
	if { $usecursor != 0 } {
		set getcurs [$db cursor]
		error_check_good getcurs [is_valid_cursor $getcurs $db] TRUE
	}

	if { $usecursor == 0 } {
		# Disallowed flags for basic -multi get
		set badflags [list consume consume_wait {rmw some_key}]

		foreach flag $badflags {
			catch {eval $db get -multi $bufsize -$flag} ret
			error_check_good \
			    db:get:multi:$flag [is_substr $errorCode EINVAL] 1
		}
       } else {
		# Disallowed flags for cursor -multi get
		set cbadflags [list last get_recno join_item \
		    {multi_key 1000} prev prevnodup]

		set dbc [$db cursor]
		$dbc get -first
		foreach flag $cbadflags {
			catch {eval $dbc get -multi $bufsize -$flag} ret
			error_check_good dbc:get:multi:$flag \
				[is_substr $errorCode EINVAL] 1
		}
		error_check_good dbc_close [$dbc close] 0
	}
	if { $usecursor != 0 } {
		error_check_good getcurs_close [$getcurs close] 0
	}
	puts "\t\tTest0$tnum.$letter completed"
}

# Verify that a passed-in list of key/data pairs all match the predicted
# structure (e.g. {{thing1 thing1.0}}, {{key2 key2.0} {key2 key2.1}}).
proc t95_verify { res multiple_keys } {
	global alphabet

	set i 0

	set orig_key [lindex [lindex $res 0] 0]
	set nkeys [string trim $orig_key $alphabet']
	set base_key [string trim $orig_key 0123456789]
	set datum_count 0

	while { 1 } {
		set key [lindex [lindex $res $i] 0]
		set datum [lindex [lindex $res $i] 1]

		if { $datum_count >= $nkeys } {
			if { [llength $key] != 0 } {
				# If there are keys beyond $nkeys, we'd
				# better have multiple_keys set.
				error_check_bad "keys beyond number $i allowed"\
				    $multiple_keys FALSE

				# If multiple_keys is set, accept the new key.
				set orig_key $key
				set nkeys [eval string trim \
				    $orig_key {$alphabet'}]
				set base_key [eval string trim \
				    $orig_key 0123456789]
				set datum_count 0
			} else {
				# datum_count has hit nkeys.  We're done.
				return
			}
		}

		error_check_good returned_key($i) $key $orig_key
		error_check_good returned_datum($i) \
		    $datum $base_key.[format %4u $datum_count]
		incr datum_count
		incr i
	}
}

# Add nsets dup sets, each consisting of {word$ndups word$n} pairs,
# with "word" having (i * pad_bytes)  bytes extra padding.
proc t95_populate { db did nsets pad_bytes } {
	set txn ""
	for { set i 1 } { $i <= $nsets } { incr i } {
		# basekey is a padded dictionary word
		gets $did basekey

		append basekey [repeat "a" [expr $pad_bytes * $i]]

		# key is basekey with the number of dups stuck on.
		set key $basekey$i

		for { set j 0 } { $j < $i } { incr j } {
			set data $basekey.[format %4u $j]
			error_check_good db_put($key,$data) \
			    [eval {$db put} $txn {$key $data}] 0
		}
	}

	# This will make debugging easier, and since the database is
	# read-only from here out, it's cheap.
	error_check_good db_sync [$db sync] 0
}

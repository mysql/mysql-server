# See the file LICENSE for redistribution information.
#
# Copyright (c) 1999, 2000
#	Sleepycat Software.  All rights reserved.
#
#	$Id: test087.tcl,v 11.6 2000/12/11 17:24:55 sue Exp $
#
# DB Test 87: Test of cursor stability on duplicate pages w/aborts.
# Does the following:
#    a. Initialize things by DB->putting ndups dups and
#       setting a reference cursor to point to each.
#    b. c_put ndups dups (and correspondingly expanding
#       the set of reference cursors) after the last one, making sure
#       after each step that all the reference cursors still point to
#       the right item.
#    c. Ditto, but before the first one.
#    d. Ditto, but after each one in sequence first to last.
#    e. Ditto, but after each one in sequence from last to first.
#       occur relative to the new datum)
#    f. Ditto for the two sequence tests, only doing a
#       DBC->c_put(DB_CURRENT) of a larger datum instead of adding a
#       new one.
proc test087 { method {pagesize 512} {ndups 50} {tnum 87} args } {
	source ./include.tcl
	global alphabet

	set omethod [convert_method $method]
	set args [convert_args $method $args]

	puts "Test0$tnum $omethod ($args): "
	set eindex [lsearch -exact $args "-env"]
	#
	# If we are using an env, then return
	if { $eindex != -1 } {
		puts "Environment specified;  skipping."
		return
	}
	set pgindex [lsearch -exact $args "-pagesize"]
	if { $pgindex != -1 } {
		puts "Test087: skipping for specific pagesizes"
		return
	}
	env_cleanup $testdir
	set testfile test0$tnum.db
	set key "the key"
	append args " -pagesize $pagesize -dup"

	if { [is_record_based $method] || [is_rbtree $method] } {
		puts "Skipping for method $method."
		return
	} else {
		puts "Cursor stability on dup. pages w/ aborts."
	}

	set env [berkdb env -create -home $testdir -txn]
	error_check_good env_create [is_valid_env $env] TRUE

	set db [eval {berkdb_open -env $env \
	     -create -mode 0644} $omethod $args $testfile]
	error_check_good "db open" [is_valid_db $db] TRUE

	# Number of outstanding keys.
	set keys 0

	puts "\tTest0$tnum.a.1: Initializing put loop; $ndups dups, short data."
	set txn [$env txn]
	error_check_good txn [is_valid_txn $txn $env] TRUE
	for { set i 0 } { $i < $ndups } { incr i } {
		set datum [makedatum_t73 $i 0]

		error_check_good "db put ($i)" [$db put -txn $txn $key $datum] 0

		set is_long($i) 0
		incr keys
	}
	error_check_good txn_commit [$txn commit] 0

	puts "\tTest0$tnum.a.2: Initializing cursor get loop; $keys dups."
	set txn [$env txn]
	error_check_good txn [is_valid_txn $txn $env] TRUE
	for { set i 0 } { $i < $keys } { incr i } {
		set datum [makedatum_t73 $i 0]

		set dbc($i) [$db cursor -txn $txn]
		error_check_good "db cursor ($i)"\
		    [is_valid_cursor $dbc($i) $db] TRUE
		error_check_good "dbc get -get_both ($i)"\
		    [$dbc($i) get -get_both $key $datum]\
		    [list [list $key $datum]]
	}

	puts "\tTest0$tnum.b: Cursor put (DB_KEYLAST); $ndups new dups,\
	    short data."

	set ctxn [$env txn -parent $txn]
	error_check_good ctxn($i) [is_valid_txn $ctxn $env] TRUE
	for { set i 0 } { $i < $ndups } { incr i } {
		# !!! keys contains the number of the next dup
		# to be added (since they start from zero)

		set datum [makedatum_t73 $keys 0]
		set curs [$db cursor -txn $ctxn]
		error_check_good "db cursor create" [is_valid_cursor $curs $db]\
		    TRUE
		error_check_good "c_put(DB_KEYLAST, $keys)"\
		    [$curs put -keylast $key $datum] 0

		# We can't do a verification while a child txn is active,
		# or we'll run into trouble when DEBUG_ROP is enabled.
		# If this test has trouble, though, uncommenting this
		# might be illuminating--it makes things a bit more rigorous
		# and works fine when DEBUG_ROP is not enabled.
		# verify_t73 is_long dbc $keys $key
		error_check_good curs_close [$curs close] 0
	}
	error_check_good ctxn_abort [$ctxn abort] 0
	verify_t73 is_long dbc $keys $key

	puts "\tTest0$tnum.c: Cursor put (DB_KEYFIRST); $ndups new dups,\
	    short data."

	set ctxn [$env txn -parent $txn]
	error_check_good ctxn($i) [is_valid_txn $ctxn $env] TRUE
	for { set i 0 } { $i < $ndups } { incr i } {
		# !!! keys contains the number of the next dup
		# to be added (since they start from zero)

		set datum [makedatum_t73 $keys 0]
		set curs [$db cursor -txn $ctxn]
		error_check_good "db cursor create" [is_valid_cursor $curs $db]\
		    TRUE
		error_check_good "c_put(DB_KEYFIRST, $keys)"\
		    [$curs put -keyfirst $key $datum] 0

		# verify_t73 is_long dbc $keys $key
		error_check_good curs_close [$curs close] 0
	}
	# verify_t73 is_long dbc $keys $key
	# verify_t73 is_long dbc $keys $key
	error_check_good ctxn_abort [$ctxn abort] 0
	verify_t73 is_long dbc $keys $key

	puts "\tTest0$tnum.d: Cursor put (DB_AFTER) first to last;\
	    $keys new dups, short data"
	# We want to add a datum after each key from 0 to the current
	# value of $keys, which we thus need to save.
	set ctxn [$env txn -parent $txn]
	error_check_good ctxn($i) [is_valid_txn $ctxn $env] TRUE
	set keysnow $keys
	for { set i 0 } { $i < $keysnow } { incr i } {
		set datum [makedatum_t73 $keys 0]
		set curs [$db cursor -txn $ctxn]
		error_check_good "db cursor create" [is_valid_cursor $curs $db]\
		    TRUE

		# Which datum to insert this guy after.
		set curdatum [makedatum_t73 $i 0]
		error_check_good "c_get(DB_GET_BOTH, $i)"\
		    [$curs get -get_both $key $curdatum]\
		    [list [list $key $curdatum]]
		error_check_good "c_put(DB_AFTER, $i)"\
		    [$curs put -after $datum] 0

		# verify_t73 is_long dbc $keys $key
		error_check_good curs_close [$curs close] 0
	}
	error_check_good ctxn_abort [$ctxn abort] 0
	verify_t73 is_long dbc $keys $key

	puts "\tTest0$tnum.e: Cursor put (DB_BEFORE) last to first;\
	    $keys new dups, short data"
	set ctxn [$env txn -parent $txn]
	error_check_good ctxn($i) [is_valid_txn $ctxn $env] TRUE
	for { set i [expr $keys - 1] } { $i >= 0 } { incr i -1 } {
		set datum [makedatum_t73 $keys 0]
		set curs [$db cursor -txn $ctxn]
		error_check_good "db cursor create" [is_valid_cursor $curs $db]\
		    TRUE

		# Which datum to insert this guy before.
		set curdatum [makedatum_t73 $i 0]
		error_check_good "c_get(DB_GET_BOTH, $i)"\
		    [$curs get -get_both $key $curdatum]\
		    [list [list $key $curdatum]]
		error_check_good "c_put(DB_BEFORE, $i)"\
		    [$curs put -before $datum] 0

		# verify_t73 is_long dbc $keys $key
		error_check_good curs_close [$curs close] 0
	}
	error_check_good ctxn_abort [$ctxn abort] 0
	verify_t73 is_long dbc $keys $key

	puts "\tTest0$tnum.f: Cursor put (DB_CURRENT), first to last,\
	    growing $keys data."
	set ctxn [$env txn -parent $txn]
	error_check_good ctxn($i) [is_valid_txn $ctxn $env] TRUE
	for { set i 0 } { $i < $keysnow } { incr i } {
		set olddatum [makedatum_t73 $i 0]
		set newdatum [makedatum_t73 $i 1]
		set curs [$db cursor -txn $ctxn]
		error_check_good "db cursor create" [is_valid_cursor $curs $db]\
		    TRUE

		error_check_good "c_get(DB_GET_BOTH, $i)"\
		    [$curs get -get_both $key $olddatum]\
		    [list [list $key $olddatum]]
		error_check_good "c_put(DB_CURRENT, $i)"\
		    [$curs put -current $newdatum] 0

		set is_long($i) 1

		# verify_t73 is_long dbc $keys $key
		error_check_good curs_close [$curs close] 0
	}
	error_check_good ctxn_abort [$ctxn abort] 0
	for { set i 0 } { $i < $keysnow } { incr i } {
		set is_long($i) 0
	}
	verify_t73 is_long dbc $keys $key

	# Now delete the first item, abort the deletion, and make sure
	# we're still sane.
	puts "\tTest0$tnum.g: Cursor delete first item, then abort delete."
	set ctxn [$env txn -parent $txn]
	error_check_good ctxn($i) [is_valid_txn $ctxn $env] TRUE
	set curs [$db cursor -txn $ctxn]
	error_check_good "db cursor create" [is_valid_cursor $curs $db] TRUE
	set datum [makedatum_t73 0 0]
	error_check_good "c_get(DB_GET_BOTH, 0)"\
	    [$curs get -get_both $key $datum] [list [list $key $datum]]
	error_check_good "c_del(0)" [$curs del] 0
	error_check_good curs_close [$curs close] 0
	error_check_good ctxn_abort [$ctxn abort] 0
	verify_t73 is_long dbc $keys $key

	# Ditto, for the last item.
	puts "\tTest0$tnum.h: Cursor delete last item, then abort delete."
	set ctxn [$env txn -parent $txn]
	error_check_good ctxn($i) [is_valid_txn $ctxn $env] TRUE
	set curs [$db cursor -txn $ctxn]
	error_check_good "db cursor create" [is_valid_cursor $curs $db] TRUE
	set datum [makedatum_t73 [expr $keys - 1] 0]
	error_check_good "c_get(DB_GET_BOTH, [expr $keys - 1])"\
	    [$curs get -get_both $key $datum] [list [list $key $datum]]
	error_check_good "c_del(0)" [$curs del] 0
	error_check_good curs_close [$curs close] 0
	error_check_good ctxn_abort [$ctxn abort] 0
	verify_t73 is_long dbc $keys $key

	# Ditto, for all the items.
	puts "\tTest0$tnum.i: Cursor delete all items, then abort delete."
	set ctxn [$env txn -parent $txn]
	error_check_good ctxn($i) [is_valid_txn $ctxn $env] TRUE
	set curs [$db cursor -txn $ctxn]
	error_check_good "db cursor create" [is_valid_cursor $curs $db] TRUE
	set datum [makedatum_t73 0 0]
	error_check_good "c_get(DB_GET_BOTH, 0)"\
	    [$curs get -get_both $key $datum] [list [list $key $datum]]
	error_check_good "c_del(0)" [$curs del] 0
	for { set i 1 } { $i < $keys } { incr i } {
		error_check_good "c_get(DB_NEXT, $i)"\
		    [$curs get -next] [list [list $key [makedatum_t73 $i 0]]]
		error_check_good "c_del($i)" [$curs del] 0
	}
	error_check_good curs_close [$curs close] 0
	error_check_good ctxn_abort [$ctxn abort] 0
	verify_t73 is_long dbc $keys $key

	# Close cursors.
	puts "\tTest0$tnum.j: Closing cursors."
	for { set i 0 } { $i < $keys } { incr i } {
		error_check_good "dbc close ($i)" [$dbc($i) close] 0
	}
	error_check_good txn_commit [$txn commit] 0
	error_check_good "db close" [$db close] 0
	error_check_good "env close" [$env close] 0
}

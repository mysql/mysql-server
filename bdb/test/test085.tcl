# See the file LICENSE for redistribution information.
#
# Copyright (c) 2000
#	Sleepycat Software.  All rights reserved.
#
#	$Id: test085.tcl,v 1.4 2000/12/11 17:24:55 sue Exp $
#
# DB Test 85: Test of cursor behavior when a cursor is pointing to a deleted
# btree key which then has duplicates added.
proc test085 { method {pagesize 512} {onp 3} {offp 10} {tnum 85} args } {
	source ./include.tcl
	global alphabet

	set omethod [convert_method $method]
	set args [convert_args $method $args]


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

	set pgindex [lsearch -exact $args "-pagesize"]
	if { $pgindex != -1 } {
		puts "Test085: skipping for specific pagesizes"
		return
	}
	cleanup $testdir $env

	# Keys must sort $prekey < $key < $postkey.
	set prekey "AA"
	set key "BBB"
	set postkey "CCCC"

	# Make these distinguishable from each other and from the
	# alphabets used for the $key's data.
	set predatum "1234567890"
	set datum $alphabet
	set postdatum "0987654321"

	append args " -pagesize $pagesize -dup"

	puts -nonewline "Test0$tnum $omethod ($args): "

	# Skip for all non-btrees.  (Rbtrees don't count as btrees, for
	# now, since they don't support dups.)
	if { [is_btree $method] != 1 } {
		puts "Skipping for method $method."
		return
	} else {
		puts "Duplicates w/ deleted item cursor."
	}

	# Repeat the test with both on-page and off-page numbers of dups.
	foreach	ndups "$onp $offp" {
		# Put operations we want to test on a cursor set to the 
		# deleted item, the key to use with them, and what should 
		# come before and after them given a placement of
		# the deleted item at the beginning or end of the dupset.
		set final [expr $ndups - 1]
		set putops {
		{{-before} "" $predatum	{[test085_ddatum 0]} beginning}
		{{-before} "" {[test085_ddatum $final]} $postdatum end}
		{{-current} "" $predatum {[test085_ddatum 0]} beginning}
		{{-current} "" {[test085_ddatum $final]} $postdatum end}
		{{-keyfirst} $key $predatum {[test085_ddatum 0]} beginning}
		{{-keyfirst} $key $predatum {[test085_ddatum 0]} end}
		{{-keylast} $key {[test085_ddatum $final]} $postdatum beginning}
		{{-keylast} $key {[test085_ddatum $final]} $postdatum end}
		{{-after} "" $predatum {[test085_ddatum 0]} beginning}
		{{-after} "" {[test085_ddatum $final]} $postdatum end}
		}

		# Get operations we want to test on a cursor set to the
		# deleted item, any args to get, and the expected key/data pair.
		set getops {
		{{-current} "" "" "" beginning}
		{{-current} "" "" "" end}
		{{-next} "" $key {[test085_ddatum 0]} beginning}
		{{-next} "" $postkey $postdatum end}
		{{-prev} "" $prekey $predatum beginning}
		{{-prev} "" $key {[test085_ddatum $final]} end}
		{{-first} "" $prekey $predatum beginning}
		{{-first} "" $prekey $predatum end}
		{{-last} "" $postkey $postdatum beginning}
		{{-last} "" $postkey $postdatum end}
		{{-nextdup} "" $key {[test085_ddatum 0]} beginning}
		{{-nextdup} "" EMPTYLIST "" end}
		{{-nextnodup} "" $postkey $postdatum beginning}
		{{-nextnodup} "" $postkey $postdatum end}
		{{-prevnodup} "" $prekey $predatum beginning}
		{{-prevnodup} "" $prekey $predatum end}
		}

		foreach pair $getops {
			set op [lindex $pair 0]
			puts "\tTest0$tnum: Get ($op) with $ndups duplicates,\
			    cursor at the [lindex $pair 4]."
			set db [eval {berkdb_open -create \
	     	    	    -truncate -mode 0644} $omethod $args $testfile]
			error_check_good "db open" [is_valid_db $db] TRUE

			set dbc [test085_setup $db]

			set beginning [expr [string compare \
			    [lindex $pair 4] "beginning"] == 0]

			for { set i 0 } { $i < $ndups } { incr i } {
				if { $beginning } {
					error_check_good db_put($i) \
					    [$db put $key [test085_ddatum $i]] 0
				} else {
					set c [$db cursor]
					set j [expr $ndups - $i - 1]
					error_check_good db_cursor($j) \
					    [is_valid_cursor $c $db] TRUE
					set d [test085_ddatum $j]
					error_check_good dbc_put($j) \
					    [$c put -keyfirst $key $d] 0
					error_check_good c_close [$c close] 0
				}
			}
		
			set gargs [lindex $pair 1]
			set ekey ""
			set edata ""
			eval set ekey [lindex $pair 2]
			eval set edata [lindex $pair 3]

			set dbt [eval $dbc get $op $gargs] 
			if { [string compare $ekey EMPTYLIST] == 0 } {
				error_check_good dbt($op,$ndups) \
				    [llength $dbt] 0
			} else {
				error_check_good dbt($op,$ndups) $dbt \
				    [list [list $ekey $edata]]
			}
			error_check_good "dbc close" [$dbc close] 0
			error_check_good "db close" [$db close] 0
			verify_dir $testdir "\t\t"
		}

		foreach pair $putops {
			# Open and set up database.
			set op [lindex $pair 0]
			puts "\tTest0$tnum: Put ($op) with $ndups duplicates,\
			    cursor at the [lindex $pair 4]."
			set db [eval {berkdb_open -create \
	     	    	    -truncate -mode 0644} $omethod $args $testfile]
			error_check_good "db open" [is_valid_db $db] TRUE

			set beginning [expr [string compare \
			    [lindex $pair 4] "beginning"] == 0]
			
			set dbc [test085_setup $db]

			# Put duplicates.
			for { set i 0 } { $i < $ndups } { incr i } {
				if { $beginning } {
					error_check_good db_put($i) \
					    [$db put $key [test085_ddatum $i]] 0
				} else {
					set c [$db cursor]
					set j [expr $ndups - $i - 1]
					error_check_good db_cursor($j) \
					    [is_valid_cursor $c $db] TRUE
					set d [test085_ddatum $j]
					error_check_good dbc_put($j) \
					    [$c put -keyfirst $key $d] 0
					error_check_good c_close [$c close] 0
				}
			}

			# Set up cursors for stability test.
			set pre_dbc [$db cursor]
			error_check_good pre_set [$pre_dbc get -set $prekey] \
			    [list [list $prekey $predatum]]
			set post_dbc [$db cursor]
			error_check_good post_set [$post_dbc get -set $postkey]\
			    [list [list $postkey $postdatum]]
			set first_dbc [$db cursor]
			error_check_good first_set \
			    [$first_dbc get -get_both $key [test085_ddatum 0]] \
			    [list [list $key [test085_ddatum 0]]]
			set last_dbc [$db cursor]
			error_check_good last_set \
			    [$last_dbc get -get_both $key [test085_ddatum \
			    [expr $ndups - 1]]] \
			    [list [list $key [test085_ddatum [expr $ndups -1]]]]

			set k [lindex $pair 1]
			set d_before ""
			set d_after ""
			eval set d_before [lindex $pair 2]
			eval set d_after [lindex $pair 3]
			set newdatum "NewDatum"
			error_check_good dbc_put($op,$ndups) \
			    [eval $dbc put $op $k $newdatum] 0
			error_check_good dbc_prev($op,$ndups) \
			    [lindex [lindex [$dbc get -prev] 0] 1] \
			    $d_before
			error_check_good dbc_current($op,$ndups) \
			    [lindex [lindex [$dbc get -next] 0] 1] \
			    $newdatum

			error_check_good dbc_next($op,$ndups) \
			    [lindex [lindex [$dbc get -next] 0] 1] \
			    $d_after

			# Verify stability of pre- and post- cursors.
			error_check_good pre_stable [$pre_dbc get -current] \
			    [list [list $prekey $predatum]]
			error_check_good post_stable [$post_dbc get -current] \
			    [list [list $postkey $postdatum]]
			error_check_good first_stable \
			    [$first_dbc get -current] \
			    [list [list $key [test085_ddatum 0]]]
			error_check_good last_stable \
			    [$last_dbc get -current] \
			    [list [list $key [test085_ddatum [expr $ndups -1]]]]


			foreach c "$pre_dbc $post_dbc $first_dbc $last_dbc" {
				error_check_good ${c}_close [$c close] 0
			}

			error_check_good "dbc close" [$dbc close] 0
			error_check_good "db close" [$db close] 0
			verify_dir $testdir "\t\t"	
		}
	}
}


# Set up the test database;  put $prekey, $key, and $postkey with their 
# respective data, and then delete $key with a new cursor.  Return that
# cursor, still pointing to the deleted item.
proc test085_setup { db } {
	upvar key key
	upvar prekey prekey
	upvar postkey postkey
	upvar predatum predatum
	upvar postdatum postdatum

	# no one else should ever see this one!
	set datum "bbbbbbbb"	

	error_check_good pre_put [$db put $prekey $predatum] 0
	error_check_good main_put [$db put $key $datum] 0
	error_check_good post_put [$db put $postkey $postdatum] 0

	set dbc [$db cursor]
	error_check_good db_cursor [is_valid_cursor $dbc $db] TRUE

	error_check_good dbc_getset [$dbc get -get_both $key $datum] \
	    [list [list $key $datum]]

	error_check_good dbc_del [$dbc del] 0

	return $dbc
}

proc test085_ddatum { a } {
	global alphabet
	return $a$alphabet
}

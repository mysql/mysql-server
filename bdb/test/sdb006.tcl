# See the file LICENSE for redistribution information.
#
# Copyright (c) 1999, 2000
#	Sleepycat Software.  All rights reserved.
#
#	$Id: sdb006.tcl,v 11.12 2000/09/20 13:22:03 sue Exp $
#
# We'll test 2-way, 3-way, and 4-way joins and figure that if those work,
# everything else does as well.  We'll create test databases called
# sub1.db, sub2.db, sub3.db, and sub4.db.  The number on the database
# describes the duplication -- duplicates are of the form 0, N, 2N, 3N, ...
# where N is the number of the database.  Primary.db is the primary database,
# and sub0.db is the database that has no matching duplicates.  All of
# these are within a single database.
#
# We should test this on all btrees, all hash, and a combination thereof
proc subdb006 {method {nentries 100} args } {
	source ./include.tcl
	global rand_init

	# NB: these flags are internal only, ok
	set args [convert_args $method $args]
	set omethod [convert_method $method]

	if { [is_record_based $method] == 1 || [is_rbtree $method] } {
		puts "\tSubdb006 skipping for method $method."
		return
	}

	berkdb srand $rand_init

	foreach opt {" -dup" " -dupsort"} {
		append args $opt

		puts "Subdb006: $method ( $args ) Intra-subdb join"
		set txn ""
		#
		# Get a cursor in each subdb and move past the end of each
		# subdb.  Make sure we don't end up in another subdb.
		#
		puts "\tSubdb006.a: Intra-subdb join"

		cleanup $testdir NULL
		set testfile $testdir/subdb006.db

		set psize [list 8192]
		set duplist {0 50 25 16 12}
		set numdb [llength $duplist]
		build_all_subdb $testfile [list $method] $psize \
		    $duplist $nentries $args

		# Build the primary
		puts "Subdb006: Building the primary database $method"
		set oflags "-create -mode 0644 [conv $omethod \
		    [berkdb random_int 1 2]]"
		set db [eval {berkdb_open} $oflags $testfile primary.db]
		error_check_good dbopen [is_valid_db $db] TRUE
		for { set i 0 } { $i < 1000 } { incr i } {
			set key [format "%04d" $i]
			set ret [$db put $key stub]
			error_check_good "primary put" $ret 0
		}
		error_check_good "primary close" [$db close] 0
		set did [open $dict]
		gets $did str
		do_join_subdb $testfile primary.db "1 0" $str
		gets $did str
		do_join_subdb $testfile primary.db "2 0" $str
		gets $did str
		do_join_subdb $testfile primary.db "3 0" $str
		gets $did str
		do_join_subdb $testfile primary.db "4 0" $str
		gets $did str
		do_join_subdb $testfile primary.db "1" $str
		gets $did str
		do_join_subdb $testfile primary.db "2" $str
		gets $did str
		do_join_subdb $testfile primary.db "3" $str
		gets $did str
		do_join_subdb $testfile primary.db "4" $str
		gets $did str
		do_join_subdb $testfile primary.db "1 2" $str
		gets $did str
		do_join_subdb $testfile primary.db "1 2 3" $str
		gets $did str
		do_join_subdb $testfile primary.db "1 2 3 4" $str
		gets $did str
		do_join_subdb $testfile primary.db "2 1" $str
		gets $did str
		do_join_subdb $testfile primary.db "3 2 1" $str
		gets $did str
		do_join_subdb $testfile primary.db "4 3 2 1" $str
		gets $did str
		do_join_subdb $testfile primary.db "1 3" $str
		gets $did str
		do_join_subdb $testfile primary.db "3 1" $str
		gets $did str
		do_join_subdb $testfile primary.db "1 4" $str
		gets $did str
		do_join_subdb $testfile primary.db "4 1" $str
		gets $did str
		do_join_subdb $testfile primary.db "2 3" $str
		gets $did str
		do_join_subdb $testfile primary.db "3 2" $str
		gets $did str
		do_join_subdb $testfile primary.db "2 4" $str
		gets $did str
		do_join_subdb $testfile primary.db "4 2" $str
		gets $did str
		do_join_subdb $testfile primary.db "3 4" $str
		gets $did str
		do_join_subdb $testfile primary.db "4 3" $str
		gets $did str
		do_join_subdb $testfile primary.db "2 3 4" $str
		gets $did str
		do_join_subdb $testfile primary.db "3 4 1" $str
		gets $did str
		do_join_subdb $testfile primary.db "4 2 1" $str
		gets $did str
		do_join_subdb $testfile primary.db "0 2 1" $str
		gets $did str
		do_join_subdb $testfile primary.db "3 2 0" $str
		gets $did str
		do_join_subdb $testfile primary.db "4 3 2 1" $str
		gets $did str
		do_join_subdb $testfile primary.db "4 3 0 1" $str

		close $did
	}
}

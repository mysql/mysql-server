# See the file LICENSE for redistribution information.
#
# Copyright (c) 2001-2002
#	Sleepycat Software.  All rights reserved.
#
# $Id: sindex.tcl,v 1.8 2002/05/07 17:15:46 krinsky Exp $
#
# Secondary index test driver and maintenance routines.
#
# Breaking from the usual convention, we put the driver function
# for the secondary index tests here, in its own file.  The reason
# for this is that it's something which compartmentalizes nicely,
# has little in common with other driver functions, and
# is likely to be run on its own from time to time.
#
# The secondary index tests themselves live in si0*.tcl.

# Standard number of secondary indices to create if a single-element
# list of methods is passed into the secondary index tests.
global nsecondaries
set nsecondaries 2

# Run the secondary index tests.
proc sindex { {verbose 0} args } {
	global verbose_check_secondaries
	set verbose_check_secondaries $verbose

	# Run basic tests with a single secondary index and a small number
	# of keys, then again with a larger number of keys.  (Note that
	# we can't go above 5000, since we use two items from our
	# 10K-word list for each key/data pair.)
	foreach n { 200 5000 } {
		foreach pm { btree hash recno frecno queue queueext } {
			foreach sm { dbtree dhash ddbtree ddhash btree hash } {
				sindex001 [list $pm $sm $sm] $n
				sindex002 [list $pm $sm $sm] $n
				# Skip tests 3 & 4 for large lists;
				# they're not that interesting.
				if { $n < 1000 } {
					sindex003 [list $pm $sm $sm] $n
					sindex004 [list $pm $sm $sm] $n
				}

				sindex006 [list $pm $sm $sm] $n
			}
		}
	}

	# Run secondary index join test.  (There's no point in running
	# this with both lengths, the primary is unhappy for now with fixed-
	# length records (XXX), and we need unsorted dups in the secondaries.)
	foreach pm { btree hash recno } {
		foreach sm { btree hash } {
			sindex005 [list $pm $sm $sm] 1000
		}
		sindex005 [list $pm btree hash] 1000
		sindex005 [list $pm hash btree] 1000
	}


	# Run test with 50 secondaries.
	foreach pm { btree hash } {
		set methlist [list $pm]
		for { set i 0 } { $i < 50 } { incr i } {
			# XXX this should incorporate hash after #3726
			if { $i % 2 == 0 } {
				lappend methlist "dbtree"
			} else {
				lappend methlist "ddbtree"
			}
		}
		sindex001 $methlist 500
		sindex002 $methlist 500
		sindex003 $methlist 500
		sindex004 $methlist 500
	}
}

# The callback function we use for each given secondary in most tests
# is a simple function of its place in the list of secondaries (0-based)
# and the access method (since recnos may need different callbacks).
#
# !!!
# Note that callbacks 0-3 return unique secondary keys if the input data
# are unique;  callbacks 4 and higher may not, so don't use them with
# the normal wordlist and secondaries that don't support dups.
# The callbacks that incorporate a key don't work properly with recno
# access methods, at least not in the current test framework (the
# error_check_good lines test for e.g. 1foo, when the database has
# e.g. 0x010x000x000x00foo).
proc callback_n { n } {
	switch $n {
		0 { return _s_reversedata }
		1 { return _s_noop }
		2 { return _s_concatkeydata }
		3 { return _s_concatdatakey }
		4 { return _s_reverseconcat }
		5 { return _s_truncdata }
		6 { return _s_alwayscocacola }
	}
	return _s_noop
}

proc _s_reversedata { a b } { return [reverse $b] }
proc _s_truncdata { a b } { return [string range $b 1 end] }
proc _s_concatkeydata { a b } { return $a$b }
proc _s_concatdatakey { a b } { return $b$a }
proc _s_reverseconcat { a b } { return [reverse $a$b] }
proc _s_alwayscocacola { a b } { return "Coca-Cola" }
proc _s_noop { a b } { return $b }

# Should the check_secondary routines print lots of output?
set verbose_check_secondaries 0

# Given a primary database handle, a list of secondary handles, a
# number of entries, and arrays of keys and data, verify that all
# databases have what they ought to.
proc check_secondaries { pdb sdbs nentries keyarr dataarr {pref "Check"} } {
	upvar $keyarr keys
	upvar $dataarr data
	global verbose_check_secondaries

	# Make sure each key/data pair is in the primary.
	if { $verbose_check_secondaries } {
		puts "\t\t$pref.1: Each key/data pair is in the primary"
	}
	for { set i 0 } { $i < $nentries } { incr i } {
		error_check_good pdb_get($i) [$pdb get $keys($i)] \
		    [list [list $keys($i) $data($i)]]
	}

	for { set j 0 } { $j < [llength $sdbs] } { incr j } {
		# Make sure each key/data pair is in this secondary.
		if { $verbose_check_secondaries } {
			puts "\t\t$pref.2:\
			    Each skey/key/data tuple is in secondary #$j"
		}
		for { set i 0 } { $i < $nentries } { incr i } {
			set sdb [lindex $sdbs $j]
			set skey [[callback_n $j] $keys($i) $data($i)]
			error_check_good sdb($j)_pget($i) \
			    [$sdb pget -get_both $skey $keys($i)] \
			    [list [list $skey $keys($i) $data($i)]]
		}

		# Make sure this secondary contains only $nentries
		# items.
		if { $verbose_check_secondaries } {
			puts "\t\t$pref.3: Secondary #$j has $nentries items"
		}
		set dbc [$sdb cursor]
		error_check_good dbc($i) \
		    [is_valid_cursor $dbc $sdb] TRUE
		for { set k 0 } { [llength [$dbc get -next]] > 0 } \
		    { incr k } { }
		error_check_good numitems($i) $k $nentries
		error_check_good dbc($i)_close [$dbc close] 0
	}

	if { $verbose_check_secondaries } {
		puts "\t\t$pref.4: Primary has $nentries items"
	}
	set dbc [$pdb cursor]
	error_check_good pdbc [is_valid_cursor $dbc $pdb] TRUE
	for { set k 0 } { [llength [$dbc get -next]] > 0 } { incr k } { }
	error_check_good numitems $k $nentries
	error_check_good pdbc_close [$dbc close] 0
}

# Given a primary database handle and a list of secondary handles, walk
# through the primary and make sure all the secondaries are correct,
# then walk through the secondaries and make sure the primary is correct.
#
# This is slightly less rigorous than the normal check_secondaries--we
# use it whenever we don't have up-to-date "keys" and "data" arrays.
proc cursor_check_secondaries { pdb sdbs nentries { pref "Check" } } {
	global verbose_check_secondaries

	# Make sure each key/data pair in the primary is in each secondary.
	set pdbc [$pdb cursor]
	error_check_good ccs_pdbc [is_valid_cursor $pdbc $pdb] TRUE
	set i 0
	if { $verbose_check_secondaries } {
		puts "\t\t$pref.1:\
		    Key/data in primary => key/data in secondaries"
	}

	for { set dbt [$pdbc get -first] } { [llength $dbt] > 0 } \
	    { set dbt [$pdbc get -next] } {
		incr i
		set pkey [lindex [lindex $dbt 0] 0]
		set pdata [lindex [lindex $dbt 0] 1]
		for { set j 0 } { $j < [llength $sdbs] } { incr j } {
			set sdb [lindex $sdbs $j]
			set sdbt [$sdb pget -get_both \
			    [[callback_n $j] $pkey $pdata] $pkey]
			error_check_good pkey($pkey,$j) \
			    [lindex [lindex $sdbt 0] 1] $pkey
			error_check_good pdata($pdata,$j) \
			    [lindex [lindex $sdbt 0] 2] $pdata
		}
	}
	error_check_good ccs_pdbc_close [$pdbc close] 0
	error_check_good primary_has_nentries $i $nentries

	for { set j 0 } { $j < [llength $sdbs] } { incr j } {
		if { $verbose_check_secondaries } {
			puts "\t\t$pref.2:\
			    Key/data in secondary #$j => key/data in primary"
		}
		set sdb [lindex $sdbs $j]
		set sdbc [$sdb cursor]
		error_check_good ccs_sdbc($j) [is_valid_cursor $sdbc $sdb] TRUE
		set i 0
		for { set dbt [$sdbc pget -first] } { [llength $dbt] > 0 } \
		    { set dbt [$sdbc pget -next] } {
			incr i
			set pkey [lindex [lindex $dbt 0] 1]
			set pdata [lindex [lindex $dbt 0] 2]
			error_check_good pdb_get($pkey/$pdata,$j) \
			    [$pdb get -get_both $pkey $pdata] \
			    [list [list $pkey $pdata]]
		}
		error_check_good secondary($j)_has_nentries $i $nentries

		# To exercise pget -last/pget -prev, we do it backwards too.
		set i 0
		for { set dbt [$sdbc pget -last] } { [llength $dbt] > 0 } \
		    { set dbt [$sdbc pget -prev] } {
			incr i
			set pkey [lindex [lindex $dbt 0] 1]
			set pdata [lindex [lindex $dbt 0] 2]
			error_check_good pdb_get_bkwds($pkey/$pdata,$j) \
			    [$pdb get -get_both $pkey $pdata] \
			    [list [list $pkey $pdata]]
		}
		error_check_good secondary($j)_has_nentries_bkwds $i $nentries

		error_check_good ccs_sdbc_close($j) [$sdbc close] 0
	}
}

# The secondary index tests take a list of the access methods that
# each array ought to use.  Convert at one blow into a list of converted
# argses and omethods for each method in the list.
proc convert_argses { methods largs } {
	set ret {}
	foreach m $methods {
		lappend ret [convert_args $m $largs]
	}
	return $ret
}
proc convert_methods { methods } {
	set ret {}
	foreach m $methods {
		lappend ret [convert_method $m]
	}
	return $ret
}

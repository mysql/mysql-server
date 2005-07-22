# See the file LICENSE for redistribution information.
#
# Copyright (c) 2001-2004
#	Sleepycat Software.  All rights reserved.
#
# $Id: si003.tcl,v 1.12 2004/10/27 20:40:25 carol Exp $
#
# TEST	si003
# TEST	si001 with secondaries created and closed mid-test
# TEST	Basic secondary index put/delete test with secondaries
# TEST	created mid-test.
proc si003 { methods {nentries 200} {tnum "003"} args } {
	source ./include.tcl
	global dict nsecondaries

	# There's no reason to run this test on large lists.
	if { $nentries > 1000 } {
		puts "Skipping si003 for large lists (over 1000 items)"
		return
	}

	# Primary method/args.
	set pmethod [lindex $methods 0]
	set pargs [convert_args $pmethod $args]
	set pomethod [convert_method $pmethod]

	# Method/args for all the secondaries.  If only one method
	# was specified, assume the same method and a standard N
	# secondaries.
	set methods [lrange $methods 1 end]
	if { [llength $methods] == 0 } {
		for { set i 0 } { $i < $nsecondaries } { incr i } {
			lappend methods $pmethod
		}
	}

	set argses [convert_argses $methods $args]
	set omethods [convert_methods $methods]

	puts "si$tnum \{\[ list $pmethod $methods \]\} $nentries" 
	env_cleanup $testdir

	set pname "primary$tnum.db"
	set snamebase "secondary$tnum"

	# Open an environment
	# XXX if one is not supplied!
	set env [eval {berkdb_env -create -home $testdir}]
	error_check_good env_open [is_valid_env $env] TRUE

	# Open the primary.
	set pdb [eval {berkdb_open -create -env} $env $pomethod $pargs $pname]
	error_check_good primary_open [is_valid_db $pdb] TRUE

	puts -nonewline "\tSi$tnum.a: Put loop ... "
	set did [open $dict]
	for { set n 0 } { [gets $did str] != -1 && $n < $nentries } { incr n } {
		if { [is_record_based $pmethod] == 1 } {
			set key [expr $n + 1]
			set datum $str
		} else {
			set key $str
			gets $did datum
		}
		set keys($n) $key
		set data($n) [pad_data $pmethod $datum]

		set ret [eval {$pdb put} {$key [chop_data $pmethod $datum]}]
		error_check_good put($n) $ret 0
	}
	close $did

	# Open and associate the secondaries
	set sdbs {}
	puts "opening secondaries."
	for { set i 0 } { $i < [llength $omethods] } { incr i } {
		set sdb [eval {berkdb_open -create -env} $env \
		    [lindex $omethods $i] [lindex $argses $i] $snamebase.$i.db]
		error_check_good second_open($i) [is_valid_db $sdb] TRUE

		error_check_good db_associate($i) \
		    [$pdb associate -create [callback_n $i] $sdb] 0
		lappend sdbs $sdb
	}
	check_secondaries $pdb $sdbs $nentries keys data "Si$tnum.a"

	puts -nonewline "\tSi$tnum.b: Put/overwrite loop ... "
	for { set n 0 } { $n < $nentries } { incr n } {
		set newd $data($n).$keys($n)
		set ret [eval {$pdb put} {$keys($n) [chop_data $pmethod $newd]}]
		error_check_good put_overwrite($n) $ret 0
		set data($n) [pad_data $pmethod $newd]
	}

	# Close the secondaries again.
	puts "closing secondaries."
	for { set sdb [lindex $sdbs end] } { [string length $sdb] > 0 } \
	    { set sdb [lindex $sdbs end] } {
		error_check_good second_close($sdb) [$sdb close] 0
		set sdbs [lrange $sdbs 0 end-1]
		check_secondaries \
		    $pdb $sdbs $nentries keys data "Si$tnum.b"
	}

	# Delete the second half of the entries through the primary.
	# We do the second half so we can just pass keys(0 ... n/2)
	# to check_secondaries.
	set half [expr $nentries / 2]
	puts -nonewline \
	    "\tSi$tnum.c: Primary delete loop: deleting $half entries ..."
	for { set n $half } { $n < $nentries } { incr n } {
		set ret [$pdb del $keys($n)]
		error_check_good pdel($n) $ret 0
	}

	# Open and associate the secondaries
	set sdbs {}
	puts "\n\t\topening secondaries."
	for { set i 0 } { $i < [llength $omethods] } { incr i } {
		set sdb [eval {berkdb_open -create -env} $env \
		    [lindex $omethods $i] [lindex $argses $i] \
		    $snamebase.r2.$i.db]
		error_check_good second_open($i) [is_valid_db $sdb] TRUE

		error_check_good db_associate($i) \
		    [$pdb associate -create [callback_n $i] $sdb] 0
		lappend sdbs $sdb
	}
	check_secondaries $pdb $sdbs $half keys data "Si$tnum.c"

	# Delete half of what's left, through the first secondary.
	set quar [expr $half / 2]
	puts "\tSi$tnum.d: Secondary delete loop: deleting $quar entries"
	set sdb [lindex $sdbs 0]
	set callback [callback_n 0]
	for { set n $quar } { $n < $half } { incr n } {
		set skey [$callback $keys($n) [pad_data $pmethod $data($n)]]
		set ret [$sdb del $skey]
		error_check_good sdel($n) $ret 0
	}
	check_secondaries $pdb $sdbs $quar keys data "Si$tnum.d"

	foreach sdb $sdbs {
		error_check_good secondary_close [$sdb close] 0
	}
	error_check_good primary_close [$pdb close] 0
	error_check_good env_close [$env close] 0
}

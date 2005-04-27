# See the file LICENSE for redistribution information.
#
# Copyright (c) 2001-2002
#	Sleepycat Software.  All rights reserved.
#
# $Id: si006.tcl,v 1.2 2002/05/15 17:18:03 sandstro Exp $
#
# TEST	sindex006
# TEST	Basic secondary index put/delete test with transactions
proc sindex006 { methods {nentries 200} {tnum 6} args } {
	source ./include.tcl
	global dict nsecondaries

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

	puts "Sindex00$tnum ($pmethod/$methods) $nentries equal key/data pairs"
	puts "    with transactions"
	env_cleanup $testdir

	set pname "primary00$tnum.db"
	set snamebase "secondary00$tnum"

	# Open an environment
	# XXX if one is not supplied!
	set env [berkdb_env -create -home $testdir -txn]
	error_check_good env_open [is_valid_env $env] TRUE

	# Open the primary.
	set pdb [eval {berkdb_open -create -auto_commit -env} $env $pomethod \
	    $pargs $pname]
	error_check_good primary_open [is_valid_db $pdb] TRUE

	# Open and associate the secondaries
	set sdbs {}
	for { set i 0 } { $i < [llength $omethods] } { incr i } {
		set sdb [eval {berkdb_open -create -auto_commit -env} $env \
		    [lindex $omethods $i] [lindex $argses $i] $snamebase.$i.db]
		error_check_good second_open($i) [is_valid_db $sdb] TRUE

		error_check_good db_associate($i) \
		    [$pdb associate -auto_commit [callback_n $i] $sdb] 0
		lappend sdbs $sdb
	}

	puts "\tSindex00$tnum.a: Put loop"
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

		set txn [$env txn]
		set ret [eval {$pdb put} -txn $txn \
		    {$key [chop_data $pmethod $datum]}]
		error_check_good put($n) $ret 0
		error_check_good txn_commit($n) [$txn commit] 0
	}
	close $did
	check_secondaries $pdb $sdbs $nentries keys data "Sindex00$tnum.a"

	puts "\tSindex00$tnum.b: Put/overwrite loop"
	for { set n 0 } { $n < $nentries } { incr n } {
		set newd $data($n).$keys($n)

		set txn [$env txn]
		set ret [eval {$pdb put} -txn $txn \
		    {$keys($n) [chop_data $pmethod $newd]}]
		error_check_good put_overwrite($n) $ret 0
		set data($n) [pad_data $pmethod $newd]
		error_check_good txn_commit($n) [$txn commit] 0
	}
	check_secondaries $pdb $sdbs $nentries keys data "Sindex00$tnum.b"

	# Delete the second half of the entries through the primary.
	# We do the second half so we can just pass keys(0 ... n/2)
	# to check_secondaries.
	set half [expr $nentries / 2]
	puts "\tSindex00$tnum.c: Primary delete loop: deleting $half entries"
	for { set n $half } { $n < $nentries } { incr n } {
		set txn [$env txn]
		set ret [$pdb del -txn $txn $keys($n)]
		error_check_good pdel($n) $ret 0
		error_check_good txn_commit($n) [$txn commit] 0
	}
	check_secondaries $pdb $sdbs $half keys data "Sindex00$tnum.c"

	# Delete half of what's left, through the first secondary.
	set quar [expr $half / 2]
	puts "\tSindex00$tnum.d: Secondary delete loop: deleting $quar entries"
	set sdb [lindex $sdbs 0]
	set callback [callback_n 0]
	for { set n $quar } { $n < $half } { incr n } {
		set skey [$callback $keys($n) [pad_data $pmethod $data($n)]]
		set txn [$env txn]
		set ret [$sdb del -txn $txn $skey]
		error_check_good sdel($n) $ret 0
		error_check_good txn_commit($n) [$txn commit] 0
	}
	check_secondaries $pdb $sdbs $quar keys data "Sindex00$tnum.d"

	puts "\tSindex00$tnum.e: Closing/disassociating primary first"
	error_check_good primary_close [$pdb close] 0
	foreach sdb $sdbs {
		error_check_good secondary_close [$sdb close] 0
	}
	error_check_good env_close [$env close] 0
}

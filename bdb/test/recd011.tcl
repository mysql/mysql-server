# See the file LICENSE for redistribution information.
#
# Copyright (c) 2000
#	Sleepycat Software.  All rights reserved.
#
#	$Id: recd011.tcl,v 11.13 2000/12/06 17:09:54 sue Exp $
#
# Recovery Test 11.
# Test recovery to a specific timestamp.
proc recd011 { method {niter 200} {ckpt_freq 15} {sleep_time 1} args } {
	source ./include.tcl

	set args [convert_args $method $args]
	set omethod [convert_method $method]
	set tnum 11

	puts "Recd0$tnum ($args): Test recovery to a specific timestamp."

	set testfile recd0$tnum.db
	env_cleanup $testdir

	set i 0
	if { [is_record_based $method] == 1 } {
		set key 1
	} else {
		set key KEY
	}

	puts "\tRecd0$tnum.a: Create environment and database."
	set flags "-create -txn -home $testdir"

	set env_cmd "berkdb env $flags"
	set dbenv [eval $env_cmd]
	error_check_good dbenv [is_valid_env $dbenv] TRUE

	set oflags "-env $dbenv -create -mode 0644 $args $omethod"
	set db [eval {berkdb_open} $oflags $testfile]
	error_check_good dbopen [is_valid_db $db] TRUE

	# Main loop:  every second or so, increment the db in a txn.
	puts "\t\tInitial Checkpoint"
	error_check_good "Initial Checkpoint" [$dbenv txn_checkpoint] 0

	puts "\tRecd0$tnum.b ($niter iterations):\
	    Transaction-protected increment loop."
	for { set i 0 } { $i <= $niter } { incr i } {
		set data $i

		# Put, in a txn.
		set txn [$dbenv txn]
		error_check_good txn_begin [is_valid_txn $txn $dbenv] TRUE
		error_check_good db_put \
		    [$db put -txn $txn $key [chop_data $method $data]] 0
		error_check_good txn_commit [$txn commit] 0

		set timeof($i) [timestamp -r]

		# If an appropriate period has elapsed, checkpoint.
		if { $i % $ckpt_freq == $ckpt_freq - 1 } {
			puts "\t\tIteration $i: Checkpointing."
			error_check_good ckpt($i) [$dbenv txn_checkpoint] 0
		}

		# sleep for N seconds.
		tclsleep $sleep_time
	}
	error_check_good db_close [$db close] 0
	error_check_good env_close [$dbenv close] 0

	# Now, loop through and recover to each timestamp, verifying the
	# expected increment.
	puts "\tRecd0$tnum.c: Recover to each timestamp and check."
	for { set i 0 } { $i <= $niter } { incr i } {

		# Run db_recover.
		berkdb debug_check
		set t [clock format $timeof($i) -format "%y%m%d%H%M.%S"]
		set ret [catch {exec $util_path/db_recover -h $testdir -t $t} r]
		error_check_good db_recover($i,$t) $ret 0

		# Now open the db and check the timestamp.
		set db [eval {berkdb_open} $testdir/$testfile]
		error_check_good db_open($i) [is_valid_db $db] TRUE

		set dbt [$db get $key]
		set datum [lindex [lindex $dbt 0] 1]
		error_check_good timestamp_recover $datum [pad_data $method $i]

		error_check_good db_close [$db close] 0
	}

	# Finally, recover to a time well before the first timestamp
	# and well after the last timestamp.  The latter should
	# be just like the last timestamp;  the former should fail.
	puts "\tRecd0$tnum.d: Recover to before the first timestamp."
	set t [clock format [expr $timeof(0) - 1000] -format "%y%m%d%H%M.%S"]
	set ret [catch {exec $util_path/db_recover -h $testdir -t $t} r]
	error_check_bad db_recover(before,$t) $ret 0

	puts "\tRecd0$tnum.e: Recover to after the last timestamp."
	set t [clock format \
	    [expr $timeof($niter) + 1000] -format "%y%m%d%H%M.%S"]
	set ret [catch {exec $util_path/db_recover -h $testdir -t $t} r]
	error_check_good db_recover(after,$t) $ret 0

	# Now open the db and check the timestamp.
	set db [eval {berkdb_open} $testdir/$testfile]
	error_check_good db_open(after) [is_valid_db $db] TRUE

	set dbt [$db get $key]
	set datum [lindex [lindex $dbt 0] 1]

	error_check_good timestamp_recover $datum [pad_data $method $niter]
	error_check_good db_close [$db close] 0
}

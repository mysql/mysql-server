# See the file LICENSE for redistribution information.
#
# Copyright (c) 1996, 1997, 1998, 1999, 2000
#	Sleepycat Software.  All rights reserved.
#
#	$Id: recd001.tcl,v 11.28 2000/12/07 19:13:46 sue Exp $
#
# Recovery Test 1.
# These are the most basic recovery tests.  We do individual recovery
# tests for each operation in the access method interface.  First we
# create a file and capture the state of the database (i.e., we copy
# it.  Then we run a transaction containing a single operation.  In
# one test, we abort the transaction and compare the outcome to the
# original copy of the file.  In the second test, we restore the
# original copy of the database and then run recovery and compare
# this against the actual database.
proc recd001 { method {select 0} args} {
	global fixed_len
	source ./include.tcl

	set orig_fixed_len $fixed_len
	set opts [convert_args $method $args]
	set omethod [convert_method $method]

	puts "Recd001: $method operation/transaction tests"

	# Create the database and environment.
	env_cleanup $testdir

	# The recovery tests were originally written to
	# do a command, abort, do it again, commit, and then
	# repeat the sequence with another command.  Each command
	# tends to require that the previous command succeeded and
	# left the database a certain way.  To avoid cluttering up the
	# op_recover interface as well as the test code, we create two
	# databases;  one does abort and then commit for each op, the
	# other does prepare, prepare-abort, and prepare-commit for each
	# op.  If all goes well, this allows each command to depend
	# exactly one successful iteration of the previous command.
	set testfile recd001.db
	set testfile2 recd001-2.db

	set flags "-create -txn -home $testdir"

	puts "\tRecd001.a.0: creating environment"
	set env_cmd "berkdb env $flags"
	set dbenv [eval $env_cmd]
	error_check_good dbenv [is_valid_env $dbenv] TRUE

	#
	# We need to create a database to get the pagesize (either
	# the default or whatever might have been specified).
	# Then remove it so we can compute fixed_len and create the
	# real database.
	set oflags "-create $omethod -mode 0644 \
	    -env $dbenv $opts $testfile"
	set db [eval {berkdb_open} $oflags]
	error_check_good db_open [is_valid_db $db] TRUE
	set stat [$db stat]
	#
	# Compute the fixed_len based on the pagesize being used.
	# We want the fixed_len to be 1/4 the pagesize.
	#
	set pg [get_pagesize $stat]
	error_check_bad get_pagesize $pg -1
	set fixed_len [expr $pg / 4]
	error_check_good db_close [$db close] 0
	error_check_good dbremove [berkdb dbremove -env $dbenv $testfile] 0

	# Convert the args again because fixed_len is now real.
	# Create the databases and close the environment.
	# cannot specify db truncate in txn protected env!!!
	set opts [convert_args $method ""]
	set omethod [convert_method $method]
	set oflags "-create $omethod -mode 0644 \
	    -env $dbenv $opts $testfile"
	set db [eval {berkdb_open} $oflags]
	error_check_good db_open [is_valid_db $db] TRUE
	error_check_good db_close [$db close] 0

	set oflags "-create $omethod -mode 0644 \
	    -env $dbenv $opts $testfile2"
	set db [eval {berkdb_open} $oflags]
	error_check_good db_open [is_valid_db $db] TRUE
	error_check_good db_close [$db close] 0

	error_check_good env_close [$dbenv close] 0

	puts "\tRecd001.a.1: Verify db_printlog can read logfile"
	set tmpfile $testdir/printlog.out
	set stat [catch {exec $util_path/db_printlog -h $testdir \
	    > $tmpfile} ret]
	error_check_good db_printlog $stat 0
	fileremove $tmpfile

	# List of recovery tests: {CMD MSG} pairs.
	set rlist {
	{ {DB put -txn TXNID $key $data}	"Recd001.b: put"}
	{ {DB del -txn TXNID $key}		"Recd001.c: delete"}
	{ {DB put -txn TXNID $bigkey $data}	"Recd001.d: big key put"}
	{ {DB del -txn TXNID $bigkey}		"Recd001.e: big key delete"}
	{ {DB put -txn TXNID $key $bigdata}	"Recd001.f: big data put"}
	{ {DB del -txn TXNID $key}		"Recd001.g: big data delete"}
	{ {DB put -txn TXNID $key $data}	"Recd001.h: put (change state)"}
	{ {DB put -txn TXNID $key $newdata}	"Recd001.i: overwrite"}
	{ {DB put -txn TXNID -partial {$off $len} $key $partial_grow}
	  "Recd001.j: partial put growing"}
	{ {DB put -txn TXNID $key $newdata}	"Recd001.k: overwrite (fix)"}
	{ {DB put -txn TXNID -partial {$off $len} $key $partial_shrink}
	  "Recd001.l: partial put shrinking"}
	{ {DB put -txn TXNID -append $data}	"Recd001.m: put -append"}
	{ {DB get -txn TXNID -consume}		"Recd001.n: db get -consume"}
	}

	# These are all the data values that we're going to need to read
	# through the operation table and run the recovery tests.

	if { [is_record_based $method] == 1 } {
		set key 1
	} else {
		set key recd001_key
	}
	set data recd001_data
	set newdata NEWrecd001_dataNEW
	set off 3
	set len 12
	set partial_grow replacement_record_grow
	set partial_shrink xxx
	if { [is_fixed_length $method] == 1 } {
		set len [string length $partial_grow]
		set partial_shrink $partial_grow
	}
	set bigdata [replicate $key $fixed_len]
	if { [is_record_based $method] == 1 } {
		set bigkey $fixed_len
	} else {
		set bigkey [replicate $key $fixed_len]
	}

	foreach pair $rlist {
		set cmd [subst [lindex $pair 0]]
		set msg [lindex $pair 1]
		if { $select != 0 } {
			set tag [lindex $msg 0]
			set tail [expr [string length $tag] - 2]
			set tag [string range $tag $tail $tail]
			if { [lsearch $select $tag] == -1 } {
				continue
			}
		}

		if { [is_queue $method] != 1 } {
			if { [string first append $cmd] != -1 } {
				continue
			}
			if { [string first consume $cmd] != -1 } {
				continue
			}
		}

#		if { [is_fixed_length $method] == 1 } {
#			if { [string first partial $cmd] != -1 } {
#				continue
#			}
#		}
		op_recover abort $testdir $env_cmd $testfile $cmd $msg
		op_recover commit $testdir $env_cmd $testfile $cmd $msg
		op_recover prepare $testdir $env_cmd $testfile2 $cmd $msg
		op_recover prepare-abort $testdir $env_cmd $testfile2 $cmd $msg
		op_recover prepare-commit $testdir $env_cmd $testfile2 $cmd $msg
	}
	set fixed_len $orig_fixed_len

	puts "\tRecd001.o: Verify db_printlog can read logfile"
	set tmpfile $testdir/printlog.out
	set stat [catch {exec $util_path/db_printlog -h $testdir \
	    > $tmpfile} ret]
	error_check_good db_printlog $stat 0
	fileremove $tmpfile
}

# See the file LICENSE for redistribution information.
#
# Copyright (c) 1996-2002
#	Sleepycat Software.  All rights reserved.
#
# $Id: log001.tcl,v 11.29 2002/04/30 20:27:56 sue Exp $
#

# TEST	log001
# TEST	Read/write log records.
proc log001 { } {
	global passwd
	global rand_init

	berkdb srand $rand_init
	set iter 1000
	set max [expr 1024 * 128]
	log001_body $max $iter 1
	log001_body $max $iter 0
	log001_body $max $iter 1 "-encryptaes $passwd"
	log001_body $max $iter 0 "-encryptaes $passwd"
	log001_body $max [expr $iter * 15] 1
	log001_body $max [expr $iter * 15] 0
	log001_body $max [expr $iter * 15] 1 "-encryptaes $passwd"
	log001_body $max [expr $iter * 15] 0 "-encryptaes $passwd"
}

proc log001_body { max nrecs fixedlength {encargs ""} } {
	source ./include.tcl

	puts -nonewline "Log001: Basic put/get log records "
	if { $fixedlength == 1 } {
		puts "(fixed-length $encargs)"
	} else {
		puts "(variable-length $encargs)"
	}

	env_cleanup $testdir

	set env [eval {berkdb_env -log -create -home $testdir -mode 0644} \
	    $encargs -log_max $max]
	error_check_good envopen [is_valid_env $env] TRUE

	# We will write records to the log and make sure we can
	# read them back correctly.  We'll use a standard pattern
	# repeated some number of times for each record.
	set lsn_list {}
	set rec_list {}
	puts "\tLog001.a: Writing $nrecs log records"
	for { set i 0 } { $i < $nrecs } { incr i } {
		set rec ""
		for { set j 0 } { $j < [expr $i % 10 + 1] } {incr j} {
			set rec $rec$i:logrec:$i
		}
		if { $fixedlength != 1 } {
			set rec $rec:[random_data 237 0 0]
		}
		set lsn [$env log_put $rec]
		error_check_bad log_put [is_substr $lsn log_cmd] 1
		lappend lsn_list $lsn
		lappend rec_list $rec
	}

	# Open a log cursor.
	set logc [$env log_cursor]
	error_check_good logc [is_valid_logc $logc $env] TRUE

	puts "\tLog001.b: Retrieving log records sequentially (forward)"
	set i 0
	for { set grec [$logc get -first] } { [llength $grec] != 0 } {
		set grec [$logc get -next]} {
		error_check_good log_get:seq [lindex $grec 1] \
						 [lindex $rec_list $i]
		incr i
	}

	puts "\tLog001.c: Retrieving log records sequentially (backward)"
	set i [llength $rec_list]
	for { set grec [$logc get -last] } { [llength $grec] != 0 } {
	    set grec [$logc get -prev] } {
		incr i -1
		error_check_good \
		    log_get:seq [lindex $grec 1] [lindex $rec_list $i]
	}

	puts "\tLog001.d: Retrieving log records sequentially by LSN"
	set i 0
	foreach lsn $lsn_list {
		set grec [$logc get -set $lsn]
		error_check_good \
		    log_get:seq [lindex $grec 1] [lindex $rec_list $i]
		incr i
	}

	puts "\tLog001.e: Retrieving log records randomly by LSN"
	set m [expr [llength $lsn_list] - 1]
	for { set i 0 } { $i < $nrecs } { incr i } {
		set recno [berkdb random_int 0 $m ]
		set lsn [lindex $lsn_list $recno]
		set grec [$logc get -set $lsn]
		error_check_good \
		    log_get:seq [lindex $grec 1] [lindex $rec_list $recno]
	}

	puts "\tLog001.f: Retrieving first/current, last/current log record"
	set grec [$logc get -first]
	error_check_good log_get:seq [lindex $grec 1] [lindex $rec_list 0]
	set grec [$logc get -current]
	error_check_good log_get:seq [lindex $grec 1] [lindex $rec_list 0]
	set i [expr [llength $rec_list] - 1]
	set grec [$logc get -last]
	error_check_good log_get:seq [lindex $grec 1] [lindex $rec_list $i]
	set grec [$logc get -current]
	error_check_good log_get:seq [lindex $grec 1] [lindex $rec_list $i]

	# Close and unlink the file
	error_check_good log_cursor:close:$logc [$logc close] 0
	error_check_good env:close [$env close] 0
	error_check_good envremove [berkdb envremove -home $testdir] 0
}

# See the file LICENSE for redistribution information.
#
# Copyright (c) 1999, 2000
#	Sleepycat Software.  All rights reserved.
#
#	$Id: env007.tcl,v 11.5 2000/08/25 14:21:50 sue Exp $
#
# Env Test 007
# Test various config file options.
#	1) Make sure command line option is respected
#	2) Make sure that config file option is respected
#	3) Make sure that if -both- DB_CONFIG and the set_<whatever>
#		method is used,	only the file is respected.
proc env007 { } {
	#   env007 is essentially just a small driver that runs
	# env007_body twice.  First, it supplies a "set" argument
	# to use with environment opens, and the second time it sets
	# DB_CONFIG instead.
	#   Note that env007_body itself calls env007_run_test to run
	# the body of the actual test.

	source ./include.tcl

	puts "Env007: DB_CONFIG test."

	#
	# Test only those options we can easily check via stat
	#
	set rlist {
	{ " -txn_max " "set_tx_max" "19" "31" "Env007.a: Txn Max"
	    "txn_stat" "Max Txns"}
	{ " -lock_max " "set_lk_max" "19" "31" "Env007.b: Lock Max"
	    "lock_stat" "Max locks"}
	{ " -log_buffer " "set_lg_bsize" "65536" "131072" "Env007.c: Log Bsize"
	    "log_stat" "Log record cache size"}
	{ " -log_max " "set_lg_max" "8388608" "9437184" "Env007.d: Log Max"
	    "log_stat" "Maximum log file size"}
	}

	set e "berkdb env -create -mode 0644 -home $testdir -log -lock -txn "
	foreach item $rlist {
		set envarg [lindex $item 0]
		set configarg [lindex $item 1]
		set envval [lindex $item 2]
		set configval [lindex $item 3]
		set msg [lindex $item 4]
		set statcmd [lindex $item 5]
		set statstr [lindex $item 6]

		env_cleanup $testdir
		# First verify using just env args
		puts "\t$msg Environment argument only"
		set env [eval $e $envarg $envval]
		error_check_good envopen:0 [is_valid_env $env] TRUE
		env007_check $env $statcmd $statstr $envval
		error_check_good envclose:0 [$env close] 0

		env_cleanup $testdir
		env007_make_config $configarg $configval

		#  verify using just config file
		puts "\t$msg Config file only"
		set env [eval $e]
		error_check_good envopen:1 [is_valid_env $env] TRUE
		env007_check $env $statcmd $statstr $configval
		error_check_good envclose:1 [$env close] 0

		# First verify using just env args
		puts "\t$msg Environment arg and config file"
		set env [eval $e $envarg $envval]
		error_check_good envopen:2 [is_valid_env $env] TRUE
		env007_check $env $statcmd $statstr $configval
		error_check_good envclose:2 [$env close] 0
	}
}

proc env007_check { env statcmd statstr testval } {
	set stat [$env $statcmd]
	set checked 0
	foreach statpair $stat {
		if {$checked == 1} {
			break
		}
		set statmsg [lindex $statpair 0]
		set statval [lindex $statpair 1]
		if {[is_substr $statmsg $statstr] != 0} {
			set checked 1
			error_check_good $statstr:ck $statval $testval
		}
	}
	error_check_good $statstr:test $checked 1
}

proc env007_make_config { carg cval } {
	global testdir

	set cid [open $testdir/DB_CONFIG w]
	puts $cid "$carg $cval"
	close $cid
}

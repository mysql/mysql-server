# See the file LICENSE for redistribution information.
#
# Copyright (c) 1999-2002
#	Sleepycat Software.  All rights reserved.
#
# $Id: env007.tcl,v 11.21 2002/08/12 20:49:36 sandstro Exp $
#
# TEST	env007
# TEST	Test various DB_CONFIG config file options.
# TEST	1) Make sure command line option is respected
# TEST	2) Make sure that config file option is respected
# TEST	3) Make sure that if -both- DB_CONFIG and the set_<whatever>
# TEST		method is used,	only the file is respected.
# TEST	Then test all known config options.
proc env007 { } {
	global errorInfo

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
	{ " -lock_max_locks " "set_lk_max_locks" "17" "29" "Env007.b: Lock Max"
	    "lock_stat" "Maximum locks"}
	{ " -lock_max_lockers " "set_lk_max_lockers" "1500" "2000"
	    "Env007.c: Max Lockers" "lock_stat" "Maximum lockers"}
	{ " -lock_max_objects " "set_lk_max_objects" "1500" "2000"
	    "Env007.d: Max Objects" "lock_stat" "Maximum objects"}
	{ " -log_buffer " "set_lg_bsize" "65536" "131072" "Env007.e: Log Bsize"
	    "log_stat" "Log record cache size"}
	{ " -log_max " "set_lg_max" "8388608" "9437184" "Env007.f: Log Max"
	    "log_stat" "Current log file size"}
	}

	set e "berkdb_env -create -mode 0644 -home $testdir -log -lock -txn "
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

	#
	# Test all options.  For all config options, write it out
	# to the file and make sure we can open the env.  We cannot
	# necessarily check via stat that it worked but this execs
	# the config file code itself.
	#
	set cfglist {
	{ "set_cachesize" "0 1048576 0" }
	{ "set_data_dir" "." }
	{ "set_flags" "db_cdb_alldb" }
	{ "set_flags" "db_direct_db" }
	{ "set_flags" "db_direct_log" }
	{ "set_flags" "db_nolocking" }
	{ "set_flags" "db_nommap" }
	{ "set_flags" "db_nopanic" }
	{ "set_flags" "db_overwrite" }
	{ "set_flags" "db_region_init" }
	{ "set_flags" "db_txn_nosync" }
	{ "set_flags" "db_txn_write_nosync" }
	{ "set_flags" "db_yieldcpu" }
	{ "set_lg_bsize" "65536" }
	{ "set_lg_dir" "." }
	{ "set_lg_max" "8388608" }
	{ "set_lg_regionmax" "65536" }
	{ "set_lk_detect" "db_lock_default" }
	{ "set_lk_detect" "db_lock_expire" }
	{ "set_lk_detect" "db_lock_maxlocks" }
	{ "set_lk_detect" "db_lock_minlocks" }
	{ "set_lk_detect" "db_lock_minwrite" }
	{ "set_lk_detect" "db_lock_oldest" }
	{ "set_lk_detect" "db_lock_random" }
	{ "set_lk_detect" "db_lock_youngest" }
	{ "set_lk_max" "50" }
	{ "set_lk_max_lockers" "1500" }
	{ "set_lk_max_locks" "29" }
	{ "set_lk_max_objects" "1500" }
	{ "set_lock_timeout" "100" }
	{ "set_mp_mmapsize" "12582912" }
	{ "set_region_init" "1" }
	{ "set_shm_key" "15" }
	{ "set_tas_spins" "15" }
	{ "set_tmp_dir" "." }
	{ "set_tx_max" "31" }
	{ "set_txn_timeout" "100" }
	{ "set_verbose" "db_verb_chkpoint" }
	{ "set_verbose" "db_verb_deadlock" }
	{ "set_verbose" "db_verb_recovery" }
	{ "set_verbose" "db_verb_waitsfor" }
	}

	puts "\tEnv007.g: Config file settings"
	set e "berkdb_env -create -mode 0644 -home $testdir -log -lock -txn "
	foreach item $cfglist {
		env_cleanup $testdir
		set configarg [lindex $item 0]
		set configval [lindex $item 1]

		env007_make_config $configarg $configval

		#  verify using just config file
		puts "\t\t $configarg $configval"
		set env [eval $e]
		error_check_good envvalid:1 [is_valid_env $env] TRUE
		error_check_good envclose:1 [$env close] 0
	}

	set cfglist {
	{ "set_cachesize" "1048576" }
	{ "set_flags" "db_xxx" }
	{ "set_flags" "1" }
	{ "set_flags" "db_txn_nosync x" }
	{ "set_lg_bsize" "db_xxx" }
	{ "set_lg_max" "db_xxx" }
	{ "set_lg_regionmax" "db_xxx" }
	{ "set_lk_detect" "db_xxx" }
	{ "set_lk_detect" "1" }
	{ "set_lk_detect" "db_lock_youngest x" }
	{ "set_lk_max" "db_xxx" }
	{ "set_lk_max_locks" "db_xxx" }
	{ "set_lk_max_lockers" "db_xxx" }
	{ "set_lk_max_objects" "db_xxx" }
	{ "set_mp_mmapsize" "db_xxx" }
	{ "set_region_init" "db_xxx" }
	{ "set_shm_key" "db_xxx" }
	{ "set_tas_spins" "db_xxx" }
	{ "set_tx_max" "db_xxx" }
	{ "set_verbose" "db_xxx" }
	{ "set_verbose" "1" }
	{ "set_verbose" "db_verb_recovery x" }
	}
	puts "\tEnv007.h: Config value errors"
	set e "berkdb_env_noerr -create -mode 0644 \
	    -home $testdir -log -lock -txn "
	foreach item $cfglist {
		set configarg [lindex $item 0]
		set configval [lindex $item 1]

		env007_make_config $configarg $configval

		#  verify using just config file
		puts "\t\t $configarg $configval"
		set stat [catch {eval $e} ret]
		error_check_good envopen $stat 1
		error_check_good error [is_substr $errorInfo \
		    "incorrect arguments for name-value pair"] 1
	}

	puts "\tEnv007.i: Config name error set_xxx"
	set e "berkdb_env_noerr -create -mode 0644 \
	    -home $testdir -log -lock -txn "
	env007_make_config "set_xxx" 1
	set stat [catch {eval $e} ret]
	error_check_good envopen $stat 1
	error_check_good error [is_substr $errorInfo \
		    "unrecognized name-value pair"] 1
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

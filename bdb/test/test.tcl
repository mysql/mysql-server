# See the file LICENSE for redistribution information.
#
# Copyright (c) 1996-2002
#	Sleepycat Software.  All rights reserved.
#
# $Id: test.tcl,v 11.225 2002/09/10 18:51:38 sue Exp $

source ./include.tcl

# Load DB's TCL API.
load $tcllib

if { [file exists $testdir] != 1 } {
	file mkdir $testdir
}

global __debug_print
global __debug_on
global __debug_test
global util_path

#
# Test if utilities work to figure out the path.  Most systems
# use ., but QNX has a problem with execvp of shell scripts which
# causes it to break.
#
set stat [catch {exec ./db_printlog -?} ret]
if { [string first "exec format error" $ret] != -1 } {
	set util_path ./.libs
} else {
	set util_path .
}
set __debug_print 0
set encrypt 0
set old_encrypt 0
set passwd test_passwd

# This is where the test numbering and parameters now live.
source $test_path/testparams.tcl

# Error stream that (should!) always go to the console, even if we're
# redirecting to ALL.OUT.
set consoleerr stderr

foreach sub $subs {
	if { [info exists num_test($sub)] != 1 } {
		puts stderr "Subsystem $sub has no number of tests specified in\
		    testparams.tcl; skipping."
		continue
	}
	set end $num_test($sub)
	for { set i 1 } { $i <= $end } {incr i} {
		set name [format "%s%03d.tcl" $sub $i]
		source $test_path/$name
	}
}

source $test_path/archive.tcl
source $test_path/byteorder.tcl
source $test_path/dbm.tcl
source $test_path/hsearch.tcl
source $test_path/join.tcl
source $test_path/logtrack.tcl
source $test_path/ndbm.tcl
source $test_path/parallel.tcl
source $test_path/reputils.tcl
source $test_path/sdbutils.tcl
source $test_path/shelltest.tcl
source $test_path/sindex.tcl
source $test_path/testutils.tcl
source $test_path/upgrade.tcl

set dict $test_path/wordlist
set alphabet "abcdefghijklmnopqrstuvwxyz"
set datastr "abcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyz"

# Random number seed.
global rand_init
set rand_init 101301

# Default record length and padding character for
# fixed record length access method(s)
set fixed_len 20
set fixed_pad 0

set recd_debug	0
set log_log_record_types 0
set ohandles {}

# Normally, we're not running an all-tests-in-one-env run.  This matters
# for error stream/error prefix settings in berkdb_open.
global is_envmethod
set is_envmethod 0

# For testing locker id wrap around.
global lock_curid
global lock_maxid
set lock_curid 0
set lock_maxid 2147483647
global txn_curid
global txn_maxid
set txn_curid 2147483648
set txn_maxid 4294967295

# Set up any OS-specific values
global tcl_platform
set is_windows_test [is_substr $tcl_platform(os) "Win"]
set is_hp_test [is_substr $tcl_platform(os) "HP-UX"]
set is_qnx_test [is_substr $tcl_platform(os) "QNX"]

# From here on out, test.tcl contains the procs that are used to
# run all or part of the test suite.

proc run_std { args } {
	global num_test
	source ./include.tcl

	set exflgs [eval extractflags $args]
	set args [lindex $exflgs 0]
	set flags [lindex $exflgs 1]

	set display 1
	set run 1
	set am_only 0
	set no_am 0
	set std_only 1
	set rflags {--}
	foreach f $flags {
		switch $f {
			A {
				set std_only 0
			}
			M {
				set no_am 1
				puts "run_std: all but access method tests."
			}
			m {
				set am_only 1
				puts "run_std: access method tests only."
			}
			n {
				set display 1
				set run 0
				set rflags [linsert $rflags 0 "-n"]
			}
		}
	}

	if { $std_only == 1 } {
		fileremove -f ALL.OUT

		set o [open ALL.OUT a]
		if { $run == 1 } {
			puts -nonewline "Test suite run started at: "
			puts [clock format [clock seconds] -format "%H:%M %D"]
			puts [berkdb version -string]

			puts -nonewline $o "Test suite run started at: "
			puts $o [clock format [clock seconds] -format "%H:%M %D"]
			puts $o [berkdb version -string]
		}
		close $o
	}

	set test_list {
	{"environment"		"env"}
	{"archive"		"archive"}
	{"locking"		"lock"}
	{"logging"		"log"}
	{"memory pool"		"memp"}
	{"mutex"		"mutex"}
	{"transaction"		"txn"}
	{"deadlock detection"	"dead"}
	{"subdatabase"		"sdb"}
	{"byte-order"		"byte"}
	{"recno backing file"	"rsrc"}
	{"DBM interface"	"dbm"}
	{"NDBM interface"	"ndbm"}
	{"Hsearch interface"	"hsearch"}
	{"secondary index"	"sindex"}
	}

	if { $am_only == 0 } {

		foreach pair $test_list {
			set msg [lindex $pair 0]
			set cmd [lindex $pair 1]
			puts "Running $msg tests"
			if [catch {exec $tclsh_path \
			    << "source $test_path/test.tcl; r $rflags $cmd" \
			    >>& ALL.OUT } res] {
				set o [open ALL.OUT a]
				puts $o "FAIL: $cmd test"
				close $o
			}
		}

		# Run recovery tests.
		#
		# XXX These too are broken into separate tclsh instantiations
		# so we don't require so much memory, but I think it's cleaner
		# and more useful to do it down inside proc r than here,
		# since "r recd" gets done a lot and needs to work.
		#
		# Note that we still wrap the test in an exec so that
		# its output goes to ALL.OUT.  run_recd will wrap each test
		# so that both error streams go to stdout (which here goes
		# to ALL.OUT);  information that run_recd wishes to print
		# to the "real" stderr, but outside the wrapping for each test,
		# such as which tests are being skipped, it can still send to
		# stderr.
		puts "Running recovery tests"
		if [catch {
		    exec $tclsh_path \
			<< "source $test_path/test.tcl; r $rflags recd" \
			2>@ stderr >> ALL.OUT
		    } res] {
			set o [open ALL.OUT a]
			puts $o "FAIL: recd tests"
			close $o
		}

		# Run join test
		#
		# XXX
		# Broken up into separate tclsh instantiations so we don't
		# require so much memory.
		puts "Running join test"
		foreach i "join1 join2 join3 join4 join5 join6" {
			if [catch {exec $tclsh_path \
			    << "source $test_path/test.tcl; r $rflags $i" \
			    >>& ALL.OUT } res] {
				set o [open ALL.OUT a]
				puts $o "FAIL: $i test"
				close $o
			}
		}
	}

	if { $no_am == 0 } {
		# Access method tests.
		#
		# XXX
		# Broken up into separate tclsh instantiations so we don't
		# require so much memory.
		foreach i \
		    "btree hash queue queueext recno rbtree frecno rrecno" {
			puts "Running $i tests"
			for { set j 1 } { $j <= $num_test(test) } {incr j} {
				if { $run == 0 } {
					set o [open ALL.OUT a]
					run_method -$i $j $j $display $run $o
					close $o
				}
				if { $run } {
					if [catch {exec $tclsh_path \
					    << "source $test_path/test.tcl; \
					    run_method -$i $j $j $display $run"\
					    >>& ALL.OUT } res] {
						set o [open ALL.OUT a]
						puts $o "FAIL:\
						    [format "test%03d" $j] $i"
						close $o
					}
				}
			}
		}
	}

	# If not actually running, no need to check for failure.
	# If running in the context of the larger 'run_all' we don't
	# check for failure here either.
	if { $run == 0 || $std_only == 0 } {
		return
	}

	set failed [check_failed_run ALL.OUT]

	set o [open ALL.OUT a]
	if { $failed == 0 } {
		puts "Regression Tests Succeeded"
		puts $o "Regression Tests Succeeded"
	} else {
		puts "Regression Tests Failed; see ALL.OUT for log"
		puts $o "Regression Tests Failed"
	}

	puts -nonewline "Test suite run completed at: "
	puts [clock format [clock seconds] -format "%H:%M %D"]
	puts -nonewline $o "Test suite run completed at: "
	puts $o [clock format [clock seconds] -format "%H:%M %D"]
	close $o
}

proc check_failed_run { file {text "^FAIL"}} {
	set failed 0
	set o [open $file r]
	while { [gets $o line] >= 0 } {
		set ret [regexp $text $line]
		if { $ret != 0 } {
			set failed 1
		}
	}
	close $o

	return $failed
}

proc r { args } {
	global num_test
	source ./include.tcl

	set exflgs [eval extractflags $args]
	set args [lindex $exflgs 0]
	set flags [lindex $exflgs 1]

	set display 1
	set run 1
	set saveflags "--"
	foreach f $flags {
		switch $f {
			n {
				set display 1
				set run 0
				set saveflags "-n $saveflags"
			}
		}
	}

	if {[catch {
		set sub [ lindex $args 0 ]
		switch $sub {
			byte {
				if { $display } {
					puts "run_test byteorder"
				}
				if { $run } {
					check_handles
					run_test byteorder
				}
			}
			archive -
			dbm -
			hsearch -
			ndbm -
			shelltest -
			sindex {
				if { $display } { puts "r $sub" }
				if { $run } {
					check_handles
					$sub
				}
			}
			bigfile -
			dead -
			env -
			lock -
			log -
			memp -
			mutex -
			rsrc -
			sdbtest -
			txn {
				if { $display } { run_subsystem $sub 1 0 }
				if { $run } {
					run_subsystem $sub
				}
			}
			join {
				eval r $saveflags join1
				eval r $saveflags join2
				eval r $saveflags join3
				eval r $saveflags join4
				eval r $saveflags join5
				eval r $saveflags join6
			}
			join1 {
				if { $display } { puts jointest }
				if { $run } {
					check_handles
					jointest
				}
			}
			joinbench {
				puts "[timestamp]"
				eval r $saveflags join1
				eval r $saveflags join2
				puts "[timestamp]"
			}
			join2 {
				if { $display } { puts "jointest 512" }
				if { $run } {
					check_handles
					jointest 512
				}
			}
			join3 {
				if { $display } {
					puts "jointest 8192 0 -join_item"
				}
				if { $run } {
					check_handles
					jointest 8192 0 -join_item
				}
			}
			join4 {
				if { $display } { puts "jointest 8192 2" }
				if { $run } {
					check_handles
					jointest 8192 2
				}
			}
			join5 {
				if { $display } { puts "jointest 8192 3" }
				if { $run } {
					check_handles
					jointest 8192 3
				}
			}
			join6 {
				if { $display } { puts "jointest 512 3" }
				if { $run } {
					check_handles
					jointest 512 3
				}
			}
			recd {
				check_handles
				run_recds $run $display [lrange $args 1 end]
			}
			rep {
				for { set j 1 } { $j <= $num_test(test) } \
				    { incr j } {
					if { $display } {
						puts "eval run_test \
						    run_repmethod 0 $j $j"
					}
					if { $run } {
						eval run_test \
						    run_repmethod 0 $j $j
					}
				}
				for { set i 1 } \
				    { $i <= $num_test(rep) } {incr i} {
					set test [format "%s%03d" $sub $i]
					if { $i == 2 } {
						if { $run } {
							puts "Skipping rep002 \
							    (waiting on SR #6195)"
						}
						continue
					}
					if { $display } {
						puts "run_test $test"
					}
					if { $run } {
						run_test $test
					}
				}
			}
			rpc {
				if { $display } { puts "r $sub" }
				global rpc_svc svc_list
				set old_rpc_src $rpc_svc
				foreach rpc_svc $svc_list {
					if { !$run || \
					  ![file exist $util_path/$rpc_svc] } {
						continue
					}
					run_subsystem rpc
					if { [catch {run_rpcmethod -txn} ret] != 0 } {
						puts $ret
					}
					run_test run_rpcmethod
				}
				set rpc_svc $old_rpc_src
			}
			sec {
				if { $display } {
					run_subsystem $sub 1 0
				}
				if { $run } {
					run_subsystem $sub 0 1
				}
				for { set j 1 } { $j <= $num_test(test) } \
				    { incr j } {
					if { $display } {
						puts "eval run_test \
						    run_secmethod $j $j"
						puts "eval run_test \
						    run_secenv $j $j"
					}
					if { $run } {
						eval run_test \
						    run_secmethod $j $j
						eval run_test \
						    run_secenv $j $j
					}
				}
			}
			sdb {
				if { $display } {
					puts "eval r $saveflags sdbtest"
					for { set j 1 } \
					    { $j <= $num_test(sdb) } \
					    { incr j } {
						puts "eval run_test \
						    subdb $j $j"
					}
				}
				if { $run } {
					eval r $saveflags sdbtest
					for { set j 1 } \
					    { $j <= $num_test(sdb) } \
					    { incr j } {
						eval run_test subdb $j $j
					}
				}
			}
			btree -
			rbtree -
			hash -
			queue -
			queueext -
			recno -
			frecno -
			rrecno {
				eval run_method [lindex $args 0] \
				    1 0 $display $run [lrange $args 1 end]
			}

			default {
				error \
				    "FAIL:[timestamp] r: $args: unknown command"
			}
		}
		flush stdout
		flush stderr
	} res] != 0} {
		global errorInfo;

		set fnl [string first "\n" $errorInfo]
		set theError [string range $errorInfo 0 [expr $fnl - 1]]
		if {[string first FAIL $errorInfo] == -1} {
			error "FAIL:[timestamp] r: $args: $theError"
		} else {
			error $theError;
		}
	}
}

proc run_subsystem { prefix { display 0 } { run 1} } {
	global num_test
	if { [info exists num_test($prefix)] != 1 } {
		puts stderr "Subsystem $sub has no number of tests specified in\
		    testparams.tcl; skipping."
		return
	}
	for { set i 1 } { $i <= $num_test($prefix) } {incr i} {
		set name [format "%s%03d" $prefix $i]
		if { $display } {
			puts "eval $name"
		}
		if { $run } {
			check_handles
			catch {eval $name}
		}
	}
}

proc run_test { testname args } {
	source ./include.tcl
	foreach method "hash queue queueext recno rbtree frecno rrecno btree" {
	 	check_handles
		eval $testname -$method $args
		verify_dir $testdir "" 1
	}
}

proc run_method { method {start 1} {stop 0} {display 0} {run 1} \
    { outfile stdout } args } {
	global __debug_on
	global __debug_print
	global num_test
	global parms
	source ./include.tcl

	if { $stop == 0 } {
		set stop $num_test(test)
	}
	if { $run == 1 } {
		puts $outfile "run_method: $method $start $stop $args"
	}

	if {[catch {
		for { set i $start } { $i <= $stop } {incr i} {
			set name [format "test%03d" $i]
			if { [info exists parms($name)] != 1 } {
				puts stderr "[format Test%03d $i] disabled in\
				    testparams.tcl; skipping."
				continue
			}
			if { $display } {
				puts -nonewline $outfile "eval $name $method"
				puts -nonewline $outfile " $parms($name) $args"
				puts $outfile " ; verify_dir $testdir \"\" 1"
			}
			if { $run } {
				check_handles $outfile
				puts $outfile "[timestamp]"
				eval $name $method $parms($name) $args
				if { $__debug_print != 0 } {
					puts $outfile ""
				}
				# verify all databases the test leaves behind
				verify_dir $testdir "" 1
				if { $__debug_on != 0 } {
					debug
				}
			}
			flush stdout
			flush stderr
		}
	} res] != 0} {
		global errorInfo;

		set fnl [string first "\n" $errorInfo]
		set theError [string range $errorInfo 0 [expr $fnl - 1]]
		if {[string first FAIL $errorInfo] == -1} {
			error "FAIL:[timestamp]\
			    run_method: $method $i: $theError"
		} else {
			error $theError;
		}
	}
}

proc run_rpcmethod { method {start 1} {stop 0} {largs ""} } {
	global __debug_on
	global __debug_print
	global num_test
	global parms
	global is_envmethod
	global rpc_svc
	source ./include.tcl

	if { $stop == 0 } {
		set stop $num_test(test)
	}
	puts "run_rpcmethod: $method $start $stop $largs"

	set save_largs $largs
	if { [string compare $rpc_server "localhost"] == 0 } {
	       set dpid [exec $util_path/$rpc_svc -h $rpc_testdir &]
	} else {
	       set dpid [exec rsh $rpc_server $rpc_path/$rpc_svc \
		   -h $rpc_testdir &]
	}
	puts "\tRun_rpcmethod.a: starting server, pid $dpid"
	tclsleep 10
	remote_cleanup $rpc_server $rpc_testdir $testdir

	set home [file tail $rpc_testdir]

	set is_envmethod 1
	set use_txn 0
	if { [string first "txn" $method] != -1 } {
		set use_txn 1
	}
	if { $use_txn == 1 } {
		if { $start == 1 } {
			set ntxns 32
		} else {
			set ntxns $start
		}
		set i 1
		check_handles
		remote_cleanup $rpc_server $rpc_testdir $testdir
		set env [eval {berkdb_env -create -mode 0644 -home $home \
		    -server $rpc_server -client_timeout 10000} -txn]
		error_check_good env_open [is_valid_env $env] TRUE

		set stat [catch {eval txn001_suba $ntxns $env} res]
		if { $stat == 0 } {
			set stat [catch {eval txn001_subb $ntxns $env} res]
		}
		error_check_good envclose [$env close] 0
		set stat [catch {eval txn003} res]
	} else {
		set stat [catch {
			for { set i $start } { $i <= $stop } {incr i} {
				check_handles
				set name [format "test%03d" $i]
				if { [info exists parms($name)] != 1 } {
					puts stderr "[format Test%03d $i]\
					    disabled in testparams.tcl;\
					    skipping."
					continue
				}
				remote_cleanup $rpc_server $rpc_testdir $testdir
				#
				# Set server cachesize to 1Mb.  Otherwise some
				# tests won't fit (like test084 -btree).
				#
				set env [eval {berkdb_env -create -mode 0644 \
				    -home $home -server $rpc_server \
				    -client_timeout 10000 \
				    -cachesize {0 1048576 1}}]
				error_check_good env_open \
				    [is_valid_env $env] TRUE
				append largs " -env $env "

				puts "[timestamp]"
				eval $name $method $parms($name) $largs
				if { $__debug_print != 0 } {
					puts ""
				}
				if { $__debug_on != 0 } {
					debug
				}
				flush stdout
				flush stderr
				set largs $save_largs
				error_check_good envclose [$env close] 0
			}
		} res]
	}
	if { $stat != 0} {
		global errorInfo;

		set fnl [string first "\n" $errorInfo]
		set theError [string range $errorInfo 0 [expr $fnl - 1]]
		tclkill $dpid
		if {[string first FAIL $errorInfo] == -1} {
			error "FAIL:[timestamp]\
			    run_rpcmethod: $method $i: $theError"
		} else {
			error $theError;
		}
	}
	set is_envmethod 0
	tclkill $dpid
}

proc run_rpcnoserver { method {start 1} {stop 0} {largs ""} } {
	global __debug_on
	global __debug_print
	global num_test
	global parms
	global is_envmethod
	source ./include.tcl

	if { $stop == 0 } {
		set stop $num_test(test)
	}
	puts "run_rpcnoserver: $method $start $stop $largs"

	set save_largs $largs
	remote_cleanup $rpc_server $rpc_testdir $testdir
	set home [file tail $rpc_testdir]

	set is_envmethod 1
	set use_txn 0
	if { [string first "txn" $method] != -1 } {
		set use_txn 1
	}
	if { $use_txn == 1 } {
		if { $start == 1 } {
			set ntxns 32
		} else {
			set ntxns $start
		}
		set i 1
		check_handles
		remote_cleanup $rpc_server $rpc_testdir $testdir
		set env [eval {berkdb_env -create -mode 0644 -home $home \
		    -server $rpc_server -client_timeout 10000} -txn]
		error_check_good env_open [is_valid_env $env] TRUE

		set stat [catch {eval txn001_suba $ntxns $env} res]
		if { $stat == 0 } {
			set stat [catch {eval txn001_subb $ntxns $env} res]
		}
		error_check_good envclose [$env close] 0
	} else {
		set stat [catch {
			for { set i $start } { $i <= $stop } {incr i} {
				check_handles
				set name [format "test%03d" $i]
				if { [info exists parms($name)] != 1 } {
					puts stderr "[format Test%03d $i]\
					    disabled in testparams.tcl;\
					    skipping."
					continue
				}
				remote_cleanup $rpc_server $rpc_testdir $testdir
				#
				# Set server cachesize to 1Mb.  Otherwise some
				# tests won't fit (like test084 -btree).
				#
				set env [eval {berkdb_env -create -mode 0644 \
				    -home $home -server $rpc_server \
				    -client_timeout 10000 \
				    -cachesize {0 1048576 1} }]
				error_check_good env_open \
				    [is_valid_env $env] TRUE
				append largs " -env $env "

				puts "[timestamp]"
				eval $name $method $parms($name) $largs
				if { $__debug_print != 0 } {
					puts ""
				}
				if { $__debug_on != 0 } {
					debug
				}
				flush stdout
				flush stderr
				set largs $save_largs
				error_check_good envclose [$env close] 0
			}
		} res]
	}
	if { $stat != 0} {
		global errorInfo;

		set fnl [string first "\n" $errorInfo]
		set theError [string range $errorInfo 0 [expr $fnl - 1]]
		if {[string first FAIL $errorInfo] == -1} {
			error "FAIL:[timestamp]\
			    run_rpcnoserver: $method $i: $theError"
		} else {
			error $theError;
		}
	set is_envmethod 0
	}

}

#
# Run method tests in secure mode.
#
proc run_secmethod { method {start 1} {stop 0} {display 0} {run 1} \
    { outfile stdout } args } {
	global passwd

	append largs " -encryptaes $passwd "
	eval run_method $method $start $stop $display $run $outfile $largs
}

#
# Run method tests in its own, new secure environment.
#
proc run_secenv { method {start 1} {stop 0} {largs ""} } {
	global __debug_on
	global __debug_print
	global is_envmethod
	global num_test
	global parms
	global passwd
	source ./include.tcl

	if { $stop == 0 } {
		set stop $num_test(test)
	}
	puts "run_secenv: $method $start $stop $largs"

	set save_largs $largs
	env_cleanup $testdir
	set is_envmethod 1
	set stat [catch {
		for { set i $start } { $i <= $stop } {incr i} {
			check_handles
			set env [eval {berkdb_env -create -mode 0644 \
			    -home $testdir -encryptaes $passwd \
			    -cachesize {0 1048576 1}}]
			error_check_good env_open [is_valid_env $env] TRUE
			append largs " -env $env "

			puts "[timestamp]"
			set name [format "test%03d" $i]
			if { [info exists parms($name)] != 1 } {
				puts stderr "[format Test%03d $i] disabled in\
				    testparams.tcl; skipping."
				continue
			}

			#
			# Run each test multiple times in the secure env.
			# Once with a secure env + clear database
			# Once with a secure env + secure database
			#
			eval $name $method $parms($name) $largs
			append largs " -encrypt "
			eval $name $method $parms($name) $largs

			if { $__debug_print != 0 } {
				puts ""
			}
			if { $__debug_on != 0 } {
				debug
			}
			flush stdout
			flush stderr
			set largs $save_largs
			error_check_good envclose [$env close] 0
			error_check_good envremove [berkdb envremove \
			    -home $testdir -encryptaes $passwd] 0
		}
	} res]
	if { $stat != 0} {
		global errorInfo;

		set fnl [string first "\n" $errorInfo]
		set theError [string range $errorInfo 0 [expr $fnl - 1]]
		if {[string first FAIL $errorInfo] == -1} {
			error "FAIL:[timestamp]\
			    run_secenv: $method $i: $theError"
		} else {
			error $theError;
		}
	set is_envmethod 0
	}

}

#
# Run replication method tests in master and client env.
#
proc run_reptest { method test {droppct 0} {nclients 1} {do_del 0} \
    {do_sec 0} {do_oob 0} {largs "" } } {
	source ./include.tcl
	global __debug_on
	global __debug_print
	global __debug_test
	global is_envmethod
	global num_test
	global parms
	global passwd
	global rand_init

	berkdb srand $rand_init
	set c [string index $test 0]
	if { $c == "s" } {
		set i [string range $test 1 end]
		set name [format "subdb%03d" $i]
	} else {
		set i $test
		set name [format "test%03d" $i]
	}
	puts "run_reptest: $method $name"

	env_cleanup $testdir
	set is_envmethod 1
	set stat [catch {
		if { $do_sec } {
			set envargs "-encryptaes $passwd"
			append largs " -encrypt "
		} else {
			set envargs ""
		}
		check_handles
		#
		# This will set up the master and client envs
		# and will return us the args to pass to the
		# test.
		set largs [repl_envsetup \
		    $envargs $largs $test $nclients $droppct $do_oob]

		puts "[timestamp]"
		if { [info exists parms($name)] != 1 } {
			puts stderr "[format Test%03d $i] \
			    disabled in\
			    testparams.tcl; skipping."
			continue
		}
		puts -nonewline \
		    "Repl: $name: dropping $droppct%, $nclients clients "
		if { $do_del } {
			puts -nonewline " with delete verification;"
		} else {
			puts -nonewline " no delete verification;"
		}
		if { $do_sec } {
			puts -nonewline " with security;"
		} else {
			puts -nonewline " no security;"
		}
		if { $do_oob } {
			puts -nonewline " with out-of-order msgs;"
		} else {
			puts -nonewline " no out-of-order msgs;"
		}
		puts ""

		eval $name $method $parms($name) $largs

		if { $__debug_print != 0 } {
			puts ""
		}
		if { $__debug_on != 0 } {
			debug $__debug_test
		}
		flush stdout
		flush stderr
		repl_envprocq $i $nclients $do_oob
		repl_envver0 $i $method $nclients
		if { $do_del } {
			repl_verdel $i $method $nclients
		}
		repl_envclose $i $envargs
	} res]
	if { $stat != 0} {
		global errorInfo;

		set fnl [string first "\n" $errorInfo]
		set theError [string range $errorInfo 0 [expr $fnl - 1]]
		if {[string first FAIL $errorInfo] == -1} {
			error "FAIL:[timestamp]\
			    run_reptest: $method $i: $theError"
		} else {
			error $theError;
		}
	}
	set is_envmethod 0
}

#
# Run replication method tests in master and client env.
#
proc run_repmethod { method {numcl 0} {start 1} {stop 0} {display 0}
    {run 1} {outfile stdout} {largs ""} } {
	source ./include.tcl
	global __debug_on
	global __debug_print
	global __debug_test
	global is_envmethod
	global num_test
	global parms
	global passwd
	global rand_init

	set stopsdb $num_test(sdb)
	if { $stop == 0 } {
		set stop $num_test(test)
	} else {
		if { $stopsdb > $stop } {
			set stopsdb $stop
		}
	}
	berkdb srand $rand_init

	#
	# We want to run replication both normally and with crypto.
	# So run it once and then run again with crypto.
	#
	set save_largs $largs
	env_cleanup $testdir

	if { $display == 1 } {
		for { set i $start } { $i <= $stop } { incr i } {
			puts $outfile "eval run_repmethod $method \
			    0 $i $i 0 1 stdout $largs"
		}
	}
	if { $run == 1 } {
		set is_envmethod 1
		#
		# Use an array for number of clients because we really don't
		# want to evenly-weight all numbers of clients.  Favor smaller
		# numbers but test more clients occasionally.
		set drop_list { 0 0 0 0 0 1 1 5 5 10 20 }
		set drop_len [expr [llength $drop_list] - 1]
		set client_list { 1 1 2 1 1 1 2 2 3 1 }
		set cl_len [expr [llength $client_list] - 1]
		set stat [catch {
			for { set i $start } { $i <= $stopsdb } {incr i} {
				if { $numcl == 0 } {
					set clindex [berkdb random_int 0 $cl_len]
					set nclients [lindex $client_list $clindex]
				} else {
					set nclients $numcl
				}
				set drindex [berkdb random_int 0 $drop_len]
				set droppct [lindex $drop_list $drindex]
	 			set do_sec [berkdb random_int 0 1]
				set do_oob [berkdb random_int 0 1]
	 			set do_del [berkdb random_int 0 1]

				if { $do_sec } {
					set envargs "-encryptaes $passwd"
					append largs " -encrypt "
				} else {
					set envargs ""
				}
				check_handles
				#
				# This will set up the master and client envs
				# and will return us the args to pass to the
				# test.
				set largs [repl_envsetup $envargs $largs \
				    $i $nclients $droppct $do_oob]

				puts "[timestamp]"
				set name [format "subdb%03d" $i]
				if { [info exists parms($name)] != 1 } {
					puts stderr "[format Subdb%03d $i] \
					    disabled in\
					    testparams.tcl; skipping."
					continue
				}
				puts -nonewline "Repl: $name: dropping $droppct%, \
				    $nclients clients "
				if { $do_del } {
					puts -nonewline " with delete verification;"
				} else {
					puts -nonewline " no delete verification;"
				}
				if { $do_sec } {
					puts -nonewline " with security;"
				} else {
					puts -nonewline " no security;"
				}
				if { $do_oob } {
					puts -nonewline " with out-of-order msgs;"
				} else {
					puts -nonewline " no out-of-order msgs;"
				}
				puts ""

				eval $name $method $parms($name) $largs

				if { $__debug_print != 0 } {
					puts ""
				}
				if { $__debug_on != 0 } {
					debug $__debug_test
				}
				flush stdout
				flush stderr
				repl_envprocq $i $nclients $do_oob
				repl_envver0 $i $method $nclients
				if { $do_del } {
					repl_verdel $i $method $nclients
				}
				repl_envclose $i $envargs
				set largs $save_largs
			}
		} res]
		if { $stat != 0} {
			global errorInfo;

			set fnl [string first "\n" $errorInfo]
			set theError [string range $errorInfo 0 [expr $fnl - 1]]
			if {[string first FAIL $errorInfo] == -1} {
				error "FAIL:[timestamp]\
				    run_repmethod: $method $i: $theError"
			} else {
				error $theError;
			}
		}
		set stat [catch {
			for { set i $start } { $i <= $stop } {incr i} {
				if { $numcl == 0 } {
					set clindex [berkdb random_int 0 $cl_len]
					set nclients [lindex $client_list $clindex]
				} else {
					set nclients $numcl
				}
				set drindex [berkdb random_int 0 $drop_len]
				set droppct [lindex $drop_list $drindex]
				set do_sec [berkdb random_int 0 1]
				set do_oob [berkdb random_int 0 1]
				set do_del [berkdb random_int 0 1]

				if { $do_sec } {
					set envargs "-encryptaes $passwd"
					append largs " -encrypt "
				} else {
					set envargs ""
				}
				check_handles
				#
				# This will set up the master and client envs
				# and will return us the args to pass to the
				# test.
				set largs [repl_envsetup $envargs $largs \
				    $i $nclients $droppct $do_oob]

				puts "[timestamp]"
				set name [format "test%03d" $i]
				if { [info exists parms($name)] != 1 } {
					puts stderr "[format Test%03d $i] \
					    disabled in\
					    testparams.tcl; skipping."
					continue
				}
				puts -nonewline "Repl: $name: dropping $droppct%, \
				    $nclients clients "
				if { $do_del } {
					puts -nonewline " with delete verification;"
				} else {
					puts -nonewline " no delete verification;"
				}
				if { $do_sec } {
					puts -nonewline " with security;"
				} else {
					puts -nonewline " no security;"
				}
				if { $do_oob } {
					puts -nonewline " with out-of-order msgs;"
				} else {
					puts -nonewline " no out-of-order msgs;"
				}
				puts ""

				eval $name $method $parms($name) $largs

				if { $__debug_print != 0 } {
					puts ""
				}
				if { $__debug_on != 0 } {
					debug $__debug_test
				}
				flush stdout
				flush stderr
				repl_envprocq $i $nclients $do_oob
				repl_envver0 $i $method $nclients
				if { $do_del } {
					repl_verdel $i $method $nclients
				}
				repl_envclose $i $envargs
				set largs $save_largs
			}
		} res]
		if { $stat != 0} {
			global errorInfo;

			set fnl [string first "\n" $errorInfo]
			set theError [string range $errorInfo 0 [expr $fnl - 1]]
			if {[string first FAIL $errorInfo] == -1} {
				error "FAIL:[timestamp]\
				    run_repmethod: $method $i: $theError"
			} else {
				error $theError;
			}
		}
		set is_envmethod 0
	}
}

#
# Run method tests, each in its own, new environment.  (As opposed to
# run_envmethod1 which runs all the tests in a single environment.)
#
proc run_envmethod { method {start 1} {stop 0} {display 0} {run 1} \
    {outfile stdout }  { largs "" } } {
	global __debug_on
	global __debug_print
	global __debug_test
	global is_envmethod
	global num_test
	global parms
	source ./include.tcl

	set stopsdb $num_test(sdb)
	if { $stop == 0 } {
		set stop $num_test(test)
	} else {
		if { $stopsdb > $stop } {
			set stopsdb $stop
		}
	}

	set save_largs $largs
	env_cleanup $testdir

	if { $display == 1 } {
		for { set i $start } { $i <= $stop } { incr i } {
			puts $outfile "eval run_envmethod $method \
			    $i $i 0 1 stdout $largs"
		}
	}

	if { $run == 1 } {
		set is_envmethod 1
		#
		# Run both subdb and normal tests for as long as there are
		# some of each type.  Start with the subdbs:
		set stat [catch {
			for { set i $start } { $i <= $stopsdb } {incr i} {
				check_handles
				set env [eval {berkdb_env -create -txn \
				    -mode 0644 -home $testdir}]
				error_check_good env_open \
				    [is_valid_env $env] TRUE
				append largs " -env $env "

				puts "[timestamp]"
				set name [format "subdb%03d" $i]
				if { [info exists parms($name)] != 1 } {
					puts stderr \
					    "[format Subdb%03d $i] disabled in\
				    	    testparams.tcl; skipping."
					continue
				}
				eval $name $method $parms($name) $largs

				error_check_good envclose [$env close] 0
				error_check_good envremove [berkdb envremove \
				    -home $testdir] 0
				flush stdout
				flush stderr
				set largs $save_largs
			}
		} res]
		if { $stat != 0} {
			global errorInfo;

			set fnl [string first "\n" $errorInfo]
			set theError [string range $errorInfo 0 [expr $fnl - 1]]
			if {[string first FAIL $errorInfo] == -1} {
				error "FAIL:[timestamp]\
				    run_envmethod: $method $i: $theError"
			} else {
			error $theError;
			}
		}
		# Subdb tests are done, now run through the regular tests:
		set stat [catch {
			for { set i $start } { $i <= $stop } {incr i} {
				check_handles
				set env [eval {berkdb_env -create -txn \
				    -mode 0644 -home $testdir}]
				error_check_good env_open \
				    [is_valid_env $env] TRUE
				append largs " -env $env "

				puts "[timestamp]"
				set name [format "test%03d" $i]
				if { [info exists parms($name)] != 1 } {
					puts stderr \
					    "[format Test%03d $i] disabled in\
					    testparams.tcl; skipping."
					continue
				}
				eval $name $method $parms($name) $largs

				if { $__debug_print != 0 } {
					puts ""
				}
				if { $__debug_on != 0 } {
					debug $__debug_test
				}
				flush stdout
				flush stderr
				set largs $save_largs
				error_check_good envclose [$env close] 0
				error_check_good envremove [berkdb envremove \
				    -home $testdir] 0
			}
		} res]
		if { $stat != 0} {
			global errorInfo;

			set fnl [string first "\n" $errorInfo]
			set theError [string range $errorInfo 0 [expr $fnl - 1]]
			if {[string first FAIL $errorInfo] == -1} {
				error "FAIL:[timestamp]\
				    run_envmethod: $method $i: $theError"
			} else {
				error $theError;
			}
		}
		set is_envmethod 0
	}
}

proc subdb { method {start 1} {stop 0} {display 0} {run 1} \
    {outfile stdout} args} {
	global num_test testdir
	global parms

	for { set i $start } { $i <= $stop } {incr i} {
		set name [format "subdb%03d" $i]
		if { [info exists parms($name)] != 1 } {
			puts stderr "[format Subdb%03d $i] disabled in\
			    testparams.tcl; skipping."
			continue
		}
		if { $display } {
			puts -nonewline $outfile "eval $name $method"
			puts -nonewline $outfile " $parms($name) $args;"
			puts $outfile "verify_dir $testdir \"\" 1"
		}
		if { $run } {
			check_handles $outfile
			eval $name $method $parms($name) $args
			verify_dir $testdir "" 1
		}
		flush stdout
		flush stderr
	}
}

proc run_recd { method {start 1} {stop 0} {run 1} {display 0} args } {
	global __debug_on
	global __debug_print
	global __debug_test
	global parms
	global num_test
	global log_log_record_types
	source ./include.tcl

	if { $stop == 0 } {
		set stop $num_test(recd)
	}
	if { $run == 1 } {
		puts "run_recd: $method $start $stop $args"
	}

	if {[catch {
		for { set i $start } { $i <= $stop } {incr i} {
			set name [format "recd%03d" $i]
			if { [info exists parms($name)] != 1 } {
				puts stderr "[format Recd%03d $i] disabled in\
				    testparams.tcl; skipping."
				continue
			}
			if { $display } {
				puts "eval $name $method $parms($name) $args"
			}
			if { $run } {
				check_handles
				puts "[timestamp]"
				# By redirecting stdout to stdout, we make exec
				# print output rather than simply returning it.
				# By redirecting stderr to stdout too, we make
				# sure everything winds up in the ALL.OUT file.
				set ret [catch { exec $tclsh_path << \
				    "source $test_path/test.tcl; \
				    set log_log_record_types \
				    $log_log_record_types; eval $name \
				    $method $parms($name) $args" \
				    >&@ stdout
				} res]

				# Don't die if the test failed;  we want
				# to just proceed.
				if { $ret != 0 } {
					puts "FAIL:[timestamp] $res"
				}

				if { $__debug_print != 0 } {
					puts ""
				}
				if { $__debug_on != 0 } {
					debug $__debug_test
				}
				flush stdout
				flush stderr
			}
		}
	} res] != 0} {
		global errorInfo;

		set fnl [string first "\n" $errorInfo]
		set theError [string range $errorInfo 0 [expr $fnl - 1]]
		if {[string first FAIL $errorInfo] == -1} {
			error "FAIL:[timestamp]\
			    run_recd: $method $i: $theError"
		} else {
			error $theError;
		}
	}
}

proc run_recds { {run 1} {display 0} args } {
	global log_log_record_types

	set log_log_record_types 1
	logtrack_init
	foreach method \
	    "btree rbtree hash queue queueext recno frecno rrecno" {
		check_handles
		if { [catch {eval \
		    run_recd -$method 1 0 $run $display $args} ret ] != 0 } {
			puts $ret
		}
	}
	if { $run } {
		logtrack_summary
	}
	set log_log_record_types 0
}

proc run_all { args } {
	global num_test
	source ./include.tcl

	fileremove -f ALL.OUT

	set exflgs [eval extractflags $args]
	set flags [lindex $exflgs 1]
	set display 1
	set run 1
	set am_only 0
	set parallel 0
	set nparalleltests 0
	set rflags {--}
	foreach f $flags {
		switch $f {
			m {
				set am_only 1
			}
			n {
				set display 1
				set run 0
				set rflags [linsert $rflags 0 "-n"]
			}
		}
	}

	set o [open ALL.OUT a]
	if { $run == 1 } {
		puts -nonewline "Test suite run started at: "
		puts [clock format [clock seconds] -format "%H:%M %D"]
		puts [berkdb version -string]

		puts -nonewline $o "Test suite run started at: "
		puts $o [clock format [clock seconds] -format "%H:%M %D"]
		puts $o [berkdb version -string]
	}
	close $o
	#
	# First run standard tests.  Send in a -A to let run_std know
	# that it is part of the "run_all" run, so that it doesn't
	# print out start/end times.
	#
	lappend args -A
	eval {run_std} $args

	set test_pagesizes [get_test_pagesizes]
	set args [lindex $exflgs 0]
	set save_args $args

	foreach pgsz $test_pagesizes {
		set args $save_args
		append args " -pagesize $pgsz -chksum"
		if { $am_only == 0 } {
			# Run recovery tests.
			#
			# XXX These don't actually work at multiple pagesizes;
			# disable them for now.
			#
			# XXX These too are broken into separate tclsh
			# instantiations so we don't require so much
			# memory, but I think it's cleaner
			# and more useful to do it down inside proc r than here,
			# since "r recd" gets done a lot and needs to work.
			#
			# XXX See comment in run_std for why this only directs
			# stdout and not stderr.  Don't worry--the right stuff
			# happens.
			#puts "Running recovery tests with pagesize $pgsz"
			#if [catch {exec $tclsh_path \
			#    << "source $test_path/test.tcl; \
			#    r $rflags recd $args" \
			#    2>@ stderr >> ALL.OUT } res] {
			#	set o [open ALL.OUT a]
			#	puts $o "FAIL: recd test:"
			#	puts $o $res
			#	close $o
			#}
		}

		# Access method tests.
		#
		# XXX
		# Broken up into separate tclsh instantiations so
		# we don't require so much memory.
		foreach i \
		   "btree rbtree hash queue queueext recno frecno rrecno" {
			puts "Running $i tests with pagesize $pgsz"
			for { set j 1 } { $j <= $num_test(test) } {incr j} {
				if { $run == 0 } {
					set o [open ALL.OUT a]
					eval {run_method -$i $j $j $display \
					    $run $o} $args
					close $o
				}
				if { $run } {
					if [catch {exec $tclsh_path \
					    << "source $test_path/test.tcl; \
					    eval {run_method -$i $j $j \
					    $display $run stdout} $args" \
					    >>& ALL.OUT } res] {
						set o [open ALL.OUT a]
						puts $o \
						    "FAIL: [format \
						    "test%03d" $j] $i"
						close $o
					}
				}
			}

			#
			# Run subdb tests with varying pagesizes too.
			#
			for { set j 1 } { $j <= $num_test(sdb) } {incr j} {
				if { $run == 0 } {
					set o [open ALL.OUT a]
					eval {subdb -$i $j $j $display \
					    $run $o} $args
					close $o
				}
				if { $run == 1 } {
					if [catch {exec $tclsh_path \
					    << "source $test_path/test.tcl; \
					    eval {subdb -$i $j $j $display \
					    $run stdout} $args" \
					    >>& ALL.OUT } res] {
						set o [open ALL.OUT a]
						puts $o "FAIL: subdb -$i $j $j"
						close $o
					}
				}
			}
		}
	}
	set args $save_args
	#
	# Run access method tests at default page size in one env.
	#
	foreach i "btree rbtree hash queue queueext recno frecno rrecno" {
		puts "Running $i tests in a txn env"
		for { set j 1 } { $j <= $num_test(test) } { incr j } {
			if { $run == 0 } {
				set o [open ALL.OUT a]
				run_envmethod -$i $j $j $display \
				    $run $o $args
				close $o
			}
			if { $run } {
				if [catch {exec $tclsh_path \
				    << "source $test_path/test.tcl; \
				    run_envmethod -$i $j $j \
			  	    $display $run stdout $args" \
				    >>& ALL.OUT } res] {
					set o [open ALL.OUT a]
					puts $o \
					    "FAIL: run_envmethod $i $j $j"
					close $o
				}
			}
		}
	}
	#
	# Run tests using proc r.  The replication tests have been
	# moved from run_std to run_all.
	#
	set test_list {
	{"replication"	"rep"}
	{"security"	"sec"}
	}
	#
	# If configured for RPC, then run rpc tests too.
	#
	if { [file exists ./berkeley_db_svc] ||
	     [file exists ./berkeley_db_cxxsvc] ||
	     [file exists ./berkeley_db_javasvc] } {
		append test_list {{"RPC"	"rpc"}}
	}

	foreach pair $test_list {
		set msg [lindex $pair 0]
		set cmd [lindex $pair 1]
		puts "Running $msg tests"
		if [catch {exec $tclsh_path \
		    << "source $test_path/test.tcl; \
		    r $rflags $cmd $args" >>& ALL.OUT } res] {
			set o [open ALL.OUT a]
			puts $o "FAIL: $cmd test"
			close $o
		}
	}

	# If not actually running, no need to check for failure.
	if { $run == 0 } {
		return
	}

	set failed 0
	set o [open ALL.OUT r]
	while { [gets $o line] >= 0 } {
		if { [regexp {^FAIL} $line] != 0 } {
			set failed 1
		}
	}
	close $o
	set o [open ALL.OUT a]
	if { $failed == 0 } {
		puts "Regression Tests Succeeded"
		puts $o "Regression Tests Succeeded"
	} else {
		puts "Regression Tests Failed; see ALL.OUT for log"
		puts $o "Regression Tests Failed"
	}

	puts -nonewline "Test suite run completed at: "
	puts [clock format [clock seconds] -format "%H:%M %D"]
	puts -nonewline $o "Test suite run completed at: "
	puts $o [clock format [clock seconds] -format "%H:%M %D"]
	close $o
}

#
# Run method tests in one environment.  (As opposed to run_envmethod
# which runs each test in its own, new environment.)
#
proc run_envmethod1 { method {start 1} {stop 0} {display 0} {run 1} \
    { outfile stdout } args } {
	global __debug_on
	global __debug_print
	global __debug_test
	global is_envmethod
	global num_test
	global parms
	source ./include.tcl

	set stopsdb $num_test(sdb)
	if { $stop == 0 } {
		set stop $num_test(test)
	} else {
		if { $stopsdb > $stop } {
			set stopsdb $stop
		}
	}
	if { $run == 1 } {
		puts "run_envmethod1: $method $start $stop $args"
	}

	set is_envmethod 1
	if { $run == 1 } {
		check_handles
		env_cleanup $testdir
		error_check_good envremove [berkdb envremove -home $testdir] 0
		set env [eval {berkdb_env -create -cachesize {0 10000000 0}} \
		    {-mode 0644 -home $testdir}]
		error_check_good env_open [is_valid_env $env] TRUE
		append largs " -env $env "
	}

	if { $display } {
		# The envmethod1 tests can't be split up, since they share
		# an env.
		puts $outfile "eval run_envmethod1 $method $args"
	}

	set stat [catch {
		for { set i $start } { $i <= $stopsdb } {incr i} {
			set name [format "subdb%03d" $i]
			if { [info exists parms($name)] != 1 } {
				puts stderr "[format Subdb%03d $i] disabled in\
				    testparams.tcl; skipping."
				continue
			}
			if { $run } {
				puts $outfile "[timestamp]"
				eval $name $method $parms($name) $largs
				if { $__debug_print != 0 } {
					puts $outfile ""
				}
				if { $__debug_on != 0 } {
					debug $__debug_test
				}
			}
			flush stdout
			flush stderr
		}
	} res]
	if { $stat != 0} {
		global errorInfo;

		set fnl [string first "\n" $errorInfo]
		set theError [string range $errorInfo 0 [expr $fnl - 1]]
		if {[string first FAIL $errorInfo] == -1} {
			error "FAIL:[timestamp]\
			    run_envmethod: $method $i: $theError"
		} else {
			error $theError;
		}
	}
	set stat [catch {
		for { set i $start } { $i <= $stop } {incr i} {
			set name [format "test%03d" $i]
			if { [info exists parms($name)] != 1 } {
				puts stderr "[format Test%03d $i] disabled in\
				    testparams.tcl; skipping."
				continue
			}
			if { $run } {
				puts $outfile "[timestamp]"
				eval $name $method $parms($name) $largs
				if { $__debug_print != 0 } {
					puts $outfile ""
				}
				if { $__debug_on != 0 } {
					debug $__debug_test
				}
			}
			flush stdout
			flush stderr
		}
	} res]
	if { $stat != 0} {
		global errorInfo;

		set fnl [string first "\n" $errorInfo]
		set theError [string range $errorInfo 0 [expr $fnl - 1]]
		if {[string first FAIL $errorInfo] == -1} {
			error "FAIL:[timestamp]\
			    run_envmethod1: $method $i: $theError"
		} else {
			error $theError;
		}
	}
	if { $run == 1 } {
		error_check_good envclose [$env close] 0
		check_handles $outfile
	}
	set is_envmethod 0

}

# We want to test all of 512b, 8Kb, and 64Kb pages, but chances are one
# of these is the default pagesize.  We don't want to run all the AM tests
# twice, so figure out what the default page size is, then return the
# other two.
proc get_test_pagesizes { } {
	# Create an in-memory database.
	set db [berkdb_open -create -btree]
	error_check_good gtp_create [is_valid_db $db] TRUE
	set statret [$db stat]
	set pgsz 0
	foreach pair $statret {
		set fld [lindex $pair 0]
		if { [string compare $fld {Page size}] == 0 } {
			set pgsz [lindex $pair 1]
		}
	}

	error_check_good gtp_close [$db close] 0

	error_check_bad gtp_pgsz $pgsz 0
	switch $pgsz {
		512 { return {8192 32768} }
		8192 { return {512 32768} }
		32768 { return {512 8192} }
		default { return {512 8192 32768} }
	}
	error_check_good NOTREACHED 0 1
}

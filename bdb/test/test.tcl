# See the file LICENSE for redistribution information.
#
# Copyright (c) 1996, 1997, 1998, 1999, 2000
#	Sleepycat Software.  All rights reserved.
#
#	$Id: test.tcl,v 11.114 2001/01/09 21:28:52 sue Exp $

source ./include.tcl

# Load DB's TCL API.
load $tcllib

if { [file exists $testdir] != 1 } {
	file mkdir $testdir
}

global __debug_print
global __debug_on
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
set __debug_on 0

# This is where the test numbering and parameters now live.
source $test_path/testparams.tcl

for { set i 1 } { $i <= $deadtests } {incr i} {
	set name [format "dead%03d.tcl" $i]
	source $test_path/$name
}
for { set i 1 } { $i <= $envtests } {incr i} {
	set name [format "env%03d.tcl" $i]
	source $test_path/$name
}
for { set i 1 } { $i <= $recdtests } {incr i} {
	set name [format "recd%03d.tcl" $i]
	source $test_path/$name
}
for { set i 1 } { $i <= $rpctests } {incr i} {
	set name [format "rpc%03d.tcl" $i]
	source $test_path/$name
}
for { set i 1 } { $i <= $rsrctests } {incr i} {
	set name [format "rsrc%03d.tcl" $i]
	source $test_path/$name
}
for { set i 1 } { $i <= $runtests } {incr i} {
	set name [format "test%03d.tcl" $i]
	# Test numbering may be sparse.
	if { [file exists $test_path/$name] == 1 } {
		source $test_path/$name
	}
}
for { set i 1 } { $i <= $subdbtests } {incr i} {
	set name [format "sdb%03d.tcl" $i]
	source $test_path/$name
}

source $test_path/archive.tcl
source $test_path/byteorder.tcl
source $test_path/dbm.tcl
source $test_path/hsearch.tcl
source $test_path/join.tcl
source $test_path/lock001.tcl
source $test_path/lock002.tcl
source $test_path/lock003.tcl
source $test_path/log.tcl
source $test_path/logtrack.tcl
source $test_path/mpool.tcl
source $test_path/mutex.tcl
source $test_path/ndbm.tcl
source $test_path/sdbtest001.tcl
source $test_path/sdbtest002.tcl
source $test_path/sdbutils.tcl
source $test_path/testutils.tcl
source $test_path/txn.tcl
source $test_path/upgrade.tcl

set dict $test_path/wordlist
set alphabet "abcdefghijklmnopqrstuvwxyz"

# Random number seed.
global rand_init
set rand_init 1013

# Default record length and padding character for
# fixed record length access method(s)
set fixed_len 20
set fixed_pad 0

set recd_debug	0
set log_log_record_types 0
set ohandles {}

# Set up any OS-specific values
global tcl_platform
set is_windows_test [is_substr $tcl_platform(os) "Win"]
set is_hp_test [is_substr $tcl_platform(os) "HP-UX"]
set is_qnx_test [is_substr $tcl_platform(os) "QNX"]

# From here on out, test.tcl contains the procs that are used to
# run all or part of the test suite.

proc run_am { } {
	global runtests
	source ./include.tcl

	fileremove -f ALL.OUT

	# Access method tests.
	#
	# XXX
	# Broken up into separate tclsh instantiations so we don't require
	# so much memory.
	foreach i "btree rbtree hash queue queueext recno frecno rrecno" {
		puts "Running $i tests"
		for { set j 1 } { $j <= $runtests } {incr j} {
			if [catch {exec $tclsh_path \
			    << "source $test_path/test.tcl; \
			    run_method -$i $j $j" >>& ALL.OUT } res] {
				set o [open ALL.OUT a]
				puts $o "FAIL: [format "test%03d" $j] $i"
				close $o
			}
		}
		if [catch {exec $tclsh_path \
		    << "source $test_path/test.tcl; \
		    subdb -$i 0 1" >>& ALL.OUT } res] {
			set o [open ALL.OUT a]
			puts $o "FAIL: subdb -$i test"
			close $o
		}
	}
}

proc run_std { args } {
	global runtests
	global subdbtests
	source ./include.tcl

	set exflgs [eval extractflags $args]
	set args [lindex $exflgs 0]
	set flags [lindex $exflgs 1]

	set display 1
	set run 1
	set am_only 0
	set std_only 1
	set rflags {--}
	foreach f $flags {
		switch $f {
			A {
				set std_only 0
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
	{"memory pool"		"mpool"}
	{"mutex"		"mutex"}
	{"transaction"		"txn"}
	{"deadlock detection"	"dead"}
	{"subdatabase"		"subdb_gen"}
	{"byte-order"		"byte"}
	{"recno backing file"	"rsrc"}
	{"DBM interface"	"dbm"}
	{"NDBM interface"	"ndbm"}
	{"Hsearch interface"	"hsearch"}
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
		puts "Running recovery tests"
		if [catch {exec $tclsh_path \
		    << "source $test_path/test.tcl; \
			r $rflags recd" >>& ALL.OUT } res] {
			set o [open ALL.OUT a]
			puts $o "FAIL: recd test"
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

	# Access method tests.
	#
	# XXX
	# Broken up into separate tclsh instantiations so we don't require
	# so much memory.
	foreach i "btree rbtree hash queue queueext recno frecno rrecno" {
		puts "Running $i tests"
		for { set j 1 } { $j <= $runtests } {incr j} {
			if { $run == 0 } {
				set o [open ALL.OUT a]
				run_method -$i $j $j $display $run $o
				close $o
			}
			if { $run } {
				if [catch {exec $tclsh_path \
				    << "source $test_path/test.tcl; \
				    run_method -$i $j $j $display $run" \
				    >>& ALL.OUT } res] {
					set o [open ALL.OUT a]
					puts $o \
					    "FAIL: [format "test%03d" $j] $i"
					close $o
				}
			}
		}
		if [catch {exec $tclsh_path \
		    << "source $test_path/test.tcl; \
		    subdb -$i $display $run" >>& ALL.OUT } res] {
			set o [open ALL.OUT a]
			puts $o "FAIL: subdb -$i test"
			close $o
		}
	}

	# If not actually running, no need to check for failure.
	# If running in the context of the larger 'run_all' we don't
	# check for failure here either.
	if { $run == 0 || $std_only == 0 } {
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

proc r { args } {
	global envtests
	global recdtests
	global subdbtests
	global deadtests
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
		set l [ lindex $args 0 ]
		switch $l {
			archive {
				if { $display } {
					puts "eval archive [lrange $args 1 end]"
				}
				if { $run } {
					check_handles
					eval archive [lrange $args 1 end]
				}
			}
			byte {
				foreach method \
			"-hash -btree -recno -queue -queueext -frecno" {
					if { $display } {
						puts "byteorder $method"
					}
					if { $run } {
						check_handles
						byteorder $method
					}
				}
			}
			dbm {
				if { $display } {
					puts "dbm"
				}
				if { $run } {
					check_handles
					dbm
				}
			}
			dead {
				for { set i 1 } { $i <= $deadtests } \
				    { incr i } {
					if { $display } {
						puts "eval dead00$i\
						    [lrange $args 1 end]"
					}
					if { $run } {
						check_handles
						eval dead00$i\
						    [lrange $args 1 end]
					}
				}
			}
			env {
				for { set i 1 } { $i <= $envtests } {incr i} {
					if { $display } {
						puts "eval env00$i"
					}
					if { $run } {
						check_handles
						eval env00$i
					}
				}
			}
			hsearch {
				if { $display } { puts "hsearch" }
				if { $run } {
					check_handles
					hsearch
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
			lock {
				if { $display } {
					puts \
					    "eval locktest [lrange $args 1 end]"
				}
				if { $run } {
					check_handles
					eval locktest [lrange $args 1 end]
				}
			}
			log {
				if { $display } {
					puts "eval logtest [lrange $args 1 end]"
				}
				if { $run } {
					check_handles
					eval logtest [lrange $args 1 end]
				}
			}
			mpool {
				eval r $saveflags mpool1
				eval r $saveflags mpool2
				eval r $saveflags mpool3
			}
			mpool1 {
				if { $display } {
					puts "eval mpool [lrange $args 1 end]"
				}
				if { $run } {
					check_handles
					eval mpool [lrange $args 1 end]
				}
			}
			mpool2 {
				if { $display } {
					puts "eval mpool\
					    -mem system [lrange $args 1 end]"
				}
				if { $run } {
					check_handles
					eval mpool\
					    -mem system [lrange $args 1 end]
				}
			}
			mpool3 {
				if { $display } {
					puts "eval mpool\
					    -mem private [lrange $args 1 end]"
				}
				if { $run } {
					eval mpool\
					    -mem private [lrange $args 1 end]
				}
			}
			mutex {
				if { $display } {
					puts "eval mutex [lrange $args 1 end]"
				}
				if { $run } {
					check_handles
					eval mutex [lrange $args 1 end]
				}
			}
			ndbm {
				if { $display } { puts ndbm }
				if { $run } {
					check_handles
					ndbm
				}
			}
			recd {
				if { $display } { puts run_recds }
				if { $run } {
					check_handles
					run_recds
				}
			}
			rpc {
				# RPC must be run as one unit due to server,
				# so just print "r rpc" in the display case.
				if { $display } { puts "r rpc" }
				if { $run } {
					check_handles
					eval rpc001
					check_handles
					eval rpc002
					if { [catch {run_rpcmethod -txn} ret]\
					    != 0 } {
						puts $ret
					}
					foreach method \
			"hash queue queueext recno frecno rrecno rbtree btree" {
						if { [catch {run_rpcmethod \
						    -$method} ret] != 0 } {
							puts $ret
						}
					}
				}
			}
			rsrc {
				if { $display } { puts "rsrc001\nrsrc002" }
				if { $run } {
					check_handles
					rsrc001
					check_handles
					rsrc002
				}
			}
			subdb {
				eval r $saveflags subdb_gen

				foreach method \
			"btree rbtree hash queue queueext recno frecno rrecno" {
					check_handles
					eval subdb -$method $display $run
				}
			}
			subdb_gen {
				if { $display } {
					puts "subdbtest001 ; verify_dir"
					puts "subdbtest002 ; verify_dir"
				}
				if { $run } {
					check_handles
					eval subdbtest001
					verify_dir
					check_handles
					eval subdbtest002
					verify_dir
				}
			}
			txn {
				if { $display } {
					puts "txntest [lrange $args 1 end]"
				}
				if { $run } {
					check_handles
					eval txntest [lrange $args 1 end]
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

proc run_method { method {start 1} {stop 0} {display 0} {run 1} \
    { outfile stdout } args } {
	global __debug_on
	global __debug_print
	global parms
	global runtests
	source ./include.tcl

	if { $stop == 0 } {
		set stop $runtests
	}
	if { $run == 1 } {
		puts $outfile "run_method: $method $start $stop $args"
	}

	if {[catch {
		for { set i $start } { $i <= $stop } {incr i} {
			set name [format "test%03d" $i]
			if { [info exists parms($name)] != 1 } {
				puts "[format Test%03d $i] disabled in\
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

proc run_rpcmethod { type {start 1} {stop 0} {largs ""} } {
	global __debug_on
	global __debug_print
	global parms
	global runtests
	source ./include.tcl

	if { $stop == 0 } {
		set stop $runtests
	}
	puts "run_rpcmethod: $type $start $stop $largs"

	set save_largs $largs
	if { [string compare $rpc_server "localhost"] == 0 } {
	       set dpid [exec $util_path/berkeley_db_svc -h $rpc_testdir &]
	} else {
	       set dpid [exec rsh $rpc_server $rpc_path/berkeley_db_svc \
		   -h $rpc_testdir &]
	}
	puts "\tRun_rpcmethod.a: starting server, pid $dpid"
	tclsleep 2
	remote_cleanup $rpc_server $rpc_testdir $testdir

	set home [file tail $rpc_testdir]

	set txn ""
	set use_txn 0
	if { [string first "txn" $type] != -1 } {
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
		set env [eval {berkdb env -create -mode 0644 -home $home \
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
					puts "[format Test%03d $i] disabled in\
					    testparams.tcl; skipping."
					continue
				}
				remote_cleanup $rpc_server $rpc_testdir $testdir
				#
				# Set server cachesize to 1Mb.  Otherwise some
				# tests won't fit (like test084 -btree).
				#
				set env [eval {berkdb env -create -mode 0644 \
				    -home $home -server $rpc_server \
				    -client_timeout 10000 \
				    -cachesize {0 1048576 1} }]
				error_check_good env_open \
				    [is_valid_env $env] TRUE
				append largs " -env $env "

				puts "[timestamp]"
				eval $name $type $parms($name) $largs
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
		exec $KILL $dpid
		if {[string first FAIL $errorInfo] == -1} {
			error "FAIL:[timestamp]\
			    run_rpcmethod: $type $i: $theError"
		} else {
			error $theError;
		}
	}
	exec $KILL $dpid

}

proc run_rpcnoserver { type {start 1} {stop 0} {largs ""} } {
	global __debug_on
	global __debug_print
	global parms
	global runtests
	source ./include.tcl

	if { $stop == 0 } {
		set stop $runtests
	}
	puts "run_rpcnoserver: $type $start $stop $largs"

	set save_largs $largs
	remote_cleanup $rpc_server $rpc_testdir $testdir
	set home [file tail $rpc_testdir]

	set txn ""
	set use_txn 0
	if { [string first "txn" $type] != -1 } {
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
		set env [eval {berkdb env -create -mode 0644 -home $home \
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
					puts "[format Test%03d $i] disabled in\
					    testparams.tcl; skipping."
					continue
				}
				remote_cleanup $rpc_server $rpc_testdir $testdir
				#
				# Set server cachesize to 1Mb.  Otherwise some
				# tests won't fit (like test084 -btree).
				#
				set env [eval {berkdb env -create -mode 0644 \
				    -home $home -server $rpc_server \
				    -client_timeout 10000 \
				    -cachesize {0 1048576 1} }]
				error_check_good env_open \
				    [is_valid_env $env] TRUE
				append largs " -env $env "

				puts "[timestamp]"
				eval $name $type $parms($name) $largs
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
			    run_rpcnoserver: $type $i: $theError"
		} else {
			error $theError;
		}
	}

}

#
# Run method tests in one environment.  (As opposed to run_envmethod1
# which runs each test in its own, new environment.)
#
proc run_envmethod { type {start 1} {stop 0} {largs ""} } {
	global __debug_on
	global __debug_print
	global parms
	global runtests
	source ./include.tcl

	if { $stop == 0 } {
		set stop $runtests
	}
	puts "run_envmethod: $type $start $stop $largs"

	set save_largs $largs
	env_cleanup $testdir
	set txn ""
	set stat [catch {
		for { set i $start } { $i <= $stop } {incr i} {
			check_handles
			set env [eval {berkdb env -create -mode 0644 \
			    -home $testdir}]
			error_check_good env_open [is_valid_env $env] TRUE
			append largs " -env $env "

			puts "[timestamp]"
			set name [format "test%03d" $i]
			if { [info exists parms($name)] != 1 } {
				puts "[format Test%03d $i] disabled in\
				    testparams.tcl; skipping."
				continue
			}
			eval $name $type $parms($name) $largs
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
			    -home $testdir] 0
		}
	} res]
	if { $stat != 0} {
		global errorInfo;

		set fnl [string first "\n" $errorInfo]
		set theError [string range $errorInfo 0 [expr $fnl - 1]]
		if {[string first FAIL $errorInfo] == -1} {
			error "FAIL:[timestamp]\
			    run_envmethod: $type $i: $theError"
		} else {
			error $theError;
		}
	}

}

proc subdb { method display run {outfile stdout} args} {
	global subdbtests testdir
	global parms

	for { set i 1 } {$i <= $subdbtests} {incr i} {
		set name [format "subdb%03d" $i]
		if { [info exists parms($name)] != 1 } {
			puts "[format Subdb%03d $i] disabled in\
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

proc run_recd { method {start 1} {stop 0} args } {
	global __debug_on
	global __debug_print
	global parms
	global recdtests
	global log_log_record_types
	source ./include.tcl

	if { $stop == 0 } {
		set stop $recdtests
	}
	puts "run_recd: $method $start $stop $args"

	if {[catch {
		for { set i $start } { $i <= $stop } {incr i} {
			check_handles
			puts "[timestamp]"
			set name [format "recd%03d" $i]
			# By redirecting stdout to stdout, we make exec
			# print output rather than simply returning it.
			exec $tclsh_path << "source $test_path/test.tcl; \
			    set log_log_record_types $log_log_record_types; \
			    eval $name $method" >@ stdout
			if { $__debug_print != 0 } {
				puts ""
			}
			if { $__debug_on != 0 } {
				debug
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
			    run_recd: $method $i: $theError"
		} else {
			error $theError;
		}
	}
}

proc run_recds { } {
	global log_log_record_types

	set log_log_record_types 1
	logtrack_init
	foreach method \
	    "btree rbtree hash queue queueext recno frecno rrecno" {
		check_handles
		if { [catch \
		    {run_recd -$method} ret ] != 0 } {
			puts $ret
		}
	}
	logtrack_summary
	set log_log_record_types 0
}

proc run_all { args } {
	global runtests
	global subdbtests
	source ./include.tcl

	fileremove -f ALL.OUT

	set exflgs [eval extractflags $args]
	set flags [lindex $exflgs 1]
	set display 1
	set run 1
	set am_only 0
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

	set test_pagesizes { 512 8192 65536 }
	set args [lindex $exflgs 0]
	set save_args $args

	foreach pgsz $test_pagesizes {
		set args $save_args
		append args " -pagesize $pgsz"
		if { $am_only == 0 } {
			# Run recovery tests.
			#
			# XXX These too are broken into separate tclsh
			# instantiations so we don't require so much 
			# memory, but I think it's cleaner
			# and more useful to do it down inside proc r than here,
			# since "r recd" gets done a lot and needs to work.
			puts "Running recovery tests with pagesize $pgsz"
			if [catch {exec $tclsh_path \
			    << "source $test_path/test.tcl; \
				r $rflags recd $args" >>& ALL.OUT } res] {
				set o [open ALL.OUT a]
				puts $o "FAIL: recd test"
				close $o
			}
		}
	
		# Access method tests.
		#
		# XXX
		# Broken up into separate tclsh instantiations so 
		# we don't require so much memory.
		foreach i \
		   "btree rbtree hash queue queueext recno frecno rrecno" {
			puts "Running $i tests with pagesize $pgsz"
			for { set j 1 } { $j <= $runtests } {incr j} {
				if { $run == 0 } {
					set o [open ALL.OUT a]
					run_method -$i $j $j $display \
					    $run $o $args
					close $o
				}
				if { $run } {
					if [catch {exec $tclsh_path \
					    << "source $test_path/test.tcl; \
					    run_method -$i $j $j $display \
					    $run stdout $args" \
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
			if { $run == 0 } {
				set o [open ALL.OUT a]
				subdb -$i $display $run $o $args
				close $o
			}
			if { $run == 1 } {
				if [catch {exec $tclsh_path \
				    << "source $test_path/test.tcl; \
				    subdb -$i $display $run stdout $args" \
				    >>& ALL.OUT } res] {
					set o [open ALL.OUT a]
					puts $o "FAIL: subdb -$i test"
					close $o
				}
			}
		}
	}
	set args $save_args
	#
	# Run access method tests at default page size in one env.
	#
	foreach i "btree rbtree hash queue queueext recno frecno rrecno" {
		puts "Running $i tests in an env"
		if { $run == 0 } {
			set o [open ALL.OUT a]
			run_envmethod1 -$i 1 $runtests $display \
			    $run $o $args
			close $o
		}
		if { $run } {
			if [catch {exec $tclsh_path \
			    << "source $test_path/test.tcl; \
			    run_envmethod1 -$i 1 $runtests $display \
			    $run stdout $args" \
			    >>& ALL.OUT } res] {
				set o [open ALL.OUT a]
				puts $o \
				    "FAIL: run_envmethod1 $i"
				close $o
			}
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
	global parms
	global runtests
	source ./include.tcl

	if { $stop == 0 } {
		set stop $runtests
	}
	if { $run == 1 } {
		puts "run_envmethod1: $method $start $stop $args"
	}

	set txn ""
	if { $run == 1 } {
		check_handles
		env_cleanup $testdir
		error_check_good envremove [berkdb envremove -home $testdir] 0
		set env [eval {berkdb env -create -mode 0644 -home $testdir}]
		error_check_good env_open [is_valid_env $env] TRUE
		append largs " -env $env "
	}

	set stat [catch {
		for { set i $start } { $i <= $stop } {incr i} {
			set name [format "test%03d" $i]
			if { [info exists parms($name)] != 1 } {
				puts "[format Test%03d $i] disabled in\
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
				eval $name $method $parms($name) $largs
				if { $__debug_print != 0 } {
					puts $outfile ""
				}
				if { $__debug_on != 0 } {
					debug
				}
			}
			flush stdout
			flush stderr
		}
	} res]
	if { $run == 1 } {
		error_check_good envclose [$env close] 0
	}
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

}

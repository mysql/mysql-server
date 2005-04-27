# Code to load up the tests in to the Queue database
# $Id: parallel.tcl,v 11.28 2002/09/05 17:23:06 sandstro Exp $
proc load_queue { file  {dbdir RUNQUEUE} nitems } {

	puts -nonewline "Loading run queue with $nitems items..."
	flush stdout

	set env [berkdb_env -create -lock -home $dbdir]
	error_check_good dbenv [is_valid_env $env] TRUE

	set db [eval {berkdb_open -env $env -create -truncate \
            -mode 0644 -len 120 -queue queue.db} ]
        error_check_good dbopen [is_valid_db $db] TRUE

	set fid [open $file]

	set count 0

        while { [gets $fid str] != -1 } {
		set testarr($count) $str
		incr count
	}

	# Randomize array of tests.
	set rseed [pid]
	berkdb srand $rseed
	puts -nonewline "randomizing..."
	flush stdout
	for { set i 0 } { $i < $count } { incr i } {
		set j [berkdb random_int $i [expr $count - 1]]

		set tmp $testarr($i)
		set testarr($i) $testarr($j)
		set testarr($j) $tmp
	}

	if { [string compare ALL $nitems] != 0 } {
		set maxload $nitems
	} else {
		set maxload $count
	}

	puts "loading..."
	flush stdout
	for { set i 0 } { $i < $maxload } { incr i } {
		set str $testarr($i)
                set ret [eval {$db put -append $str} ]
                error_check_good put:$db $ret [expr $i + 1]
        }

	puts "Loaded $maxload records (out of $count)."
	close $fid
	$db close
	$env close
}

proc init_runqueue { {dbdir RUNQUEUE} nitems list} {

	if { [file exists $dbdir] != 1 } {
		file mkdir $dbdir
	}
	puts "Creating test list..."
	$list -n
	load_queue ALL.OUT $dbdir $nitems
	file delete TEST.LIST
	file rename ALL.OUT TEST.LIST
#	file delete ALL.OUT
}

proc run_parallel { nprocs {list run_all} {nitems ALL} } {
	set basename ./PARALLEL_TESTDIR
	set queuedir ./RUNQUEUE
	source ./include.tcl

	mkparalleldirs $nprocs $basename $queuedir

	init_runqueue $queuedir $nitems $list

	set basedir [pwd]
	set pidlist {}
	set queuedir ../../[string range $basedir \
	    [string last "/" $basedir] end]/$queuedir

	for { set i 1 } { $i <= $nprocs } { incr i } {
		fileremove -f ALL.OUT.$i
		set ret [catch {
			set p [exec $tclsh_path << \
			    "source $test_path/test.tcl;\
			    run_queue $i $basename.$i $queuedir $nitems" &]
			lappend pidlist $p
			set f [open $testdir/begin.$p w]
			close $f
		} res]
	}
	watch_procs $pidlist 300 360000

	set failed 0
	for { set i 1 } { $i <= $nprocs } { incr i } {
		if { [check_failed_run ALL.OUT.$i] != 0 } {
			set failed 1
			puts "Regression tests failed in process $i."
		}
	}
	if { $failed == 0 } {
		puts "Regression tests succeeded."
	}
}

proc run_queue { i rundir queuedir nitems } {
	set builddir [pwd]
	file delete $builddir/ALL.OUT.$i
	cd $rundir

	puts "Parallel run_queue process $i (pid [pid]) starting."

	source ./include.tcl
	global env

	set dbenv [berkdb_env -create -lock -home $queuedir]
	error_check_good dbenv [is_valid_env $dbenv] TRUE

	set db [eval {berkdb_open -env $dbenv \
            -mode 0644 -len 120 -queue queue.db} ]
        error_check_good dbopen [is_valid_db $db] TRUE

	set dbc  [eval $db cursor]
        error_check_good cursor [is_valid_cursor $dbc $db] TRUE

	set count 0
	set waitcnt 0

	while { $waitcnt < 5 } {
		set line [$db get -consume]
		if { [ llength $line ] > 0 } {
			set cmd [lindex [lindex $line 0] 1]
			set num [lindex [lindex $line 0] 0]
			set o [open $builddir/ALL.OUT.$i a]
			puts $o "\nExecuting record $num ([timestamp -w]):\n"
			set tdir "TESTDIR.$i"
			regsub {TESTDIR} $cmd $tdir cmd
			puts $o $cmd
			close $o
			if { [expr {$num % 10} == 0] } {
				puts "Starting test $num of $nitems"
			}
			#puts "Process $i, record $num:\n$cmd"
			set env(PURIFYOPTIONS) \
	"-log-file=./test$num.%p -follow-child-processes -messages=first"
			set env(PURECOVOPTIONS) \
	"-counts-file=./cov.pcv -log-file=./cov.log -follow-child-processes"
			if [catch {exec $tclsh_path \
			     << "source $test_path/test.tcl; $cmd" \
			     >>& $builddir/ALL.OUT.$i } res] {
                                set o [open $builddir/ALL.OUT.$i a]
                                puts $o "FAIL: '$cmd': $res"
                                close $o
                        }
			env_cleanup $testdir
			set o [open $builddir/ALL.OUT.$i a]
			puts $o "\nEnding record $num ([timestamp])\n"
			close $o
			incr count
		} else {
			incr waitcnt
			tclsleep 1
		}
	}

	puts "Process $i: $count commands executed"

	$dbc close
	$db close
	$dbenv close

	#
	# We need to put the pid file in the builddir's idea
	# of testdir, not this child process' local testdir.
	# Therefore source builddir's include.tcl to get its
	# testdir.
	# !!! This resets testdir, so don't do anything else
	# local to the child after this.
	source $builddir/include.tcl

	set f [open $builddir/$testdir/end.[pid] w]
	close $f
}

proc mkparalleldirs { nprocs basename queuedir } {
	source ./include.tcl
	set dir [pwd]

	if { $is_windows_test != 1 } {
	        set EXE ""
	} else {
		set EXE ".exe"
        }
	for { set i 1 } { $i <= $nprocs } { incr i } {
		set destdir $basename.$i
		catch {file mkdir $destdir}
		puts "Created $destdir"
		if { $is_windows_test == 1 } {
			catch {file mkdir $destdir/Debug}
			catch {eval file copy \
			    [eval glob {$dir/Debug/*.dll}] $destdir/Debug}
		}
		catch {eval file copy \
		    [eval glob {$dir/{.libs,include.tcl}}] $destdir}
		# catch {eval file copy $dir/$queuedir $destdir}
		catch {eval file copy \
		    [eval glob {$dir/db_{checkpoint,deadlock}$EXE} \
		    {$dir/db_{dump,load,printlog,recover,stat,upgrade}$EXE} \
		    {$dir/db_{archive,verify}$EXE}] \
		    $destdir}

		# Create modified copies of include.tcl in parallel
		# directories so paths still work.

		set infile [open ./include.tcl r]
		set d [read $infile]
		close $infile

		regsub {test_path } $d {test_path ../} d
		regsub {src_root } $d {src_root ../} d
		set tdir "TESTDIR.$i"
		regsub -all {TESTDIR} $d $tdir d
		regsub {KILL \.} $d {KILL ..} d
		set outfile [open $destdir/include.tcl w]
		puts $outfile $d
		close $outfile

		global svc_list
		foreach svc_exe $svc_list {
			if { [file exists $dir/$svc_exe] } {
				catch {eval file copy $dir/$svc_exe $destdir}
			}
		}
	}
}

proc run_ptest { nprocs test args } {
	global parms
	set basename ./PARALLEL_TESTDIR
	set queuedir NULL
	source ./include.tcl

	mkparalleldirs $nprocs $basename $queuedir

	if { [info exists parms($test)] } {
		foreach method \
		    "hash queue queueext recno rbtree frecno rrecno btree" {
			if { [eval exec_ptest $nprocs $basename \
			    $test $method $args] != 0 } {
				break
			}
		}
	} else {
		eval exec_ptest $nprocs $basename $test $args
	}
}

proc exec_ptest { nprocs basename test args } {
	source ./include.tcl

	set basedir [pwd]
	set pidlist {}
	puts "Running $nprocs parallel runs of $test"
	for { set i 1 } { $i <= $nprocs } { incr i } {
		set outf ALL.OUT.$i
		fileremove -f $outf
		set ret [catch {
			set p [exec $tclsh_path << \
		 	    "cd $basename.$i;\
		            source ../$test_path/test.tcl;\
		            $test $args" >& $outf &]
			lappend pidlist $p
			set f [open $testdir/begin.$p w]
			close $f
		} res]
	}
	watch_procs $pidlist 30 36000
	set failed 0
	for { set i 1 } { $i <= $nprocs } { incr i } {
		if { [check_failed_run ALL.OUT.$i] != 0 } {
			set failed 1
			puts "Test $test failed in process $i."
		}
	}
	if { $failed == 0 } {
		puts "Test $test succeeded all processes"
		return 0
	} else {
		puts "Test failed: stopping"
		return 1
	}
}

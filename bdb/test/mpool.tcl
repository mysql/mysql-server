# See the file LICENSE for redistribution information.
#
# Copyright (c) 1996, 1997, 1998, 1999, 2000
#	Sleepycat Software.  All rights reserved.
#
#	$Id: mpool.tcl,v 11.34 2001/01/18 04:58:07 krinsky Exp $
#
# Options are:
# -cachesize {gbytes bytes ncache}
# -nfiles <files>
# -iterations <iterations>
# -pagesize <page size in bytes>
# -dir <directory in which to store memp>
# -stat
proc memp_usage {} {
	puts "memp -cachesize {gbytes bytes ncache}"
	puts "\t-nfiles <files>"
	puts "\t-iterations <iterations>"
	puts "\t-pagesize <page size in bytes>"
	puts "\t-dir <memp directory>"
	puts "\t-mem {private system}"
	return
}

proc mpool { args } {
	source ./include.tcl
	global errorCode

	puts "mpool {$args} running"
	# Set defaults
	set cachearg " -cachesize {0 200000 3}"
	set nfiles 5
	set iterations 500
	set pagesize "512 1024 2048 4096 8192"
	set npages 100
	set procs 4
	set seeds ""
	set shm_key 1
	set dostat 0
	set flags ""
	for { set i 0 } { $i < [llength $args] } {incr i} {
		switch -regexp -- [lindex $args $i] {
			-c.* {
			    incr i
			    set cachesize [lindex $args $i]
			    set cachearg " -cachesize $cachesize"
			}
			-d.* { incr i; set testdir [lindex $args $i] }
			-i.* { incr i; set iterations [lindex $args $i] }
			-me.* {
				incr i
				if { [string \
				    compare [lindex $args $i] private] == 0 } {
					set flags -private
				} elseif { [string \
				    compare [lindex $args $i] system] == 0 } {
					#
					# We need to use a shm id.  Use one
					# that is the same each time so that
					# we do not grow segments infinitely.
					set flags "-system_mem -shm_key $shm_key"
				} else {
					puts -nonewline \
					    "FAIL:[timestamp] Usage: "
					memp_usage
					return
				}
			}
			-nf.* { incr i; set nfiles [lindex $args $i] }
			-np.* { incr i; set npages [lindex $args $i] }
			-pa.* { incr i; set pagesize [lindex $args $i] }
			-pr.* { incr i; set procs [lindex $args $i] }
			-se.* { incr i; set seeds [lindex $args $i] }
			-st.* { set dostat 1 }
			default {
				puts -nonewline "FAIL:[timestamp] Usage: "
				memp_usage
				return
			}
		}
	}

	# Clean out old directory
	env_cleanup $testdir

	# Open the memp with region init specified
	set ret [catch {eval {berkdb env -create -mode 0644}\
	    $cachearg {-region_init -home $testdir} $flags} res]
	if { $ret == 0 } {
		set env $res
	} else {
		# If the env open failed, it may be because we're on a platform
		# such as HP-UX 10 that won't support mutexes in shmget memory.
		# Or QNX, which doesn't support system memory at all.
		# Verify that the return value was EINVAL or EOPNOTSUPP
		# and bail gracefully.
		error_check_good is_shm_test [is_substr $flags -system_mem] 1
		error_check_good returned_error [expr \
		    [is_substr $errorCode EINVAL] || \
		    [is_substr $errorCode EOPNOTSUPP]] 1
		puts "Warning:\
		     platform does not support mutexes in shmget memory."
		puts "Skipping shared memory mpool test."
		return
	}
	error_check_good env_open [is_substr $env env] 1

	reset_env $env
	env_cleanup $testdir

	# Now open without region init
	set env [eval {berkdb env -create -mode 0644}\
	    $cachearg {-home $testdir} $flags]
	error_check_good evn_open [is_substr $env env] 1

	memp001 $env \
	    $testdir $nfiles $iterations [lindex $pagesize 0] $dostat $flags
	reset_env $env
	set ret [berkdb envremove -home $testdir]
	error_check_good env_remove $ret 0
	env_cleanup $testdir

	memp002 $testdir \
	    $procs $pagesize $iterations $npages $seeds $dostat $flags
	set ret [berkdb envremove -home $testdir]
	error_check_good env_remove $ret 0
	env_cleanup $testdir

	memp003 $testdir $iterations $flags
	set ret [berkdb envremove -home $testdir]
	error_check_good env_remove $ret 0

	env_cleanup $testdir
}

proc memp001 {env dir n iter psize dostat flags} {
	source ./include.tcl
	global rand_init

	puts "Memp001: {$flags} random update $iter iterations on $n files."

	# Open N memp files
	for {set i 1} {$i <= $n} {incr i} {
		set fname "data_file.$i"
		file_create $dir/$fname 50 $psize

		set mpools($i) \
		    [$env mpool -create -pagesize $psize -mode 0644 $fname]
		error_check_good mp_open [is_substr $mpools($i) $env.mp] 1
	}

	# Now, loop, picking files at random
	berkdb srand $rand_init
	for {set i 0} {$i < $iter} {incr i} {
		set mpool $mpools([berkdb random_int 1 $n])
		set p1 [get_range $mpool 10]
		set p2 [get_range $mpool 10]
		set p3 [get_range $mpool 10]
		set p1 [replace $mpool $p1]
		set p3 [replace $mpool $p3]
		set p4 [get_range $mpool 20]
		set p4 [replace $mpool $p4]
		set p5 [get_range $mpool 10]
		set p6 [get_range $mpool 20]
		set p7 [get_range $mpool 10]
		set p8 [get_range $mpool 20]
		set p5 [replace $mpool $p5]
		set p6 [replace $mpool $p6]
		set p9 [get_range $mpool 40]
		set p9 [replace $mpool $p9]
		set p10 [get_range $mpool 40]
		set p7 [replace $mpool $p7]
		set p8 [replace $mpool $p8]
		set p9 [replace $mpool $p9]
		set p10 [replace $mpool $p10]
	}

	if { $dostat == 1 } {
		puts [$env mpool_stat]
		for {set i 1} {$i <= $n} {incr i} {
			error_check_good mp_sync [$mpools($i) fsync] 0
		}
	}

	# Close N memp files
	for {set i 1} {$i <= $n} {incr i} {
		error_check_good memp_close:$mpools($i) [$mpools($i) close] 0
		fileremove -f $dir/data_file.$i
	}
}

proc file_create { fname nblocks blocksize } {
	set fid [open $fname w]
	for {set i 0} {$i < $nblocks} {incr i} {
		seek $fid [expr $i * $blocksize] start
		puts -nonewline $fid $i
	}
	seek $fid [expr $nblocks * $blocksize - 1]

	# We don't end the file with a newline, because some platforms (like
	# Windows) emit CR/NL.  There does not appear to be a BINARY open flag
	# that prevents this.
	puts -nonewline $fid "Z"
	close $fid

	# Make sure it worked
	if { [file size $fname] != $nblocks * $blocksize } {
		error "FAIL: file_create could not create correct file size"
	}
}

proc get_range { mpool max } {
	set pno [berkdb random_int 0 $max]
	set p [$mpool get $pno]
	error_check_good page [is_valid_page $p $mpool] TRUE
	set got [$p pgnum]
	if { $got != $pno } {
		puts "Get_range: Page mismatch page |$pno| val |$got|"
	}
	set ret [$p init "Page is pinned by [pid]"]
	error_check_good page_init $ret 0

	return $p
}

proc replace { mpool p } {
	set pgno [$p pgnum]

	set ret [$p init "Page is unpinned by [pid]"]
	error_check_good page_init $ret 0

	set ret [$p put -dirty]
	error_check_good page_put $ret 0

	set p2 [$mpool get $pgno]
	error_check_good page [is_valid_page $p2 $mpool] TRUE

	return $p2
}

proc memp002 { dir procs psizes iterations npages seeds dostat flags } {
	source ./include.tcl

	puts "Memp002: {$flags} Multiprocess mpool tester"

	if { [is_substr $flags -private] != 0 } {
		puts "Memp002 skipping\
		    multiple processes not supported by private memory"
		return
	}
	set iter [expr $iterations / $procs]

	# Clean up old stuff and create new.
	env_cleanup $dir

	for { set i 0 } { $i < [llength $psizes] } { incr i } {
		fileremove -f $dir/file$i
	}
	set e [eval {berkdb env -create -lock -home $dir} $flags]
	error_check_good dbenv [is_valid_widget $e env] TRUE

	set pidlist {}
	for { set i 0 } { $i < $procs } {incr i} {
		if { [llength $seeds] == $procs } {
			set seed [lindex $seeds $i]
		} else {
			set seed -1
		}

		puts "$tclsh_path\
		    $test_path/mpoolscript.tcl $dir $i $procs \
		    $iter $psizes $npages 3 $flags > \
		    $dir/memp002.$i.out &"
		set p [exec $tclsh_path $test_path/wrap.tcl \
		    mpoolscript.tcl $dir/memp002.$i.out $dir $i $procs \
		    $iter $psizes $npages 3 $flags &]
		lappend pidlist $p
	}
	puts "Memp002: $procs independent processes now running"
	watch_procs

	reset_env $e
}

# Test reader-only/writer process combinations; we use the access methods
# for testing.
proc memp003 { dir {nentries 10000} flags } {
	global alphabet
	source ./include.tcl

	puts "Memp003: {$flags} Reader/Writer tests"

	if { [is_substr $flags -private] != 0 } {
		puts "Memp003 skipping\
		    multiple processes not supported by private memory"
		return
	}

	env_cleanup $dir
	set psize 1024
	set testfile mpool.db
	set t1 $dir/t1

	# Create an environment that the two processes can share
	set c [list 0 [expr $psize * 10] 3]
	set dbenv [eval {berkdb env \
	    -create -lock -home $dir -cachesize $c} $flags]
	error_check_good dbenv [is_valid_env $dbenv] TRUE

	# First open and create the file.

	set db [berkdb_open -env $dbenv -create -truncate \
	    -mode 0644 -pagesize $psize -btree $testfile]
	error_check_good dbopen/RW [is_valid_db $db] TRUE

	set did [open $dict]
	set txn ""
	set count 0

	puts "\tMemp003.a: create database"
	set keys ""
	# Here is the loop where we put and get each key/data pair
	while { [gets $did str] != -1 && $count < $nentries } {
		lappend keys $str

		set ret [eval {$db put} $txn {$str $str}]
		error_check_good put $ret 0

		set ret [eval {$db get} $txn {$str}]
		error_check_good get $ret [list [list $str $str]]

		incr count
	}
	close $did
	error_check_good close [$db close] 0

	# Now open the file for read-only
	set db [berkdb_open -env $dbenv -rdonly $testfile]
	error_check_good dbopen/RO [is_substr $db db] 1

	puts "\tMemp003.b: verify a few keys"
	# Read and verify a couple of keys; saving them to check later
	set testset ""
	for { set i 0 } { $i < 10 } { incr i } {
		set ndx [berkdb random_int 0 [expr $nentries - 1]]
		set key [lindex $keys $ndx]
		if { [lsearch $testset $key] != -1 } {
			incr i -1
			continue;
		}

		# The remote process stuff is unhappy with
		# zero-length keys;  make sure we don't pick one.
		if { [llength $key] == 0 } {
			incr i -1
			continue
		}

		lappend testset $key

		set ret [eval {$db get} $txn {$key}]
		error_check_good get/RO $ret [list [list $key $key]]
	}

	puts "\tMemp003.c: retrieve and modify keys in remote process"
	# Now open remote process where we will open the file RW
	set f1 [open |$tclsh_path r+]
	puts $f1 "source $test_path/test.tcl"
	puts $f1 "flush stdout"
	flush $f1

	set c [concat "{" [list 0 [expr $psize * 10] 3] "}" ]
	set remote_env [send_cmd $f1 \
	    "berkdb env -create -lock -home $dir -cachesize $c $flags"]
	error_check_good remote_dbenv [is_valid_env $remote_env] TRUE

	set remote_db [send_cmd $f1 "berkdb_open -env $remote_env $testfile"]
	error_check_good remote_dbopen [is_valid_db $remote_db] TRUE

	foreach k $testset {
		# Get the key
		set ret [send_cmd $f1 "$remote_db get $k"]
		error_check_good remote_get $ret [list [list $k $k]]

		# Now replace the key
		set ret [send_cmd $f1 "$remote_db put $k $k$k"]
		error_check_good remote_put $ret 0
	}

	puts "\tMemp003.d: verify changes in local process"
	foreach k $testset {
		set ret [eval {$db get} $txn {$key}]
		error_check_good get_verify/RO $ret [list [list $key $key$key]]
	}

	puts "\tMemp003.e: Fill up the cache with dirty buffers"
	foreach k $testset {
		# Now rewrite the keys with BIG data
		set data [replicate $alphabet 32]
		set ret [send_cmd $f1 "$remote_db put $k $data"]
		error_check_good remote_put $ret 0
	}

	puts "\tMemp003.f: Get more pages for the read-only file"
	dump_file $db $txn $t1 nop

	puts "\tMemp003.g: Sync from the read-only file"
	error_check_good db_sync [$db sync] 0
	error_check_good db_close [$db close] 0

	set ret [send_cmd $f1 "$remote_db close"]
	error_check_good remote_get $ret 0

	# Close the environment both remotely and locally.
	set ret [send_cmd $f1 "$remote_env close"]
	error_check_good remote:env_close $ret 0
	close $f1

	reset_env $dbenv
}

# See the file LICENSE for redistribution information.
#
# Copyright (c) 1996, 1997, 1998, 1999, 2000
#	Sleepycat Software.  All rights reserved.
#
#	$Id: testutils.tcl,v 11.86 2001/01/18 23:21:14 krinsky Exp $
#
# Test system utilities
#
# Timestamp -- print time along with elapsed time since last invocation
# of timestamp.
proc timestamp {{opt ""}} {
	global __timestamp_start

	if {[string compare $opt "-r"] == 0} {
		clock seconds
	} elseif {[string compare $opt "-t"] == 0} {
		# -t gives us the current time in the format expected by
		# db_recover -t.
		return [clock format [clock seconds] -format "%y%m%d%H%M.%S"]
	} else {
		set now [clock seconds]

		if {[catch {set start $__timestamp_start}] != 0} {
			set __timestamp_start $now
		}
		set start $__timestamp_start

		set elapsed [expr $now - $start]
		set the_time [clock format $now -format ""]
		set __timestamp_start $now

		format "%02d:%02d:%02d (%02d:%02d:%02d)" \
		    [__fix_num [clock format $now -format "%H"]] \
		    [__fix_num [clock format $now -format "%M"]] \
		    [__fix_num [clock format $now -format "%S"]] \
		    [expr $elapsed / 3600] \
		    [expr ($elapsed % 3600) / 60] \
		    [expr ($elapsed % 3600) % 60]
	}
}

proc __fix_num { num } {
	set num [string trimleft $num "0"]
	if {[string length $num] == 0} {
		set num "0"
	}
	return $num
}

# Add a {key,data} pair to the specified database where
# key=filename and data=file contents.
proc put_file { db txn flags file } {
	source ./include.tcl

	set fid [open $file r]
	fconfigure $fid -translation binary
	set data [read $fid]
	close $fid

	set ret [eval {$db put} $txn $flags {$file $data}]
	error_check_good put_file $ret 0
}

# Get a {key,data} pair from the specified database where
# key=filename and data=file contents and then write the
# data to the specified file.
proc get_file { db txn flags file outfile } {
	source ./include.tcl

	set fid [open $outfile w]
	fconfigure $fid -translation binary
	if [catch {eval {$db get} $txn $flags {$file}} data] {
		puts -nonewline $fid $data
	} else {
		# Data looks like {{key data}}
		set data [lindex [lindex $data 0] 1]
		puts -nonewline $fid $data
	}
	close $fid
}

# Add a {key,data} pair to the specified database where
# key=file contents and data=file name.
proc put_file_as_key { db txn flags file } {
	source ./include.tcl

	set fid [open $file r]
	fconfigure $fid -translation binary
	set filecont [read $fid]
	close $fid

	# Use not the file contents, but the file name concatenated
	# before the file contents, as a key, to ensure uniqueness.
	set data $file$filecont

	set ret [eval {$db put} $txn $flags {$data $file}]
	error_check_good put_file $ret 0
}

# Get a {key,data} pair from the specified database where
# key=file contents and data=file name
proc get_file_as_key { db txn flags file} {
	source ./include.tcl

	set fid [open $file r]
	fconfigure $fid -translation binary
	set filecont [read $fid]
	close $fid

	set data $file$filecont

	return [eval {$db get} $txn $flags {$data}]
}

# open file and call dump_file to dumpkeys to tempfile
proc open_and_dump_file {
    dbname dbenv txn outfile checkfunc dump_func beg cont} {
	source ./include.tcl
	if { $dbenv == "NULL" } {
		set db [berkdb open -rdonly -unknown $dbname]
		error_check_good dbopen [is_valid_db $db] TRUE
	} else {
		set db [berkdb open -env $dbenv -rdonly -unknown $dbname]
		error_check_good dbopen [is_valid_db $db] TRUE
	}
	$dump_func $db $txn $outfile $checkfunc $beg $cont
	error_check_good db_close [$db close] 0
}

# open file and call dump_file to dumpkeys to tempfile
proc open_and_dump_subfile {
    dbname dbenv txn outfile checkfunc dump_func beg cont subdb} {
	source ./include.tcl

	if { $dbenv == "NULL" } {
		set db [berkdb open -rdonly -unknown $dbname $subdb]
		error_check_good dbopen [is_valid_db $db] TRUE
	} else {
		set db [berkdb open -env $dbenv -rdonly -unknown $dbname $subdb]
		error_check_good dbopen [is_valid_db $db] TRUE
	}
	$dump_func $db $txn $outfile $checkfunc $beg $cont
	error_check_good db_close [$db close] 0
}

# Sequentially read a file and call checkfunc on each key/data pair.
# Dump the keys out to the file specified by outfile.
proc dump_file { db txn outfile checkfunc } {
	source ./include.tcl

	dump_file_direction $db $txn $outfile $checkfunc "-first" "-next"
}

proc dump_file_direction { db txn outfile checkfunc start continue } {
	source ./include.tcl

	set outf [open $outfile w]
	# Now we will get each key from the DB and dump to outfile
	set c [eval {$db cursor} $txn]
	error_check_good db_cursor [is_valid_cursor $c $db] TRUE
	for {set d [$c get $start] } { [llength $d] != 0 } {
	    set d [$c get $continue] } {
		set kd [lindex $d 0]
		set k [lindex $kd 0]
		set d2 [lindex $kd 1]
		$checkfunc $k $d2
		puts $outf $k
		# XXX: Geoff Mainland
		# puts $outf "$k $d2"
	}
	close $outf
	error_check_good curs_close [$c close] 0
}

proc dump_binkey_file { db txn outfile checkfunc } {
	source ./include.tcl

	dump_binkey_file_direction $db $txn $outfile $checkfunc \
	    "-first" "-next"
}
proc dump_bin_file { db txn outfile checkfunc } {
	source ./include.tcl

	dump_bin_file_direction $db $txn $outfile $checkfunc "-first" "-next"
}

# Note: the following procedure assumes that the binary-file-as-keys were
# inserted into the database by put_file_as_key, and consist of the file
# name followed by the file contents as key, to ensure uniqueness.
proc dump_binkey_file_direction { db txn outfile checkfunc begin cont } {
	source ./include.tcl

	set d1 $testdir/d1

	set outf [open $outfile w]

	# Now we will get each key from the DB and dump to outfile
	set c [eval {$db cursor} $txn]
	error_check_good db_cursor [is_valid_cursor $c $db] TRUE

	set inf $d1
	for {set d [$c get $begin] } { [llength $d] != 0 } \
	    {set d [$c get $cont] } {
		set kd [lindex $d 0]
		set keyfile [lindex $kd 0]
		set data [lindex $kd 1]

		set ofid [open $d1 w]
		fconfigure $ofid -translation binary

		# Chop off the first few bytes--that's the file name,
		# added for uniqueness in put_file_as_key, which we don't
		# want in the regenerated file.
		set namelen [string length $data]
		set keyfile [string range $keyfile $namelen end]
		puts -nonewline $ofid $keyfile
		close $ofid

		$checkfunc $data $d1
		puts $outf $data
		flush $outf
	}
	close $outf
	error_check_good curs_close [$c close] 0
	fileremove $d1
}

proc dump_bin_file_direction { db txn outfile checkfunc begin cont } {
	source ./include.tcl

	set d1 $testdir/d1

	set outf [open $outfile w]

	# Now we will get each key from the DB and dump to outfile
	set c [eval {$db cursor} $txn]

	for {set d [$c get $begin] } \
	    { [llength $d] != 0 } {set d [$c get $cont] } {
		set k [lindex [lindex $d 0] 0]
		set data [lindex [lindex $d 0] 1]
		set ofid [open $d1 w]
		fconfigure $ofid -translation binary
		puts -nonewline $ofid $data
		close $ofid

		$checkfunc $k $d1
		puts $outf $k
	}
	close $outf
	error_check_good curs_close [$c close] 0
	fileremove -f $d1
}

proc make_data_str { key } {
	set datastr ""
	for {set i 0} {$i < 10} {incr i} {
		append datastr $key
	}
	return $datastr
}

proc error_check_bad { func result bad {txn 0}} {
	if { [binary_compare $result $bad] == 0 } {
		if { $txn != 0 } {
			$txn abort
		}
		flush stdout
		flush stderr
		error "FAIL:[timestamp] $func returned error value $bad"
	}
}

proc error_check_good { func result desired {txn 0} } {
	if { [binary_compare $desired $result] != 0 } {
		if { $txn != 0 } {
			$txn abort
		}
		flush stdout
		flush stderr
		error "FAIL:[timestamp]\
		    $func: expected $desired, got $result"
	}
}

# Locks have the prefix of their manager.
proc is_substr { l mgr } {
	if { [string first $mgr $l]  == -1 } {
		return 0
	} else {
		return 1
	}
}

proc release_list { l } {

	# Now release all the locks
	foreach el $l {
		set ret [$el put]
		error_check_good lock_put $ret 0
	}
}

proc debug { {stop 0} } {
	global __debug_on
	global __debug_print
	global __debug_test

	set __debug_on 1
	set __debug_print 1
	set __debug_test $stop
}

# Check if each key appears exactly [llength dlist] times in the file with
# the duplicate tags matching those that appear in dlist.
proc dup_check { db txn tmpfile dlist {extra 0}} {
	source ./include.tcl

	set outf [open $tmpfile w]
	# Now we will get each key from the DB and dump to outfile
	set c [eval {$db cursor} $txn]
	set lastkey ""
	set done 0
	while { $done != 1} {
		foreach did $dlist {
			set rec [$c get "-next"]
			if { [string length $rec] == 0 } {
				set done 1
				break
			}
			set key [lindex [lindex $rec 0] 0]
			set fulldata [lindex [lindex $rec 0] 1]
			set id [id_of $fulldata]
			set d [data_of $fulldata]
			if { [string compare $key $lastkey] != 0 && \
			    $id != [lindex $dlist 0] } {
				set e [lindex $dlist 0]
				error "FAIL: \tKey \
				    $key, expected dup id $e, got $id"
			}
			error_check_good dupget.data $d $key
			error_check_good dupget.id $id $did
			set lastkey $key
		}
		#
		# Some tests add an extra dup (like overflow entries)
		# Check id if it exists.
		if { $extra != 0} {
			set okey $key
			set rec [$c get "-next"]
			if { [string length $rec] != 0 } {
				set key [lindex [lindex $rec 0] 0]
				#
				# If this key has no extras, go back for
				# next iteration.
				if { [string compare $key $lastkey] != 0 } {
					set key $okey
					set rec [$c get "-prev"]
				} else {
					set fulldata [lindex [lindex $rec 0] 1]
					set id [id_of $fulldata]
					set d [data_of $fulldata]
					error_check_bad dupget.data1 $d $key
					error_check_good dupget.id1 $id $extra
				}
			}
		}
		if { $done != 1 } {
			puts $outf $key
		}
	}
	close $outf
	error_check_good curs_close [$c close] 0
}

# Parse duplicate data entries of the form N:data. Data_of returns
# the data part; id_of returns the numerical part
proc data_of {str} {
	set ndx [string first ":" $str]
	if { $ndx == -1 } {
		return ""
	}
	return [ string range $str [expr $ndx + 1] end]
}

proc id_of {str} {
	set ndx [string first ":" $str]
	if { $ndx == -1 } {
		return ""
	}

	return [ string range $str 0 [expr $ndx - 1]]
}

proc nop { {args} } {
	return
}

# Partial put test procedure.
# Munges a data val through three different partial puts.  Stores
# the final munged string in the dvals array so that you can check
# it later (dvals should be global).  We take the characters that
# are being replaced, make them capitals and then replicate them
# some number of times (n_add).  We do this at the beginning of the
# data, at the middle and at the end. The parameters are:
# db, txn, key -- as per usual.  Data is the original data element
# from which we are starting.  n_replace is the number of characters
# that we will replace.  n_add is the number of times we will add
# the replaced string back in.
proc partial_put { method db txn gflags key data n_replace n_add } {
	global dvals
	source ./include.tcl

	# Here is the loop where we put and get each key/data pair
	# We will do the initial put and then three Partial Puts
	# for the beginning, middle and end of the string.

	eval {$db put} $txn {$key [chop_data $method $data]}

	# Beginning change
	set s [string range $data 0 [ expr $n_replace - 1 ] ]
	set repl [ replicate [string toupper $s] $n_add ]

	# This is gross, but necessary:  if this is a fixed-length
	# method, and the chopped length of $repl is zero,
	# it's because the original string was zero-length and our data item
	# is all nulls.  Set repl to something non-NULL.
	if { [is_fixed_length $method] && \
	    [string length [chop_data $method $repl]] == 0 } {
		set repl [replicate "." $n_add]
	}

	set newstr [chop_data $method $repl[string range $data $n_replace end]]
	set ret [eval {$db put} $txn {-partial [list 0 $n_replace] \
	    $key [chop_data $method $repl]}]
	error_check_good put $ret 0

	set ret [eval {$db get} $gflags $txn {$key}]
	error_check_good get $ret [list [list $key [pad_data $method $newstr]]]

	# End Change
	set len [string length $newstr]
	set spl [expr $len - $n_replace]
	# Handle case where $n_replace > $len
	if { $spl < 0 } {
		set spl 0
	}

	set s [string range $newstr [ expr $len - $n_replace ] end ]
	# Handle zero-length keys
	if { [string length $s] == 0 } { set s "A" }

	set repl [ replicate [string toupper $s] $n_add ]
	set newstr [chop_data $method \
	    [string range $newstr 0 [expr $spl - 1 ] ]$repl]

	set ret [eval {$db put} $txn \
	    {-partial [list $spl $n_replace] $key [chop_data $method $repl]}]
	error_check_good put $ret 0

	set ret [eval {$db get} $gflags $txn {$key}]
	error_check_good get $ret [list [list $key [pad_data $method $newstr]]]

	# Middle Change
	set len [string length $newstr]
	set mid [expr $len / 2 ]
	set beg [expr $mid - [expr $n_replace / 2] ]
	set end [expr $beg + $n_replace - 1]
	set s [string range $newstr $beg $end]
	set repl [ replicate [string toupper $s] $n_add ]
	set newstr [chop_data $method [string range $newstr 0 \
	    [expr $beg - 1 ] ]$repl[string range $newstr [expr $end + 1] end]]

	set ret [eval {$db put} $txn {-partial [list $beg $n_replace] \
	    $key [chop_data $method $repl]}]
	error_check_good put $ret 0

	set ret [eval {$db get} $gflags $txn {$key}]
	error_check_good get $ret [list [list $key [pad_data $method $newstr]]]

	set dvals($key) [pad_data $method $newstr]
}

proc replicate { str times } {
	set res $str
	for { set i 1 } { $i < $times } { set i [expr $i * 2] } {
		append res $res
	}
	return $res
}

proc repeat { str n } {
	set ret ""
	while { $n > 0 } {
		set ret $str$ret
		incr n -1
	}
	return $ret
}

proc isqrt { l } {
	set s [expr sqrt($l)]
	set ndx [expr [string first "." $s] - 1]
	return [string range $s 0 $ndx]
}

# If we run watch_procs multiple times without an intervening
# testdir cleanup, it's possible that old sentinel files will confuse
# us.  Make sure they're wiped out before we spawn any other processes.
proc sentinel_init { } {
	source ./include.tcl

	set filelist {}
	set ret [catch {glob $testdir/begin.*} result]
	if { $ret == 0 } { 
		set filelist $result
	}

	set ret [catch {glob $testdir/end.*} result]
	if { $ret == 0 } {
		set filelist [concat $filelist $result]
	}

	foreach f $filelist {
		fileremove $f
	}
}

proc watch_procs { {delay 30} {max 3600} } {
	source ./include.tcl

	set elapsed 0
	while { 1 } {

		tclsleep $delay
		incr elapsed $delay

		# Find the list of processes withoutstanding sentinel
		# files (i.e. a begin.pid and no end.pid).
		set beginlist {}
		set endlist {}
		set ret [catch {glob $testdir/begin.*} result]
		if { $ret == 0 } {
			set beginlist $result
		}
		set ret [catch {glob $testdir/end.*} result]
		if { $ret == 0 } {
			set endlist $result
		}

		set bpids {}
		catch {unset epids}
		foreach begfile $beginlist {
			lappend bpids [string range $begfile \
			    [string length $testdir/begin.] end]
		}
		foreach endfile $endlist {
			set epids([string range $endfile \
			    [string length $testdir/end.] end]) 1
		}

		# The set of processes that we still want to watch, $l,
		# is the set of pids that have begun but not ended
		# according to their sentinel files.
		set l {}
		foreach p $bpids {
			if { [info exists epids($p)] == 0 } {
				lappend l $p
			}
		}

		set rlist {}
		foreach i $l {
			set r [ catch { exec $KILL -0 $i } result ]
			if { $r == 0 } {
				lappend rlist $i
			}
		}
		if { [ llength $rlist] == 0 } {
			break
		} else {
			puts "[timestamp] processes running: $rlist"
		}

		if { $elapsed > $max } {
			# We have exceeded the limit; kill processes
			# and report an error
			set rlist {}
			foreach i $l {
				set r [catch { exec $KILL $i } result]
				if { $r == 0 } {
					lappend rlist $i
				}
			}
			error_check_good "Processes still running" \
			    [llength $rlist] 0
		}
	}
	puts "All processes have exited."
}

# These routines are all used from within the dbscript.tcl tester.
proc db_init { dbp do_data } {
	global a_keys
	global l_keys
	source ./include.tcl

	set txn ""
	set nk 0
	set lastkey ""

	set a_keys() BLANK
	set l_keys ""

	set c [$dbp cursor]
	for {set d [$c get -first] } { [llength $d] != 0 } {
	    set d [$c get -next] } {
		set k [lindex [lindex $d 0] 0]
		set d2 [lindex [lindex $d 0] 1]
		incr nk
		if { $do_data == 1 } {
			if { [info exists a_keys($k)] } {
				lappend a_keys($k) $d2]
			} else {
				set a_keys($k) $d2
			}
		}

		lappend l_keys $k
	}
	error_check_good curs_close [$c close] 0

	return $nk
}

proc pick_op { min max n } {
	if { $n == 0 } {
		return add
	}

	set x [berkdb random_int 1 12]
	if {$n < $min} {
		if { $x <= 4 } {
			return put
		} elseif { $x <= 8} {
			return get
		} else {
			return add
		}
	} elseif {$n >  $max} {
		if { $x <= 4 } {
			return put
		} elseif { $x <= 8 } {
			return get
		} else {
			return del
		}

	} elseif { $x <= 3 } {
		return del
	} elseif { $x <= 6 } {
		return get
	} elseif { $x <= 9 } {
		return put
	} else {
		return add
	}
}

# random_data: Generate a string of random characters.
# If recno is 0 - Use average to pick a length between 1 and 2 * avg.
# If recno is non-0, generate a number between 1 and 2 ^ (avg * 2),
#   that will fit into a 32-bit integer.
# If the unique flag is 1, then make sure that the string is unique
# in the array "where".
proc random_data { avg unique where {recno 0} } {
	upvar #0 $where arr
	global debug_on
	set min 1
	set max [expr $avg+$avg-1]
	if { $recno  } {
		#
		# Tcl seems to have problems with values > 30.
		#
		if { $max > 30 } {
			set max 30
		}
		set maxnum [expr int(pow(2, $max))]
	}
	while {1} {
		set len [berkdb random_int $min $max]
		set s ""
		if {$recno} {
			set s [berkdb random_int 1 $maxnum]
		} else {
			for {set i 0} {$i < $len} {incr i} {
				append s [int_to_char [berkdb random_int 0 25]]
			}
		}

		if { $unique == 0 || [info exists arr($s)] == 0 } {
			break
		}
	}

	return $s
}

proc random_key { } {
	global l_keys
	global nkeys
	set x [berkdb random_int 0 [expr $nkeys - 1]]
	return [lindex $l_keys $x]
}

proc is_err { desired } {
	set x [berkdb random_int 1 100]
	if { $x <= $desired } {
		return 1
	} else {
		return 0
	}
}

proc pick_cursput { } {
	set x [berkdb random_int 1 4]
	switch $x {
		1 { return "-keylast" }
		2 { return "-keyfirst" }
		3 { return "-before" }
		4 { return "-after" }
	}
}

proc random_cursor { curslist } {
	global l_keys
	global nkeys

	set x [berkdb random_int 0 [expr [llength $curslist] - 1]]
	set dbc [lindex $curslist $x]

	# We want to randomly set the cursor.  Pick a key.
	set k [random_key]
	set r [$dbc get "-set" $k]
	error_check_good cursor_get:$k [is_substr Error $r] 0

	# Now move forward or backward some hops to randomly
	# position the cursor.
	set dist [berkdb random_int -10 10]

	set dir "-next"
	set boundary "-first"
	if { $dist < 0 } {
		set dir "-prev"
		set boundary "-last"
		set dist [expr 0 - $dist]
	}

	for { set i 0 } { $i < $dist } { incr i } {
		set r [ record $dbc get $dir $k ]
		if { [llength $d] == 0 } {
			set r [ record $dbc get $k $boundary ]
		}
		error_check_bad dbcget [llength $r] 0
	}
	return { [linsert r 0 $dbc] }
}

proc record { args } {
# Recording every operation makes tests ridiculously slow on
# NT, so we are commenting this out; for debugging purposes,
# it will undoubtedly be useful to uncomment this.
#	puts $args
#	flush stdout
	return [eval $args]
}

proc newpair { k data } {
	global l_keys
	global a_keys
	global nkeys

	set a_keys($k) $data
	lappend l_keys $k
	incr nkeys
}

proc rempair { k } {
	global l_keys
	global a_keys
	global nkeys

	unset a_keys($k)
	set n [lsearch $l_keys $k]
	error_check_bad rempair:$k $n -1
	set l_keys [lreplace $l_keys $n $n]
	incr nkeys -1
}

proc changepair { k data } {
	global l_keys
	global a_keys
	global nkeys

	set a_keys($k) $data
}

proc changedup { k olddata newdata } {
	global l_keys
	global a_keys
	global nkeys

	set d $a_keys($k)
	error_check_bad changedup:$k [llength $d] 0

	set n [lsearch $d $olddata]
	error_check_bad changedup:$k $n -1

	set a_keys($k) [lreplace $a_keys($k) $n $n $newdata]
}

# Insert a dup into the a_keys array with DB_KEYFIRST.
proc adddup { k olddata newdata } {
	global l_keys
	global a_keys
	global nkeys

	set d $a_keys($k)
	if { [llength $d] == 0 } {
		lappend l_keys $k
		incr nkeys
		set a_keys($k) { $newdata }
	}

	set ndx 0

	set d [linsert d $ndx $newdata]
	set a_keys($k) $d
}

proc remdup { k data } {
	global l_keys
	global a_keys
	global nkeys

	set d [$a_keys($k)]
	error_check_bad changedup:$k [llength $d] 0

	set n [lsearch $d $olddata]
	error_check_bad changedup:$k $n -1

	set a_keys($k) [lreplace $a_keys($k) $n $n]
}

proc dump_full_file { db txn outfile checkfunc start continue } {
	source ./include.tcl

	set outf [open $outfile w]
	# Now we will get each key from the DB and dump to outfile
	set c [eval {$db cursor} $txn]
	error_check_good dbcursor [is_valid_cursor $c $db] TRUE

	for {set d [$c get $start] } { [string length $d] != 0 } {
		set d [$c get $continue] } {
		set k [lindex [lindex $d 0] 0]
		set d2 [lindex [lindex $d 0] 1]
		$checkfunc $k $d2
		puts $outf "$k\t$d2"
	}
	close $outf
	error_check_good curs_close [$c close] 0
}

proc int_to_char { i } {
	global alphabet

	return [string index $alphabet $i]
}

proc dbcheck { key data } {
	global l_keys
	global a_keys
	global nkeys
	global check_array

	if { [lsearch $l_keys $key] == -1 } {
		error "FAIL: Key |$key| not in list of valid keys"
	}

	set d $a_keys($key)

	if { [info exists check_array($key) ] } {
		set check $check_array($key)
	} else {
		set check {}
	}

	if { [llength $d] > 1 } {
		if { [llength $check] != [llength $d] } {
			# Make the check array the right length
			for { set i [llength $check] } { $i < [llength $d] } \
			    {incr i} {
				lappend check 0
			}
			set check_array($key) $check
		}

		# Find this data's index
		set ndx [lsearch $d $data]
		if { $ndx == -1 } {
			error "FAIL: \
			    Data |$data| not found for key $key.  Found |$d|"
		}

		# Set the bit in the check array
		set check_array($key) [lreplace $check_array($key) $ndx $ndx 1]
	} elseif { [string compare $d $data] != 0 } {
		error "FAIL: \
		    Invalid data |$data| for key |$key|. Expected |$d|."
	} else {
		set check_array($key) 1
	}
}

# Dump out the file and verify it
proc filecheck { file txn } {
	global check_array
	global l_keys
	global nkeys
	global a_keys
	source ./include.tcl

	if { [info exists check_array] == 1 } {
		unset check_array
	}

	open_and_dump_file $file NULL $txn $file.dump dbcheck dump_full_file \
	    "-first" "-next"

	# Check that everything we checked had all its data
	foreach i [array names check_array] {
		set count 0
		foreach j $check_array($i) {
			if { $j != 1 } {
				puts -nonewline "Key |$i| never found datum"
				puts " [lindex $a_keys($i) $count]"
			}
			incr count
		}
	}

	# Check that all keys appeared in the checked array
	set count 0
	foreach k $l_keys {
		if { [info exists check_array($k)] == 0 } {
			puts "filecheck: key |$k| not found.  Data: $a_keys($k)"
		}
		incr count
	}

	if { $count != $nkeys } {
		puts "filecheck: Got $count keys; expected $nkeys"
	}
}

proc esetup { dir } {
	source ./include.tcl

	set ret [berkdb envremove -home $dir]

	fileremove -f $dir/file0 $dir/file1 $dir/file2 $dir/file3
	set mp [memp $dir 0644 -create -cachesize { 0 10240 }]
	set lp [lock_open "" -create 0644]
	error_check_good memp_close [$mp close] 0
	error_check_good lock_close [$lp close] 0
}

proc cleanup { dir env } {
	global gen_upgrade
	global upgrade_dir
	global upgrade_be
	global upgrade_method
	global upgrade_name
	source ./include.tcl

	if { $gen_upgrade == 1 } {
		set vers [berkdb version]
		set maj [lindex $vers 0]
		set min [lindex $vers 1]

		if { $upgrade_be == 1 } {
			set version_dir "$maj.${min}be"
		} else {
			set version_dir "$maj.${min}le"
		}

		set dest $upgrade_dir/$version_dir/$upgrade_method/$upgrade_name

		catch {exec mkdir -p $dest}
		catch {exec sh -c "mv $dir/*.db $dest"}
		catch {exec sh -c "mv $dir/__dbq.* $dest"}
	}

#	check_handles
	set remfiles {}
	set ret [catch { glob $dir/* } result]
	if { $ret == 0 } {
		foreach file $result {
			#
			# We:
			# - Ignore any env-related files, which are
			# those that have __db.* or log.* if we are
			# running in an env.
			# - Call 'dbremove' on any databases.
			# Remove any remaining temp files.
			#
			switch -glob -- $file {
			*/__db.* -
			*/log.*	{
				if { $env != "NULL" } {
					continue
				} else {
					lappend remfiles $file
				}
				}
			*.db	{
				set envargs ""
				if { $env != "NULL"} {
					set file [file tail $file]
					set envargs " -env $env "
				}

				# If a database is left in a corrupt
				# state, dbremove might not be able to handle
				# it (it does an open before the remove).
				# Be prepared for this, and if necessary,
				# just forcibly remove the file with a warning
				# message.
				set ret [catch \
				    {eval {berkdb dbremove} $envargs $file} res]
				if { $ret != 0 } {
					puts \
				    "FAIL: dbremove in cleanup failed: $res"
					lappend remfiles $file
				}
				}
			default	{
				lappend remfiles $file
				}
			}
		}
		if {[llength $remfiles] > 0} {
			eval fileremove -f $remfiles
		}
	}
}

proc log_cleanup { dir } {
	source ./include.tcl

	set files [glob -nocomplain $dir/log.*]
	if { [llength $files] != 0} {
		foreach f $files {
			fileremove -f $f
		}
	}
}

proc env_cleanup { dir } {
	source ./include.tcl

	set stat [catch {berkdb envremove -home $dir} ret]
	#
	# If something failed and we are left with a region entry
	# in /dev/shmem that is zero-length, the envremove will
	# succeed, and the shm_unlink will succeed, but it will not
	# remove the zero-length entry from /dev/shmem.  Remove it
	# using fileremove or else all other tests using an env
	# will immediately fail.
	#
	if { $is_qnx_test == 1 } {
		set region_files [glob -nocomplain /dev/shmem/$dir*]
		if { [llength $region_files] != 0 } {
			foreach f $region_files {
				fileremove -f $f
			}
		}
	}
	log_cleanup $dir
	cleanup $dir NULL
}

proc remote_cleanup { server dir localdir } {
	set home [file tail $dir]
	error_check_good cleanup:remove [berkdb envremove -home $home \
	    -server $server] 0
	catch {exec rsh $server rm -f $dir/*} ret
	cleanup $localdir NULL
}

proc help { cmd } {
	if { [info command $cmd] == $cmd } {
		set is_proc [lsearch [info procs $cmd] $cmd]
		if { $is_proc == -1 } {
			# Not a procedure; must be a C command
			# Let's hope that it takes some parameters
			# and that it prints out a message
			puts "Usage: [eval $cmd]"
		} else {
			# It is a tcl procedure
			puts -nonewline "Usage: $cmd"
			set args [info args $cmd]
			foreach a $args {
				set is_def [info default $cmd $a val]
				if { $is_def != 0 } {
					# Default value
					puts -nonewline " $a=$val"
				} elseif {$a == "args"} {
					# Print out flag values
					puts " options"
					args
				} else {
					# No default value
					puts -nonewline " $a"
				}
			}
			puts ""
		}
	} else {
		puts "$cmd is not a command"
	}
}

# Run a recovery test for a particular operation
# Notice that we catch the return from CP and do not do anything with it.
# This is because Solaris CP seems to exit non-zero on occasion, but
# everything else seems to run just fine.
proc op_recover { encodedop dir env_cmd dbfile cmd msg } {
	global log_log_record_types
	global recd_debug
	global recd_id
	global recd_op
	source ./include.tcl

	#puts "op_recover: $encodedop $dir $env_cmd $dbfile $cmd $msg"

	set init_file $dir/t1
	set afterop_file $dir/t2
	set final_file $dir/t3

	set op ""
	set op2 ""
	if { $encodedop == "prepare-abort" } {
		set op "prepare"
		set op2 "abort"
	} elseif { $encodedop == "prepare-commit" } {
		set op "prepare"
		set op2 "commit"
	} else {
		set op $encodedop
	}

	puts "\t$msg $encodedop"

	# Keep track of the log types we've seen
	if { $log_log_record_types == 1} {
		logtrack_read $dir
	}

	# Save the initial file and open the environment and the file
	catch { file copy -force $dir/$dbfile $dir/$dbfile.init } res
	copy_extent_file $dir $dbfile init

	set env [eval $env_cmd]
	set db [berkdb open -env $env $dbfile]
	error_check_good dbopen [is_valid_db $db] TRUE

	# Dump out file contents for initial case
	set tflags ""
	open_and_dump_file $dbfile $env $tflags $init_file nop \
	    dump_file_direction "-first" "-next"

	set t [$env txn]
	error_check_bad txn_begin $t NULL
	error_check_good txn_begin [is_substr $t "txn"] 1

	# Now fill in the db, tmgr, and the txnid in the command
	set exec_cmd $cmd

	set i [lsearch $cmd ENV]
	if { $i != -1 } {
		set exec_cmd [lreplace $exec_cmd $i $i $env]
	}

	set i [lsearch $cmd TXNID]
	if { $i != -1 } {
		set exec_cmd [lreplace $exec_cmd $i $i $t]
	}

	set i [lsearch $exec_cmd DB]
	if { $i != -1 } {
		set exec_cmd [lreplace $exec_cmd $i $i $db]
	}

	# To test DB_CONSUME, we need to expect a record return, not "0".
	set i [lsearch $exec_cmd "-consume"]
	if { $i	!= -1 } {
		set record_exec_cmd_ret 1
	} else {
		set record_exec_cmd_ret 0
	}

	# For the DB_APPEND test, we need to expect a return other than
	# 0;  set this flag to be more lenient in the error_check_good.
	set i [lsearch $exec_cmd "-append"]
	if { $i != -1 } {
		set lenient_exec_cmd_ret 1
	} else {
		set lenient_exec_cmd_ret 0
	}

	# Execute command and commit/abort it.
	set ret [eval $exec_cmd]
	if { $record_exec_cmd_ret == 1 } {
		error_check_good "\"$exec_cmd\"" [llength [lindex $ret 0]] 2
	} elseif { $lenient_exec_cmd_ret == 1 } {
		error_check_good "\"$exec_cmd\"" [expr $ret > 0] 1
	} else {
		error_check_good "\"$exec_cmd\"" $ret 0
	}

	set record_exec_cmd_ret 0
	set lenient_exec_cmd_ret 0

	# Sync the file so that we can capture a snapshot to test
	# recovery.
	error_check_good sync:$db [$db sync] 0

	catch { file copy -force $dir/$dbfile $dir/$dbfile.afterop } res
	copy_extent_file $dir $dbfile afterop

	#set tflags "-txn $t"
	open_and_dump_file $dir/$dbfile.afterop NULL $tflags \
		$afterop_file nop dump_file_direction \
		"-first" "-next"
	#puts "\t\t\tExecuting txn_$op:$t"
	error_check_good txn_$op:$t [$t $op] 0
	if { $op2 != "" } {
		#puts "\t\t\tExecuting txn_$op2:$t"
		error_check_good txn_$op2:$t [$t $op2] 0
	}

	switch $encodedop {
		"commit" { puts "\t\tCommand executed and committed." }
		"abort" { puts "\t\tCommand executed and aborted." }
		"prepare" { puts "\t\tCommand executed and prepared." }
		"prepare-commit" {
			puts "\t\tCommand executed, prepared, and committed."
		}
		"prepare-abort" {
			puts "\t\tCommand executed, prepared, and aborted."
		}
	}

	# Dump out file and save a copy.
	error_check_good sync:$db [$db sync] 0
	open_and_dump_file $dir/$dbfile NULL $tflags $final_file nop \
	    dump_file_direction "-first" "-next"

	catch { file copy -force $dir/$dbfile $dir/$dbfile.final } res
	copy_extent_file $dir $dbfile final

	# If this is an abort or prepare-abort, it should match the
	#   original file.
	# If this was a commit or prepare-commit, then this file should
	#   match the afterop file.
	# If this was a prepare without an abort or commit, we still
	#   have transactions active, and peering at the database from
	#   another environment will show data from uncommitted transactions.
	#   Thus we just skip this in the prepare-only case;  what
	#   we care about are the results of a prepare followed by a
	#   recovery, which we test later.
	if { $op == "commit" || $op2 == "commit" } {
		filesort $afterop_file $afterop_file.sort
		filesort $final_file $final_file.sort
		error_check_good \
		    diff(post-$op,pre-commit):diff($afterop_file,$final_file) \
		    [filecmp $afterop_file.sort $final_file.sort] 0
	} elseif { $op == "abort" || $op2 == "abort" } {
		filesort $init_file $init_file.sort
		filesort $final_file $final_file.sort
		error_check_good \
		    diff(initial,post-$op):diff($init_file,$final_file) \
		    [filecmp $init_file.sort $final_file.sort] 0
	} else {
		# Make sure this really is a prepare-only
		error_check_good assert:prepare-only $encodedop "prepare"
	}

	# Running recovery on this database should not do anything.
	# Flush all data to disk, close the environment and save the
	# file.
	error_check_good close:$db [$db close] 0

	# If all we've done is a prepare, then there's still a
	# transaction active, and an env close will return DB_RUNRECOVERY
	if { $encodedop == "prepare" } {
		catch {$env close} ret
		error_check_good env_close \
			[is_substr $ret DB_RUNRECOVERY] 1
	} else {
		reset_env $env
	}

	berkdb debug_check
	puts -nonewline "\t\tRunning recovery ... "
	flush stdout

	set stat [catch {exec $util_path/db_recover -h $dir -c} result]
	if { $stat == 1 } {
		error "FAIL: Recovery error: $result."
	}
	puts -nonewline "complete ... "

	error_check_good db_verify [verify_dir $testdir "\t\t" 0 1] 0

	puts "verified"

	berkdb debug_check
	set env [eval $env_cmd]
	error_check_good dbenv [is_valid_widget $env env] TRUE
	open_and_dump_file $dir/$dbfile NULL $tflags $final_file nop \
	    dump_file_direction "-first" "-next"
	if { $op == "commit" || $op2 == "commit" } {
		filesort $afterop_file $afterop_file.sort
		filesort $final_file $final_file.sort
		error_check_good \
		    diff(post-$op,pre-commit):diff($afterop_file,$final_file) \
		    [filecmp $afterop_file.sort $final_file.sort] 0
	} else {
		filesort $init_file $init_file.sort
		filesort $final_file $final_file.sort
		error_check_good \
		    diff(initial,post-$op):diff($init_file,$final_file) \
		    [filecmp $init_file.sort $final_file.sort] 0
	}

	# Now close the environment, substitute a file that will need
	# recovery and try running recovery again.
	reset_env $env
	if { $op == "commit" || $op2 == "commit" } {
		catch { file copy -force $dir/$dbfile.init $dir/$dbfile } res
		move_file_extent $dir $dbfile init copy
	} else {
		catch { file copy -force $dir/$dbfile.afterop $dir/$dbfile } res
		move_file_extent $dir $dbfile afterop copy
	}

	berkdb debug_check
	puts -nonewline \
	    "\t\tRunning recovery on pre-op database ... "
	flush stdout

	set stat [catch {exec $util_path/db_recover -h $dir -c} result]
	if { $stat == 1 } {
		error "FAIL: Recovery error: $result."
	}
	puts -nonewline "complete ... "

	error_check_good db_verify_preop [verify_dir $testdir "\t\t" 0 1] 0

	puts "verified"

	set env [eval $env_cmd]

	open_and_dump_file $dir/$dbfile NULL $tflags $final_file nop \
	    dump_file_direction "-first" "-next"
	if { $op == "commit" || $op2 == "commit" } {
		filesort $final_file $final_file.sort
		filesort $afterop_file $afterop_file.sort
		error_check_good \
		    diff(post-$op,recovered):diff($afterop_file,$final_file) \
		    [filecmp $afterop_file.sort $final_file.sort] 0
	} else {
		filesort $init_file $init_file.sort
		filesort $final_file $final_file.sort
		error_check_good \
		    diff(initial,post-$op):diff($init_file,$final_file) \
		    [filecmp $init_file.sort $final_file.sort] 0
	}

	# This should just close the environment, not blow it away.
	reset_env $env
}

proc populate { db method txn n dups bigdata } {
	source ./include.tcl

	set did [open $dict]
	set count 0
	while { [gets $did str] != -1 && $count < $n } {
		if { [is_record_based $method] == 1 } {
			set key [expr $count + 1]
		} elseif { $dups == 1 } {
			set key duplicate_key
		} else {
			set key $str
		}
		if { $bigdata == 1 && [berkdb random_int 1 3] == 1} {
			set str [replicate $str 1000]
		}

		set ret [$db put -txn $txn $key $str]
		error_check_good db_put:$key $ret 0
		incr count
	}
	close $did
	return 0
}

proc big_populate { db txn n } {
	source ./include.tcl

	set did [open $dict]
	set count 0
	while { [gets $did str] != -1 && $count < $n } {
		set key [replicate $str 50]
		set ret [$db put -txn $txn $key $str]
		error_check_good db_put:$key $ret 0
		incr count
	}
	close $did
	return 0
}

proc unpopulate { db txn num } {
	source ./include.tcl

	set c [eval {$db cursor} "-txn $txn"]
	error_check_bad $db:cursor $c NULL
	error_check_good $db:cursor [is_substr $c $db] 1

	set i 0
	for {set d [$c get -first] } { [llength $d] != 0 } {
		set d [$c get -next] } {
		$c del
		incr i
		if { $num != 0 && $ >= $num } {
			break
		}
	}
	error_check_good cursor_close [$c close] 0
	return 0
}

proc reset_env { env } {
	error_check_good env_close [$env close] 0
}

# This routine will let us obtain a ring of deadlocks.
# Each locker will get a lock on obj_id, then sleep, and
# then try to lock (obj_id + 1) % num.
# When the lock is finally granted, we release our locks and
# return 1 if we got both locks and DEADLOCK if we deadlocked.
# The results here should be that 1 locker deadlocks and the
# rest all finish successfully.
proc ring { myenv locker_id obj_id num } {
	source ./include.tcl

	if {[catch {$myenv lock_get write $locker_id $obj_id} lock1] != 0} {
		puts $errorInfo
		return ERROR
	} else {
		error_check_good lockget:$obj_id [is_substr $lock1 $myenv] 1
	}

	tclsleep 30
	set nextobj [expr ($obj_id + 1) % $num]
	set ret 1
	if {[catch {$myenv lock_get write $locker_id $nextobj} lock2] != 0} {
		if {[string match "*DEADLOCK*" $lock2] == 1} {
			set ret DEADLOCK
		} else {
			set ret ERROR
		}
	} else {
		error_check_good lockget:$obj_id [is_substr $lock2 $myenv] 1
	}

	# Now release the first lock
	error_check_good lockput:$lock1 [$lock1 put] 0

	if {$ret == 1} {
		error_check_bad lockget:$obj_id $lock2 NULL
		error_check_good lockget:$obj_id [is_substr $lock2 $myenv] 1
		error_check_good lockput:$lock2 [$lock2 put] 0
	}
	return $ret
}

# This routine will create massive deadlocks.
# Each locker will get a readlock on obj_id, then sleep, and
# then try to upgrade the readlock to a write lock.
# When the lock is finally granted, we release our first lock and
# return 1 if we got both locks and DEADLOCK if we deadlocked.
# The results here should be that 1 locker succeeds in getting all
# the locks and everyone else deadlocks.
proc clump { myenv locker_id obj_id num } {
	source ./include.tcl

	set obj_id 10
	if {[catch {$myenv lock_get read $locker_id $obj_id} lock1] != 0} {
		puts $errorInfo
		return ERROR
	} else {
		error_check_good lockget:$obj_id \
		    [is_valid_lock $lock1 $myenv] TRUE
	}

	tclsleep 30
	set ret 1
	if {[catch {$myenv lock_get write $locker_id $obj_id} lock2] != 0} {
		if {[string match "*DEADLOCK*" $lock2] == 1} {
			set ret DEADLOCK
		} else {
			set ret ERROR
		}
	} else {
		error_check_good \
		    lockget:$obj_id [is_valid_lock $lock2 $myenv] TRUE
	}

	# Now release the first lock
	error_check_good lockput:$lock1 [$lock1 put] 0

	if {$ret == 1} {
		error_check_good \
		    lockget:$obj_id [is_valid_lock $lock2 $myenv] TRUE
		error_check_good lockput:$lock2 [$lock2 put] 0
	}
	return $ret
 }

proc dead_check { t procs dead clean other } {
	error_check_good $t:$procs:other $other 0
	switch $t {
		ring {
			error_check_good $t:$procs:deadlocks $dead 1
			error_check_good $t:$procs:success $clean \
			    [expr $procs - 1]
		}
		clump {
			error_check_good $t:$procs:deadlocks $dead \
			    [expr $procs - 1]
			error_check_good $t:$procs:success $clean 1
		}
		default {
			error "Test $t not implemented"
		}
	}
}

proc rdebug { id op where } {
	global recd_debug
	global recd_id
	global recd_op

	set recd_debug $where
	set recd_id $id
	set recd_op $op
}

proc rtag { msg id } {
	set tag [lindex $msg 0]
	set tail [expr [string length $tag] - 2]
	set tag [string range $tag $tail $tail]
	if { $id == $tag } {
		return 1
	} else {
		return 0
	}
}

proc zero_list { n } {
	set ret ""
	while { $n > 0 } {
		lappend ret 0
		incr n -1
	}
	return $ret
}

proc check_dump { k d } {
	puts "key: $k data: $d"
}

proc reverse { s } {
	set res ""
	for { set i 0 } { $i < [string length $s] } { incr i } {
		set res "[string index $s $i]$res"
	}

	return $res
}

proc is_valid_widget { w expected } {
	# First N characters must match "expected"
	set l [string length $expected]
	incr l -1
	if { [string compare [string range $w 0 $l] $expected] != 0 } {
		return $w
	}

	# Remaining characters must be digits
	incr l 1
	for { set i $l } { $i < [string length $w] } { incr i} {
		set c [string index $w $i]
		if { $c < "0" || $c > "9" } {
			return $w
		}
	}

	return TRUE
}

proc is_valid_db { db } {
	return [is_valid_widget $db db]
}

proc is_valid_env { env } {
	return [is_valid_widget $env env]
}

proc is_valid_cursor { dbc db } {
	return [is_valid_widget $dbc $db.c]
}

proc is_valid_lock { lock env } {
	return [is_valid_widget $lock $env.lock]
}

proc is_valid_mpool { mpool env } {
	return [is_valid_widget $mpool $env.mp]
}

proc is_valid_page { page mpool } {
	return [is_valid_widget $page $mpool.pg]
}

proc is_valid_txn { txn env } {
	return [is_valid_widget $txn $env.txn]
}

proc is_valid_mutex { m env } {
	return [is_valid_widget $m $env.mutex]
}

proc send_cmd { fd cmd {sleep 2}} {
	source ./include.tcl

	puts $fd "set v \[$cmd\]"
	puts $fd "puts \$v"
	puts $fd "flush stdout"
	flush $fd
	berkdb debug_check
	tclsleep $sleep

	set r [rcv_result $fd]
	return $r
}

proc rcv_result { fd } {
	set r [gets $fd result]
	error_check_bad remote_read $r -1

	return $result
}

proc send_timed_cmd { fd rcv_too cmd } {
	set c1 "set start \[timestamp -r\]; "
	set c2 "puts \[expr \[timestamp -r\] - \$start\]"
	set full_cmd [concat $c1 $cmd ";" $c2]

	puts $fd $full_cmd
	puts $fd "flush stdout"
	flush $fd
	return 0
}

#
# The rationale behind why we have *two* "data padding" routines is outlined
# below:
#
# Both pad_data and chop_data truncate data that is too long. However,
# pad_data also adds the pad character to pad data out to the fixed length
# record length.
#
# Which routine you call does not depend on the length of the data you're
# using, but on whether you're doing a put or a get. When we do a put, we
# have to make sure the data isn't longer than the size of a record because
# otherwise we'll get an error (use chop_data). When we do a get, we want to
# check that db padded everything correctly (use pad_data on the value against
# which we are comparing).
#
# We don't want to just use the pad_data routine for both purposes, because
# we want to be able to test whether or not db is padding correctly. For
# example, the queue access method had a bug where when a record was
# overwritten (*not* a partial put), only the first n bytes of the new entry
# were written, n being the new entry's (unpadded) length.  So, if we did
# a put with key,value pair (1, "abcdef") and then a put (1, "z"), we'd get
# back (1,"zbcdef"). If we had used pad_data instead of chop_data, we would
# have gotten the "correct" result, but we wouldn't have found this bug.
proc chop_data {method data} {
	global fixed_len

	if {[is_fixed_length $method] == 1 && \
	    [string length $data] > $fixed_len} {
		return [eval {binary format a$fixed_len $data}]
	} else {
		return $data
	}
}

proc pad_data {method data} {
	global fixed_len

	if {[is_fixed_length $method] == 1} {
		return [eval {binary format a$fixed_len $data}]
	} else {
		return $data
	}
}

proc make_fixed_length {method data {pad 0}} {
	global fixed_len
	global fixed_pad

	if {[is_fixed_length $method] == 1} {
		if {[string length $data] > $fixed_len } {
		    error_check_bad make_fixed_len:TOO_LONG 1 1
		}
		while { [string length $data] < $fixed_len } {
			set data [format $data%c $fixed_pad]
		}
	}
	return $data
}

# shift data for partial
# pad with fixed pad (which is NULL)
proc partial_shift { data offset direction} {
	global fixed_len

	set len [expr $fixed_len - 1]

	if { [string compare $direction "right"] == 0 } {
		for { set i 1} { $i <= $offset } {incr i} {
			set data [binary format x1a$len $data]
		}
	} elseif { [string compare $direction "left"] == 0 } {
		for { set i 1} { $i <= $offset } {incr i} {
			set data [string range $data 1 end]
			set data [binary format a$len $data]
		}
	}
	return $data
}

# string compare does not always work to compare
# this data, nor does expr (==)
# specialized routine for comparison
# (for use in fixed len recno and q)
proc binary_compare { data1 data2 } {
	if { [string length $data1] != [string length $data2] || \
	    [string compare -length \
	    [string length $data1] $data1 $data2] != 0 } {
		return 1
	} else {
		return 0
	}
}

proc convert_method { method } {
	switch -- $method {
		-btree -
		-dbtree -
		-ddbtree -
		-rbtree -
		BTREE -
		DB_BTREE -
		DB_RBTREE -
		RBTREE -
		bt -
		btree -
		db_btree -
		db_rbtree -
		rbt -
		rbtree { return "-btree" }

		-dhash -
		-hash -
		DB_HASH -
		HASH -
		db_hash -
		h -
		hash { return "-hash" }

		-queue -
		DB_QUEUE -
		QUEUE -
		db_queue -
		q -
		qam -
		queue { return "-queue" }

		-queueextent -
		QUEUEEXTENT -
		qe -
		qamext -
		-queueext -
		queueextent - 
		queueext { return "-queue" }

		-frecno -
		-recno -
		-rrecno -
		DB_FRECNO -
		DB_RECNO -
		DB_RRECNO -
		FRECNO -
		RECNO -
		RRECNO -
		db_frecno -
		db_recno -
		db_rrecno -
		frec -
		frecno -
		rec -
		recno -
		rrec -
		rrecno { return "-recno" }

		default { error "FAIL:[timestamp] $method: unknown method" }
	}
}

# If recno-with-renumbering or btree-with-renumbering is specified, then
# fix the arguments to specify the DB_RENUMBER/DB_RECNUM option for the
# -flags argument.
proc convert_args { method {largs ""} } {
	global fixed_len
	global fixed_pad
	global gen_upgrade
	global upgrade_be
	source ./include.tcl

	if { [string first - $largs] == -1 &&\
	    [string compare $largs ""] != 0 } {
		set errstring "args must contain a hyphen; does this test\
		    have no numeric args?"
		puts "FAIL:[timestamp] $errstring"
		return -code return
	}

	if { $gen_upgrade == 1 && $upgrade_be == 1 } {
		append largs " -lorder 4321 "
	} elseif { $gen_upgrade == 1 && $upgrade_be != 1 } {
		append largs " -lorder 1234 "
	}

	if { [is_rrecno $method] == 1 } {
		append largs " -renumber "
	} elseif { [is_rbtree $method] == 1 } {
		append largs " -recnum "
	} elseif { [is_dbtree $method] == 1 } {
		append largs " -dup "
	} elseif { [is_ddbtree $method] == 1 } {
		append largs " -dup "
		append largs " -dupsort "
	} elseif { [is_dhash $method] == 1 } {
		append largs " -dup "
	} elseif { [is_queueext $method] == 1 } {
		append largs " -extent 2 "
	}

	if {[is_fixed_length $method] == 1} {
		append largs " -len $fixed_len -pad $fixed_pad "
	}
	return $largs
}

proc is_btree { method } {
	set names { -btree BTREE DB_BTREE bt btree }
	if { [lsearch $names $method] >= 0 } {
		return 1
	} else {
		return 0
	}
}

proc is_dbtree { method } {
	set names { -dbtree }
	if { [lsearch $names $method] >= 0 } {
		return 1
	} else {
		return 0
	}
}

proc is_ddbtree { method } {
	set names { -ddbtree }
	if { [lsearch $names $method] >= 0 } {
		return 1
	} else {
		return 0
	}
}

proc is_rbtree { method } {
	set names { -rbtree rbtree RBTREE db_rbtree DB_RBTREE rbt }
	if { [lsearch $names $method] >= 0 } {
		return 1
	} else {
		return 0
	}
}

proc is_recno { method } {
	set names { -recno DB_RECNO RECNO db_recno rec recno}
	if { [lsearch $names $method] >= 0 } {
		return 1
	} else {
		return 0
	}
}

proc is_rrecno { method } {
	set names { -rrecno rrecno RRECNO db_rrecno DB_RRECNO rrec }
	if { [lsearch $names $method] >= 0 } {
		return 1
	} else {
		return 0
	}
}

proc is_frecno { method } {
	set names { -frecno frecno frec FRECNO db_frecno DB_FRECNO}
	if { [lsearch $names $method] >= 0 } {
		return 1
	} else {
		return 0
	}
}

proc is_hash { method } {
	set names { -hash DB_HASH HASH db_hash h hash }
	if { [lsearch $names $method] >= 0 } {
		return 1
	} else {
		return 0
	}
}

proc is_dhash { method } {
	set names { -dhash }
	if { [lsearch $names $method] >= 0 } {
		return 1
	} else {
		return 0
	}
}

proc is_queue { method } {
	if { [is_queueext $method] == 1 } {
		return 1
	}

	set names { -queue DB_QUEUE QUEUE db_queue q queue qam }
	if { [lsearch $names $method] >= 0 } {
		return 1
	} else {
		return 0
	}
}

proc is_queueext { method } {
	set names { -queueextent queueextent QUEUEEXTENT qe qamext \
	    queueext -queueext }
	if { [lsearch $names $method] >= 0 } {
		return 1
	} else {
		return 0
	}
}

proc is_record_based { method } {
	if { [is_recno $method] || [is_frecno $method] ||
	    [is_rrecno $method] || [is_queue $method] } {
		return 1
	} else {
		return 0
	}
}

proc is_fixed_length { method } {
	if { [is_queue $method] || [is_frecno $method] } {
		return 1
	} else {
		return 0
	}
}

# Sort lines in file $in and write results to file $out.
# This is a more portable alternative to execing the sort command,
# which has assorted issues on NT [#1576].
# The addition of a "-n" argument will sort numerically.
proc filesort { in out { arg "" } } {
	set i [open $in r]

	set ilines {}
	while { [gets $i line] >= 0 } {
		lappend ilines $line
	}

	if { [string compare $arg "-n"] == 0 } {
		set olines [lsort -integer $ilines]
	} else {
		set olines [lsort $ilines]
	}

	close $i

	set o [open $out w]
	foreach line $olines {
		puts $o $line
	}

	close $o
}

# Print lines up to the nth line of infile out to outfile, inclusive.
# The optional beg argument tells us where to start.
proc filehead { n infile outfile { beg 0 } } {
	set in [open $infile r]
	set out [open $outfile w]

	# Sed uses 1-based line numbers, and so we do too.
	for { set i 1 } { $i < $beg } { incr i } {
		if { [gets $in junk] < 0 } {
			break
		}
	}

	for { } { $i <= $n } { incr i } {
		if { [gets $in line] < 0 } {
			break
		}
		puts $out $line
	}

	close $in
	close $out
}

# Remove file (this replaces $RM).
# Usage: fileremove filenames =~ rm;  fileremove -f filenames =~ rm -rf.
proc fileremove { args } {
	set forceflag ""
	foreach a $args {
		if { [string first - $a] == 0 } {
			# It's a flag.  Better be f.
			if { [string first f $a] != 1 } {
				return -code error "bad flag to fileremove"
			} else {
				set forceflag "-force"
			}
		} else {
			eval {file delete $forceflag $a}
		}
	}
}

proc findfail { args } {
	foreach a $args {
		if { [file exists $a] == 0 } {
			continue
		}
		set f [open $a r]
		while { [gets $f line] >= 0 } {
			if { [string first FAIL $line] == 0 } {
				close $f
				return 1
			}
		}
		close $f
	}
	return 0
}

# Sleep for s seconds.
proc tclsleep { s } {
	# On Windows, the system time-of-day clock may update as much
	# as 55 ms late due to interrupt timing.  Don't take any
	# chances;  sleep extra-long so that when tclsleep 1 returns,
	# it's guaranteed to be a new second.
	after [expr $s * 1000 + 56]
}

# Compare two files, a la diff.  Returns 1 if non-identical, 0 if identical.
proc filecmp { file_a file_b } {
	set fda [open $file_a r]
	set fdb [open $file_b r]

	set nra 0
	set nrb 0

	# The gets can't be in the while condition because we'll
	# get short-circuit evaluated.
	while { $nra >= 0 && $nrb >= 0 } {
		set nra [gets $fda aline]
		set nrb [gets $fdb bline]

		if { $nra != $nrb || [string compare $aline $bline] != 0} {
			close $fda
			close $fdb
			return 1
		}
	}

	close $fda
	close $fdb
	return 0
}

# Verify all .db files in the specified directory.
proc verify_dir { \
    {directory "./TESTDIR"} { pref "" } { noredo 0 } { quiet 0 } } {
	# If we're doing database verification between tests, we don't
	# want to do verification twice without an intervening cleanup--some
	# test was skipped.  Always verify by default (noredo == 0) so
	# that explicit calls to verify_dir during tests don't require
	# cleanup commands.
	if { $noredo == 1 } { 
		if { [file exists $directory/NOREVERIFY] == 1 } {
			if { $quiet == 0 } { 
				puts "Skipping verification."
			}
			return
		}
		set f [open $directory/NOREVERIFY w]
		close $f
	}

	if { [catch {glob $directory/*.db} dbs] != 0 } {
		# No files matched
		return
	}
	if { [file exists /dev/stderr] == 1 } {
		set errfilearg "-errfile /dev/stderr "
	} else {
		set errfilearg ""
	}
	set errpfxarg {-errpfx "FAIL: verify" }
	set errarg $errfilearg$errpfxarg
	set ret 0
	foreach db $dbs {
		if { [catch {eval {berkdb dbverify} $errarg $db} res] != 0 } {
			puts $res
			puts "FAIL:[timestamp] Verification of $db failed."
			set ret 1
		} else {
			error_check_good verify:$db $res 0
			if { $quiet == 0 } { 
				puts "${pref}Verification of $db succeeded."
			}
		}
	}
	return $ret
}

# Generate randomly ordered, guaranteed-unique four-character strings that can
# be used to differentiate duplicates without creating duplicate duplicates.
# (test031 & test032) randstring_init is required before the first call to
# randstring and initializes things for up to $i distinct strings;  randstring
# gets the next string.
proc randstring_init { i } {
	global rs_int_list alphabet

	# Fail if we can't generate sufficient unique strings.
	if { $i > [expr 26 * 26 * 26 * 26] } {
		set errstring\
		    "Duplicate set too large for random string generator"
		puts "FAIL:[timestamp] $errstring"
		return -code return $errstring
	}

	set rs_int_list {}

	# generate alphabet array
	for { set j 0 } { $j < 26 } { incr j } {
		set a($j) [string index $alphabet $j]
	}

	# Generate a list with $i elements, { aaaa, aaab, ... aaaz, aaba ...}
	for { set d1 0 ; set j 0 } { $d1 < 26 && $j < $i } { incr d1 } {
		for { set d2 0 } { $d2 < 26 && $j < $i } { incr d2 } {
			for { set d3 0 } { $d3 < 26 && $j < $i } { incr d3 } {
				for { set d4 0 } { $d4 < 26 && $j < $i } \
				    { incr d4 } {
					lappend rs_int_list \
						$a($d1)$a($d2)$a($d3)$a($d4)
					incr j
				}
			}
		}
	}

	# Randomize the list.
	set rs_int_list [randomize_list $rs_int_list]
}

# Randomize a list.  Returns a randomly-reordered copy of l.
proc randomize_list { l } {
	set i [llength $l]

	for { set j 0 } { $j < $i } { incr j } {
		# Pick a random element from $j to the end
		set k [berkdb random_int $j [expr $i - 1]]

		# Swap it with element $j
		set t1 [lindex $l $j]
		set t2 [lindex $l $k]

		set l [lreplace $l $j $j $t2]
		set l [lreplace $l $k $k $t1]
	}

	return $l
}

proc randstring {} {
	global rs_int_list

	if { [info exists rs_int_list] == 0 || [llength $rs_int_list] == 0 } {
		set errstring "randstring uninitialized or used too often"
		puts "FAIL:[timestamp] $errstring"
		return -code return $errstring
	}

	set item [lindex $rs_int_list 0]
	set rs_int_list [lreplace $rs_int_list 0 0]

	return $item
}

# Takes a variable-length arg list, and returns a list containing the list of
# the non-hyphenated-flag arguments, followed by a list of each alphanumeric
# flag it finds.
proc extractflags { args } {
	set inflags 1
	set flags {}
	while { $inflags == 1 } {
		set curarg [lindex $args 0]
		if { [string first "-" $curarg] == 0 } {
			set i 1
			while {[string length [set f \
			    [string index $curarg $i]]] > 0 } {
				incr i
				if { [string compare $f "-"] == 0 } {
					set inflags 0
					break
				} else {
					lappend flags $f
				}
			}
			set args [lrange $args 1 end]
		} else {
			set inflags 0
		}
	}
	return [list $args $flags]
}

# Wrapper for berkdb open, used throughout the test suite so that we can
# set an errfile/errpfx as appropriate.
proc berkdb_open { args } {
	set errargs {}
	if { [file exists /dev/stderr] == 1 } {
		append errargs " -errfile /dev/stderr "
		append errargs " -errpfx \\F\\A\\I\\L "
	}

	eval {berkdb open} $errargs $args
}

# Version without errpfx/errfile, used when we're expecting a failure.
proc berkdb_open_noerr { args } {
	eval {berkdb open} $args
}

proc check_handles { {outf stdout} } {
	global ohandles

	set handles [berkdb handles]
	if {[llength $handles] != [llength $ohandles]} {
		puts $outf "WARNING: Open handles during cleanup: $handles"
	}
	set ohandles $handles
}

proc open_handles { } {
	return [llength [berkdb handles]]
}

proc move_file_extent { dir dbfile tag op } {
	set files [get_extfiles $dir $dbfile $tag]
	foreach extfile $files {
		set i [string last "." $extfile]
		incr i
		set extnum [string range $extfile $i end]
		set dbq [make_ext_filename $dir $dbfile $extnum]
		#
		# We can either copy or rename
		#
		file $op -force $extfile $dbq
	}
}

proc copy_extent_file { dir dbfile tag { op copy } } {
	set files [get_extfiles $dir $dbfile ""]
	foreach extfile $files {
		set i [string last "." $extfile]
		incr i
		set extnum [string range $extfile $i end]
		file $op -force $extfile $dir/__dbq.$dbfile.$tag.$extnum
	}
}

proc get_extfiles { dir dbfile tag } {
	if { $tag == "" } {
		set filepat $dir/__dbq.$dbfile.\[0-9\]*
	} else {
		set filepat $dir/__dbq.$dbfile.$tag.\[0-9\]*
	}
	return [glob -nocomplain -- $filepat]
}

proc make_ext_filename { dir dbfile extnum } {
	return $dir/__dbq.$dbfile.$extnum
}

# All pids for Windows 9X are negative values.  When we want to have
# unsigned int values, unique to the process, we'll take the absolute
# value of the pid.  This avoids unsigned/signed mistakes, yet
# guarantees uniqueness, since each system has pids that are all
# either positive or negative.
#
proc sanitized_pid { } {
	set mypid [pid]
	if { $mypid < 0 } {
		set mypid [expr - $mypid]
	}
	puts "PID: [pid] $mypid\n"
	return $mypid
}

#
# Extract the page size field from a stat record.  Return -1 if
# none is found.
#
proc get_pagesize { stat } {
	foreach field $stat {
		set title [lindex $field 0]
		if {[string compare $title "Page size"] == 0} {
			return [lindex $field 1]
		}
	}
	return -1
}

# See the file LICENSE for redistribution information.
#
# Copyright (c) 1999, 2000
#	Sleepycat Software.  All rights reserved.
#
#	$Id: test051.tcl,v 11.14 2000/08/25 14:21:57 sue Exp $
#
# Test51:
#	Test of the fixed recno method.
#		0. Test various flags (legal and illegal) to open
#		1. Test partial puts where dlen != size (should fail)
#		2. Partial puts for existent record -- replaces at beg, mid, and
#			end of record, as well as full replace
#
proc test051 { method { args "" } } {
	global fixed_len
	global errorInfo
	global errorCode
	source ./include.tcl

	set args [convert_args $method $args]
	set omethod [convert_method $method]

	puts "Test051: Test of the fixed length records."
	if { [is_fixed_length $method] != 1 } {
		puts "Test051: skipping for method $method"
		return
	}

	# Create the database and open the dictionary
	set eindex [lsearch -exact $args "-env"]
	#
	# If we are using an env, then testfile should just be the db name.
	# Otherwise it is the test directory and the name.
	if { $eindex == -1 } {
		set testfile $testdir/test051.db
		set testfile1 $testdir/test051a.db
		set env NULL
	} else {
		set testfile test051.db
		set testfile1 test051a.db
		incr eindex
		set env [lindex $args $eindex]
	}
	cleanup $testdir $env
	set oflags "-create -truncate -mode 0644 $args"

	# Test various flags (legal and illegal) to open
	puts "\tTest051.a: Test correct flag behavior on open."
	set errorCode NONE
	foreach f { "-dup" "-dup -dupsort" "-recnum" } {
		puts "\t\tTest051.a: Test flag $f"
		error_check_good dbopen:flagtest:catch \
		    [catch {set db \
		    [eval {berkdb_open_noerr} $oflags $f $omethod \
		    $testfile]} ret] 1
		error_check_good \
		    dbopen:flagtest:$f [is_substr $errorCode EINVAL] 1
		set errorCode NONE
	}
	set f "-renumber"
	puts "\t\tTest051.a: Test $f"
	if { [is_frecno $method] == 1 } {
		set db [eval {berkdb_open} $oflags $f $omethod $testfile]
		error_check_good dbopen:flagtest:$f [is_valid_db $db] TRUE
		$db close
	} else {
		error_check_good \
		    dbopen:flagtest:catch [catch {set db [eval \
			{berkdb_open_noerr} $oflags $f \
			$omethod $testfile]} ret] 1
		error_check_good \
		    dbopen:flagtest:$f [is_substr $errorCode EINVAL] 1
	}


	# Test partial puts where dlen != size (should fail)
	# it is an error to specify a partial put w/ different
	# dlen and size in fixed length recno/queue
	set key 1
	set data ""
	set test_char "a"

	set db [eval {berkdb_open_noerr} $oflags $omethod $testfile1]
	error_check_good dbopen [is_valid_db $db] TRUE

	puts "\tTest051.b: Partial puts with dlen != size."
	foreach dlen { 1 16 20 32 } {
		foreach doff { 0 10 20 32 } {
			# dlen < size
			puts "\t\tTest051.e: dlen: $dlen, doff: $doff, \
			    size: [expr $dlen+1]"
			set data [repeat $test_char [expr $dlen + 1]]
			error_check_good catch:put 1 [catch {$db \
			    put -partial [list $doff $dlen] $key $data} ret]
			#
			# We don't get back the server error string just
			# the result.
			#
			if { $eindex == -1 } {
				error_check_good "dbput:partial: dlen < size" \
				    [is_substr $errorInfo "Length improper"] 1
			} else {
				error_check_good "dbput:partial: dlen < size" \
				    [is_substr $errorCode "EINVAL"] 1
			}

			# dlen > size
			puts "\t\tTest051.e: dlen: $dlen, doff: $doff, \
			    size: [expr $dlen-1]"
			set data [repeat $test_char [expr $dlen - 1]]
			error_check_good catch:put 1 [catch {$db \
			    put -partial [list $doff $dlen] $key $data} ret]
			if { $eindex == -1 } {
				error_check_good "dbput:partial: dlen > size" \
				    [is_substr $errorInfo "Length improper"] 1
			} else {
				error_check_good "dbput:partial: dlen < size" \
				    [is_substr $errorCode "EINVAL"] 1
			}
		}
	}

	$db close

	# Partial puts for existent record -- replaces at beg, mid, and
	# end of record, as well as full replace
	puts "\tTest051.f: Partial puts within existent record."
	set db [eval {berkdb_open} $oflags $omethod $testfile]
	error_check_good dbopen [is_valid_db $db] TRUE

	puts "\t\tTest051.f: First try a put and then a full replace."
	set data [repeat "a" $fixed_len]

	set ret [$db put 1 $data]
	error_check_good dbput $ret 0
	error_check_good dbget $data [lindex [lindex [$db get -recno 1] 0] 1]

	set data [repeat "b" $fixed_len]
	set ret [$db put -partial [list 0 $fixed_len] 1 $data]
	error_check_good dbput $ret 0
	error_check_good dbget $data [lindex [lindex [$db get -recno 1] 0] 1]

	set data "InitialData"
	set pdata "PUT"
	set dlen [string length $pdata]
	set ilen [string length $data]
	set mid [expr $ilen/2]

	# put initial data
	set key 0

	set offlist [list 0 $mid [expr $ilen -1] [expr $fixed_len - $dlen]]
	puts "\t\tTest051.g: Now replace at different offsets ($offlist)."
	foreach doff $offlist {
		incr key
		set ret [$db put $key $data]
		error_check_good dbput:init $ret 0

		puts "\t\t  Test051.g: Replace at offset $doff."
		set ret [$db put -partial [list $doff $dlen] $key $pdata]
		error_check_good dbput:partial $ret 0

		if { $doff == 0} {
			set beg ""
			set end [string range $data $dlen $ilen]
		} else {
			set beg [string range $data 0 [expr $doff - 1]]
			set end [string range $data [expr $doff + $dlen] $ilen]
		}
		if { $doff > $ilen } {
			# have to put padding between record and inserted
			# string
			set newdata [format %s%s $beg $end]
			set diff [expr $doff - $ilen]
			set nlen [string length $newdata]
			set newdata [binary \
			    format a[set nlen]x[set diff]a$dlen $newdata $pdata]
		} else {
			set newdata [make_fixed_length \
			    frecno [format %s%s%s $beg $pdata $end]]
		}
		set ret [$db get -recno $key]
		error_check_good compare($newdata,$ret) \
		    [binary_compare [lindex [lindex $ret 0] 1] $newdata] 0
	}

	$db close

	puts "\tTest051 complete."
}

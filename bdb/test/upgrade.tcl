# See the file LICENSE for redistribution information.
#
# Copyright (c) 1999, 2000
#	Sleepycat Software.  All rights reserved.
#
#	$Id: upgrade.tcl,v 11.16 2000/10/27 13:23:56 sue Exp $

source ./include.tcl

global upgrade_dir
# set upgrade_dir "$test_path/upgrade_test"
set upgrade_dir "$test_path/upgrade/databases"

global gen_upgrade
set gen_upgrade 0

global upgrade_dir
global upgrade_be
global upgrade_method

proc upgrade { { archived_test_loc "DEFAULT" } } {
	source ./include.tcl
	global upgrade_dir

	set saved_upgrade_dir $upgrade_dir

	puts -nonewline "Upgrade test: "
	if { $archived_test_loc == "DEFAULT" } {
		puts "using default archived databases in $upgrade_dir."
	} else {
		set upgrade_dir $archived_test_loc
		puts "using archived databases in $upgrade_dir."
	}

	foreach version [glob $upgrade_dir/*] {
		if { [string first CVS $version] != -1 } { continue }
		regexp \[^\/\]*$ $version version
		foreach method [glob $upgrade_dir/$version/*] {
			regexp \[^\/\]*$ $method method
			foreach file [glob $upgrade_dir/$version/$method/*] {
				regexp (\[^\/\]*)\.tar\.gz$ $file dummy name

				cleanup $testdir NULL
				#puts  "$upgrade_dir/$version/$method/$name.tar.gz"
				set curdir [pwd]
				cd $testdir
				set tarfd [open "|tar xf -" w]
				cd $curdir

				catch {exec gunzip -c "$upgrade_dir/$version/$method/$name.tar.gz" >@$tarfd}
				close $tarfd

				set f [open $testdir/$name.tcldump {RDWR CREAT}]
				close $f

				# It may seem suboptimal to exec a separate
				# tclsh for each subtest, but this is
				# necessary to keep the testing process
				# from consuming a tremendous amount of
				# memory.
				if { [file exists $testdir/$name-le.db] } {
					set ret [catch {exec $tclsh_path\
					    << "source $test_path/test.tcl;\
					    _upgrade_test $testdir $version\
					    $method\
					    $name le"} message]
					puts $message
					if { $ret != 0 } {
						#exit
					}
				}

				if { [file exists $testdir/$name-be.db] } {
					set ret [catch {exec $tclsh_path\
					    << "source $test_path/test.tcl;\
					    _upgrade_test $testdir $version\
					    $method\
					    $name be"} message]
					puts $message
					if { $ret != 0 } {
						#exit
					}
				}

				set ret [catch {exec $tclsh_path\
				    << "source $test_path/test.tcl;\
				    _db_load_test $testdir $version $method\
				    $name"} message]
					puts $message
				if { $ret != 0 } {
					#exit
				}

			}
		}
	}
	set upgrade_dir $saved_upgrade_dir

	# Don't provide a return value.
	return
}

proc _upgrade_test { temp_dir version method file endianness } {
	source include.tcl
	global errorInfo

	puts "Upgrade: $version $method $file $endianness"

	set ret [berkdb upgrade "$temp_dir/$file-$endianness.db"]
	error_check_good dbupgrade $ret 0

	upgrade_dump "$temp_dir/$file-$endianness.db" "$temp_dir/temp.dump"

	error_check_good "Upgrade diff.$endianness: $version $method $file" \
	    [filecmp "$temp_dir/$file.tcldump" "$temp_dir/temp.dump"] 0
}

proc _db_load_test { temp_dir version method file } {
	source include.tcl
	global errorInfo

	puts "db_load: $version $method $file"

	set ret [catch \
	    {exec $util_path/db_load -f "$temp_dir/$file.dump" \
	    "$temp_dir/upgrade.db"} message]
	error_check_good \
	    "Upgrade load: $version $method $file $message" $ret 0

	upgrade_dump "$temp_dir/upgrade.db" "$temp_dir/temp.dump"

	error_check_good "Upgrade diff.1.1: $version $method $file" \
	    [filecmp "$temp_dir/$file.tcldump" "$temp_dir/temp.dump"] 0
}

proc gen_upgrade { dir } {
	global gen_upgrade
	global upgrade_dir
	global upgrade_be
	global upgrade_method
	global runtests
	source ./include.tcl

	set gen_upgrade 1
	set upgrade_dir $dir

	foreach upgrade_be { 0 1 } {
		foreach i "btree rbtree hash recno rrecno queue frecno" {
			puts "Running $i tests"
			set upgrade_method $i
			set start 1
			for { set j $start } { $j <= $runtests } {incr j} {
				if [catch {exec $tclsh_path \
				    << "source $test_path/test.tcl;\
				    global upgrade_be;\
				    set upgrade_be $upgrade_be;\
				    run_method -$i $j $j"} res] {
					puts "FAIL: [format "test%03d" $j] $i"
				}
				puts $res
				cleanup $testdir NULL
			}
		}
	}

	set gen_upgrade 0
}

proc upgrade_dump { database file {stripnulls 0} } {
	global errorInfo

	set db [berkdb open $database]
	set dbc [$db cursor]

	set f [open $file w+]
	fconfigure $f -encoding binary -translation binary

	#
	# Get a sorted list of keys
	#
	set key_list ""
	set pair [$dbc get -first]

	while { 1 } {
		if { [llength $pair] == 0 } {
			break
		}
		set k [lindex [lindex $pair 0] 0]
		lappend key_list $k
		set pair [$dbc get -next]
	}

	# Discard duplicated keys;  we now have a key for each
	# duplicate, not each unique key, and we don't want to get each
	# duplicate multiple times when we iterate over key_list.
	set uniq_keys ""
	foreach key $key_list {
		if { [info exists existence_list($key)] == 0 } {
			lappend uniq_keys $key
		}
		set existence_list($key) 1
	}
	set key_list $uniq_keys

	set key_list [lsort -command _comp $key_list]

	#
	# Get the data for each key
	#
	set i 0
	foreach key $key_list {
		set pair [$dbc get -set $key]
		if { $stripnulls != 0 } {
			# the Tcl interface to db versions before 3.X
			# added nulls at the end of all keys and data, so
			# we provide functionality to strip that out.
			set key [strip_null $key]
		}
		set data_list {}
		catch { while { [llength $pair] != 0 } {
			set data [lindex [lindex $pair 0] 1]
			if { $stripnulls != 0 } {
				set data [strip_null $data]
			}
			lappend data_list [list $data]
			set pair [$dbc get -nextdup]
		} }
		#lsort -command _comp data_list
		set data_list [lsort -command _comp $data_list]
		puts -nonewline $f [binary format i [string length $key]]
		puts -nonewline $f $key
		puts -nonewline $f [binary format i [llength $data_list]]
		for { set j 0 } { $j < [llength $data_list] } { incr j } {
			puts -nonewline $f [binary format i [string length [concat [lindex $data_list $j]]]]
			puts -nonewline $f [concat [lindex $data_list $j]]
		}
		if { [llength $data_list] == 0 } {
			puts "WARNING: zero-length data list"
		}
		incr i
	}

	close $f
}

proc _comp { a b } {
	if { 0 } {
	# XXX
		set a [strip_null [concat $a]]
		set b [strip_null [concat $b]]
		#return [expr [concat $a] < [concat $b]]
	} else {
		set an [string first "\0" $a]
		set bn [string first "\0" $b]

		if { $an != -1 } {
			set a [string range $a 0 [expr $an - 1]]
		}
		if { $bn != -1 } {
			set b [string range $b 0 [expr $bn - 1]]
		}
	}
	#puts "$a $b"
	return [string compare $a $b]
}

proc strip_null { str } {
	set len [string length $str]
	set last [expr $len - 1]

	set termchar [string range $str $last $last]
	if { [string compare $termchar \0] == 0 } {
		set ret [string range $str 0 [expr $last - 1]]
	} else {
		set ret $str
	}

	return $ret
}

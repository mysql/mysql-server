# See the file LICENSE for redistribution information.
#
# Copyright (c) 1996, 1997, 1998, 1999, 2000
#	Sleepycat Software.  All rights reserved.
#
#	$Id: mdbscript.tcl,v 11.23 2000/10/09 02:26:11 krinsky Exp $
#
# Process script for the multi-process db tester.

source ./include.tcl
source $test_path/test.tcl
source $test_path/testutils.tcl

global dbenv
global klock
global l_keys
global procid
global alphabet

# In Tcl, when there are multiple catch handlers, *all* handlers
# are called, so we have to resort to this hack.
#
global exception_handled

set exception_handled 0

set datastr $alphabet$alphabet

# Usage: mdbscript dir file nentries iter procid procs seed
# dir: DBHOME directory
# file: db file on which to operate
# nentries: number of entries taken from dictionary
# iter: number of operations to run
# procid: this processes' id number
# procs: total number of processes running
set usage "mdbscript method dir file nentries iter procid procs"

# Verify usage
if { $argc != 7 } {
	puts "FAIL:[timestamp] test042: Usage: $usage"
	exit
}

# Initialize arguments
set method [lindex $argv 0]
set dir [lindex $argv 1]
set file [lindex $argv 2]
set nentries [ lindex $argv 3 ]
set iter [ lindex $argv 4 ]
set procid [ lindex $argv 5 ]
set procs [ lindex $argv 6 ]

set pflags ""
set gflags ""
set txn ""

set renum [is_rrecno $method]
set omethod [convert_method $method]

if { [is_record_based $method] == 1 } {
   append gflags " -recno"
}

# Initialize seed
global rand_init

# We want repeatable results, but we also want each instance of mdbscript
# to do something different.  So we add the procid to the fixed seed.
# (Note that this is a serial number given by the caller, not a pid.)
berkdb srand [expr $rand_init + $procid]

puts "Beginning execution for [pid] $method"
puts "$dir db_home"
puts "$file database"
puts "$nentries data elements"
puts "$iter iterations"
puts "$procid process id"
puts "$procs processes"

set klock NOLOCK
flush stdout

set dbenv [berkdb env -create -cdb -home $dir]
#set dbenv [berkdb env -create -cdb -log -home $dir]
error_check_good dbenv [is_valid_env $dbenv] TRUE

set db [berkdb_open -env $dbenv -create -mode 0644 $omethod $file]
error_check_good dbopen [is_valid_db $db] TRUE

# Init globals (no data)
set nkeys [db_init $db 0]
puts "Initial number of keys: $nkeys"
error_check_good db_init $nkeys $nentries
tclsleep 5

proc get_lock { k } {
	global dbenv
	global procid
	global klock
	global DB_LOCK_WRITE
	global DB_LOCK_NOWAIT
	global errorInfo
	global exception_handled
	# Make sure that the key isn't in the middle of
	# a delete operation
	if {[catch {$dbenv lock_get -nowait write $procid $k} klock] != 0 } {
		set exception_handled 1

		error_check_good \
		    get_lock [is_substr $errorInfo "DB_LOCK_NOTGRANTED"] 1
		puts "Warning: key $k locked"
		set klock NOLOCK
		return 1
	} else  {
		error_check_good get_lock [is_valid_lock $klock $dbenv] TRUE
	}
	return 0
}

# On each iteration we're going to randomly pick a key.
# 1. We'll either get it (verifying that its contents are reasonable).
# 2. Put it (using an overwrite to make the data be datastr:ID).
# 3. Get it and do a put through the cursor, tacking our ID on to
# 4. Get it, read forward some random number of keys.
# 5. Get it, read forward some random number of keys and do a put (replace).
# 6. Get it, read forward some random number of keys and do a del.  And then
#	do a put of the key.
set gets 0
set getput 0
set overwrite 0
set seqread 0
set seqput 0
set seqdel 0
set dlen [string length $datastr]

for { set i 0 } { $i < $iter } { incr i } {
	set op [berkdb random_int 0 5]
	puts "iteration $i operation $op"
	flush stdout
	if {[catch {
	switch $op {
		0 {
			incr gets
			set k [rand_key $method $nkeys $renum $procs]
			if {[is_record_based $method] == 1} {
				set key $k
			} else  {
				set key [lindex $l_keys $k]
			}

			if { [get_lock $key] == 1 } {
				incr i -1
				continue;
			}

			set rec [eval {$db get} $txn $gflags {$key}]
			error_check_bad "$db get $key" [llength $rec] 0
			set partial [string range \
			    [lindex [lindex $rec 0] 1] 0 [expr $dlen - 1]]
			error_check_good \
			    "$db get $key" $partial [pad_data $method $datastr]
		}
		1 {
			incr overwrite
			set k [rand_key $method $nkeys $renum $procs]
			if {[is_record_based $method] == 1} {
				set key $k
			} else  {
				set key [lindex $l_keys $k]
			}

			set data $datastr:$procid
			set ret [eval {$db put} \
			    $txn $pflags {$key [chop_data $method $data]}]
			error_check_good "$db put $key" $ret 0
		}
		2 {
			incr getput
			set dbc [$db cursor -update]
			error_check_good "$db cursor" \
			    [is_valid_cursor $dbc $db] TRUE
			set close_cursor 1
			set k [rand_key $method $nkeys $renum $procs]
			if {[is_record_based $method] == 1} {
				set key $k
			} else  {
				set key [lindex $l_keys $k]
			}

			if { [get_lock  $key] == 1 } {
				incr i -1
				error_check_good "$dbc close" \
				    [$dbc close] 0
				set close_cursor 0
				continue;
			}

			set ret [$dbc get -set $key]
			error_check_good \
			    "$dbc get $key" [llength [lindex $ret 0]] 2
			set rec [lindex [lindex $ret 0] 1]
			set partial [string range $rec 0 [expr $dlen - 1]]
			error_check_good \
			    "$dbc get $key" $partial [pad_data $method $datastr]
			append rec ":$procid"
			set ret [$dbc put \
			    -current [chop_data $method $rec]]
			error_check_good "$dbc put $key" $ret 0
			error_check_good "$dbc close" [$dbc close] 0
			set close_cursor 0
		}
		3 -
		4 -
		5 {
			if { $op == 3 } {
				set flags ""
			} else {
				set flags -update
			}
			set dbc [eval {$db cursor} $flags]
			error_check_good "$db cursor" \
			    [is_valid_cursor $dbc $db] TRUE
			set close_cursor 1
			set k [rand_key $method $nkeys $renum $procs]
			if {[is_record_based $method] == 1} {
				set key $k
			} else  {
				set key [lindex $l_keys $k]
			}

			if { [get_lock $key] == 1 } {
				incr i -1
				error_check_good "$dbc close" \
				    [$dbc close] 0
				set close_cursor 0
				continue;
			}

			set ret [$dbc get -set $key]
			error_check_good \
			    "$dbc get $key" [llength [lindex $ret 0]] 2

			# Now read a few keys sequentially
			set nloop [berkdb random_int 0 10]
			if { [berkdb random_int 0 1] == 0 } {
				set flags -next
			} else {
				set flags -prev
			}
			while { $nloop > 0 } {
				set lastret $ret
				set ret [eval {$dbc get} $flags]
				# Might read beginning/end of file
				if { [llength $ret] == 0} {
					set ret $lastret
					break
				}
				incr nloop -1
			}
			switch $op {
				3 {
					incr seqread
				}
				4 {
					incr seqput
					set rec [lindex [lindex $ret 0] 1]
					set partial [string range $rec 0 \
					    [expr $dlen - 1]]
					error_check_good "$dbc get $key" \
					    $partial [pad_data $method $datastr]
					append rec ":$procid"
					set ret [$dbc put -current \
					    [chop_data $method $rec]]
					error_check_good \
					    "$dbc put $key" $ret 0
				}
				5 {
					incr seqdel
					set k [lindex [lindex $ret 0] 0]
					# We need to lock the item we're
					# deleting so that someone else can't
					# try to do a get while we're
					# deleting
					error_check_good "$klock put" \
					    [$klock put] 0
					set klock NOLOCK
					set cur [$dbc get -current]
					error_check_bad get_current \
					    [llength $cur] 0
					set key [lindex [lindex $cur 0] 0]
					if { [get_lock $key] == 1 } {
						incr i -1
						error_check_good "$dbc close" \
						     [$dbc close] 0
						set close_cursor 0
						continue
					}
					set ret [$dbc del]
					error_check_good "$dbc del" $ret 0
					set rec $datastr
					append rec ":$procid"
					if { $renum == 1 } {
						set ret [$dbc put -before \
						    [chop_data $method $rec]]
						error_check_good \
						    "$dbc put $k" $ret $k
					} elseif { \
					    [is_record_based $method] == 1 } {
						error_check_good "$dbc close" \
						    [$dbc close] 0
						set close_cursor 0
						set ret [$db put $k \
						    [chop_data $method $rec]]
						error_check_good \
						    "$db put $k" $ret 0
					} else {
						set ret [$dbc put -keylast $k \
						    [chop_data $method $rec]]
						error_check_good \
						    "$dbc put $k" $ret 0
					}
				}
			}
			if { $close_cursor == 1 } {
				error_check_good \
				    "$dbc close" [$dbc close] 0
				set close_cursor 0
			}
		}
	}
	} res] != 0} {
		global errorInfo;
		global exception_handled;

		puts $errorInfo

		set fnl [string first "\n" $errorInfo]
		set theError [string range $errorInfo 0 [expr $fnl - 1]]

		flush stdout
		if { [string compare $klock NOLOCK] != 0 } {
			catch {$klock put}
		}
		if {$close_cursor == 1} {
			catch {$dbc close}
			set close_cursor 0
		}

		if {[string first FAIL $theError] == 0 && \
		    $exception_handled != 1} {
			error "FAIL:[timestamp] test042: key $k: $theError"
		}
		set exception_handled 0
	} else {
		flush stdout
		if { [string compare $klock NOLOCK] != 0 } {
			error_check_good "$klock put" [$klock put] 0
			set klock NOLOCK
		}
	}
}

if {[catch {$db close} ret] != 0 } {
	error_check_good close [is_substr $errorInfo "DB_INCOMPLETE"] 1
	puts "Warning: sync incomplete on close ([pid])"
} else  {
	error_check_good close $ret 0
}
$dbenv close

exit

puts "[timestamp] [pid] Complete"
puts "Successful ops: "
puts "\t$gets gets"
puts "\t$overwrite overwrites"
puts "\t$getput getputs"
puts "\t$seqread seqread"
puts "\t$seqput seqput"
puts "\t$seqdel seqdel"
flush stdout

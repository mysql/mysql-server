# See the file LICENSE for redistribution information.
#
# Copyright (c) 1999, 2000
#	Sleepycat Software.  All rights reserved.
#
#	$Id: conscript.tcl,v 11.12 2000/12/01 04:28:36 ubell Exp $
#
# Script for DB_CONSUME test (test070.tcl).
# Usage: conscript dir file runtype nitems outputfile tnum args
# dir: DBHOME directory
# file: db file on which to operate
# runtype: PRODUCE or CONSUME--which am I?
# nitems: number of items to put or get
# outputfile: where to log consumer results
# tnum: test number

proc consumescript_produce { db_cmd nitems tnum args } {
	source ./include.tcl
	global mydata

	set pid [pid]
	puts "\tTest0$tnum: Producer $pid starting, producing $nitems items."

	set db [eval $db_cmd]
	error_check_good db_open:$pid [is_valid_db $db] TRUE

	set oret -1
	set ret 0
	for { set ndx 0 } { $ndx < $nitems } { incr ndx } {
		set oret $ret
		set ret [$db put -append [chop_data q $mydata]]
		error_check_good db_put \
		    [expr $ret > 0 ? $oret < $ret : \
		    $oret < 0 ? $oret < $ret : $oret > $ret] 1

	}
	# XXX: We permit incomplete syncs because they seem to
	# be unavoidable and not damaging.
	set ret [catch {$db close} res]
	error_check_good db_close:$pid [expr ($ret == 0) ||\
		([is_substr $res DB_INCOMPLETE] == 1)] 1
	puts "\t\tTest0$tnum: Producer $pid finished."
}

proc consumescript_consume { db_cmd nitems tnum outputfile mode args } {
	source ./include.tcl
	global mydata
	set pid [pid]
	puts "\tTest0$tnum: Consumer $pid starting, seeking $nitems items."

	set db [eval $db_cmd]
	error_check_good db_open:$pid [is_valid_db $db] TRUE

	set oid [open $outputfile w]

	for { set ndx 0 } { $ndx < $nitems } { } {
		set ret [$db get $mode]
		if { [llength $ret] > 0 } {
			error_check_good correct_data:$pid \
				[lindex [lindex $ret 0] 1] [pad_data q $mydata]
			set rno [lindex [lindex $ret 0] 0]
			puts $oid $rno
			incr ndx
		} else {
			# No data to consume;  wait.
		}
	}

	error_check_good output_close:$pid [close $oid] ""
	# XXX: see above note.
	set ret [catch {$db close} res]
	error_check_good db_close:$pid [expr ($ret == 0) ||\
		([is_substr $res DB_INCOMPLETE] == 1)] 1
	puts "\t\tTest0$tnum: Consumer $pid finished."
}

source ./include.tcl
source $test_path/test.tcl

# Verify usage
if { $argc < 6 } {
	puts stderr "FAIL:[timestamp] Usage: $usage"
	exit
}

set usage "conscript.tcl dir file runtype nitems outputfile tnum"

# Initialize arguments
set dir [lindex $argv 0]
set file [lindex $argv 1]
set runtype [lindex $argv 2]
set nitems [lindex $argv 3]
set outputfile [lindex $argv 4]
set tnum [lindex $argv 5]
# args is the string "{ -len 20 -pad 0}", so we need to extract the
# " -len 20 -pad 0" part.
set args [lindex [lrange $argv 6 end] 0]

set mydata "consumer data"

# Open env
set dbenv [berkdb env -home $dir ]
error_check_good db_env_create [is_valid_env $dbenv] TRUE

# Figure out db opening command.
set db_cmd [concat {berkdb_open -create -mode 0644 -queue -env}\
	$dbenv $args $file]

# Invoke consumescript_produce or consumescript_consume based on $runtype
if { $runtype == "PRODUCE" } {
	# Producers have nothing to log;  make sure outputfile is null.
	error_check_good no_producer_outputfile $outputfile ""
	consumescript_produce $db_cmd $nitems $tnum $args
} elseif { $runtype == "CONSUME" } {
	consumescript_consume $db_cmd $nitems $tnum $outputfile -consume $args
} elseif { $runtype == "WAIT" } {
	consumescript_consume $db_cmd $nitems $tnum $outputfile -consume_wait \
		$args
} else {
	error_check_good bad_args $runtype "either PRODUCE, CONSUME or WAIT"
}
error_check_good env_close [$dbenv close] 0
exit

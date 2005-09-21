# See the file LICENSE for redistribution information.
#
# Copyright (c) 1996-2004
#	Sleepycat Software.  All rights reserved.
#
# $Id: recdscript.tcl,v 11.6 2004/01/28 03:36:29 bostic Exp $
#
# Recovery txn prepare script
# Usage: recdscript op dir envcmd dbfile cmd
# op: primary txn operation
# dir: test directory
# envcmd: command to open env
# dbfile: name of database file
# gidf: name of global id file
# cmd: db command to execute

source ./include.tcl
source $test_path/test.tcl

set usage "recdscript op dir envcmd dbfile gidfile cmd"

# Verify usage
if { $argc != 6 } {
	puts stderr "FAIL:[timestamp] Usage: $usage"
	exit
}

# Initialize arguments
set op [ lindex $argv 0 ]
set dir [ lindex $argv 1 ]
set envcmd [ lindex $argv 2 ]
set dbfile [ lindex $argv 3 ]
set gidfile [ lindex $argv 4 ]
set cmd [ lindex $argv 5 ]

op_recover_prep $op $dir $envcmd $dbfile $gidfile $cmd
flush stdout

# See the file LICENSE for redistribution information.
#
# Copyright (c) 1996-2002
#	Sleepycat Software.  All rights reserved.
#
# $Id: txn008.tcl,v 11.3 2002/05/10 17:55:54 sue Exp $
#

# TEST	txn008
# TEST	Test of wraparound txnids (txn002)
proc txn008 { } {
	source ./include.tcl
	global txn_curid
	global txn_maxid

	set orig_curid $txn_curid
	set orig_maxid $txn_maxid
	puts "\tTxn008.1: wraparound txnids"
	set txn_curid [expr $txn_maxid - 2]
	txn002 "08.1"
	puts "\tTxn008.2: closer wraparound txnids"
	set txn_curid [expr $txn_maxid - 3]
	set txn_maxid [expr $txn_maxid - 2]
	txn002 "08.2"

	puts "\tTxn008.3: test wraparound txnids"
	txn_idwrap_check $testdir
	set txn_curid $orig_curid
	set txn_maxid $orig_maxid
	return
}


# See the file LICENSE for redistribution information.
#
# Copyright (c) 1999, 2000
#	Sleepycat Software.  All rights reserved.
#
#	$Id: test071.tcl,v 11.6 2000/12/01 04:28:36 ubell Exp $
#
# DB Test 71: Test of DB_CONSUME.
#	This is DB Test 70, with one consumer, one producers, and 10000 items.
proc test071 { method {nconsumers 1} {nproducers 1}\
    {nitems 10000} {mode CONSUME} {start 0 } {txn -txn} {tnum 71} args } {

	eval test070 $method \
	    $nconsumers $nproducers $nitems $mode $start $txn $tnum $args
}

# See the file LICENSE for redistribution information.
#
# Copyright (c) 2000
#	Sleepycat Software.  All rights reserved.
#
#	$Id: test091.tcl,v 11.4 2000/12/01 04:28:36 ubell Exp $
#
# DB Test 91 {access method}
# Check for CONSUME_WAIT functionality
#
proc test091 { method {nconsumers 4} \
    {nproducers 2} {nitems 1000} {start 0 } {tnum "91"} args} {
	if { [is_queue $method ] == 0 } {
		puts "Skipping test0$tnum for $method."
		return;
	}
	eval {test070 $method \
	    $nconsumers $nproducers $nitems WAIT $start -txn $tnum } $args
	eval {test070 $method \
	    $nconsumers $nproducers $nitems WAIT $start -cdb $tnum } $args
}

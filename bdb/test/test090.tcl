# See the file LICENSE for redistribution information.
#
# Copyright (c) 2000-2002
#	Sleepycat Software.  All rights reserved.
#
# $Id: test090.tcl,v 11.10 2002/08/15 20:55:21 sandstro Exp $
#
# TEST	test090
# TEST	Test for functionality near the end of the queue using test001.
proc test090 { method {nentries 10000} {txn -txn} {tnum "90"} args} {
	if { [is_queueext $method ] == 0 } {
		puts "Skipping test0$tnum for $method."
		return;
	}
	eval {test001 $method $nentries 4294967000 $tnum 0} $args
}

# See the file LICENSE for redistribution information.
#
# Copyright (c) 1996, 1997, 1998, 1999, 2000
#	Sleepycat Software.  All rights reserved.
#
#	$Id: byteorder.tcl,v 11.7 2000/11/16 23:56:18 ubell Exp $
#
# Byte Order Test
# Use existing tests and run with both byte orders.
proc byteorder { method {nentries 1000} } {
	puts "Byteorder: $method $nentries"

	eval {test001 $method $nentries 0 "01" -lorder 1234}
	eval {test001 $method $nentries 0 "01" -lorder 4321}
	eval {test003 $method -lorder 1234}
	eval {test003 $method -lorder 4321}
	eval {test010 $method $nentries 5 10 -lorder 1234}
	eval {test010 $method $nentries 5 10 -lorder 4321}
	eval {test011 $method $nentries 5 11 -lorder 1234}
	eval {test011 $method $nentries 5 11 -lorder 4321}
	eval {test018 $method $nentries -lorder 1234}
	eval {test018 $method $nentries -lorder 4321}
}

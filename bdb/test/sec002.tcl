# See the file LICENSE for redistribution information.
#
# Copyright (c) 1999-2001
#	Sleepycat Software.  All rights reserved.
#
# $Id: sec002.tcl,v 11.3 2002/04/24 19:04:59 bostic Exp $
#
# TEST	sec002
# TEST	Test of security interface and catching errors in the
# TEST  face of attackers overwriting parts of existing files.
proc sec002 { } {
	global errorInfo
	global errorCode

	source ./include.tcl

	set testfile1 $testdir/sec002-1.db
	set testfile2 $testdir/sec002-2.db
	set testfile3 $testdir/sec002-3.db
	set testfile4 $testdir/sec002-4.db

	puts "Sec002: Test of basic encryption interface."
	env_cleanup $testdir

	set passwd1 "passwd1"
	set passwd2 "passwd2"
	set key "key"
	set data "data"
	set pagesize 1024

	#
	# Set up 4 databases, two encrypted, but with different passwords
	# and one unencrypt, but with checksumming turned on and one
	# unencrypted and no checksumming.  Place the exact same data
	# in each one.
	#
	puts "\tSec002.a: Setup databases"
	set db_cmd "-create -pagesize $pagesize -btree "
	set db [eval {berkdb_open} -encryptaes $passwd1 $db_cmd $testfile1]
	error_check_good db [is_valid_db $db] TRUE
	error_check_good dbput [$db put $key $data] 0
	error_check_good dbclose [$db close] 0

	set db [eval {berkdb_open} -encryptaes $passwd2 $db_cmd $testfile2]
	error_check_good db [is_valid_db $db] TRUE
	error_check_good dbput [$db put $key $data] 0
	error_check_good dbclose [$db close] 0

	set db [eval {berkdb_open} -chksum $db_cmd $testfile3]
	error_check_good db [is_valid_db $db] TRUE
	error_check_good dbput [$db put $key $data] 0
	error_check_good dbclose [$db close] 0

	set db [eval {berkdb_open} $db_cmd $testfile4]
	error_check_good db [is_valid_db $db] TRUE
	error_check_good dbput [$db put $key $data] 0
	error_check_good dbclose [$db close] 0

	#
	# First just touch some bits in the file.  We know that in btree
	# meta pages, bytes 92-459 are unused.  Scribble on them in both
	# an encrypted, and both unencrypted files.  We should get
	# a checksum error for the encrypted, and checksummed files.
	# We should get no error for the normal file.
	#
	set fidlist {}
	set fid [open $testfile1 r+]
	lappend fidlist $fid
	set fid [open $testfile3 r+]
	lappend fidlist $fid
	set fid [open $testfile4 r+]
	lappend fidlist $fid

	puts "\tSec002.b: Overwrite unused space in meta-page"
	foreach f $fidlist {
		fconfigure $f -translation binary
		seek $f 100 start
		set byte [read $f 1]
		binary scan $byte c val
		set newval [expr ~$val]
		set newbyte [binary format c $newval]
		seek $f 100 start
		puts -nonewline $f $newbyte
		close $f
	}
	puts "\tSec002.c: Reopen modified databases"
	set stat [catch {berkdb_open_noerr -encryptaes $passwd1 $testfile1} ret]
	error_check_good db:$testfile1 $stat 1
	error_check_good db:$testfile1:fail \
	    [is_substr $ret "metadata page checksum error"] 1

	set stat [catch {berkdb_open_noerr -chksum $testfile3} ret]
	error_check_good db:$testfile3 $stat 1
	error_check_good db:$testfile3:fail \
	    [is_substr $ret "metadata page checksum error"] 1

	set stat [catch {berkdb_open_noerr $testfile4} db]
	error_check_good db:$testfile4 $stat 0
	error_check_good dbclose [$db close] 0

	puts "\tSec002.d: Replace root page in encrypted w/ encrypted"
	set fid1 [open $testfile1 r+]
	set fid2 [open $testfile2 r+]
	seek $fid1 $pagesize start
	seek $fid2 $pagesize start
	set root1 [read $fid1 $pagesize]
	close $fid1
	puts -nonewline $fid2 $root1
	close $fid2

	set db [berkdb_open_noerr -encryptaes $passwd2 $testfile2]
	error_check_good db [is_valid_db $db] TRUE
	set stat [catch {$db get $key} ret]
	error_check_good dbget $stat 1
	error_check_good db:$testfile2:fail \
	    [is_substr $ret "checksum error: catastrophic recovery required"] 1
	set stat [catch {$db close} ret]
	error_check_good dbclose $stat 1
	error_check_good db:$testfile2:fail [is_substr $ret "DB_RUNRECOVERY"] 1

	puts "\tSec002.e: Replace root page in encrypted w/ unencrypted"
	set fid2 [open $testfile2 r+]
	set fid4 [open $testfile4 r+]
	seek $fid2 $pagesize start
	seek $fid4 $pagesize start
	set root4 [read $fid4 $pagesize]
	close $fid4
	puts -nonewline $fid2 $root4
	close $fid2

	set db [berkdb_open_noerr -encryptaes $passwd2 $testfile2]
	error_check_good db [is_valid_db $db] TRUE
	set stat [catch {$db get $key} ret]
	error_check_good dbget $stat 1
	error_check_good db:$testfile2:fail \
	    [is_substr $ret "checksum error: catastrophic recovery required"] 1
	set stat [catch {$db close} ret]
	error_check_good dbclose $stat 1
	error_check_good db:$testfile2:fail [is_substr $ret "DB_RUNRECOVERY"] 1

	cleanup $testdir NULL 1
	puts "\tSec002 complete."
}

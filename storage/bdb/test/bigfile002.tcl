# See the file LICENSE for redistribution information.
#
# Copyright (c) 2001-2004
#	Sleepycat Software.  All rights reserved.
#
# $Id: bigfile002.tcl,v 11.9 2004/01/28 03:36:26 bostic Exp $
#
# TEST	bigfile002
# TEST	This one should be faster and not require so much disk space,
# TEST	although it doesn't test as extensively.  Create an mpool file
# TEST	with 1K pages.  Dirty page 6000000.  Sync.
proc bigfile002 { args } {
	source ./include.tcl

	puts -nonewline \
	    "Bigfile002: Creating large, sparse file through mpool..."
	flush stdout

	env_cleanup $testdir

	# Create env.
	set env [berkdb_env -create -home $testdir]
	error_check_good valid_env [is_valid_env $env] TRUE

	# Create the file.
	set name big002.file
	set file [$env mpool -create -pagesize 1024 $name]

	# Dirty page 6000000
	set pg [$file get -create 6000000]
	error_check_good pg_init [$pg init A] 0
	error_check_good pg_set [$pg is_setto A] 1

	# Put page back.
	error_check_good pg_put [$pg put -dirty] 0

	# Fsync.
	error_check_good fsync [$file fsync] 0

	puts "succeeded."

	# Close.
	error_check_good fclose [$file close] 0
	error_check_good env_close [$env close] 0
}

# See the file LICENSE for redistribution information.
#
# Copyright (c) 1999-2004
#       Sleepycat Software.  All rights reserved.
#
# $Id: env011.tcl,v 1.4 2004/01/28 03:36:27 bostic Exp $
#
# TEST	env011
# TEST	Run with region overwrite flag.
proc env011 { } {
	source ./include.tcl

	puts "Env011: Test of region overwriting."
	env_cleanup $testdir

	puts "\tEnv011: Creating/closing env for open test."
	set e [berkdb_env -create -overwrite -home $testdir -txn]
	error_check_good dbenv [is_valid_env $e] TRUE
	set db [eval \
	    {berkdb_open -auto_commit -env $e -btree -create -mode 0644} ]
	error_check_good dbopen [is_valid_db $db] TRUE
	set ret [eval {$db put} -auto_commit "aaa" "data"]
	error_check_good put $ret 0
	set ret [eval {$db put} -auto_commit "bbb" "data"]
	error_check_good put $ret 0
	error_check_good db_close [$db close] 0
	error_check_good envclose [$e close] 0

	puts "\tEnv011: Opening the environment with overwrite set."
	set e [berkdb_env -create -overwrite -home $testdir -txn -recover]
	error_check_good dbenv [is_valid_env $e] TRUE
	error_check_good envclose [$e close] 0

	puts "\tEnv011: Removing the environment with overwrite set."
	error_check_good berkdb:envremove \
		[berkdb envremove -home $testdir -overwrite] 0

	puts "\tEnv011 complete."
}

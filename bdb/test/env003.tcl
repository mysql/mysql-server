# See the file LICENSE for redistribution information.
#
# Copyright (c) 1999, 2000
#	Sleepycat Software.  All rights reserved.
#
#	$Id: env003.tcl,v 11.12 2000/08/25 14:21:50 sue Exp $
#
# Env Test 003
# Test DB_TMP_DIR and env name resolution
# With an environment path specified using -home, and then again
# with it specified by the environment variable DB_HOME:
#	1) Make sure that the DB_TMP_DIR config file option is respected
#		a) as a relative pathname.
#		b) as an absolute pathname.
#	2) Make sure that the DB_TMP_DIR db_config argument is respected,
#		again as relative and absolute pathnames.
#	3) Make sure that if -both- db_config and a file are present,
#		only the file is respected (see doc/env/naming.html).
proc env003 { } {
	#   env003 is essentially just a small driver that runs
	# env003_body twice.  First, it supplies a "home" argument
	# to use with environment opens, and the second time it sets
	# DB_HOME instead.
	#   Note that env003_body itself calls env003_run_test to run
	# the body of the actual test.

	global env
	source ./include.tcl

	puts "Env003: DB_TMP_DIR test."

	puts "\tEnv003: Running with -home argument to berkdb env."
	env003_body "-home $testdir"

	puts "\tEnv003: Running with environment variable DB_HOME set."
	set env(DB_HOME) $testdir
	env003_body "-use_environ"

	unset env(DB_HOME)

	puts "\tEnv003: Running with both DB_HOME and -home set."
	# Should respect -only- -home, so we give it a bogus
	# environment variable setting.
	set env(DB_HOME) $testdir/bogus_home
	env003_body "-use_environ -home $testdir"
	unset env(DB_HOME)

}

proc env003_body { home_arg } {
	source ./include.tcl

	env_cleanup $testdir
	set tmpdir "tmpfiles_in_here"

	file mkdir $testdir/$tmpdir

	# Set up full path to $tmpdir for when we test absolute paths.
	set curdir [pwd]
	cd $testdir/$tmpdir
	set fulltmpdir [pwd]
	cd $curdir

	# Run test with the temp dir. nonexistent--it checks for failure.
	env_cleanup $testdir

	env003_make_config $tmpdir

	# Run the meat of the test.
	env003_run_test a 1 "relative path, config file" $home_arg \
		$testdir/$tmpdir

	env_cleanup $testdir

	env003_make_config $fulltmpdir

	# Run the test again
	env003_run_test a 2 "absolute path, config file" $home_arg \
		$fulltmpdir

	env_cleanup $testdir

	# Now we try without a config file, but instead with db_config
	# relative paths
	env003_run_test b 1 "relative path, db_config" "$home_arg \
		-tmp_dir $tmpdir -data_dir ." \
		$testdir/$tmpdir

	env_cleanup $testdir

	# absolute
	env003_run_test b 2 "absolute path, db_config" "$home_arg \
		-tmp_dir $fulltmpdir -data_dir ." \
		$fulltmpdir

	env_cleanup $testdir

	# Now, set db_config -and- have a # DB_CONFIG file, and make
	# sure only the latter is honored.

	# Make a temp directory that actually does exist to supply
	# as a bogus argument--the test checks for -nonexistent- temp
	# dirs., as success is harder to detect.
	file mkdir $testdir/bogus
	env003_make_config $tmpdir

	# note that we supply an -existent- tmp dir to db_config as
	# a red herring
	env003_run_test c 1 "relative path, both db_config and file" \
		"$home_arg -tmp_dir $testdir/bogus -data_dir ." \
		$testdir/$tmpdir
	env_cleanup $testdir

	file mkdir $fulltmpdir
	file mkdir $fulltmpdir/bogus
	env003_make_config $fulltmpdir/nonexistent

	# note that we supply an -existent- tmp dir to db_config as
	# a red herring
	env003_run_test c 2 "relative path, both db_config and file" \
		"$home_arg -tmp_dir $fulltmpdir/bogus -data_dir ." \
		$fulltmpdir
}

proc env003_run_test { major minor msg env_args tmp_path} {
	global testdir
	global alphabet
	global errorCode

	puts "\t\tEnv003.$major.$minor: $msg"

	# Create an environment and small-cached in-memory database to
	# use.
	set dbenv [eval {berkdb env -create -home $testdir} $env_args \
	    {-cachesize {0 40960 1}}]
	error_check_good env_open [is_valid_env $dbenv] TRUE
	set db [berkdb_open_noerr -env $dbenv -create -btree]
	error_check_good db_open [is_valid_db $db] TRUE

	# Fill the database with more than its cache can fit.
	# !!!
	# This is actually trickier than it sounds.  The tempfile
	# gets unlinked as soon as it's created, so there's no straightforward
	# way to check for its existence.  Instead, we make sure
	# DB_TMP_DIR points somewhere bogus, and make sure that the temp
	# dir. does -not- exist.  But to do this, we have to know
	# which call to DB->put is going to fail--the temp file is
	# created lazily, so the failure only occurs when the cache finally
	# overflows.
	# The data we've conjured up will fit nicely once, but the second
	# call will overflow the cache.  Thus we check for success once,
	# then failure.
	#
	set key1 "key1"
	set key2 "key2"
	set data [repeat $alphabet 1000]

	# First put should succeed.
	error_check_good db_put_1 [$db put $key1 $data] 0

	# Second one should return ENOENT.
	set errorCode NONE
	catch {$db put $key2 $data} res
	error_check_good db_put_2 [is_substr $errorCode ENOENT] 1

	error_check_good db_close [$db close] 0
	error_check_good env_close [$dbenv close] 0
}

proc env003_make_config { tmpdir } {
	global testdir

	set cid [open $testdir/DB_CONFIG w]
	puts $cid "set_data_dir ."
	puts $cid "set_tmp_dir $tmpdir"
	close $cid
}

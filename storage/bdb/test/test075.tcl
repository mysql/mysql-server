# See the file LICENSE for redistribution information.
#
# Copyright (c) 2000-2002
#	Sleepycat Software.  All rights reserved.
#
# $Id: test075.tcl,v 11.21 2002/08/08 15:38:11 bostic Exp $
#
# TEST	test075
# TEST	Test of DB->rename().
# TEST	(formerly test of DB_TRUNCATE cached page invalidation [#1487])
proc test075 { method { tnum 75 } args } {
	global encrypt
	global errorCode
	global errorInfo

	source ./include.tcl
	set omethod [convert_method $method]
	set args [convert_args $method $args]

	puts "Test0$tnum: $method ($args): Test of DB->rename()"
	# If we are using an env, then testfile should just be the
	# db name.  Otherwise it is the test directory and the name.
	set eindex [lsearch -exact $args "-env"]
	if { $eindex != -1 } {
		# If we are using an env, then skip this test.
		# It needs its own.
		incr eindex
		set env [lindex $args $eindex]
		puts "Skipping test075 for env $env"
		return
	}
	if { $encrypt != 0 } {
		puts "Skipping test075 for security"
		return
	}

	# Define absolute pathnames
	set curdir [pwd]
	cd $testdir
	set fulldir [pwd]
	cd $curdir
	set reldir $testdir

	# Set up absolute and relative pathnames for test
	set paths [list $fulldir $reldir]
	foreach path $paths {
		puts "\tTest0$tnum: starting test of $path path"
		set oldfile $path/test0$tnum-old.db
		set newfile $path/test0$tnum.db
		set env NULL
		set envargs ""

		# Loop through test using the following rename options
		# 1. no environment, not in transaction
		# 2. with environment, not in transaction
		# 3. rename with auto-commit
		# 4. rename in committed transaction
		# 5. rename in aborted transaction

		foreach op "noenv env auto commit abort" {

			puts "\tTest0$tnum.a: Create/rename file with $op"

			# Make sure we're starting with a clean slate.

			if { $op == "noenv" } {
				cleanup $path $env
				if { $env == "NULL" } {
					error_check_bad "$oldfile exists" \
					    [file exists $oldfile] 1
					error_check_bad "$newfile exists" \
					    [file exists $newfile] 1
				}
			}

			if { $op == "env" } {
				env_cleanup $path
				set env [berkdb_env -create -home $path]
				set envargs "-env $env"
				error_check_good env_open [is_valid_env $env] TRUE
			}

			if { $op == "auto" || $op == "commit" || $op == "abort" } {
				env_cleanup $path
				set env [berkdb_env -create -home $path -txn]
				set envargs "-env $env"
				error_check_good env_open [is_valid_env $env] TRUE
			}

			puts "\t\tTest0$tnum.a.1: create"
			set db [eval {berkdb_open -create -mode 0644} \
			    $omethod $envargs $args $oldfile]
			error_check_good dbopen [is_valid_db $db] TRUE

			if { $env == "NULL" } {
				error_check_bad \
				   "$oldfile exists" [file exists $oldfile] 0
				error_check_bad \
				   "$newfile exists" [file exists $newfile] 1
			}

			# The nature of the key and data are unimportant;
			# use numeric key to record-based methods don't need
			# special treatment.
			set key 1
			set data [pad_data $method data]

			error_check_good dbput [$db put $key $data] 0
			error_check_good dbclose [$db close] 0

			puts "\t\tTest0$tnum.a.2: rename"
			if { $env == "NULL" } {
				error_check_bad \
				    "$oldfile exists" [file exists $oldfile] 0
				error_check_bad \
				    "$newfile exists" [file exists $newfile] 1
			}

			# Regular renames use berkdb dbrename but transaction
			# protected renames must use $env dbrename.
			if { $op == "noenv" || $op == "env" } {
				error_check_good rename_file [eval {berkdb dbrename} \
				    $envargs $oldfile $newfile] 0
			} elseif { $op == "auto" } {
				error_check_good rename_file [eval {$env dbrename} \
				    -auto_commit $oldfile $newfile] 0
			} else {
				# $op is "abort" or "commit"
				set txn [$env txn]
				error_check_good rename_file [eval {$env dbrename} \
				    -txn $txn $oldfile $newfile] 0
				error_check_good txn_$op [$txn $op] 0
			}

			if { $env == "NULL" } {
				error_check_bad \
				    "$oldfile exists" [file exists $oldfile] 1
				error_check_bad \
				    "$newfile exists" [file exists $newfile] 0
			}

			puts "\t\tTest0$tnum.a.3: check"
			# Open again with create to make sure we're not caching or
			# anything silly.  In the normal case (no env), we already
			# know the file doesn't exist.
			set odb [eval {berkdb_open -create -mode 0644} \
			    $envargs $omethod $args $oldfile]
			set ndb [eval {berkdb_open -create -mode 0644} \
			    $envargs $omethod $args $newfile]
			error_check_good odb_open [is_valid_db $odb] TRUE
			error_check_good ndb_open [is_valid_db $ndb] TRUE

			# The DBT from the "old" database should be empty,
			# not the "new" one, except in the case of an abort.
			set odbt [$odb get $key]
			if { $op == "abort" } {
				error_check_good odbt_has_data [llength $odbt] 1
			} else {
				set ndbt [$ndb get $key]
				error_check_good odbt_empty [llength $odbt] 0
				error_check_bad ndbt_empty [llength $ndbt] 0
				error_check_good ndbt [lindex \
				    [lindex $ndbt 0] 1] $data
			}
			error_check_good odb_close [$odb close] 0
			error_check_good ndb_close [$ndb close] 0

			# Now there's both an old and a new.  Rename the
			# "new" to the "old" and make sure that fails.
			#
			# XXX Ideally we'd do this test even when there's
			# an external environment, but that env has
			# errpfx/errfile set now.  :-(
			puts "\tTest0$tnum.b: Make sure rename fails\
			    instead of overwriting"
			if { $env != "NULL" } {
				error_check_good env_close [$env close] 0
				set env [berkdb_env_noerr -home $path]
				error_check_good env_open2 \
				    [is_valid_env $env] TRUE
				set ret [catch {eval {berkdb dbrename} \
		    	    	    -env $env $newfile $oldfile} res]
				error_check_bad rename_overwrite $ret 0
				error_check_good rename_overwrite_ret \
				    [is_substr $errorCode EEXIST] 1
			}

			# Verify and then start over from a clean slate.
			verify_dir $path "\tTest0$tnum.c: "
			cleanup $path $env
			if { $env != "NULL" } {
				error_check_good env_close [$env close] 0
						}
			if { $env == "NULL" } {
				error_check_bad "$oldfile exists" \
				    [file exists $oldfile] 1
				error_check_bad "$newfile exists" \
				    [file exists $newfile] 1

				set oldfile test0$tnum-old.db
				set newfile test0$tnum.db
			}
		}
	}
}

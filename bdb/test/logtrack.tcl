# See the file LICENSE for redistribution information
#
# Copyright (c) 2000
#       Sleepycat Software.  All rights reserved.
#
#       $Id: logtrack.tcl,v 11.6 2000/10/27 15:30:39 krinsky Exp $
#
# logtrack.tcl:  A collection of routines, formerly implemented in Perl
# as log.pl, to track which log record types the test suite hits.

set ltsname "logtrack_seen.db"
set ltlist  $test_path/logtrack.list
set tmpname "logtrack_tmp"

proc logtrack_clean { } {
	global ltsname

	file delete -force $ltsname

	return
}

proc logtrack_init { } {
	global ltsname

	logtrack_clean

	# Create an empty tracking database.
	[berkdb_open -create -truncate -btree $ltsname] close

	return
}

# Dump the logs for directory dirname and record which log
# records were seen.
proc logtrack_read { dirname } {
	global ltsname tmpname util_path

	set seendb [berkdb_open $ltsname]
	error_check_good seendb_open [is_valid_db $seendb] TRUE

	file delete -force $tmpname
	set ret [catch {exec $util_path/db_printlog -N \
	    -h "$dirname" > $tmpname} res]
	error_check_good printlog $ret 0
	error_check_good tmpfile_exists [file exists $tmpname] 1

	set f [open $tmpname r]
	while { [gets $f record] >= 0 } {
		regexp {\[[^\]]*\]\[[^\]]*\]([^\:]*)\:} $record whl name
		error_check_good seendb_put [$seendb put $name ""] 0
	}
	close $f
	file delete -force $tmpname

	error_check_good seendb_close [$seendb close] 0
}

# Print the log record types that were seen but should not have been
# seen and the log record types that were not seen but should have been seen.
proc logtrack_summary { } {
	global ltsname ltlist testdir

	set seendb [berkdb_open $ltsname]
	error_check_good seendb_open [is_valid_db $seendb] TRUE
	set existdb [berkdb_open -create -btree]
	error_check_good existdb_open [is_valid_db $existdb] TRUE
	set deprecdb [berkdb_open -create -btree]
	error_check_good deprecdb_open [is_valid_db $deprecdb] TRUE

	error_check_good ltlist_exists [file exists $ltlist] 1
	set f [open $ltlist r]
	set pref ""
	while { [gets $f line] >= 0 } {
		# Get the keyword, the first thing on the line:
		# BEGIN/DEPRECATED/PREFIX
		set keyword [lindex $line 0]

		if { [string compare $keyword PREFIX] == 0 } {
			# New prefix.
			set pref [lindex $line 1]
		} elseif { [string compare $keyword BEGIN] == 0 } {
			# A log type we care about;  put it on our list.

			# Skip noop and debug.
			if { [string compare [lindex $line 1] noop] == 0 } {
				continue
			}
			if { [string compare [lindex $line 1] debug] == 0 } {
				continue
			}

			error_check_good exist_put [$existdb put \
			    ${pref}_[lindex $line 1] ""] 0
		} elseif { [string compare $keyword DEPRECATED] == 0 } {
			error_check_good deprec_put [$deprecdb put \
			    ${pref}_[lindex $line 1] ""] 0
		}
	}

	error_check_good exist_curs \
	    [is_valid_cursor [set ec [$existdb cursor]] $existdb] TRUE
	while { [llength [set dbt [$ec get -next]]] != 0 } {
		set rec [lindex [lindex $dbt 0] 0]
		if { [$seendb count $rec] == 0 } {
			puts "FAIL: log record type $rec not seen"
		}
	}
	error_check_good exist_curs_close [$ec close] 0

	error_check_good seen_curs \
	    [is_valid_cursor [set sc [$existdb cursor]] $existdb] TRUE
	while { [llength [set dbt [$sc get -next]]] != 0 } {
		set rec [lindex [lindex $dbt 0] 0]
		if { [$existdb count $rec] == 0 } {
			if { [$deprecdb count $rec] == 0 } {
			       puts "FAIL: unknown log record type $rec seen"
			} else {
			       puts "FAIL: deprecated log record type $rec seen"
			}
		}
	}
	error_check_good seen_curs_close [$sc close] 0

	error_check_good seendb_close [$seendb close] 0
	error_check_good existdb_close [$existdb close] 0
	error_check_good deprecdb_close [$deprecdb close] 0

	logtrack_clean
}

# See the file LICENSE for redistribution information.
#
# Copyright (c) 1999, 2000
#	Sleepycat Software.  All rights reserved.
#
#	$Id: sdbutils.tcl,v 11.9 2000/05/22 12:51:38 bostic Exp $
#
proc build_all_subdb { dbname methods psize dups {nentries 100} {dbargs ""}} {
	set nsubdbs [llength $dups]
	set plen [llength $psize]
	set mlen [llength $methods]
	set savearg $dbargs
	for {set i 0} {$i < $nsubdbs} { incr i } {
		set m [lindex $methods [expr $i % $mlen]]
		set dbargs $savearg
		set p [lindex $psize [expr $i % $plen]]
		subdb_build $dbname $nentries [lindex $dups $i] \
		    $i $m $p sub$i.db $dbargs
	}
}

proc subdb_build { name nkeys ndups dup_interval method psize subdb dbargs} {
	source ./include.tcl

	set dbargs [convert_args $method $dbargs]
	set omethod [convert_method $method]

	puts "Method: $method"

	# Create the database and open the dictionary
	set oflags "-create -mode 0644 $omethod \
	    -pagesize $psize $dbargs $name $subdb"
	set db [eval {berkdb_open} $oflags]
	error_check_good dbopen [is_valid_db $db] TRUE
	set did [open $dict]
	set count 0
	if { $ndups >= 0 } {
		puts "\tBuilding $method $name $subdb. \
	$nkeys keys with $ndups duplicates at interval of $dup_interval"
	}
	if { $ndups < 0 } {
		puts "\tBuilding $method $name $subdb. \
		    $nkeys unique keys of pagesize $psize"
		#
		# If ndups is < 0, we want unique keys in each subdb,
		# so skip ahead in the dict by nkeys * iteration
		#
		for { set count 0 } \
		    { $count < [expr $nkeys * $dup_interval] } {
		    incr count} {
			set ret [gets $did str]
			if { $ret == -1 } {
				break
			}
		}
	}
	for { set count 0 } { [gets $did str] != -1 && $count < $nkeys } {
	    incr count} {
		for { set i 0 } { $i < $ndups } { incr i } {
			set data [format "%04d" [expr $i * $dup_interval]]
			set ret [$db put $str [chop_data $method $data]]
			error_check_good put $ret 0
		}

		if { $ndups == 0 } {
			set ret [$db put $str [chop_data $method NODUP]]
			error_check_good put $ret 0
		} elseif { $ndups < 0 } {
			if { [is_record_based $method] == 1 } {
				global kvals

				set num [expr $nkeys * $dup_interval]
				set num [expr $num + $count + 1]
				set ret [$db put $num [chop_data $method $str]]
				set kvals($num) [pad_data $method $str]
				error_check_good put $ret 0
			} else {
				set ret [$db put $str [chop_data $method $str]]
				error_check_good put $ret 0
			}
		}
	}
	close $did
	error_check_good close:$name [$db close] 0
}

proc do_join_subdb { db primary subdbs key } {
	source ./include.tcl

	puts "\tJoining: $subdbs on $key"

	# Open all the databases
	set p [berkdb_open -unknown $db $primary]
	error_check_good "primary open" [is_valid_db $p] TRUE

	set dblist ""
	set curslist ""

	foreach i $subdbs {
		set jdb [berkdb_open -unknown $db sub$i.db]
		error_check_good "sub$i.db open" [is_valid_db $jdb] TRUE

		lappend jlist [list $jdb $key]
		lappend dblist $jdb

	}

	set join_res [eval {$p get_join} $jlist]
	set ndups [llength $join_res]

	# Calculate how many dups we expect.
	# We go through the list of indices.  If we find a 0, then we
	# expect 0 dups.  For everything else, we look at pairs of numbers,
	# if the are relatively prime, multiply them and figure out how
	# many times that goes into 50.  If they aren't relatively prime,
	# take the number of times the larger goes into 50.
	set expected 50
	set last 1
	foreach n $subdbs {
		if { $n == 0 } {
			set expected 0
			break
		}
		if { $last == $n } {
			continue
		}

		if { [expr $last % $n] == 0 || [expr $n % $last] == 0 } {
			if { $n > $last } {
				set last $n
				set expected [expr 50 / $last]
			}
		} else {
			set last [expr $n * $last / [gcd $n $last]]
			set expected [expr 50 / $last]
		}
	}

	error_check_good number_of_dups:$subdbs $ndups $expected

	#
	# If we get here, we have the number expected, now loop
	# through each and see if it is what we expected.
	#
	for { set i 0 } { $i < $ndups } { incr i } {
		set pair [lindex $join_res $i]
		set k [lindex $pair 0]
		foreach j $subdbs {
			error_check_bad valid_dup:$j:$subdbs $j 0
			set kval [string trimleft $k 0]
			if { [string length $kval] == 0 } {
				set kval 0
			}
			error_check_good \
			    valid_dup:$j:$subdbs [expr $kval % $j] 0
		}
	}

	error_check_good close_primary [$p close] 0
	foreach i $dblist {
		error_check_good close_index:$i [$i close] 0
	}
}

proc n_to_subname { n } {
	if { $n == 0 } {
		return null.db;
	} else {
		return sub$n.db;
	}
}

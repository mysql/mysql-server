#!/usr/bin/perl
# This script takes three arguments:
#  command-to-run query-hits-list tmp-filename
# The query-hits-list is a file with a string and a number on each line
# (separated by a coma).  This script passes the string as the query
# string to the command-to-run.  It then check for a string matches
# the pattern ' ==> xxx ', where 'xxx' is the number following the string.
#
$OUTPUT_AUTOFLUSH=STDOUT;
#$OUTPUT_AUTOFLUSH=STDERR;
if ($#ARGV < 1) {
    print "\nusage:\n", $0, " executable-path query-hits-file [temp-filename]\nThe first two arguments are mandatory.\nCurrently there are $#ARGV arguments\n";
    exit;
}
if ($#ARGV > 1) {
    $tmp = "$ARGV[2]/matchCounts-output$$";
}
else {
    $tmp = "matchCounts-output$$";
}

unless (open QH, $ARGV[1]) {
    die "failed to open file ", $ARGV[1], "\n";
}

while (<QH>) {
    next if /^#/; # skip comment lines
    next if /^$/; # skip blank lines
    chop; # remove the end-of-line character
    @ln = split /;/; # split a line into query and hits
    next if (@ln < 2);

    ++ $nlines;
    $cond = $ln[0];
    $hits = $ln[1];
    $rc = 0xffff & system("$ARGV[0] -q \"$cond\" > $tmp 2>&1");
    #print "\"$ARGV[0] -q $cond > $tmp\" returned with code ", $rc, "\n";
    $bak = $cond;
    $bak =~ s/\W/_/g;
    $bak = "$tmp-$bak";
    #print "\$bak=$bak\n";
    if ($rc == 0) {
	$match = `fgrep "==> $hits " $tmp | wc -l`;
	if ($match) {
	    ++$nmatches;
	    if (`egrep "Error|Warning|warning" $tmp | wc -l` > 0) {
		system("cp", $tmp, "$bak");
		print "Query \"$cond\" completed with errors, output file saved to $bak\n";
	    }
	}
	else {
	    system("cp", $tmp, "$bak");
	    print "Query \"$cond\" failed to match expected number of hits, output file saved to $bak\n";
	}
    }
    elsif ($rc == 0xff00) {
	system("cp", $tmp, "$bak");
	print "\"$ARGV[0] -q $cond > $tmp\" failed: $!\n";
    }
    elsif (($rc & 0xff) > 0) {
	system("cp", $tmp, "$bak");
	if ($rc & 0x80) {
	    $rc &= ~0x80;
	    print "\"$ARGV[0] -q $cond > $tmp\" core dumped with signal $rc\n";
	}
	else {
	    print "\"$ARGV[0] -q $cond > $tmp\" received signal $rc\n";
	}
    }
    else {
	system("cp", $tmp, "$bak");
	print "\"$ARGV[0] -q $cond > $tmp\" received an unexpected return code $rc\n";
    }
}
#
unlink $tmp; # no longer need the tmp file
if ($nlines == $nmatches) {
    print "All $nlines queries match the expected results\n";
    exit 0;
}
elsif ($nmatches) {
    print "$nmatches out of $nlines matche the expected results\n";
    exit -1;
}
else {
    print "None of $nlines matche the expected results\n";
    exit -2;
}

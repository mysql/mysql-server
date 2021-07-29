#!/usr/bin/perl
#
# Compare two histogram files and report the maximum relative
# difference.  The input files are assumed to be coma-separated values
# (CSV) with the last column (or the second to the last column when a
# third argument is provided) to be numerical values that can be
# slightly off.  The columns in front of the last one are treated as
# keys for comparing the last value.  If there is any key that are
# different, the relative difference is reported as one and the keys are
# printed.  This script is written for comparing output from
# check-marksdb, in which case we expect one single line of output
# containing the word "acceptable."
if ($#ARGV < 1) {
    print "hcompare.pl expects two file names on the command line.\n  ",
    "If a third argument is present, the data files are exected to ",
    "contain counts as the last column\n";
    die;
}
#
open FileA, "$ARGV[0]" or die $!;
open FileB, "$ARGV[1]" or die $!;
#
if ($#ARGV >= 2) { # the input file contains counts as the last column
    while (<FileA>) {
	#print "DEBUG: processing -- $_";
	if (/(([^,]+,\s*)+)(\d+(.\d*)?([e|E][-+]?\d+)?),\s*(\d+)\s*$/) {
	    #print "DEBUG: split to -- $1, $3, $6\n";
	    my $k = $1, $v = $3, $c = $6;
	    $k =~ s/\s//g;
	    $k =~ s/,$//;
	    #print "DEBUG: split to -- $k, $v, $c\n";
	    $sumA{$k} += $v * $c;
	    $cntA{$k} += $c;
	}
	else {
	    print "Reject the following from $ARGV[0]: $_\n";
	}
    }
    while (<FileB>) {
	if (/(([^,]+,\s*)+)(\d+(.\d*)?([e|E][-+]?\d+)?),\s*(\d+)\s*$/) {
	    #print "DEBUG: split to -- $1, $3, $6\n";
	    my $k = $1, $v = $3, $c = $6;
	    $k =~ s/\s//g;
	    $k =~ s/,$//;
	    #print "DEBUG: split to -- $k, $v, $c\n";
	    $sumB{$k} += $v * $c;
	    $cntB{$k} += $c;
	}
	else {
	    print "Reject the following from $ARGV[1]: $_\n";
	}
    }
}
else {
    while (<FileA>) {
	if (/(([^,]+,\s*)+)(\d+(.\d*)?([e|E][-+]?\d+)?)\s*$/) {
	    #print "DEBUG: $1 $3\n";
	    my $k = $1, $v = $3;
	    $k =~ s/\s//g;
	    $k =~ s/,$//;
	    $sumA{$k} += $v;
	    ++ $cntA{$k};
	}
	else {
	    print "Reject the following from $ARGV[0]: $_\n";
	}
    }
    while (<FileB>) {
	if (/(([^,]+,\s*)+)(\d+(.\d*)?([e|E][-+]?\d+)?)\s*$/) {
	    my $k = $1, $v = $3;
	    $k =~ s/\s//g;
	    $k =~ s/,$//;
	    $sumB{$k} += $v;
	    ++ $cntB{$k};
	}
	else {
	    print "Reject the following from $ARGV[1]: $_\n";
	}
    }
}
close FileA;
close FileB;
#
# the maximum relative difference observed so far
$rd = 0;
# number of warnings issued
$nerrs = 0;
# attempt to match all records in A with those of B
foreach $k (sort keys %cntA) {
    if ($cntB{$k}) {
	if ($cntA{$k} == $cntB{$k}) {
	    my $vA = $sumA{$k};
	    my $vB = $sumB{$k};
	    if ($vA != 0 || $vB != 0) {
		my $d = abs($vA - $vB) /
		    (abs($vA) >= abs($vB) ? abs($vA) : abs($vB));
		if ($d > $rd) {
		    $rd = $d;
		}
	    }
	}
	else {
	    print "The counts for key \"$k\" are different: $ARGV[0] has $cntA{$k}, $ARGV[1] has $cntB{$k}\n";
	    ++ $nerrs;
	    if ($cntA{$k} > 0 && $cntB{$k} > 0) {
		my $vA = $sumA{$k} / $cntA{$k};
		my $vB = $sumB{$k} / $cntB{$k};
		if ($vA != 0 || $vB != 0) {
		    my $d = abs($vA - $vB) /
			(abs($vA) >= abs($vB) ? abs($vA) : abs($vB));
		    if ($d > $rd) {
			$rd = $d;
		    }
		}
	    }
	}
    }
    else {
	print "The key \"$k\" appears in $ARGV[0] with count $cntA{$k}, but does NOT appear in $ARGV[1]\n";
	$rd += 1.0;
	++ $nerrs;
    }
}
foreach $k (sort keys %cntB) {
    if (! $cntA{$k}) {
	print "The key \"$k\" appears in $ARGV[1] with count $cntB{$k}, but does NOT appear in $ARGV[0]\n";
	$rd += 1.0;
	++ $nerrs;
    }
}
if ($nerrs > 0) {
    print "\nComparing histograms in $ARGV[0] and $ARGV[1] produced $nerrs error", ($nerrs > 1 ? "s" : ""), "\n";
}
elsif ($rd < 1e-12) {
    print "The relative difference between the histograms in $ARGV[0] and $ARGV[1] is $rd, an acceptable level\n";
}
elsif ($rd < 1e-6) {
    print "The relative difference between the histograms in $ARGV[0] and $ARGV[1] is $rd, this different should be considered high\n";
}
else {
    print "The relative difference between the histograms in $ARGV[0] and $ARGV[1] is $rd, please examine the files for details\n";
}

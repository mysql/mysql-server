#!/usr/bin/perl
# This is a perl script to run through a number of tests to verify the
# functionality of FastBit keyword index.
# It expects two arguments:
# ibis executable
# jrf test data
@K1=("Good", "Not so good", "FiftyFifty", "Run Away");
@K2=("Strong", "Very Strong", "Extremely Strong", "More Vulnerable",
     "Less Vulnerable", "Adequate", "Currently Highly Vulnerable",
     "Currently Vulnerable", "Failed");
@K3=("A", "A-", "A+", "AA", "AA-", "AA+", "AAA", "B", "B-", "B+", "BB", "BB-",
     "BB+", "BBB", "BBB-", "BBB+", "C", "CC", "CCC", "D");
if ($#ARGV < 1) {
    print "Tests based on suggestion from Justo Ruiz Ferrer\n\nusage:\n", $0, " ibis-path jrf-data-dir\n";
    exit;
}
$IBIS = $ARGV[0];
$JRFDIR = $ARGV[1];
$nerr1 = 0;
for ($j = 0; $j <= $#K1; ++ $j) {
    $mesg = `$ARGV[0] -d $ARGV[1] -q "where k1 = \'$K1[$j]\'" 2>&1`;
    if ($mesg =~ /produced (\d+) hit/) {
	$nh1 = $1;
    }
    else {
	print "Warning -- failed to process $ARGV[0] -d $ARGV[1] -q \"where k1 = \'$K1[$j]\'\"\n";
	$nh1 = -1;
	++ $nerr1;
    }
    $mesg = `$ARGV[0] -d $ARGV[1] -q "where jc CONTAINS \'$K1[$j]\'" 2>&1`;
    if ($mesg =~ /produced (\d+) hit/) {
	$nhc = $1;
    }
    else {
	print "Warning -- failed to process $ARGV[0] -d $ARGV[1] -q \"where jc CONTAINS \'$K1[$j]\'\"\n";
	$nhc = -2;
	++ $nerr1;
    }
    $mesg = `$ARGV[0] -d $ARGV[1] -q "where js CONTAINS \'$K1[$j]\'" 2>&1`;
    if ($mesg =~ /produced (\d+) hit/) {
	$nhs = $1;
    }
    else {
	print "Warning -- failed to process $ARGV[0] -d $ARGV[1] -q \"where js CONTAINS \'$K1[$j]\'\"\n";
	$nhs = -3;
	++ $nerr1;
    }
    print "$K1[$j]\t--> $nh1, $nhc, $nhs\n";
    if ($nh1 != $nhc) {
	print "Warning -- nh1 ($nh1) does not match nhc ($nhc)\n";
	++ $nerr1;
    }
    if ($nh1 != $nhs) {
	print "Warning -- nh1 ($nh1) does not match nhs ($nhs)\n";
	++ $nerr1;
    }
    if ($nhs != $nhc) {
	print "Warning -- nhs ($nhs) does not match nhc ($nhc)\n";
	++ $nerr1;
    }
}
if ($nerr1 == 0) {
    print "Successfully passed ", 1+$#K1, " test cases involving K1\n\n";
}
else {
    print "Failed tests involving K1, nerr1 = $nerr1\n\n";
}
$nerr2 = 0;
for ($j = 0; $j <= $#K2; ++ $j) {
    $mesg = `$ARGV[0] -d $ARGV[1] -q "where k2 = \'$K2[$j]\'" 2>&1`;
    if ($mesg =~ /produced (\d+) hit/) {
	$nh2 = $1;
    }
    else {
	print "Warning -- failed to process $ARGV[0] -d $ARGV[1] -q \"where k2 = \'$K2[$j]\'\"\n";
	$nh2 = -1;
	++ $nerr2;
    }
    $mesg = `$ARGV[0] -d $ARGV[1] -q "where jc CONTAINS \'$K2[$j]\'" 2>&1`;
    if ($mesg =~ /produced (\d+) hit/) {
	$nhc = $1;
    }
    else {
	print "Warning -- failed to process $ARGV[0] -d $ARGV[1] -q \"where jc CONTAINS \'$K2[$j]\'\"\n";
	$nhc = -2;
	++ $nerr2;
    }
    $mesg = `$ARGV[0] -d $ARGV[1] -q "where js CONTAINS \'$K2[$j]\'" 2>&1`;
    if ($mesg =~ /produced (\d+) hit/) {
	$nhs = $1;
    }
    else {
	print "Warning -- failed to process $ARGV[0] -d $ARGV[1] -q \"where js CONTAINS \'$K2[$j]\'\"\n";
	$nhs = -3;
	++ $nerr2;
    }
    print "$K2[$j]\t--> $nh2, $nhc, $nhs\n";
    if ($nh2 != $nhc) {
	print "Warning -- nh2 ($nh2) does not match nhc ($nhc)\n";
	++ $nerr2;
    }
    if ($nh2 != $nhs) {
	print "Warning -- nh2 ($nh2) does not match nhs ($nhs)\n";
	++ $nerr2;
    }
    if ($nhs != $nhc) {
	print "Warning -- nhs ($nhs) does not match nhc ($nhc)\n";
	++ $nerr2;
    }
}
if ($nerr2 == 0) {
    print "Successfully passed ", 1+$#K2, " test cases involving K2\n\n";
}
else {
    print "Failed tests involving K2, nerr2 = $nerr2\n\n";
}
$nerr3 = 0;
for ($j = 0; $j <= $#K3; ++ $j) {
    $mesg = `$ARGV[0] -d $ARGV[1] -q "where k3 = \'$K3[$j]\'" 2>&1`;
    if ($mesg =~ /produced (\d+) hit/) {
	$nh3 = $1;
    }
    else {
	print "Warning -- failed to process $ARGV[0] -d $ARGV[1] -q \"where k3 = \'$K3[$j]\'\"\n";
	$nh3 = -1;
	++ $nerr3;
    }
    $mesg = `$ARGV[0] -d $ARGV[1] -q "where jc CONTAINS \'$K3[$j]\'" 2>&1`;
    if ($mesg =~ /produced (\d+) hit/) {
	$nhc = $1;
    }
    else {
	print "Warning -- failed to process $ARGV[0] -d $ARGV[1] -q \"where jc CONTAINS \'$K3[$j]\'\"\n";
	$nhc = -2;
	++ $nerr3;
    }
    $mesg = `$ARGV[0] -d $ARGV[1] -q "where js CONTAINS \'$K3[$j]\'" 2>&1`;
    if ($mesg =~ /produced (\d+) hit/) {
	$nhs = $1;
    }
    else {
	print "Warning -- failed to process $ARGV[0] -d $ARGV[1] -q \"where js CONTAINS \'$K3[$j]\'\"\n";
	$nhs = -3;
	++ $nerr3;
    }
    print "$K3[$j]\t--> $nh3, $nhc, $nhs\n";
    if ($nh3 != $nhc) {
	print "Warning -- nh3 ($nh3) does not match nhc ($nhc)\n";
	++ $nerr3;
    }
    if ($nh3 != $nhs) {
	print "Warning -- nh3 ($nh3) does not match nhs ($nhs)\n";
	++ $nerr3;
    }
    if ($nhs != $nhc) {
	print "Warning -- nhs ($nhs) does not match nhc ($nhc)\n";
	++ $nerr3;
    }
}
if ($nerr3 == 0) {
    print "Successfully passed ", 1+$#K3, " test cases involving K3\n";
}
else {
    print "Failed tests involving K3, nerr3 = $nerr3\n";
}

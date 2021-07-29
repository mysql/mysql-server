#!/usr/bin/perl
# a simple script to print Warning messages along with some selected
# information
while (<>) {
    if (/^index =/) {
	$last = $_;
    }
    elsif (/^Warning /) {
	if ($last) {
	    print "\n", $last;
	    undef $last; # to allow consecutive warnings to be printed together
	}
	print $_;
    }
}

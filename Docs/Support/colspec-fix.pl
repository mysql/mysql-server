#!/usr/bin/perl -w

#
# Script to rewrite colspecs from relative values to absolute values
#

# arjen 2002-03-14 append "cm" specifier to colwidth field.

use strict;

my $table_width  = 12.75;   # cm
my $gutter_width =  0.09;   # cm

my $str = join '', <>;

$str =~ s{([\t ]*(<colspec colwidth=\".+?\" />\s*)+)}
         {&rel2abs($1)}ges;

print STDOUT $str;
exit;

#
# Definitions for helper sub-routines
#

sub msg {
    print STDERR shift, "\n";
}

sub rel2abs {
    my $str = shift;
    my $colnum = 1;
    
    my @widths = ();
    my $total  = 0;
    my $output = '';
    
    $str =~ /^(\s+)/;
    my $ws = $1;
    
    while ($str =~ m/<colspec colwidth="(\d+)\*" \/>/g) {
        $total += $1;
        push @widths, $1;
    }

    my $unit = ($table_width - ($#widths * $gutter_width)) / ($total);

    foreach (@widths) {
        $output .= $ws . '<colspec colnum="'. $colnum .'" colwidth="'. sprintf ("%0.2f", $_ * $unit) .'cm" />' . "\n";
        ++$colnum;
    }
    
    return $output . "\n$ws";
}

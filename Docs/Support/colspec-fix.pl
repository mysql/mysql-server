#!/usr/bin/perl -w

#
# Script to rewrite colspecs from relative values to absolute values
#

# arjen 2002-03-14 append "cm" specifier to colwidth field.

use strict;

my $table_width  = 12.75; # Specify the max width of the table in cm
my $gutter_width =  0.55; # Specify the width of the gutters in cm

my $str = join '', <>; # Push stdin (or file)

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
    my $str           = shift;
    my $colnum        = 1;
    
    my @widths        = ();
    my $total         = 0;
    my $output        = '';
    
    my $gutters;
    my $content_width;
    my $total_width;
    my @num_cache;
    
    $str =~ /^(\s+)/;
    my $ws = $1;
    
    while ($str =~ m/<colspec colwidth="(\d+)\*" \/>/g) {
        $total += $1;
        push @widths, $1;
    }

    msg("!!! WARNING: Total Percent > 100%: $total%") if $total > 100;

    if (! $total) {
        die 'Something bad has happened - the script believes that there are no columns';
    }

    $gutters = $#widths * $gutter_width;
    $content_width = $table_width - $gutters;
    # Don't forget that $#... is the last offset not the count

    foreach (@widths) {
        my $temp = sprintf ("%0.2f", $_/100 * $content_width);
        $total_width += $temp;

        if ($total_width > $content_width) {
            $temp -= $total_width - $content_width;
            msg("!!! WARNING: Column width reduced from " .
                ($temp + ($total_width - $content_width)) . " to $temp !!!");
            $total_width -= $total_width - $content_width;
        }
        
        $output .= $ws . '<colspec colnum="'. $colnum .'" colwidth="'. $temp .'cm" />' . "\n";
        ++$colnum;
        push @num_cache, $temp;
    }
   
    return $output . "\n$ws";
}

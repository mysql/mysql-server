#!/usr/bin/perl

#########################################################################
# Author:  Serge Kozlov							#	
# Date:    09/21/2005							#
# Purpose: The script produces a text file ../data/charset_utf8.txt	#
#	   in UTF8MB3 format. Each line contains one UTF8MB3 character 	#
#	   between 20 and 07FF						#
#########################################################################

genfile("../data/charset_utf8.txt", 33, 2047, "utf8mb3");
genfile(">../data/charset_utf8.txt", 19968, 20479,"utf8mb3");

sub genfile ($$$$)
{
    my $fn = shift;
    my $i_from = shift;
    my $i_to = shift;
    my $typ = shift;
    open F, ">$fn";
    for ($i = $i_from; $i <= $i_to; $i++)
    {	
	if (($i > 32) && ($i < 65533) && ($i != 65279))
	{
	    if ($typ eq "utf8mb3")
	    {
		if ($i < 128) 
		{
		    print F pack("C", $i), "\n";    
		}
		elsif (($i > 127) && ($i < 2048))
		{
		    $b1 = (($i & 1984) >> 6) + 192;
		    $b2 = ($i & 63) + 128;	
		    print F pack("C", $b1);    
		    print F pack("C", $b2), "\n";    
		}
		else
		{
		    $b1 = (($i & 61440) >> 12) + 224;
		    $b2 = (($i & 4032) >> 6) + 128;
		    $b3 = ($i & 63) + 128;
		    print F pack("C", $b1);    
		    print F pack("C", $b2);    
		    print F pack("C", $b3), "\n";    
	        }
	    }
	    elsif ($typ eq "ucs2") 
	    {
		print F pack("C", 0);    
		print F pack("C", $i);    
		print F pack("C", 0), "\n";    
	    }
	}
    }
    close F;
}

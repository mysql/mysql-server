#!/usr/bin/perl -w

# Fix the output of `makeinfo --docbook` version 4.0c
# Convert the broken docbook output to well-formed XML that conforms to the O'Reilly idiom
# See code for detailed comments
# Authors: Arjen Lentz and Zak Greant (original code by Jeremy Cole)

use strict;

my $data  = '';
my @apx   = ();
my $apx   = '';
my @nodes = ();
my $nodes = '';

msg ("-- Post-processing `makeinfo --docbook` output --");
msg ("** Written to work with makeinfo version 4.0c **\n");

msg ("Discarding DTD - not required by subsequent scripts");
# <> is a magic filehandle - either reading lines from stdin or from file(s) specified on the command line
<>;

msg ("Create an XML PI with ISO-8859-1 character encoding");
$data = "<?xml version='1.0' encoding='ISO-8859-1'?>";

msg ("Get the rest of the data");
$data = $data . join "", <>;

msg ("Add missing <bookinfo> and <abstract> opening tags");
# Note the absence of the g (global) pattern modified. This situation can only happen once.
# ...as soon as we find the first instance, we can stop looking.
$data =~ s/<book lang="en">/<book lang="en"><bookinfo><abstract>/;


# arjen 2002-05-01
msg ("Processing docbook-prefix special strings");
$data =~ s/FIXUPmdashFIXUP/\&mdash\;/g;

$data =~ s/FIXUPdoubledashFIXUP/--/g;

$data =~ s/FIXUPstrongFIXUP/<emphasis\ role\=\"bold\">/g;
$data =~ s/FIXUPendstrongFIXUP/<\/emphasis>/g;

$data =~ s/FIXUPemphFIXUP/<emphasis>/g;
$data =~ s/FIXUPendemphFIXUP/<\/emphasis>/g;

$data =~ s/FIXUPfileFIXUP/<filename>/g;
$data =~ s/FIXUPendfileFIXUP/<\/filename>/g;

$data =~ s/FIXUPsampFIXUP/<literal>/g;
$data =~ s/FIXUPendsampFIXUP/<\/literal>/g;


msg ("Removing mailto: from email addresses...");
$data =~ s/mailto://g;

msg ("Removing INFORMALFIGURE...");
$data =~ s{<informalfigure>.+?</informalfigure>}
          {}gs;

msg ("Convert ampersand to XML escape sequence...");
$data =~ s/&(?!\w+;)/&amp;/g;

# arjen 2002-05-01
msg ("Changing (TM) to XML escape sequence...");
$data =~ s/MySQL \(TM\)/MySQL&trade;/g;
$data =~ s{<command>TM</command>}
          {&trade;}g;

# arjen 2002-05-01
msg ("Changing ' -- ' to XML escape sequence...");
$data =~ s/ -- /&mdash;/g;

msg ("Changing @@ to @...");
$data =~ s/@@/@/g;

msg ("Rework references of the notation '<n>'");
# Need to talk to Arjen about what the <n> bits are for
$data =~ s/<(\d)>/[$1]/g;

msg ("Changing '_' to '-' in references...");
$data =~ s{((?:id|linkend)=\".+?\")}
          {&underscore2hyphen($1)}gex;

msg ("Changing ULINK to SYSTEMITEM...");
$data =~ s{<ulink url=\"(.+?)\">\s*</ulink>}
          {<systemitem role=\"url\">$1</systemitem>}gs;

msg ("Adding PARA inside ENTRY...");
$data =~ s{<entry>(.*?)</entry>}
          {<entry><para>$1</para></entry>}gs;

msg ("Fixing spacing problem with titles...");
$data =~ s{(</\w+>)(\w{2,})}
          {$1 $2}gs;

msg ("Adding closing / to XREF and COLSPEC tags...");
$data =~ s{<(xref|colspec) (.+?)>}
          {<$1 $2 />}gs;

# arjen 2002-04-26
msg ("Removing separate target titles from LINKs and make them XREFs...");
$data =~ s{<link (linkend=.+?)>.+?</link>}
          {<xref $1 />}gs;

# Probably need to strip these
msg ('Adding "See " to XREFs that used to be @xref...');
$data =~ s{([.'!)])\s*<xref }
          {$1 See <xref }gs;

msg ('Adding "see " to (XREFs) that used to be (@pxref)...');
$data =~ s{([([,;])(\s*)<xref }
          {$1$2see <xref }gs;

msg ("Making first row in table THEAD...");
$data =~ s{( *)<tbody>(\s*<row>.+?</row>)}
          {$1<thead>$2\n$1</thead>\n$1<tbody>}gs;

msg ("Removing EMPHASIS inside THEAD...");
$data =~ s{<thead>(.+?)</thead>}
          {"<thead>".&strip_tag($1, 'emphasis')."</thead>"}gsex;

msg ("Removing empty PARA...");
$data =~ s{<para>\s*</para>}
          {}gs;

msg ("Removing lf before /PARA in ENTRY...");
$data =~ s{\n(</para></entry>)}
          {$1}gs;

msg ("Removing whitespace before /PARA if not on separate line...");
$data =~ s{(\S+)[\t ]+</para>}
          {$1</para>}g;

msg ("Removing PARA around INDEXTERM if no text in PARA...");
$data =~ s{<para>((?:<indexterm role=\"[^"]+\">(?:<(primary|secondary)>[^>]+</\2>)+?</indexterm>)+?)\s*</para>}
          {$1}gs;

@apx = ("Users", "MySQL Testimonials", "News", "GPL-license", "LGPL-license");

foreach $apx (@apx) {
    msg ("Removing appendix $apx...");
    $data =~ s{<appendix id=\"$apx\">(.+?)</appendix>}
              {}gs;

    # Skip to next appendix regex if the regex did not match anything
    next unless (defined $&);
    
    msg ("...Building list of removed nodes...");
    
    # Split the last bracketed regex match into an array
    # Extract the node names from the tags and push them into an array
    foreach (split "\n", $&) {
        push @nodes, $1 if /<\w+ id=\"(.+?)\">/
    }
}

# 2002-02-22 arjen@mysql.com (added fix " /" to end of regex, to make it match)
msg ("Fixing references to removed nodes...");
# Merge the list of node names into a set of regex alternations
$nodes = join "|", @nodes;

# Find all references to removed nodes and convert them to absolute URLs
$data =~ s{<\w+ linkend="($nodes)" />}
          {&xref2link($1)}ges;

print STDOUT $data;
exit;

#
# Definitions for helper sub-routines
#

sub msg {
    print STDERR "docbook-fixup:", shift, "\n";
}

sub strip_tag($$) {
    (my $str, my $tag) = @_;
    $str =~ s{<$tag>(.+?)</$tag>}{$1}gs;
    return $str;
}

sub underscore2hyphen($) {
    my $str = shift;
    $str =~ tr/_/-/;
    return $str;
}

sub xref2link {
    my $ref = shift;
    $ref =~ tr/ /_/;
    $ref =~ s{^((.)(.).+)$}{$2/$3/$1.html};
    return "http://www.mysql.com/doc/" . $ref;
}

# We might need to encode the high-bit characters to ensure proper representation
# msg ("Converting high-bit characters to entities");
# $data =~ s/([\200-\400])/&get_entity($1)>/gs;
# There is no get_entity function yet - no point writing it til we need it :)

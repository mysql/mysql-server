#!/usr/bin/perl

sub fix_underscore {
  $str = shift;
  $str =~ tr/_/-/;
  return $str;
};

sub strip_emph {
  $str = shift;
  $str =~ s{<emphasis>(.+?)</emphasis>}
           {$1}gs;
  return $str;
};


$data = join "", <STDIN>;

print STDERR "Changing @@ to @...\n";
$data =~ s/@@/@/gs;

print STDERR "Changing '_' to '-' in references...\n";
$data =~ s{id=\"(.+?)\"}
          {"id=\"".&fix_underscore($1)."\""}gsex;
$data =~ s{linkend=\"(.+?)\"}
          {"linkend=\"".&fix_underscore($1)."\""}gsex;

print STDERR "Changing ULINK to SYSTEMITEM...\n";
$data =~ s{<ulink url=\"(.+?)\"></ulink>}
          {<systemitem role=\"url\">$1</systemitem>}gs;

print STDERR "Removing INFORMALFIGURE...\n";
$data =~ s{<informalfigure>(.+?)</informalfigure>}
          {}gs;

print STDERR "Adding PARA inside ENTRY...\n";
$data =~ s{<entry>(.*?)</entry>}
          {<entry><para>$1</para></entry>}gs;

print STDERR "Removing mailto: from email addresses...\n";
$data =~ s{mailto:}
          {}gs;

print STDERR "Fixing spacing problem with titles...\n";
$data =~ s{</(\w+)>(\w{2,})}
          {</$1> $2}gs;

# 2002-02-15 arjen@mysql.com
print STDERR "Adding closing / to XREF...\n";
$data =~ s{<xref (.+?)>}
          {<xref $1 />}gs;

# 2002-01-30 arjen@mysql.com
print STDERR "Removing COLSPEC...\n";
$data =~ s{\n *<colspec colwidth=\"[0-9]+\*\">}
          {}gs;

# 2002-01-31 arjen@mysql.com
print STDERR "Making first row in table THEAD...\n";
$data =~ s{([ ]*)<tbody>\n([ ]*<row>(.+?)</row>)}
          {$1<thead>\n$2\n$1</thead>\n$1<tbody>}gs;

# 2002-01-31 arjen@mysql.com
print STDERR "Removing EMPHASIS inside THEAD...\n";
$data =~ s{<thead>(.+?)</thead>}
          {"<thead>".&strip_emph($1)."</thead>"}gsex;

# 2002-01-31 arjen@mysql.com
print STDERR "Removing lf before /PARA in ENTRY...\n";
$data =~ s{(<entry><para>(.+?))\n(</para></entry>)}
          {$1$3}gs;

# 2002-01-31 arjen@mysql.com (2002-02-15 added \n stuff)
print STDERR "Removing whitespace before /PARA if not on separate line...\n";
$data =~ s{([^\n ])[ ]+</para>}
          {$1</para>}gs;

# 2002-01-31 arjen@mysql.com
print STDERR "Removing empty PARA in ENTRY...\n";
$data =~ s{<entry><para></para></entry>}
          {<entry></entry>}gs;

# 2002-01-31 arjen@mysql.com
print STDERR "Removing PARA around INDEXENTRY if no text in PARA...\n";
$data =~ s{<para>((<indexterm role=\"(cp|fn)\">(<(primary|secondary)>[^<]+?</(primary|secondary)>)+?</indexterm>)+?)[\n]*</para>[\n]*}
          {$1\n}gs;

# -----

@apx = ("Users", "MySQL Testimonials", "News",
        "GPL-license", "LGPL-license");

foreach $apx (@apx) {
  print STDERR "Removing appendix $apx...\n";
  $data =~ s{<appendix id=\"$apx\">(.+?)</appendix>}
            {}gs;

  print STDERR " ... Building list of removed nodes ...\n";
  foreach(split "\n", $&) {
    push @nodes, $2 if(/<(\w+) id=\"(.+?)\">/)
  };
};

print STDERR "Fixing references to removed nodes...\n";
foreach $node (@nodes) {
  $web = $node;
  $web =~ s/[ ]/_/;
  $web = "http://www.mysql.com/doc/" .
         (join "/", (split //, $web)[0..1])."/$web.html";
  print STDERR "$node -> $web\n";
  $data =~ s{<(\w+) linkend=\"$node\">}
            {$web}gs;
};

print STDOUT $data;

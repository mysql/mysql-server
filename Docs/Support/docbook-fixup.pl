#!/usr/bin/perl

sub fix {
  $str = shift;
  $str =~ tr/_/-/;
  return $str;
};

$data = join "", <STDIN>;

print STDERR "Changing @@ to @...\n";
$data =~ s/@@/@/gs;

print STDERR "Changing '_' to '-' in references...\n";
$data =~ s{id=\"(.+?)\"}
          {"id=\"".&fix($1)."\""}gsex;
$data =~ s{linkend=\"(.+?)\"}
          {"linkend=\"".&fix($1)."\""}gsex;

print STDERR "Changing ULINK to SYSTEMITEM...\n";
$data =~ s{<ulink url=\"(.+?)\"></ulink>}
          {<systemitem role=\"url\">$1</systemitem>}gs;

print STDERR "Removing INFORMALFIGURE...\n";
$data =~ s{<informalfigure>(.+?)</informalfigure>}
          {}gs;

print STDERR "Adding PARA inside ENTRY...\n";
$data =~ s{<entry>(.+?)</entry>}
          {<entry><para>$1</para></entry>}gs;

print STDERR "Removing mailto: from email addresses...\n";
$data =~ s{mailto:}
          {}gs;

print STDERR "Fixing spacing problem with titles...\n";
$data =~ s{</(\w+)>(\w{2,})}
          {</$1> $2}gs;

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

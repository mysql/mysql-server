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

@apx = ("Users", "MySQL-customer-usage", "Credits", "News",
        "Porting", "GPL-license", "LGPL-license", "Placeholder");

foreach $apx (@apx) {
  print STDERR "Removing appendix $apx...\n";
  $data =~ s{<appendix id=\"$apx\">(.+?)</appendix>}
            {}gs;
};

print STDOUT $data;

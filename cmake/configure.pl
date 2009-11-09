#!/usr/bin/perl
use Switch;

my $cmakeargs="";
foreach $option (@ARGV)
{
  if (substr ($option, 0, 2) == "--")
  {
    $option = substr($option, 2);
  }
  if($option =~ /help/)
  {
    system("cmake -LH");
    exit(0);
  }
  if($option =~ /with-plugins=/)
  {
    my @plugins= split(/,/, substr($option,13));
    foreach $p (@plugins)
    {
      $p =~ s/-/_/g;
      $cmakeargs = $cmakeargs." -DWITH_".uc($p)."=1";
    }
    next;
  }
  if($option =~ /with-extra-charsets=/)
  {
    my $charsets= substr($option,20);
    $cmakeargs = $cmakeargs." -DWITH_EXTRA_CHARSETS=".$charsets;
    next;
  }
  if($option =~ /without-plugin=/)
  {
    $cmakeargs = $cmakeargs." -DWITHOUT_".uc(substr($option,15))."=1";
    next;
  }
  if($option =~ /with-zlib-dir=bundled/)
  {
    $cmakeargs = $cmakeargs." -DWITH_ZLIB=bundled";
    next;
  }
  if($option =~ /with-zlib-dir=/)
  {
    $cmakeargs = $cmakeargs." -DWITH_ZLIB=system";
    next;
  }
  if($option =~ /with-ssl=/)
  {
    $cmakeargs = $cmakeargs." -DWITH_SSL=bundled";
    next;
  }
  if($option =~ /with-ssl/)
  {
    $cmakeargs = $cmakeargs." -DWITH_SSL=yes";
    next;
  }
  if($option =~ /prefix=/)
  {
    my $cmake_install_prefix= substr($option, 7);
    $cmakeargs = $cmakeargs." -DCMAKE_INSTALL_PREFIX=".$cmake_install_prefix;
    next;
  }
  if ($options =~ /extra-charsets=all/)
  {
    $cmakeargs = $cmakeargs." -DWITH_CHARSETS=all"; 
    next;
  }
  if ($options =~ /extra-charsets=complex/)
  {
    $cmakeargs = $cmakeargs." -DWITH_CHARSETS=complex"; 
    next;
  }
  $option = uc($option);
  $option =~ s/-/_/g;
  $cmakeargs = $cmakeargs." -D".$option."=1";
}
print("configure.pl : calling cmake . $cmakeargs\n");
my $rc = system("cmake . $cmakeargs");
exit($rc);

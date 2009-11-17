#!/usr/bin/perl
use strict;
use Cwd 'abs_path';
use File::Basename;

my $cmakeargs = "";

# Find source root directory
# Assume this script is in <srcroot>/cmake
my $srcdir = dirname(dirname(abs_path($0)));

foreach my $option (@ARGV)
{

  if (substr ($option, 0, 2) == "--")
  {
    $option = substr($option, 2);
  }
  if($option =~ /srcdir/)
  {
    $srcdir = substr($option,7);
    next;
  }
  if($option =~ /help/)
  {
    system("cmake ${srcdir} -LH");
    exit(0);
  }
  if($option =~ /with-plugins=/)
  {
    my @plugins= split(/,/, substr($option,13));
    foreach my $p (@plugins)
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
  if ($option =~ /extra-charsets=all/)
  {
    $cmakeargs = $cmakeargs." -DWITH_CHARSETS=all"; 
    next;
  }
  if ($option =~ /extra-charsets=complex/)
  {
    $cmakeargs = $cmakeargs." -DWITH_CHARSETS=complex"; 
    next;
  }
  $option = uc($option);
  $option =~ s/-/_/g;
  $cmakeargs = $cmakeargs." -D".$option."=1";
}

print("configure.pl : calling cmake $srcdir $cmakeargs\n");
my $rc = system("cmake $srcdir $cmakeargs");
exit($rc);

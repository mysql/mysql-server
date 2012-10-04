#!/usr/bin/perl

# Copyright (c) 2009, 2011, Oracle and/or its affiliates. All rights reserved.
# 
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; version 2 of the License.
# 
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
# 
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA

use strict;
use Cwd 'abs_path';
use File::Basename;

my $cmakeargs = "";

# Find source root directory
# Assume this script is in <srcroot>/cmake
my $srcdir = dirname(dirname(abs_path($0)));
my $cmake_install_prefix="";

# Sets installation directory,  bindir, libdir, libexecdir etc
# the equivalent CMake variables are given without prefix
# e.g if --prefix is /usr and --bindir is /usr/bin
# then cmake variable (INSTALL_BINDIR) must be just "bin"

sub set_installdir 
{
   my($path, $varname) = @_;
   my $prefix_length = length($cmake_install_prefix);
   if (($prefix_length > 0) && (index($path,$cmake_install_prefix) == 0))
   {
      # path is under the prefix, so remove the prefix and maybe following "/"
      $path = substr($path, $prefix_length);
      if(length($path) > 0)
      {
        my $char = substr($path, 0, 1);
        if($char eq "/")
        {
          $path= substr($path, 1);
        }
      }
      if(length($path) > 0)
      {
        $cmakeargs = $cmakeargs." -D".$varname."=".$path;
      }
   }
}

# CMake understands CC and CXX env.variables correctly, if they  contain 1 or 2 tokens
# e.g CXX=g++ and CXX="ccache g++" are ok. However it could have a problem if there
# (recognizing g++) with more tokens ,e.g CXX="ccache g++ --pipe".
# The problem is simply fixed by splitting compiler and flags, e.g
# CXX="ccache g++ --pipe" => CXX=ccache g++ CXXFLAGS=--pipe

sub check_compiler
{
  my ($varname, $flagsvarname) = @_;
  my @tokens = split(/ /,$ENV{$varname});
  if($#tokens >= 2)  
  {
    $ENV{$varname} = $tokens[0]." ".$tokens[1];
    my $flags;

    for(my $i=2; $i<=$#tokens; $i++)
    {
      $flags= $flags." ".$tokens[$i];  
    }
    if(defined $ENV{$flagsvarname})
    {
      $flags = $flags." ".$ENV{$flagsvarname};
    }
    $ENV{$flagsvarname}=$flags;
    print("$varname=$ENV{$varname}\n");
    print("$flagsvarname=$ENV{$flagsvarname}\n");
  }  
}

check_compiler("CC", "CFLAGS");
check_compiler("CXX", "CXXFLAGS");

if(defined $ENV{"CXX"} and $ENV{"CXX"} =~ m/gcc/)
{
  my $old_cxx= $ENV{"CXX"};
  $ENV{"CXX"} =~ s/gcc/g++/;    
  print("configure.pl : switching CXX compiler from $old_cxx to $ENV{CXX}\n");
}

if(defined $ENV{"CXXFLAGS"} and $ENV{"CXXFLAGS"} =~ "-fno-exceptions")
{
  $ENV{"CXXFLAGS"} =~ s/-fno-exceptions//;
  print("configure.pl : stripping off -fno-exceptions CXXFLAGS=$ENV{CXXFLAGS}\n");
}

foreach my $option (@ARGV)
{
  if (substr ($option, 0, 2) eq "--")
  {
    $option = substr($option, 2);
  }
  else
  {
    # This must be environment variable
    my @v  = split('=', $option);
    my $name = shift(@v);
    if(@v)
    {
      $ENV{$name} = join('=', @v);  
    }	
    next;
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
  if($option =~ /with-libevent=/)
  {
    $cmakeargs = $cmakeargs." -DWITH_LIBEVENT=system";
    next;
  }
  if($option =~ /with-libevent/)
  {
    $cmakeargs = $cmakeargs." -DWITH_LIBEVENT=bundled";
    next;
  }
  if($option =~ /with-ssl=/)
  {
    $cmakeargs = $cmakeargs." -DWITH_SSL=yes";
    next;
  }
  if($option =~ /with-ssl/)
  {
    $cmakeargs = $cmakeargs." -DWITH_SSL=bundled";
    next;
  }
  if($option =~ /prefix=/)
  {
    $cmake_install_prefix= substr($option, 7);
    $cmakeargs = $cmakeargs." -DCMAKE_INSTALL_PREFIX=".$cmake_install_prefix;
    next;
  }
  if($option =~/bindir=/)
  {
    set_installdir(substr($option,7), "INSTALL_BINDIR");
    next;
  }
  if($option =~/libdir=/)
  {
    set_installdir(substr($option,7), "INSTALL_LIBDIR");
    next;
  }
  if($option =~/libexecdir=/)
  {
    set_installdir(substr($option,11), "INSTALL_SBINDIR");
    next;
  }
  if($option =~/includedir=/)
  {
    set_installdir(substr($option,11), "INSTALL_INCLUDEDIR");
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
  if ($option =~ /localstatedir=/)
  {
    $cmakeargs = $cmakeargs." -DMYSQL_DATADIR=".substr($option,14); 
    next;
  }
  if ($option =~ /with-comment=/)
  {
    $cmakeargs = $cmakeargs." \"-DWITH_COMMENT=".substr($option,13)."\""; 
    next;
  }
  if ($option =~ /mysql-maintainer-mode/)
  {
    $cmakeargs = $cmakeargs." -DMYSQL_MAINTAINER_MODE=" .
                 ($option =~ /enable/ ? "1" : "0");
    next;
  }
  if ($option =~ /with-comment=/)
  {
    $cmakeargs = $cmakeargs." \"-DWITH_COMMENT=".substr($option,13)."\""; 
    next;
  }
#ifndef MCP_NDB_BUILD_INTEGRATION
  if ($option =~ /with-classpath=/)
  {
    $cmakeargs = $cmakeargs." \"-DWITH_CLASSPATH=".substr($option,15)."\"";
    next;
  }
  if ($option =~ /with-debug=/)
  {
    $cmakeargs = $cmakeargs." -DWITH_DEBUG=1";
    next;
  }
  if ($option =~ /with-ndb-ccflags=/)
  {
    $cmakeargs = $cmakeargs." \"-DWITH_NDB_CCFLAGS=".substr($option,17)."\"";
    next;
  }
  if ($option =~ /cmake-args=/)
  {
    $cmakeargs = $cmakeargs." ".substr($option,11);
    next;
  }
#endif
  if ($option =~ /with-gcov/)
  {
      $cmakeargs = $cmakeargs." -DENABLE_GCOV=ON"; 
      next;
  }
  if ($option =~ /with-client-ldflags/)
  {
      print("configure.pl : ignoring $option\n");
      next;
  }
  if ($option =~ /with-mysqld-ldflags=/)
  {
      print("configure.pl : ignoring $option\n");
      next;
  }

  $option = uc($option);
  $option =~ s/-/_/g;
  $cmakeargs = $cmakeargs." -D".$option."=1";
}

print("configure.pl : calling cmake $srcdir $cmakeargs\n");
unlink("CMakeCache.txt");
my $rc = system("cmake $srcdir $cmakeargs");
exit($rc);

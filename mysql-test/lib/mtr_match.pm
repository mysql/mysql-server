# -*- cperl -*-
# Copyright (C) 2004-2006 MySQL AB
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
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

# This is a library file used by the Perl version of mysql-test-run,
# and is part of the translation of the Bourne shell script with the
# same name.

package mtr_match;
use strict;

use base qw(Exporter);
our @EXPORT= qw(mtr_match_prefix
                mtr_match_extension
                mtr_match_substring);

#
# Match a prefix and return what is after the prefix
#
sub mtr_match_prefix ($$) {
  my $string= shift;
  my $prefix= shift;

  if ( $string =~ /^\Q$prefix\E(.*)$/ ) # strncmp
  {
    return $1;
  }
  else
  {
    return undef;		# NULL
  }
}


#
# Match extension and return the name without extension
#
sub mtr_match_extension ($$) {
  my $file= shift;
  my $ext=  shift;

  if ( $file =~ /^(.*)\.\Q$ext\E$/ ) # strchr+strcmp or something
  {
    return $1;
  }
  else
  {
    return undef;                       # NULL
  }
}


#
# Match a substring anywere in a string
#
sub mtr_match_substring ($$) {
  my $string= shift;
  my $substring= shift;

  if ( $string =~ /(.*)\Q$substring\E(.*)$/ ) # strncmp
  {
    return $1;
  }
  else
  {
    return undef;		# NULL
  }
}


sub mtr_match_any_exact ($$) {
  my $string= shift;
  my $mlist=  shift;

  foreach my $m (@$mlist)
  {
    if ( $string eq $m )
    {
      return 1;
    }
  }
  return 0;
}

1;

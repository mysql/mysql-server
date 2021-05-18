# -*- cperl -*-
# Copyright (c) 2005, 2018, Oracle and/or its affiliates. All rights reserved.
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License, version 2.0,
# as published by the Free Software Foundation.
#
# This program is also distributed with certain software (including
# but not limited to OpenSSL) that is licensed under separate terms,
# as designated in a particular file or component or in included license
# documentation.  The authors of MySQL hereby grant you an additional
# permission to link the program and your derivative works with the
# separately licensed software that they have included with MySQL.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License, version 2.0, for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA

# This is a library file used by the Perl version of mysql-test-run,
# and is part of the translation of the Bourne shell script with the
# same name.

package mtr_match;

use strict;

use base qw(Exporter);
our @EXPORT = qw(mtr_match_prefix mtr_match_extension mtr_match_substring);

# Match a prefix and return what is after the prefix
sub mtr_match_prefix ($$) {
  my $string = shift;
  my $prefix = shift;

  if ($string =~ /^\Q$prefix\E(.*)$/)    # strncmp
  {
    return $1;
  } else {
    return undef;                        # NULL
  }
}

# Match extension and return the name without extension
sub mtr_match_extension ($$) {
  my $file = shift;
  my $ext  = shift;

  # strchr+strcmp or something
  if ($file =~ /^(.*)\.\Q$ext\E$/) {
    return $1;
  } else {
    # NULL
    return undef;
  }
}

# Match a substring anywere in a string
sub mtr_match_substring ($$) {
  my $string    = shift;
  my $substring = shift;

  if ($string =~ /(.*)\Q$substring\E(.*)$/) {
    return $1;
  } else {
    # NULL
    return undef;
  }
}

sub mtr_match_any_exact ($$) {
  my $string = shift;
  my $mlist  = shift;

  foreach my $m (@$mlist) {
    return 1 if ($string eq $m);
  }

  return 0;
}

1;

# -*- cperl -*-
# Copyright (C) 2008 MySQL AB
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


#
# One test
#
package My::Test;

use strict;
use warnings;
use Carp;
use Storable();


sub new {
  my $class= shift;
  my $self= bless {
		   @_,
		  }, $class;
  return $self;
}

sub fullname {
  my ($self)= @_;
  $self->{name} . (defined $self->{combination}
                   ? " '$self->{combination}'"
                   : "")
}

#
# Return a unique key that can be used to
# identify this test in a hash
#
sub key {
  my ($self)= @_;
  return $self->{key};
}


sub is_failed {
  my ($self)= @_;
  my $result= $self->{result};
  croak "'is_failed' can't be called until test has been run!"
    unless defined $result;

  return ($result eq 'MTR_RES_FAILED');
}


sub write_test {
  my ($test, $sock, $header)= @_;

  # Give the test a unique key before serializing it
  $test->{key}= "$test" unless defined $test->{key};

  my $serialized= Storable::freeze($test);
  $serialized =~ s/([\x0d\x0a\\])/sprintf("\\%02x", ord($1))/eg;
  print $sock $header, "\n", $serialized, "\n";
}


sub read_test {
  my ($sock)= @_;
  my $serialized= <$sock>;
  chomp($serialized);
  $serialized =~ s/\\([0-9a-fA-F]{2})/chr(hex($1))/eg;
  my $test= Storable::thaw($serialized);
  die "wrong class (hack attempt?)"
    unless ref($test) eq 'My::Test';
  return $test;
}


1;

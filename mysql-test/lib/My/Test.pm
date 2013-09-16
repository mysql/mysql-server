# -*- cperl -*-
# Copyright (c) 2008, 2011, Oracle and/or its affiliates. All rights reserved.
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
use mtr_results;


sub new {
  my $class= shift;
  my $self= bless {
		   @_,
		  }, $class;
  return $self;
}

sub copy {
  my $self= shift;
  my $copy= My::Test->new();
  while (my ($key, $value) = each(%$self)) {
    if (ref $value eq "ARRAY") {
      $copy->{$key} = [ @$value ];
    } else {
      $copy->{$key}= $value;
    }
  }
  $copy;
}

sub fullname {
  my ($self)= @_;
  $self->{name} . (defined $self->{combinations}
                   ? " '" . join(',', sort @{$self->{combinations}}) . "'"
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


my %result_names= (
		   'MTR_RES_PASSED'   =>  'pass',
		   'MTR_RES_FAILED'   =>  'fail',
		   'MTR_RES_SKIPPED'  =>  'skipped',
		  );

sub write_test {
  my ($test, $sock, $header)= @_;

  if ($::opt_resfile && defined $test->{'result'}) {
    resfile_test_info("result", $result_names{$test->{'result'}});
    if ($test->{'timeout'}) {
      resfile_test_info("comment", "Timeout");
    } elsif (defined $test->{'comment'}) {
      resfile_test_info("comment", $test->{'comment'});
    }
    resfile_test_info("result", "warning") if defined $test->{'check'};
    resfile_to_test($test);
  }

  # Give the test a unique key before serializing it
  $test->{key}= "$test" unless defined $test->{key};

  my $serialized= Storable::freeze($test);
  $serialized =~ s/([\x0d\x0a\\])/sprintf("\\%02x", ord($1))/eg;
  send $sock,$header. "\n". $serialized. "\n", 0;
}


sub read_test {
  my ($sock)= @_;
  my $serialized= <$sock>;
  chomp($serialized);
  $serialized =~ s/\\([0-9a-fA-F]{2})/chr(hex($1))/eg;
  my $test= Storable::thaw($serialized);
  use Data::Dumper;
  die "wrong class (hack attempt?): ".ref($test)."\n".Dumper(\$test, $serialized)
    unless ref($test) eq 'My::Test';
  resfile_from_test($test) if $::opt_resfile;
  return $test;
}

1;

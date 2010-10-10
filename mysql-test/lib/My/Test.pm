# -*- cperl -*-


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

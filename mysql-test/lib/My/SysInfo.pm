# -*- cperl -*-
# Copyright (c) 2008, 2013, Oracle and/or its affiliates. All rights reserved.
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


package My::SysInfo;

use strict;
use Carp;
use My::Platform;

use constant DEFAULT_BOGO_MIPS => 2000;

sub _cpuinfo {
  my ($self)= @_;

  my $info_file= "/proc/cpuinfo";
  if ( !(  -e $info_file and -f $info_file) ) {
    return undef;
  }

  my $F= IO::File->new($info_file) or return undef;

  # Set input separator to blank line
  local $/ = '';

  while ( my $cpu_chunk= <$F>) {
    chomp($cpu_chunk);

    my $cpuinfo = {};

    foreach my $cpuline ( split(/\n/, $cpu_chunk) ) {
      my ( $attribute, $value ) = split(/\s*:\s*/, $cpuline);

      $attribute =~ s/\s+/_/;
      $attribute = lc($attribute);

      if ( $value =~ /^(no|not available|yes)$/ ) {
	$value = $value eq 'yes' ? 1 : 0;
      }

      if ( $attribute eq 'flags' ) {
	@{ $cpuinfo->{flags} } = split / /, $value;
      } else {
	$cpuinfo->{$attribute} = $value;
      }
    }

    # Make sure bogomips is set to some value
    $cpuinfo->{bogomips} ||= DEFAULT_BOGO_MIPS;

    # Cpus reported once, but with 'cpu_count' set to the actual number
    my $cpu_count= $cpuinfo->{cpu_count} || 1;
    for(1..$cpu_count){
      push(@{$self->{cpus}}, $cpuinfo);
    }
  }
  $F= undef; # Close file
  return $self;
}


sub _kstat {
  my ($self)= @_;
  while (1){
    my $instance_num= $self->{cpus} ? @{$self->{cpus}} : 0;
    my $list= `kstat -p -m cpu_info -i $instance_num 2> /dev/null`;
    my @lines= split('\n', $list) or last; # Break loop

    my $cpuinfo= {};
    foreach my $line (@lines)
    {
      my ($module, $instance, $name, $statistic, $value)=
	$line=~ /(\w*):(\w*):(\w*):(\w*)\t(.*)/;

      $cpuinfo->{$statistic}= $value;
    }

    # Default value, the actual cpu values can be used to decrease this
    # on slower cpus
    $cpuinfo->{bogomips}= DEFAULT_BOGO_MIPS;

    push(@{$self->{cpus}}, $cpuinfo);
  }

  # At least one cpu should have been found
  # if this method worked
  if ( $self->{cpus} ) {
    return $self;
  }
  return undef;
}


sub _unamex {
  my ($self)= @_;
  # TODO
  return undef;
}


sub new {
  my ($class)= @_;


  my $self= bless {
		   cpus => (),
		  }, $class;

  my @info_methods =
    (
     \&_cpuinfo,
     \&_kstat,
     \&_unamex,
   );

  # Detect virtual machines
  my $isvm= 0;

  if (IS_WINDOWS) {
    # Detect vmware service
    $isvm= `tasklist` =~ /vmwareservice/i;
  }
  $self->{isvm}= $isvm;

  foreach my $method (@info_methods){
    if ($method->($self)){
      return $self;
    }
  }

  # Push a dummy cpu
  push(@{$self->{cpus}},
     {
      bogomips => DEFAULT_BOGO_MIPS,
      model_name => "unknown",
     });

  return $self;
}


# Return the list of cpus found
sub cpus {
  my ($self)= @_;
  return @{$self->{cpus}} or
    confess "INTERNAL ERROR: No cpus in list";
}


# Return the number of cpus found
sub num_cpus {
  my ($self)= @_;
  return int(@{$self->{cpus}}) or
    confess "INTERNAL ERROR: No cpus in list";
}


# Return the smallest bogomips value amongst the processors
sub min_bogomips {
  my ($self)= @_;

  my $bogomips;

  foreach my $cpu (@{$self->{cpus}}) {
    if (!defined $bogomips or $bogomips > $cpu->{bogomips}) {
      $bogomips= $cpu->{bogomips};
    }
  }

  return $bogomips;
}

sub isvm {
  my ($self)= @_;

  return $self->{isvm};
}


# Prit the cpuinfo
sub print_info {
  my ($self)= @_;

  foreach my $cpu (@{$self->{cpus}}) {
    while ((my ($key, $value)) = each(%$cpu)) {
      print " ", $key, "= ";
      if (ref $value eq "ARRAY") {
	print "[", join(", ", @$value), "]";
      } else {
	print $value;
      }
      print "\n";
    }
    print "\n";
  }
}

1;

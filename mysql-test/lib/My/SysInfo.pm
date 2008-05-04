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


package My::SysInfo;

use strict;
use Carp;
use My::Platform;



sub _cpuinfo {
  my ($self)= @_;
  print "_cpuinfo\n";

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

    push(@{$self->{cpus}}, $cpuinfo);
  }
  $F= undef; # Close file
  return $self;
}


sub _kstat {
  my ($self)= @_;
  while (1){
    my $instance_num= $self->{cpus} ? @{$self->{cpus}} : 0;
    my $list= `kstat -p -m cpu_info -i $instance_num`;
    my @lines= split('\n', $list) or return undef;

    my $cpuinfo= {};
    foreach my $line (@lines)
    {
      my ($module, $instance, $name, $statistic, $value)=
	$line=~ /(\w*):(\w*):(\w*):(\w*)\t(.*)/;

      $cpuinfo->{$statistic}= $value;
    }

    # Default value, the actual cpu values can be used to decrease it
    # on slower cpus
    $cpuinfo->{bogomips}= 2000;

    push(@{$self->{cpus}}, $cpuinfo);
  }

  return $self;
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

  foreach my $method (@info_methods){
    if ($method->($self)){
      return $self;
    }
  }

  # Push a dummy cpu
  push(@{$self->{cpus}}, {bogomips => 2000, model_name => "unknown"});

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

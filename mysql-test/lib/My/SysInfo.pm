# -*- cperl -*-
# Copyright (c) 2008, 2023, Oracle and/or its affiliates.
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

package My::SysInfo;

use strict;
use Carp;

use My::Platform;

# Use 'nproc' command to get the number of CPUs if available
sub _nproc {
  my ($self) = @_;

  my $null_dev = (IS_WINDOWS) ? 'nul' : '/dev/null';
  my $ncpu = `nproc 2> $null_dev`;
  chomp($ncpu);

  if ($ncpu ne '') {
    for (1 .. $ncpu) {
      my $cpuinfo->{processor} = $_;
      push(@{ $self->{cpus} }, $cpuinfo);
    }
    return $self;
  }
  return undef;
}

sub _cpuinfo {
  my ($self) = @_;

  my $info_file = "/proc/cpuinfo";
  if (!(-e $info_file and -f $info_file)) {
    return undef;
  }

  my $F = IO::File->new($info_file) or return undef;

  # Set input separator to blank line
  local $/ = '';

  while (my $cpu_chunk = <$F>) {
    chomp($cpu_chunk);

    my $cpuinfo = {};
    foreach my $cpuline (split(/\n/, $cpu_chunk)) {
      my ($attribute, $value) = split(/\s*:\s*/, $cpuline);

      $attribute =~ s/\s+/_/;
      $attribute = lc($attribute);

      if ($value =~ /^(no|not available|yes)$/) {
        $value = $value eq 'yes' ? 1 : 0;
      }

      if ($attribute eq 'flags') {
        @{ $cpuinfo->{flags} } = split / /, $value;
      } else {
        $cpuinfo->{$attribute} = $value;
      }
    }

    # Cpus reported once, but with 'cpu_count' set to the actual number
    my $cpu_count = $cpuinfo->{cpu_count} || 1;
    for (1 .. $cpu_count) {
      push(@{ $self->{cpus} }, $cpuinfo);
    }
  }

  # Close the file
  $F = undef;
  return $self;
}

sub _kstat {
  my ($self) = @_;
  while (1) {
    my $instance_num = $self->{cpus} ? @{ $self->{cpus} } : 0;
    my $null_dev     = (IS_WINDOWS)  ? 'nul'              : '/dev/null';
    my $list         = `kstat -p -m cpu_info -i $instance_num 2> $null_dev`;
    my @lines = split('\n', $list) or last;    # Break loop

    my $cpuinfo = {};
    foreach my $line (@lines) {
      my ($module, $instance, $name, $statistic, $value) =
        $line =~ /(\w*):(\w*):(\w*):(\w*)\t(.*)/;

      $cpuinfo->{$statistic} = $value;
    }

    push(@{ $self->{cpus} }, $cpuinfo);
  }

  # At least one cpu should have been found if this method worked.
  if ($self->{cpus}) {
    return $self;
  }

  return undef;
}

sub _sysctl {
  my ($self) = @_;
  my $null_dev = (IS_WINDOWS) ? 'nul' : '/dev/null';
  my $ncpu = `sysctl hw.ncpu 2> $null_dev`;
  if ($ncpu eq '') {
    return undef;
  }

  $ncpu =~ s/\D//g;
  my $list =
    `sysctl machdep.cpu 2> $null_dev | grep machdep\.cpu\.[^.]*: 2> $null_dev`;
  my @lines = split('\n', $list);

  my $cpuinfo = {};
  foreach my $line (@lines) {
    my ($statistic, $value) = $line =~ /machdep\.cpu\.(.*):\s+(.*)/;
    $cpuinfo->{$statistic} = $value;
  }

  for (1 .. $ncpu) {
    my $temp_cpuinfo = $cpuinfo;
    $temp_cpuinfo->{processor} = $_;
    push(@{ $self->{cpus} }, $temp_cpuinfo);
  }

  # At least one cpu should have been found if this method worked
  if ($self->{cpus}) {
    return $self;
  }

  return undef;
}

sub _unamex {
  my ($self) = @_;
  # TODO
  return undef;
}

sub new {
  my ($class) = @_;
  my $self = bless { cpus => (), }, $class;

  my @info_methods = (\&_nproc, \&_cpuinfo, \&_kstat, \&_sysctl, \&_unamex,);

  # Detect virtual machines
  my $isvm = 0;

  if (IS_WINDOWS) {
    # Detect vmware service
    $isvm = `tasklist` =~ /vmwareservice/i;
  }
  $self->{isvm} = $isvm;

  foreach my $method (@info_methods) {
    return $self if ($method->($self));
  }

  # Push a dummy cpu
  push(@{ $self->{cpus} }, { model_name => "unknown", });

  return $self;
}

# Return the list of cpus found
sub cpus {
  my ($self) = @_;
  return @{ $self->{cpus} } or
    confess "INTERNAL ERROR: No cpus in list";
}

# Return the number of cpus found
sub num_cpus {
  my ($self) = @_;
  if (IS_WINDOWS) {
    return $ENV{NUMBER_OF_PROCESSORS} || 1;
  }
  return int(@{ $self->{cpus} }) or
    confess "INTERNAL ERROR: No cpus in list";
}

sub isvm {
  my ($self) = @_;
  return $self->{isvm};
}

# Print the cpuinfo
sub print_info {
  my ($self) = @_;

  foreach my $cpu (@{ $self->{cpus} }) {
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

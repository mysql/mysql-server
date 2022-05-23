# -*- cperl -*-

# Copyright (c) 2007, 2022, Oracle and/or its affiliates.
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

package My::Config::Option;

use strict;
use warnings;
use mtr_report;
use Carp;

sub new {
  my ($class, $option_name, $option_value) = @_;
  my $self = bless({ name  => $option_name,
                     value => $option_value
                   },
                   $class);
  return $self;
}

sub name {
  my ($self) = @_;
  return $self->{name};
}

sub value {
  my ($self) = @_;
  return $self->{value};
}

sub option {
  my ($self) = @_;
  my $name   = $self->{name};
  my $value  = $self->{value};

  if ($name =~ /^--/) {
    mtr_error("Options in a config file must not begin with --");
  }

  my $opt;
  if (defined $value) {
    $opt = "--$name=$value";
  } else {
    $opt = "--$name";
  }

  return $opt;
}

package My::Config::Group;

use strict;
use warnings;
use Carp;

sub new {
  my ($class, $group_name) = @_;
  my $self = bless { name            => $group_name,
                     options         => [],
                     options_by_name => {},
  }, $class;
  return $self;
}

sub insert {
  my ($self, $option_name, $value, $if_not_exist) = @_;
  my $option = $self->option($option_name);

  if (defined($option) and !$if_not_exist) {
    $option->{value} = $value;
  } else {
    my $option = My::Config::Option->new($option_name, $value);
    # Insert option in list
    push(@{ $self->{options} }, $option);
    # Insert option in hash
    $self->{options_by_name}->{$option_name} = $option;
  }

  return $option;
}

sub remove {
  my ($self, $option_name) = @_;

  # Check that option exists
  my $option = $self->option($option_name);
  return undef unless defined $option;

  # Remove from the hash
  delete($self->{options_by_name}->{$option_name}) or croak;

  # Remove from the array
  @{ $self->{options} } =
    grep { $_->name ne $option_name } @{ $self->{options} };

  return $option;
}

sub options {
  my ($self) = @_;
  return @{ $self->{options} };
}

sub name {
  my ($self) = @_;
  return $self->{name};
}

sub suffix {
  my ($self) = @_;
  # Everything in name from the last .
  my @parts = split(/\./, $self->{name});
  my $suffix = pop(@parts);
  return ".$suffix";
}

sub after {
  my ($self, $prefix) = @_;
  die unless defined $prefix;

  # Everything after $prefix
  my $name = $self->{name};
  if ($name =~ /^\Q$prefix\E(.*)$/) {
    return $1;
  }
  die "Failed to extract the value after '$prefix' in $name";
}

sub split {
  my ($self) = @_;
  # Return an array with name parts
  return split(/\./, $self->{name});
}

# Return a specific option in the group
sub option {
  my ($self, $option_name) = @_;
  return $self->{options_by_name}->{$option_name};
}

# Return value for an option in the group, fail if it does not exist.
sub value {
  my ($self, $option_name) = @_;
  my $option = $self->option($option_name);

  croak "No option named '$option_name' in group '$self->{name}'"
    if !defined($option);

  return $option->value();
}

# Return value for an option if it exist
sub if_exist {
  my ($self, $option_name) = @_;
  my $option = $self->option($option_name);
  return undef if !defined($option);
  return $option->value();
}

package My::Config;

use strict;
use warnings;
use Carp;
use IO::File;
use File::Basename;

# Constructor for My::Config, represents a my.cnf config file.
sub new {
  my ($class, $path) = @_;
  my $group_name = undef;

  my $self = bless { groups => [] }, $class;
  my $F = IO::File->new($path, "<") or croak "Could not open '$path': $!";

  while (my $line = <$F>) {
    chomp($line);
    # Remove any trailing CR from Windows edited files
    $line =~ s/\cM$//;

    # [group]
    if ($line =~ /^\[(.*)\]/) {
      # New group found
      $group_name = $1;
      $self->insert($group_name, undef, undef);
    }

    # Magic #! option (#!name=value)
    elsif ($line =~ /^(#\![\@\w-]+)\s*=\s*(.*?)\s*$/) {
      my $option = $1;
      my $value  = $2;
      croak "Found option '$option=$value' outside of group"
        unless $group_name;
      $self->insert($group_name, $option, $value);
    }

    # Magic #! comments
    elsif ($line =~ /^#\!/) {
      my $magic = $line;
      croak "Found magic comment '$magic' outside of group"
        unless $group_name;

      $self->insert($group_name, $magic, undef);
    }

    # Comments
    elsif ($line =~ /^#/ || $line =~ /^;/) {
      # Skip comment
      next;
    }

    # Empty lines
    elsif ($line =~ /^$/) {
      # Skip empty lines
      next;
    }

    # !include <filename>
    elsif ($line =~ /^\!include\s*(.*?)\s*$/) {
      my $include_file_name = dirname($path) . "/" . $1;

      # Check that the file exists relative to path of first config file
      if (!-f $include_file_name) {
        # Try to include file relativ to current dir
        $include_file_name = $1;
      }
      croak "The include file '$include_file_name' does not exist"
        unless -f $include_file_name;

      $self->append(My::Config->new($include_file_name));
    }

    # <option>
    elsif ($line =~ /^([\@\w-]+)\s*$/) {
      my $option = $1;
      croak "Found option '$option' outside of group"
        unless $group_name;

      $self->insert($group_name, $option, undef);
    }

    # initialize=<option>=<value>
    elsif ($line =~ /^initialize(\s*=\s*|\s+)([\@\w-]+)\s*=\s*(.*?)\s*$/) {
      my $option = $2;
      my $value  = $3;

      croak "Found option '$option=$value' outside of group"
        unless $group_name;

      $self->insert($group_name, "initialize=--" . $option, $value);
    }

    # <option>=<value>
    elsif ($line =~ /^([\@\w-]+)\s*=\s*(.*?)\s*$/) {
      my $option = $1;
      my $value  = $2;
      croak "Found option '$option=$value' outside of group"
        unless $group_name;

      $self->insert($group_name, $option, $value);
    } else {
      croak "Unexpected line '$line' found in '$path'";
    }
  }

  # Close the file
  undef $F;
  return $self;
}

# Insert a new group if it does not already exist
# and add option if defined.
sub insert {
  my ($self, $group_name, $option, $value, $if_not_exist) = @_;
  my $group;

  # Create empty array for the group if it doesn't exist
  if (!$self->group_exists($group_name)) {
    $group = $self->_group_insert($group_name);
  } else {
    $group = $self->group($group_name);
  }

  if (defined $option) {
    # Add the option to the group
    $group->insert($option, $value, $if_not_exist);
  }
  return $group;
}

# Remove a option, given group and option name
sub remove {
  my ($self, $group_name, $option_name) = @_;
  my $group = $self->group($group_name);

  croak "group '$group_name' does not exist"
    unless defined($group);

  $group->remove($option_name) or
    croak "option '$option_name' does not exist";
}

# Check if group with given name exists in config
sub group_exists {
  my ($self, $group_name) = @_;
  foreach my $group ($self->groups()) {
    return 1 if $group->{name} eq $group_name;
  }
  return 0;
}

# Insert a new group into config
sub _group_insert {
  my ($self, $group_name) = @_;
  caller eq __PACKAGE__ or croak;

  # Check that group does not already exist
  croak "Group already exists" if $self->group_exists($group_name);

  my $group = My::Config::Group->new($group_name);
  push(@{ $self->{groups} }, $group);
  return $group;
}

# Append a configuration to current config
sub append {
  my ($self, $from) = @_;

  foreach my $group ($from->groups()) {
    foreach my $option ($group->options()) {
      $self->insert($group->name(), $option->name(), $option->value());
    }

  }
}

# Return a list with all the groups in config
sub groups {
  my ($self) = @_;
  return (@{ $self->{groups} });
}

# Return a list of all the groups in config starting with
# the given string.
sub like {
  my ($self, $prefix) = @_;
  return (grep ($_->{name} =~ /^$prefix/, $self->groups()));
}

# Return the first group in config starting with
# the given string.
sub first_like {
  my ($self, $prefix) = @_;
  return ($self->like($prefix))[0];
}

# Return a specific group in the config
sub group {
  my ($self, $group_name) = @_;
  foreach my $group ($self->groups()) {
    return $group if $group->{name} eq $group_name;
  }
  return undef;
}

# Return a list of all options in a specific group in the config
sub options_in_group {
  my ($self, $group_name) = @_;
  my $group = $self->group($group_name);
  return () unless defined $group;
  return $group->options();
}

# Return a value given group and option name
sub value {
  my ($self, $group_name, $option_name) = @_;
  my $group = $self->group($group_name);
  croak "group '$group_name' does not exist"
    unless defined($group);

  my $option = $group->option($option_name);
  croak "option '$option_name' does not exist"
    unless defined($option);

  return $option->value();
}

# Check if an option exists
sub exists {
  my ($self, $group_name, $option_name) = @_;
  my $group = $self->group($group_name);
  croak "group '$group_name' does not exist"
    unless defined($group);

  my $option = $group->option($option_name);
  return defined($option);
}

# Overload "to string"-operator with 'stringify'
use overload '""' => \&stringify;

# Return the config as a string in my.cnf file format
sub stringify {
  my ($self) = @_;
  my $res;

  foreach my $group ($self->groups()) {
    $res .= "[$group->{name}]\n";

    foreach my $option ($group->options()) {
      $res = $res . $option->name();
      my $value = $option->value();
      if (defined $value) {
        $res = $res . "=$value";
      }
      $res = $res . "\n";
    }
    $res = $res . "\n";
  }
  return $res;
}

# Save the config to named file
sub save {
  my ($self, $path) = @_;
  my $F = IO::File->new($path, ">") or croak "Could not open '$path': $!";
  print $F $self;
  # Close the file
  undef $F;
}

1;

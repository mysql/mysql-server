# -*- cperl -*-

# Copyright (c) 2008 Sun Microsystems, Inc.
# Use is subject to license terms.
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

package My::Config::Option;

use strict;
use warnings;


sub new {
  my ($class, $option_name, $option_value)= @_;
  my $self= bless { name => $option_name,
		    value => $option_value
		  }, $class;
  return $self;
}


sub name {
  my ($self)= @_;
  return $self->{name};
}


sub value {
  my ($self)= @_;
  return $self->{value};
}


package My::Config::Group;

use strict;
use warnings;


sub new {
  my ($class, $group_name)= @_;
  my $self= bless { name => $group_name,
		    options => [],
		    options_by_name => {},
		  }, $class;
  return $self;
}


sub insert {
  my ($self, $option_name, $value, $if_not_exist)= @_;
  my $option= $self->option($option_name);
  if (defined($option) and !$if_not_exist) {
    $option->{value}= $value;
  }
  else {
    my $option= My::Config::Option->new($option_name, $value);
    # Insert option in list
    push(@{$self->{options}}, $option);
    # Insert option in hash
    $self->{options_by_name}->{$option_name}= $option;
  }
  return $option;
}

sub remove {
  my ($self, $option_name)= @_;

  # Check that option exists
  my $option= $self->option($option_name);

  return undef unless defined $option;

  # Remove from the hash
  delete($self->{options_by_name}->{$option_name}) or die;

  # Remove from the array
  @{$self->{options}}= grep { $_->name ne $option_name } @{$self->{options}};

  return $option;
}


sub options {
  my ($self)= @_;
  return @{$self->{options}};
}


sub name {
  my ($self)= @_;
  return $self->{name};
}


#
# Return a specific option in the group
#
sub option {
  my ($self, $option_name)= @_;

  return $self->{options_by_name}->{$option_name};
}


#
# Return a specific value for an option in the group
#
sub value {
  my ($self, $option_name)= @_;
  my $option= $self->option($option_name);

  die "No option named '$option_name' in this group"
    if ! defined($option);

  return $option->value();
}


package My::Config;

use strict;
use warnings;
use IO::File;
use File::Basename;

#
# Constructor for My::Config
# - represents a my.cnf config file
#
# Array of arrays
#
sub new {
  my ($class, $path)= @_;
  my $group_name= undef;

  my $self= bless { groups => [] }, $class;
  my $F= IO::File->new($path, "<")
    or die "Could not open '$path': $!";

  while (  my $line= <$F> ) {
    chomp($line);

    # [group]
    if ( $line =~ /\[(.*)\]/ ) {
      # New group found
      $group_name= $1;
      #print "group: $group_name\n";

      $self->insert($group_name, undef, undef);
    }

    # Magic #! comments
    elsif ( $line =~ /^#\!/) {
      my $magic= $line;
      die "Found magic comment '$magic' outside of group"
	unless $group_name;

      #print "$magic\n";
      $self->insert($group_name, $magic, undef);
    }

    # Comments
    elsif ( $line =~ /^#/ || $line =~ /^;/) {
      # Skip comment
      next;
    }

    # Empty lines
    elsif ( $line =~ /^$/ ) {
      # Skip empty lines
      next;
    }

    # !include <filename>
    elsif ( $line =~ /^\!include\s*(.*?)\s*$/ ) {
      my $include_file_name= dirname($path)."/".$1;
      # Check that the file exists
      die "The include file '$include_file_name' does not exist"
	unless -f $include_file_name;

      $self->append(My::Config->new($include_file_name));
    }

    # <option>
    elsif ( $line =~ /^([\@\w-]+)\s*$/ ) {
      my $option= $1;

      die "Found option '$option' outside of group"
	unless $group_name;

      #print "$option\n";
      $self->insert($group_name, $option, undef);
    }

    # <option>=<value>
    elsif ( $line =~ /^([\@\w-]+)\s*=\s*(.*?)\s*$/ ) {
      my $option= $1;
      my $value= $2;

      die "Found option '$option=$value' outside of group"
	unless $group_name;

      #print "$option=$value\n";
      $self->insert($group_name, $option, $value);
    } else {
      die "Unexpected line '$line' found in '$path'";
    }

  }
  undef $F;			# Close the file

  return $self;
}

#
# Insert a new group if it does not already exist
# and add option if defined
#
sub insert {
  my ($self, $group_name, $option, $value, $if_not_exist)= @_;
  my $group;

  # Create empty array for the group if it doesn't exist
  if ( !$self->group_exists($group_name) ) {
    $group= $self->_group_insert($group_name);
  }
  else {
    $group= $self->group($group_name);
  }

  if ( defined $option ) {
    #print "option: $option, value: $value\n";

    # Add the option to the group
    $group->insert($option, $value, $if_not_exist);
  }
}

#
# Remove a option, given group and option name
#
sub remove {
  my ($self, $group_name, $option_name)= @_;
  my $group= $self->group($group_name);

  die "group '$group_name' does not exist"
    unless defined($group);

  $group->remove($option_name) or
    die "option '$option_name' does not exist";
}



#
# Check if group with given name exists in config
#
sub group_exists {
  my ($self, $group_name)= @_;

  foreach my $group ($self->groups()) {
    return 1 if $group->{name} eq $group_name;
  }
  return 0;
}


#
# Insert a new group into config
#
sub _group_insert {
  my ($self, $group_name)= @_;
  caller eq __PACKAGE__ or die;

  # Check that group does not already exist
  die "Group already exists" if $self->group_exists($group_name);

  my $group= My::Config::Group->new($group_name);
  push(@{$self->{groups}}, $group);
  return $group;
}


#
# Append a configuration to current config
#
sub append {
  my ($self, $from)= @_;

  foreach my $group ($from->groups()) {
    foreach my $option ($group->options()) {
      $self->insert($group->name(), $option->name(), $option->value());
    }

  }
}


#
# Return a list with all the groups in config
#
sub groups {
  my ($self)= @_;
  return ( @{$self->{groups}} );
}


#
# Return a list of all the groups in config
# starting with the given string
#
sub like {
  my ($self, $prefix)= @_;
  return ( grep ( $_->{name} =~ /^$prefix/, $self->groups()) );
}


#
# Return the first group in config
# starting with the given string
#
sub first_like {
  my ($self, $prefix)= @_;
  return ($self->like($prefix))[0];
}


#
# Return a specific group in the config
#
sub group {
  my ($self, $group_name)= @_;

  foreach my $group ( $self->groups() ) {
    return $group if $group->{name} eq $group_name;
  }
  return undef;
}


#
# Return a list of all options in a specific group in the config
#
sub options_in_group {
  my ($self, $group_name)= @_;

  my $group= $self->group($group_name);
  return () unless defined $group;
  return $group->options();
}


#
# Return a value given group and option name
#
sub value {
  my ($self, $group_name, $option_name)= @_;
  my $group= $self->group($group_name);

  die "group '$group_name' does not exist"
    unless defined($group);

  my $option= $group->option($option_name);
  die "option '$option_name' does not exist"
    unless defined($option);

  return $option->value();
}


#
# Check if an option exists
#
sub exists {
  my ($self, $group_name, $option_name)= @_;
  my $group= $self->group($group_name);

  die "group '$group_name' does not exist"
    unless defined($group);

  my $option= $group->option($option_name);
  return defined($option);
}


# Overload "to string"-operator with 'stringify'
use overload
    '""' => \&stringify;

#
# Return the config as a string in my.cnf file format
#
sub stringify {
  my ($self)= @_;
  my $res;

  foreach my $group ($self->groups()) {
    $res .= "[$group->{name}]\n";

    foreach my $option ($group->options()) {
      $res .= $option->name();
      my $value= $option->value();
      if (defined $value) {
	$res .= "=$value";
      }
      $res .= "\n";
    }
    $res .= "\n";
  }
  return $res;
}


#
# Save the config to named file
#
sub save {
    my ($self, $path)= @_;
    my $F= IO::File->new($path, ">")
	or die "Could not open '$path': $!";
    print $F $self;
    undef $F; # Close the file
}

1;

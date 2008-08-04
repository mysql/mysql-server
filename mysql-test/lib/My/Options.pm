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


package My::Options;

#
# Utility functions to work with list of options
#

use strict;


sub same($$) {
  my $l1= shift;
  my $l2= shift;
  return compare($l1,$l2) == 0;
}


sub compare ($$) {
  my $l1= shift;
  my $l2= shift;

  my @l1= @$l1;
  my @l2= @$l2;

  return -1 if @l1 < @l2;
  return  1 if @l1 > @l2;

  while ( @l1 )                         # Same length
  {
    my $e1= shift @l1;
    my $e2= shift @l2;
    my $cmp= ($e1 cmp $e2);
    return $cmp if $cmp != 0;
  }

  return 0;                             # They are the same
}


sub _split_option {
  my ($option)= @_;
  if ($option=~ /^--(.*)=(.*)$/){
    return ($1, $2);
  }
  elsif ($option=~ /^--(.*)$/){
    return ($1, undef)
  }
  elsif ($option=~ /^\$(.*)$/){ # $VAR
    return ($1, undef)
  }
  elsif ($option=~ /^(.*)=(.*)$/){
    return ($1, $2)
  }
  elsif ($option=~ /^-O$/){
    return (undef, undef);
  }
  die "Unknown option format '$option'";
}


sub _build_option {
  my ($name, $value)= @_;
  if ($name =~ /^O, /){
    return "-".$name."=".$value;
  }
  elsif ($value){
    return "--".$name."=".$value;
  }
  return "--".$name;
}


#
# Compare two list of options and return what would need
# to be done to get the server running with the new settings
#
sub diff {
  my ($from_opts, $to_opts)= @_;

  my %from;
  foreach my $from (@$from_opts)
  {
    my ($opt, $value)= _split_option($from);
    next unless defined($opt);
    $from{$opt}= $value;
  }

  #print "from: ", %from, "\n";

  my %to;
  foreach my $to (@$to_opts)
  {
    my ($opt, $value)= _split_option($to);
    next unless defined($opt);
    $to{$opt}= $value;
  }

  #print "to: ", %to, "\n";

  # Remove the ones that are in both lists
  foreach my $name (keys %from){
    if (exists $to{$name} and $to{$name} eq $from{$name}){
      #print "removing '$name'  from both lists\n";
      delete $to{$name};
      delete $from{$name};
    }
  }

  #print "from: ", %from, "\n";
  #print "to: ", %to, "\n";

  # Add all keys in "to" to result
  my @result;
  foreach my $name (keys %to){
    push(@result, _build_option($name, $to{$name}));
  }

  # Add all keys in "from" that are not in "to"
  # to result as "set to default"
  foreach my $name (keys %from){
    if (not exists $to{$name}) {
      push(@result, _build_option($name, "default"));
    }
  }

  return @result;
}


sub is_set {
  my ($opts, $set_opts)= @_;

  foreach my $opt (@$opts){

    my ($opt_name1, $value1)= _split_option($opt);

    foreach my $set_opt (@$set_opts){
      my ($opt_name2, $value2)= _split_option($set_opt);

      if ($opt_name1 eq $opt_name2){
	# Option already set
	return 1;
      }
    }
  }

  return 0;
}


sub toSQL {
  my (@options)= @_;
  my @sql;

  foreach my $option (@options) {
    my ($name, $value)= _split_option($option);
    #print "name: $name\n";
    #print "value: $value\n";
    if ($name =~ /^O, (.*)/){
      push(@sql, "SET GLOBAL $1=$value");
    }
    elsif ($name =~ /^set-variable=(.*)/){
      push(@sql, "SET GLOBAL $1=$value");
    }
    else {
      my $sql_name= $name;
      $sql_name=~ s/-/_/g;
      push(@sql, "SET GLOBAL $sql_name=$value");
    }
  }
  return join("; ", @sql);
}


sub toStr {
  my $name= shift;
  return "$name: ",
    "['", join("', '", @_), "']\n";
}


1;


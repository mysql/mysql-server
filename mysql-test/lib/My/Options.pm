# -*- cperl -*-
# Copyright (c) 2008, 2016, Oracle and/or its affiliates. All rights reserved.
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

      if (option_equals($opt_name1,$opt_name2)){
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
    my ($sql_name, $value)= _split_option($option);
    #print "name: $sql_name\n";
    #print "value: $value\n";
    $sql_name=~ s/-/_/g;
    push(@sql, "SET GLOBAL $sql_name=$value");
  }
  return join("; ", @sql);
}


sub toStr {
  my $name= shift;
  return "$name: ",
    "['", join("', '", @_), "']\n";
}


sub option_equals {
  my ($string1, $string2) = @_;

  $string1 =~ s/_/-/g;
  $string2 =~ s/_/-/g;

  return ( $string1 eq $string2 );
}

1;


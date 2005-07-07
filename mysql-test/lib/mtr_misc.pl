# -*- cperl -*-

# This is a library file used by the Perl version of mysql-test-run,
# and is part of the translation of the Bourne shell script with the
# same name.

use strict;

sub mtr_full_hostname ();
sub mtr_init_args ($);
sub mtr_add_arg ($$);
sub mtr_path_exists(@);
sub mtr_script_exists(@);
sub mtr_exe_exists(@);

##############################################################################
#
#  Misc
#
##############################################################################

# We want the fully qualified host name and hostname() may have returned
# only the short name. So we use the resolver to find out.

sub mtr_full_hostname () {

  my $hostname=  hostname();
  if ( $hostname !~ /\./ )
  {
    my $address=   gethostbyname($hostname)
      or mtr_error("Couldn't resolve $hostname : $!");
    my $fullname=  gethostbyaddr($address, AF_INET);
    $hostname= $fullname if $fullname; 
  }
  return $hostname;
}

# FIXME move to own lib

sub mtr_init_args ($) {
  my $args = shift;
  $$args = [];                            # Empty list
}

sub mtr_add_arg ($$) {
  my $args=   shift;
  my $format= shift;
  my @fargs = @_;

  push(@$args, sprintf($format, @fargs));
}

##############################################################################

sub mtr_path_exists (@) {
  foreach my $path ( @_ )
  {
    return $path if -e $path;
  }
  if ( @_ == 1 )
  {
    mtr_error("Could not find $_[0]");
  }
  else
  {
    mtr_error("Could not find any of " . join(" ", @_));
  }
}

sub mtr_script_exists (@) {
  foreach my $path ( @_ )
  {
    return $path if -x $path;
  }
  if ( @_ == 1 )
  {
    mtr_error("Could not find $_[0]");
  }
  else
  {
    mtr_error("Could not find any of " . join(" ", @_));
  }
}

sub mtr_exe_exists (@) {
  my @path= @_;
  map {$_.= ".exe"} @path if $::glob_win32;
  foreach my $path ( @path )
  {
    return $path if -x $path;
  }
  if ( @path == 1 )
  {
    mtr_error("Could not find $path[0]");
  }
  else
  {
    mtr_error("Could not find any of " . join(" ", @path));
  }
}


1;

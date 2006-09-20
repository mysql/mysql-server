# -*- cperl -*-

# This is a library file used by the Perl version of mysql-test-run,
# and is part of the translation of the Bourne shell script with the
# same name.

use strict;

sub mtr_full_hostname ();
sub mtr_short_hostname ();
sub mtr_init_args ($);
sub mtr_add_arg ($$);
sub mtr_path_exists(@);
sub mtr_script_exists(@);
sub mtr_exe_exists(@);
sub mtr_copy_dir($$);
sub mtr_same_opts($$);
sub mtr_cmp_opts($$);

##############################################################################
#
#  Misc
#
##############################################################################

# We want the fully qualified host name and hostname() may have returned
# only the short name. So we use the resolver to find out.
# Note that this might fail on some platforms

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

sub mtr_short_hostname () {

  my $hostname=  hostname();
  $hostname =~ s/\..+$//;
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
    if($::glob_win32)
    {
      return $path if -f $path;
    }
    else
    {
      return $path if -x $path;
    }
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
    if($::glob_win32)
    {
      return $path if -f $path;
    }
    else
    {
      return $path if -x $path;
    }
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

sub mtr_copy_dir($$) {
  my $srcdir= shift;
  my $dstdir= shift;

  # Create destination directory
  mkpath($dstdir);
  find(\&mtr_copy_one_file, $dstdir);
}

sub mtr_copy_one_file {
  print $File::Find::name, "\n";
}

sub mtr_same_opts ($$) {
  my $l1= shift;
  my $l2= shift;
  return mtr_cmp_opts($l1,$l2) == 0;
}

sub mtr_cmp_opts ($$) {
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

1;

# -*- cperl -*-

# This is a library file used by the Perl version of mysql-test-run,
# and is part of the translation of the Bourne shell script with the
# same name.

use strict;

sub mtr_match_prefix ($$);
sub mtr_match_extension ($$);
sub mtr_match_any_exact ($$);

##############################################################################
#
#  
#
##############################################################################

# Match a prefix and return what is after the prefix

sub mtr_match_prefix ($$) {
  my $string= shift;
  my $prefix= shift;

  if ( $string =~ /^\Q$prefix\E(.*)$/ ) # strncmp
  {
    return $1;
  }
  else
  {
    return undef;		# NULL
  }
}


# Match extension and return the name without extension

sub mtr_match_extension ($$) {
  my $file= shift;
  my $ext=  shift;

  if ( $file =~ /^(.*)\.\Q$ext\E$/ ) # strchr+strcmp or something
  {
    return $1;
  }
  else
  {
    return undef;                       # NULL
  }
}


# Match a substring anywere in a string

sub mtr_match_substring ($$) {
  my $string= shift;
  my $substring= shift;

  if ( $string =~ /(.*)\Q$substring\E(.*)$/ ) # strncmp
  {
    return $1;
  }
  else
  {
    return undef;		# NULL
  }
}


sub mtr_match_any_exact ($$) {
  my $string= shift;
  my $mlist=  shift;

  foreach my $m (@$mlist)
  {
    if ( $string eq $m )
    {
      return 1;
    }
  }
  return 0;
}

1;

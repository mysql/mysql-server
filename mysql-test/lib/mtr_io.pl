# -*- cperl -*-

# This is a library file used by the Perl version of mysql-test-run,
# and is part of the translation of the Bourne shell script with the
# same name.

use strict;

sub mtr_get_pid_from_file ($);
sub mtr_get_opts_from_file ($);
sub mtr_tofile ($@);
sub mtr_tonewfile($@);

##############################################################################
#
#  
#
##############################################################################

sub mtr_get_pid_from_file ($) {
  my $file=  shift;

  open(FILE,"<",$file) or mtr_error("can't open file \"$file\": $!");
  my $pid=  <FILE>;
  chomp($pid);
  close FILE;
  return $pid;
}

sub mtr_get_opts_from_file ($) {
  my $file=  shift;

  open(FILE,"<",$file) or mtr_error("can't open file \"$file\": $!");
  my @args;
  while ( <FILE> )
  {
    chomp;
    s/\$MYSQL_TEST_DIR/$::glob_mysql_test_dir/g;
    push(@args, split(' ', $_));
  }
  close FILE;
  return \@args;
}

sub mtr_fromfile ($) {
  my $file=  shift;

  open(FILE,"<",$file) or mtr_error("can't open file \"$file\": $!");
  my $text= join('', <FILE>);
  close FILE;
  return $text;
}

sub mtr_tofile ($@) {
  my $file=  shift;

  open(FILE,">>",$file) or mtr_error("can't open file \"$file\": $!");
  print FILE join("", @_);
  close FILE;
}

sub mtr_tonewfile ($@) {
  my $file=  shift;

  open(FILE,">",$file) or mtr_error("can't open file \"$file\": $!");
  print FILE join("", @_);
  close FILE;
}


1;

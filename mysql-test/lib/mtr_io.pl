# -*- cperl -*-

# This is a library file used by the Perl version of mysql-test-run,
# and is part of the translation of the Bourne shell script with the
# same name.

use strict;

sub mtr_get_pid_from_file ($);
sub mtr_get_opts_from_file ($);
sub mtr_fromfile ($);
sub mtr_tofile ($@);
sub mtr_tonewfile($@);

##############################################################################
#
#  
#
##############################################################################

sub mtr_get_pid_from_file ($) {
  my $pid_file_path=  shift;
  my $TOTAL_ATTEMPTS= 30;
  my $timeout= 1;

  # We should read from the file until we get correct pid. As it is
  # stated in BUG#21884, pid file can be empty at some moment. So, we should
  # read it until we get valid data.

  for (my $cur_attempt= 1; $cur_attempt <= $TOTAL_ATTEMPTS; ++$cur_attempt)
  {
    mtr_debug("Reading pid file '$pid_file_path' " .
              "($cur_attempt of $TOTAL_ATTEMPTS)...");

    open(FILE, '<', $pid_file_path)
      or mtr_error("can't open file \"$pid_file_path\": $!");

    my $pid= <FILE>;

    chomp($pid) if defined $pid;

    close FILE;

    return $pid if defined $pid && $pid ne '';

    mtr_debug("Pid file '$pid_file_path' is empty. " .
              "Sleeping $timeout second(s)...");

    sleep(1);
  }

  mtr_error("Pid file '$pid_file_path' is corrupted. " .
            "Can not retrieve PID in " .
            ($timeout * $TOTAL_ATTEMPTS) . " seconds.");
}

sub mtr_get_opts_from_file ($) {
  my $file=  shift;

  open(FILE,"<",$file) or mtr_error("can't open file \"$file\": $!");
  my @args;
  while ( <FILE> )
  {
    chomp;

    #    --set-variable=init_connect=set @a='a\\0c'
    s/^\s+//;                           # Remove leading space
    s/\s+$//;                           # Remove ending space

    # This is strange, but we need to fill whitespace inside
    # quotes with something, to remove later. We do this to
    # be able to split on space. Else, we have trouble with
    # options like 
    #
    #   --someopt="--insideopt1 --insideopt2"
    #
    # But still with this, we are not 100% sure it is right,
    # we need a shell to do it right.

#    print STDERR "\n";
#    print STDERR "AAA: $_\n";

    s/\'([^\'\"]*)\'/unspace($1,"\x0a")/ge;
    s/\"([^\'\"]*)\"/unspace($1,"\x0b")/ge;
    s/\'([^\'\"]*)\'/unspace($1,"\x0a")/ge;
    s/\"([^\'\"]*)\"/unspace($1,"\x0b")/ge;

#    print STDERR "BBB: $_\n";

#    foreach my $arg (/(--?\w.*?)(?=\s+--?\w|$)/)

    # FIXME ENV vars should be expanded!!!!

    foreach my $arg (split(/[ \t]+/))
    {
      $arg =~ tr/\x11\x0a\x0b/ \'\"/;     # Put back real chars
      # The outermost quotes has to go
      $arg =~ s/^([^\'\"]*)\'(.*)\'([^\'\"]*)$/$1$2$3/
        or $arg =~ s/^([^\'\"]*)\"(.*)\"([^\'\"]*)$/$1$2$3/;
      $arg =~ s/\\\\/\\/g;

      $arg =~ s/\$\{(\w+)\}/envsubst($1)/ge;
      $arg =~ s/\$(\w+)/envsubst($1)/ge;

#      print STDERR "ARG: $arg\n";
      push(@args, $arg);
    }
  }
  close FILE;
  return \@args;
}

sub envsubst {
  my $string= shift;

  if ( ! defined $ENV{$string} )
  {
    mtr_error("opt file referense \$$string that is unknown");
  }

  return $ENV{$string};
}

sub unspace {
  my $string= shift;
  my $quote=  shift;
  $string =~ s/[ \t]/\x11/g;
  return "$quote$string$quote";
}

sub mtr_fromfile ($) {
  my $file=  shift;

  open(FILE,"<",$file) or mtr_error("can't open file \"$file\": $!");
  my $text= join('', <FILE>);
  close FILE;
  $text =~ s/^\s+//;                    # Remove starting space, incl newlines
  $text =~ s/\s+$//;                    # Remove ending space, incl newlines
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

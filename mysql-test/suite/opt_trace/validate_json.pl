#!/usr/bin/perl
use strict;
use JSON;
use File::Spec::Functions qw/ canonpath /;
my $usage = "This is from WL#5257 \"first API for optimizer trace\".

Usage:
  %s [-q] <a_file> <another_file> <etc>

    -q      quiet mode: only display errors and warnings.

It will verify that all optimizer traces of files (usually a_file
is a .result or .reject file which contains
SELECT * FROM OPTIMIZER_TRACE; ) are JSON-compliant, and that
they contain no duplicates keys.
Exit code is 0 if all ok.";
my $retcode = 0;
my @ignored;
my @input = @ARGV;

# Filter out "-q" options
@input = grep {!/-q/} @input;

if (!@input)
{
  print "$usage\n";
  exit 1;
}

# If command line contains at least one "-q" option, it is quiet mode
my $quiet= scalar(@input) <= scalar(@ARGV) -1;
# On Windows, command line arguments specified using wildcards need to be evaluated.
# On Unix too if the arguments are passed with single quotes.
my $need_parse = grep(/\*/,@input);
if ($need_parse)
{
  my $platform_independent_dir;
  $platform_independent_dir= canonpath "@input";
  @input= glob "$platform_independent_dir";
}

foreach my $input_file (@input)
{
  handle_one_file($input_file);
  print "\n";
}

if ( @ignored )
{
  print STDERR "These files have been ignored:\n";
  foreach my $ig ( @ignored )
  {
    print "$ig\n";
  }
  print "\n";
}
if ( $retcode )
{
  print STDERR "There are errors\n";
}

else
{
  print "\n";
  print "ALL OK\n";
}

exit $retcode;

sub handle_one_file {

  my ( $input_file ) = @_;
  if ( $input_file =~ /^.*(ctype_.*|mysqldump)\.result/ )
  {
    push @ignored ,$input_file;
    return;
  }
  print "FILE $input_file\n";
  print "\n";
  open(DATA,"<$input_file") or die "Can't open file";
  my @lines = <DATA>;
  close(DATA);
  my $first_trace_line = 0;
  my $trace_line = 0;
  my @trace = undef;
  label_to: foreach my $i ( @lines )
  {
    $trace_line = $trace_line + 1;
    if (( grep(/^.*(\t)?{\n/,$i) ) and ( $first_trace_line == 0 ))
    {
      @trace = undef;
      $first_trace_line = $trace_line;
      push @trace, "{\n";
      next label_to;
    }
    if (( $i =~ /^}/ ) and ( $first_trace_line != 0))
    {
      push @trace, "}";
      check($first_trace_line,@trace);
      $first_trace_line = 0;
    }
    if ( $first_trace_line != 0 )
    {
      # Eliminate /* */ from end_marker=on (not valid JSON)
      $i =~ s/\/\*.*\*\// /g;
      push @trace, $i;
    }

  }
}


sub check {

  my ( $first_trace_line, @trace ) = @_;
  my $string = join("", @trace);
  my $parsed;
  eval { $parsed = decode_json($string); };
  unless ( $parsed )
  {
    print "Parse error at line: $first_trace_line\n";
    my $error = $@;
    print "Error: $@\n";
    # If there is a character position specified, put a mark ('&') in front of this character
    if ($error =~ /invalid character.*at character offset (\d+)/)
    {
      substr($string,$1,0) = "&";
      print "$string\n";
    }
    else
    {
      print "$string\n";
    }
    $retcode = 1;
    print "\n";
    return;
  }
  # Detect non-unique keys in one object, by counting
  # number of quote symbols ("): the json module outputs only
  # one of the non-unique keys, making the number of "
  # smaller compared to the input string.

  my $before = $string =~ tr/'"'//;
  my $re_json;
  $re_json= to_json($parsed);
  my $after = $re_json =~ tr/'"'//;
  if ( $before != $after )
  {
    print "Non-unique keys at line $first_trace_line ( $before vs $after )\n";
    print "$string\n";
    $retcode = 1;
    print "\n";
    return;
  }
  if ( !$quiet )
  {
    print "OK at line $first_trace_line\n";
  }
}

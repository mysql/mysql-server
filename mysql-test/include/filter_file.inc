# ==== Purpose ====
#
# Read the contents of a file, filter it through a perl script, and
# write it back.
#
# This is useful in conjunction with include/write_result_to_file.inc
# and cat_file or include/read_file_to_var.inc.  See
# e.g. include/show_events.inc for an example.
#
# ==== Usage ====
#
# --let $input_file= <FILE_NAME>
# [--let $output_file= <FILE_NAME>]
# --let $script= <PERL_SCRIPT>
# [--let $select_columns= <LIST OF NUMBERS>]
# [--let $pre_script= <PERL_SCRIPT>]
# [--let $rpl_debug= 1]
# --source include/filter_file.inc
#
# Parameters:
#
#   $input_file
#     File to read from.
#
#   $output_file
#     File to write to. If omitted, writes to $input_file.
#
#   $script
#     This script will be executed once for each line in $input_file.
#
#     When the script starts, the perl variable $_ will be set to the
#     current row (including the terminating newline).  The script can
#     modify $_ in any way it likes, and the result will be appended
#     to $output_file.  It is even possible to remove a row by setting
#     $_ to '', or to generate extra rows by appending "\n" to $_.
#
#     Since mysqltest is incapable of properly escaping dollar
#     characters, you have to replace any '$' in your script by
#     'DOLLAR' (otherwise mysqltest would try to interpolate parts of
#     your script).  filter_file.inc will replace 'DOLLAR' by '$'
#     before evaluating your script.
#
#   $select_columns
#     For convenience, if you set this to a space-separated list of
#     numbers, it will print only the numbered columns, in the given
#     order.
#
#   $pre_script
#     This script will be evaluated before starting to iterate over
#     the lines of $input_file.  It can be useful if you need some
#     sort of initialization; for example, you can define a subroutine
#     here and call it from $script.
#
#   $rpl_debug
#     If set, verbose debug info is printed.

--let $include_filename= filter_file.inc
--source include/begin_include_file.inc

if ($rpl_debug)
{
  --echo pre_script='$pre_script'
  --echo script='$script'
  --echo select_columns='$select_columns'
  --echo input_file='$input_file' output_file='$output_file'
}

--let _FF_PRE_SCRIPT= $pre_script
--let _FF_SCRIPT= $script
--let _FF_INPUT_FILE= $input_file
--let _FF_OUTPUT_FILE= $output_file
--let _FF_SELECT_COLUMNS= $select_columns
--let _FF_DEBUG= $rpl_debug
if (!$output_file)
{
  --let _FF_OUTPUT_FILE= $input_file
}
perl;
  my $pre_script = $ENV{'_FF_PRE_SCRIPT'};
  $pre_script =~ s/DOLLAR/\$/g;
  my $script = $ENV{'_FF_SCRIPT'};
  $script =~ s/DOLLAR/\$/g;
  my $input_file = $ENV{'_FF_INPUT_FILE'};
  my $output_file = $ENV{'_FF_OUTPUT_FILE'};
  my $select_columns = $ENV{'_FF_SELECT_COLUMNS'};
  my $debug = $ENV{'_FF_DEBUG'};
  if ($select_columns)
  {
    chomp($select_columns);
    $select_columns =~ s/[, ]+/,/g;
    $script = '
    chomp;
    my @cols = split(/\t/, $_);
    $_ = join("\t", map { $cols[$_ - 1] } ('.$select_columns.'))."\n";
    ' . $script;
  }
  unless ($keep_quotes)
  {
    $pre_script = 'my %unquote = ("n"=>"\n","t"=>"\t","\\\\"=>"\\\\");' . $pre_script;
    $script .= 's{\\\\(.)}{$unquote{$1}}ge;';
  }
  if ($debug)
  {
    $script = 'print "BEFORE:\'$_\'";' . $script . 'print "AFTER:\'$_\'";'
  }
  # Generate a script (perl is faster if we avoid many calls to eval).
  my $full_script =
'
  open FILE, "< $input_file" or die "Error opening $input_file: $!";
  my $filtered_contents = "";
  my %column_names = ();
  '.$pre_script.';
  while (<FILE>)
  {
    chomp;
    s/\015?\012?$//;
    if (!%column_names)
    {
      my $n = 1;
      %column_names = map { $_ => $n++ } split(/\t/, $_);
    }
    else
    {
      ' . $script . '
    }

    if (length($_) > 0)
    {
      $filtered_contents .= $_."\n";
    }
  }
  close FILE or die "Error closing $input_file: $!";
  open FILE, "> $output_file" or die "Error opening $output_file: $!";
  binmode FILE;
  print FILE $filtered_contents or die "Error writing filtered contents to $output_file: $!";
  close FILE or die "Error closing $output_file: $!";
  return 0;
';
  if ($debug)
  {
    print STDOUT "full_script=<<END_OF_SCRIPT\n${full_script}END_OF_SCRIPT\n"
  }
  my $eval_ret = eval($full_script);
  defined($eval_ret) or die "Parse error or 'die' invoked when evaluating perl script '$full_script': $@";
  $eval_ret == 0 or die "Non-zero exit value $eval_ret from script '$script'";
EOF

--let $include_filename= filter_file.inc
--source include/end_include_file.inc

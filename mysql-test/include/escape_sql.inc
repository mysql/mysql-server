# ==== Purpose ====
#
# Escape occurences of " or ' in a string, in order to be used inside
# an SQL statement.
#
# ==== Usage ====
#
# --let $escape_string= String
# [--let $enclosing_char= "|' (default is ")]
# --source include/escape_sql.inc
# --echo $escape_string
#
# Parameters:
#   $escape_string
#     String to search for occurences of the $enclosing_char and
#     escape them according to SQL syntax. It is also the output
#     parameter.

#   $enclosing_char
#     The character that will be used to enclose $escape_string when
#     included in an SQL statement.
#

if (!$escape_string)
{
  --die Error: set $escape_string before sourcing escape_sql.inc
}

if ($escape_string != '')
{
  --let $_enclosing_char= "
  if ($enclosing_char)
  {
    --let $_enclosing_char= $enclosing_char
  }


  --let $_output_filename= `SELECT UUID()`
  --let $_output_filename= $MYSQLTEST_VARDIR/tmp/$_output_filename
  --let full_output_filename= $_output_filename
  --let to_replace= $escape_string
  --let used_quote= $_enclosing_char

  perl;
    my $out= $ENV{'full_output_filename'};
    my $to_replace= $ENV{'to_replace'};
    my $quote= $ENV{'used_quote'};

    $to_replace =~ s/$quote/$quote$quote/g;

    open(FILE, ">", $out) or die "Error $? opening $output_filename: $!";
    print FILE $to_replace or die "Error $? writing to $output_filename: $!";
    close FILE or die "Error $? closing $output_filename: $!";
    chmod 0777, $out;
  EOF

  --let $escape_string= `SELECT LOAD_FILE('$_output_filename')`
}
# Cleanup
--remove_file $_output_filename

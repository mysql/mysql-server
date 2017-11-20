# ==== Purpose ====
#
# Run mysqlbinlog -v, capture the output, find the pseudo-SQL of
# decoded row events, convert it to correct SQL syntax, and store the
# result in $variable.
#
# ==== Usage ====
#
# --let $columns= NAME[/TYPE] [NAME[/TYPE] [NAME[/TYPE]...]]
# --let $mysqlbinlog_options= <FILE_AND_OPTIONS>
# [--let $mysqlbinlog_positions_from_sql_variables= 1]
# --source include/get_mysqlbinlog_decoded_rows.inc
# --echo Decoded row events: $decoded
# --echo SQL statements: $sql
#
# Parameters:
#   $columns
#     The column names of the table, in positional order, separated by
#     a space, and possibly with a /TYPE (e.g. /JSON) appended to it.
#     The column name is needed because the pseudo-SQL only contains
#     placeholders like @3, which this script has to map to column
#     names.  The type is needed because this script generates a WHERE
#     clause from the before-image, and values of e.g. JSON columns
#     are decoded into strings, so in order to match any row at all
#     the script needs to write the column value as CAST(<decoded
#     string> AS JSON).  Hence the type information.  Type information
#     can be omitted for columns where mysqlbinlog already outputs a
#     literal of the same type as the column, for instance strings or
#     numbers.
#
#   $mysqlbinlog_positions_from_sql_variables
#     If set, uses --start-position=X and binlog filename from
#     the SQL variables @binlog_position and @binlog_fullpath
#     (settable using include/save_binlog_position.inc)
#
#   $mysqlbinlog_options
#     Options given to mysqlbinlog, including the binlog filename.
#     The -v option is added automatically.
#
# Output:
#   $output_file
#     File containing the SQL statements printed by mysqlbinlog.
#
#   $decoded_sql
#     The pseudo-SQL statements printed by mysqlbinlog, converted to
#     correct SQL.

--let $include_filename= get_mysqlbinlog_decoded_rows.inc
--source include/begin_include_file.inc

--let GMDR_POSITIONS=
if ($mysqlbinlog_positions_from_sql_variables)
{
  --let GMDR_POSITIONS= `SELECT CONCAT('--start-position=', @binlog_position, ' ', @binlog_fullpath)`
}

--let GMDR_COLUMNS= $columns
--let $_gmdr_uuid= `SELECT UUID()`
--let GMDR_OUTPUT_FILE= $MYSQLTEST_VARDIR/tmp/_gmdr_sql_file_$_gmdr_uuid.inc
--let $_gmdr_uuid= `SELECT UUID()`
--let GMDR_DECODED_FILE= $MYSQLTEST_VARDIR/tmp/_gmdr_decoded_file_$_gmdr_uuid.inc
--let GMDR_MYSQLBINLOG_OPTIONS= $mysqlbinlog_options

--exec $MYSQL_BINLOG -v $GMDR_POSITIONS $GMDR_MYSQLBINLOG_OPTIONS > $GMDR_OUTPUT_FILE

perl;
  use strict;
  sub flush() {
    our @statement_parts;
    our $decoded_file;
    # swap SET and WHERE for UPDATE statements, since they appear in
    # reverse order in mysqlbinlog -v output
    if (@statement_parts == 3) {
      @statement_parts = ($statement_parts[0], $statement_parts[2],
                          $statement_parts[1]);
    }
    my $statement = join(' ', @statement_parts);
    print DECODED_FILE "$statement;\n"
      or die "Error printing to $decoded_file: $!";
    @statement_parts = ();
  }

  my @column_names = ();
  my @column_types = ();
  for my $col (split(/; /, $ENV{'GMDR_COLUMNS'})) {
    $col =~ m{^([^/]+)/?(.*)};
    my ($name, $type) = ($1, uc($2));
    push(@column_names, $name);
    push(@column_types, $type);
  }

  our $decoded_file = $ENV{'GMDR_DECODED_FILE'};
  use open OUT => ':raw';
  open DECODED_FILE, "> $decoded_file" or die "Error opening $decoded_file: $!";
  our @statement_parts = ();
  my $delimiter = '';
  my $need_delimiter = 0;

  our $output_file = $ENV{'GMDR_OUTPUT_FILE'};
  open OUTPUT_FILE, "< $output_file" or die "Error opening $output_file: $!";

  while (my $line = <OUTPUT_FILE>) {
    chomp($line);
    if ($line =~ /^\s*### ((?:(INSERT|UPDATE|DELETE)|SET|WHERE).*)/) {
      my $uncommented_line= $1;
      my $is_stmt_start= $2;
      if ($is_stmt_start and @statement_parts > 0) {
        flush();
      }
      push(@statement_parts, $uncommented_line);
      if ($uncommented_line eq 'SET') {
        $delimiter = ',';
      }
      elsif ($uncommented_line eq 'WHERE') {
        $delimiter = ' AND';
      }
      $need_delimiter = 0;
    }
    elsif ($line =~ /^\s*###  ( .*)/) {
      my $text = $1;
      # Currently JSON_INSERT and JSON_ARRAY_INSERT not not generate
      # JSON diffs. However, all JSON diffs that the server can
      # currently generate, for which mysqlbinlog prints JSON_INSERT
      # or JSON_ARRAY_INSERT, could equivalently use JSON_SET.
      $text =~ s/(?:JSON_INSERT|JSON_ARRAY_INSERT)/JSON_SET/;
      $text =~ s/^ +/ /;
      if ($text =~ s/^ \@(\d+)/ $column_names[$1 - 1]/) {
        my $number = $1;
        # Fixup WHERE clause
        if ($delimiter eq ' AND') {
          # Use " IS NULL" instead of "=NULL"
          $text =~ /=(.*)/;
          if ($1 eq 'NULL') {
            $text =~ s/=.*/ IS NULL/;
          }
          else {
            my $type = $column_types[$number - 1];
            # Use CAST('[1, 2]' AS JSON) instead of '[1, 2]'
            if ($type =~ /^(?:JSON|DATE|DATETIME|TIME|BINARY)$/) {
              $text =~ s/=(.*)/=CAST($1 AS $type)/;
            }
            # Use UNIX_TIMESTAMP(col)=4711 instead of col=4711
            elsif ($type =~ /^TIMESTAMP/) {
              $text =~ s/ (.*)=(.*)/ UNIX_TIMESTAMP($1)=$2/;
            }
            # FLOAT/DOUBLE may spuriously fail the comparison due to
            # either loss of precision or extra precision.
            # BLOB/BINARY may spuriously fail the comparison due to
            # charset differences in one way or the other (maybe due to
            # the specific characters used in rpl_row_jsondiff_basic.inc,
            # I don't know).
            # ENUM/SET fail due to mysqlbinlog printing them in funny
            # numeric forms.
            # Remove these from the before-image, since they are neither
            # necessary to identify the row nor to verify correctness,
            # in the currently existing tests.
            elsif ($type =~ /FLOAT|DOUBLE|BLOB|BINARY|ENUM|SET/) {
              # Skip this line.
              next;
            }
            elsif ($text =~ /=-\d+ \(\d+\)/) {
              # numbers having their highest bit set are printed like
              # @col=-100 (156), showing the unsigned value in
              # parenthesis.  This is because pre-8.0.2 servers did
              # not replicate the signedness flag.  Choose the correct
              # representation depending on the actual type
              if ($type =~ /UNSIGNED/) {
                $text =~ s/=-\d+ \((\d+)\)/=$1/;
              }
              else {
                $text =~ s/=(-\d+) \(\d+\)/=$1/;
              }
            }
          }
        }
        if ($need_delimiter) {
          $text = $delimiter . $text;
        }
        $need_delimiter = 1;
      }
      # Replace @\d that occurs in JSON function calls
      $text =~ s/\@(\d+)/$column_names[$1 - 1]/;

      $statement_parts[@statement_parts - 1] .= $text;
    }
    elsif (@statement_parts) {
      flush();
    }
  }
  close OUTPUT_FILE or die "Error closing $output_file: $!";
  close DECODED_FILE or die "Error closing $decoded_file: $!";
EOF

--let $output_file= $GMDR_OUTPUT_FILE

--let $read_from_file= $GMDR_DECODED_FILE
--source include/read_file_to_var.inc
--let $decoded_sql= $result

--remove_file $GMDR_DECODED_FILE

--let $include_filename= get_mysqlbinlog_decoded_rows.inc
--source include/end_include_file.inc

# ==== Purpose ====
#
# Assert that the binary log contains a specific sequence of event types.
#
# ==== Usage ====
#
# --let $event_sequence= SEQUENCE_OF_EVENTS
# [--let $event_separator= CHAR]
# [--let $invert= 1]
# [--let $somewhere= 0]
# [--let $limit= [OFFSET,] ROW_COUNT]
# [--let $binlog_file= FILE]
# [--let $binlog_position= POSITION]
# [--let $relay_log= 1]
# [--let $include_header_events= 1]
# [--let $wait_for_binlog_events= 1]
# [--let $slave_timeout= 1]
# [--let $rpl_debug= 1]
# [--let $keep_temp_files= 1]
# [--let $dont_print_pattern= 1]
# [--let $include_trx_payload_events=0]
# --source include/rpl/assert_binlog_events.inc
#
# Parameters:
#
#   $event_sequence
#     This is a (perl) regular expression that will be matched with a
#     sequence of event specifications. Here is an example:
#
#       Query/BEGIN # Query/INSERT.* # Query # (Query/COMMIT|Xid)
#
#         Match a sequence of a BEGIN event, followed by an
#         Query_log_event where the query begins with INSERT, followed
#         by any query, followed by either a Query_log_event with the
#         query equal to COMMIT, or a Xid_log_event.
#
#     A single event is specified using either just the 'Type' field
#     of SHOW BINLOG EVENTS, or using both the 'Type' and the 'Info'
#     fields:
#
#       Query
#         Match any query_log_event
#       Query/BEGIN
#         Match any query_log_event where the query is equal to BEGIN.
#       Query/INSERT.*
#         Match any query_log_event where the query begins with INSERT.
#
#     A sequence of events is specified by separating multiple event
#     specifications with a # character:
#
#       Query/BEGIN # Query/INSERT.* # Query/COMMIT
#
#     Regular expressions can span multiple events:
#
#       Query/BEGIN # (Query/INSERT.*){1,2} # Query/COMMIT
#         Allow either one or two INSERT statements
#
#     The '.' wildcard does *not* match the special characters # or /.
#     So the following is safe in the sense that the .* cannot
#     'swallow' multiple events:
#
#       Query/INSERT.*
#
#     The pattern must match from the beginning until the end. To
#     allow arbitrary events after the pattern, append '(#.*)*' to the
#     pattern.
#
#     The following shortcuts are available as syntactic sugar:
#
#       !Q(STATEMENT) -> Query/(use .*; )?STATEMENT
#       !Begin -> !Q(BEGIN)
#       !Commit -> (!Q(COMMIT) | Xid)
#       !Insert -> (!Q(INSERT.*) | Table_map # Write_rows)
#       !Update -> (!Q(UPDATE.*) | Table_map # Update_rows)
#       !Delete -> (!Q(DELETE.*) | Table_map # Delete_rows)
#       !Single_DML -> (!Insert | !Update | !Delete)
#       !Multi_DML -> multiple DML statements, each possibly touching multiple tables
#       !DML_transaction -> Begin # Multi_DML # Commit
#       !DDL -> !Q(neither BEGIN or COMMIT)
#       !Gtid_transaction -> Gtid # (DML_transaction | DDL)
#       !Empty_gtid_transaction -> Gtid # Begin # Commit
#       !Gtid_or_anon -> (Gtid | Anonymous_Gtid)
#
#     Whitespace around / and # is ignored.
#
#   $event_separator
#     Use $event_separator instead of # to delimit events. This must
#     only be one character long and should not be used elsewhere in
#     the pattern.
#
#   $invert
#     By default, this script asserts that SHOW BINLOG EVENTS matches
#     the specification. If $invert is set, this scripts instead asserts
#     that SHOW BINLOG EVENTS does *not* match the specification.
#
#   $somewhere
#     By default, this script asserts that SHOW BINLOG EVENTS matches
#     the specification from start to end. If $somewhere is set, this scripts
#     instead asserts that SHOW BINLOG EVENTS *somewhere* contains
#     the specification.
#
#   $limit
#     Pass this as the LIMIT clause of SHOW BINLOG EVENTS.
#
#   $binlog_position
#     Pass this as the FROM clause of SHOW BINLOG EVENTS.
#     Note that you can use include/rpl/save_binlog_file_position.inc to read
#     both $binlog_file and $binlog_position.
#
#   $binlog_file
#     Pass this as the IN clause of SHOW BINLOG EVENTS.
#     Note that you can use include/rpl/save_binlog_file_position.inc to read
#     both $binlog_file and $binlog_position.
#
#   $relay_log
#     Use SHOW RELAYLOG EVENTS instead of SHOW BINLOG EVENTS
#
#   $include_header_events
#     By default, Format_desc, Rotate, and Previous_gtids events are
#     ignore.  If this parameter is enabled, the events are included
#     in the match.
#
#   $wait_for_binlog_events
#     If this is true, the script retries until the binlog contains
#     the expected events. The timeout is given by $slave_timeout.
#
#   $slave_timeout
#     Default timeout used if $wait_for_binlog_events is enabled.
#     The default timeout is 300 seconds. You can change the timeout by
#     setting $slave_timeout. The unit for time is seconds.
#
#   $rpl_debug
#     Print lots of debug info.
#
#   $keep_temp_files
#     Keep the two temporary files that this script uses. This is for
#     debugging only and should not be used in a checked-in version of
#     a test.
#
#   $dont_print_pattern
#     By default, the pattern is printed to the result log. If this
#     variable is set, the pattern is not printed.

if ($dont_print_pattern)
{
  --let $include_filename= rpl/assert_binlog_events.inc
}
if (!$dont_print_pattern)
{
  --let $include_filename= rpl/assert_binlog_events.inc [$event_sequence]
}
--source include/begin_include_file.inc

if (!$event_sequence)
{
  --die ERROR IN TEST: specify $event_sequence before sourcing assert_binlog_events.inc. To assert that nothing was generated, set $event_sequence= ()
}

# Execute statement, write result to file.
if (!$relay_log)
{
  --let $statement= SHOW BINLOG EVENTS
}
if ($relay_log)
{
  --let $statement= SHOW RELAYLOG EVENTS
}
if ($binlog_file)
{
  --let $statement= $statement IN '$binlog_file'
}
if ($binlog_position)
{
  --let $statement= $statement FROM $binlog_position
}
if ($limit != "")
{
  --let $statement= $statement LIMIT $limit
}

if ($wait_for_binlog_events)
{
  --let $_abe_counter= 0
  --let $_abe_timeout= $slave_timeout
  # Wait 300 seconds for binlog events
  if (!$_abe_timeout)
  {
    --let $_abe_timeout= 300
  }
}

--let $_abe_verdict= 
while ($_abe_verdict != 'ok')
{
  --let $output_file= GENERATE
  --source include/write_result_to_file.inc

  if ($rpl_debug)
  {
    --echo Wrote output to $output_file
  }

  # Set environment variables used in perl.
  --let _ABE_FILE= $output_file
  --let _ABE_EVENT_SEQUENCE= $event_sequence
  --let _ABE_INVERT= $invert
  --let _ABE_SOMEWHERE= $somewhere
  --let _ABE_DEBUG= $rpl_debug
  --let _ABE_EVENT_SEPARATOR= $event_separator
  --let _ABE_INCLUDE_HEADER_EVENTS= $include_header_events
  --let _ABE_INCLUDE_TRX_PAYLOAD_EVENTS= $include_trx_payload_events

  ############################################################################
  perl;
  my $event_sequence = $ENV{'_ABE_EVENT_SEQUENCE'};
  my $file = $ENV{'_ABE_FILE'};
  my $invert = $ENV{'_ABE_INVERT'};
  my $somewhere = $ENV{'_ABE_SOMEWHERE'};
  my $debug = $ENV{'_ABE_DEBUG'};
  my $include_header_events = $ENV{'_ABE_INCLUDE_HEADER_EVENTS'};
  my $event_separator= $ENV{'_ABE_EVENT_SEPARATOR'};
  my $include_trx_payload_events = $ENV{'_ABE_INCLUDE_TRX_PAYLOAD_EVENTS'};
  if ($event_separator == '')
  {
    $event_separator = '#';
  }

  $event_sequence =~ s/$event_separator/\n/g;

  # Ignore whitespace at beginning, end, and around separators.
  $event_sequence =~ s{^\s*}{};
  $event_sequence =~ s{\s*$}{};
  $event_sequence =~ s{\s*\n\s*}{\n}g;
  $event_sequence =~ s{\s*/\s*}{/}g;

  # Expand syntactic sugar definitions.
  $event_sequence =~ s{!Gtid_transaction}{Gtid\n(!DDL|!DML_transaction)}g;
  $event_sequence =~ s{!Gtid_or_anon}{(Gtid|Anonymous_Gtid)}g;
  $event_sequence =~ s{!Empty_gtid_transaction}{Gtid\n!Begin\n!Commit}g;
  $event_sequence =~ s{!DML_transaction}{!Begin\n!Multi_DML\n!Commit}g;
  $event_sequence =~ s{!DDL}{Query/(?!BEGIN|COMMIT).*}g;
  $event_sequence =~ s{!Single_DML}{(?:Query|Table_map\n(?:Write|Update|Delete)_rows)}g;
  $event_sequence =~ s{!Multi_DML}{(?:Query|(?:Table_map\n)+(?:(?:Write|Update|Delete)_rows))(?:\nTable_map|\n(?:Write|Update|Delete)_rows|\nQuery)*}g;
  $event_sequence =~ s{!Begin}{Query/BEGIN}g;
  $event_sequence =~ s{!Commit}{(?:Query/COMMIT|Xid/COMMIT.*)}g;
  $event_sequence =~ s{!Insert}{(?:!Q(INSERT.*)|Table_map\nWrite_rows)}g;
  $event_sequence =~ s{!Update}{(?:!Q(UPDATE.*)|Table_map\nUpdate_rows)}g;
  $event_sequence =~ s{!Delete}{(?:!Q(DELETE.*)|Table_map\nDelete_rows)}g;
  $event_sequence =~ s{!Q\(([^\n]+)\)}{Query/(?:use.*; )?$1}g;

  # Allow matching 'Type' without 'Info'
  # (e.g., 'Query' instead of 'Query/xyz').
  $event_sequence =~ s{\n}{(?:/[^\n]*)?\n}g;

  # Allow 'Type' without 'Info' at the end. Allow missing \n at the end.
  $event_sequence .= "(?:/[^\n]*)?\n?";

  if (!$somewhere)
  {
    # Require match from the beginning or starting position. Require match until
    # the end.
    $event_sequence = '^' . $event_sequence . '$';
  }

  if ($debug)
  {
    print "Regex: $event_sequence\n";
  }

  # Read and filter file.
  my $result= '';
  open FILE, "< $file" or die "Error $? opening $file: $!";
  my $line_number= 1;
  while (<FILE>)
  {
    if ($line_number > 1)
    {
      # Six tab-separated fields; pick number 3 and number 6.
      s{^[^\t]+\t[^\t]+\t([^\t]+)\t[^\t]+\t[^\t]+\t([^\t]*)$}{$1/$2}
        or die "Unexpected line format in output line $line_number: $_";
      if (
        # include header events
        ($include_header_events and
         ($1 eq 'Format_desc' || $1 eq 'Rotate' || $1 eq 'Previous_gtids')) or

        # include trx payload events
        ($include_trx_payload_events and ($1 eq 'Transaction_payload')) or

        # include all other events
        ($1 ne 'Format_desc' && $1 ne 'Rotate' &&
         $1 ne 'Previous_gtids' && $1 ne 'Transaction_payload')
      )
      {
        chomp;
        $result .= $_;
        $result .= "\n";
      }
    }
    $line_number++;
  }
  close FILE or die "Error $? closing $file: $!";

  if ($debug)
  {
    print "Formatted output: $result\n";
  }

  # Check if there is a match
  my $matches = eval("\$result =~ m{$event_sequence}");
  my $is_ok = ($matches == !$invert);

  # Write 'ok', or write some debug info.
  open FILE, "> $file.verdict" or die "Error $? opening $file.verdict: $!";
  print FILE ($is_ok ? 'ok' :
              "Regex:\n$event_sequence\nFile contents:\n$result")
    or die "Error $? writing to $file.verdict: $!";
  close FILE or die "Error $? writing to $file.verdict: $!";
  EOF
  ############################################################################

  --let $_abe_verdict= `SELECT LOAD_FILE('$output_file.verdict')`
  if ($_abe_verdict != 'ok')
  {
    --let $_abe_fail= 1
    if ($wait_for_binlog_events)
    {
      --sleep 1
      --inc $_abe_counter
      if ($_abe_counter < $_abe_timeout)
      {
        --let $_abe_fail= 0
      }
    }
    if ($_abe_fail)
    {
      --source include/rpl/debug/show_debug_info.inc
      --echo event_sequence [until EOF]
      --echo $event_sequence
      --echo EOF
      --echo $_abe_verdict
      --echo binlog_position=<$binlog_position>
      --echo binlog_file=<$binlog_file>
      --echo statement=$statement
      --echo invert=$invert
      --echo somewhere=$somewhere
      --echo include_header_events=$include_header_events
      --echo include_trx_payload_events=$include_trx_payload_events
      --echo event_separator=$_ABE_EVENT_SEPARATOR
      --die Binlog contents did not match expected pattern.
    }
  }

  if (!$keep_temp_files)
  {
    --remove_file $output_file
    --remove_file $output_file.verdict
  }
  --let $output_file=
}

--let $include_filename= rpl/assert_binlog_events.inc
--source include/end_include_file.inc

# Requires $bolt_header_file to name a standalone BOLT-promoted binlog.
# Decode the promoted file and validate its LTH metadata and layout.
# These checks keep a plausible but corrupt LTH from silently directing
# optimized recovery to an incorrect terminal event.
# The awk program avoids `$` field syntax because mysqltest expands it.

--let $bolt_promoted_dump = $MYSQLTEST_VARDIR/tmp/bolt_promoted_dump.txt
--let $bolt_validator_script = $MYSQLTEST_VARDIR/tmp/validate_bolt_header.awk
--exec $MYSQL_BINLOG --force-if-open --verify-binlog-checksum --verbose $MYSQLD_DATADIR/$bolt_header_file > $bolt_promoted_dump
--exec grep -m 1 -E 'last_committed=.*sequence_number=' $bolt_promoted_dump | grep -Eq 'last_committed=0[[:space:]]+sequence_number=1'
--write_file $bolt_validator_script EOF
# Extract the end position printed by mysqlbinlog for the current event.
# Keep the full line in a variable so this does not rely on awk `$` fields.
function end_pos(line, value) {
  value = line
  sub(/^.*end_log_pos /, "", value)
  sub(/ .*/, "", value)
  return value + 0
}

BEGIN {
  # Read mysqlbinlog's verbose output and associate each event description
  # with the preceding '# at <offset>' line.
  while ((getline line) > 0) {
    if (line ~ /^# at [0-9][0-9]*/) {
      split(line, words, " ")
      event_start = words[3] + 0
      continue
    }

    # The preceding Previous-GTIDs event must end where the LTH starts.
    if (line ~ /Previous-GTIDs/ && event_start) {
      previous_start = event_start
      previous_end = end_pos(line)
      continue
    }

    # A promoted file contains exactly one ignorable LTH in its prefix.
    if (line ~ /Large transaction header[[:space:]]+Ignorable/ && event_start) {
      if (lth_start) exit 1
      lth_start = event_start
      lth_end = end_pos(line)
      continue
    }

    # Parse the LTH's recovery hint: the terminal-event offset, its expected
    # type code, and the remaining padding that fills the reserved region.
    if (line ~ /Terminating event offset /) {
      if (!lth_start || lth_offset) exit 1
      value = line
      sub(/^.*Terminating event offset /, "", value)
      split(value, fields, ", type ")
      lth_offset = fields[1] + 0
      split(fields[2], fields, ", padding ")
      lth_type = fields[1] + 0
      sub(/ bytes/, "", fields[2])
      lth_padding = fields[2] + 0
      continue
    }

    # Select only the first GTID after the LTH. The promoted active file can
    # later contain additional GTIDs, which are not part of its header.
    if (line ~ /GTID.*last_committed=.*sequence_number=/ && event_start == lth_end) {
      if (gtid_start) exit 1
      gtid_start = event_start
      gtid_end = end_pos(line)
      continue
    }

    # The recorded terminal offset must identify a decoded event with an end
    # position beyond its start.
    if (line ~ /end_log_pos/ && event_start == lth_offset)
      terminal_end = end_pos(line)
  }

  # The event type is byte 4 of a binlog event header. Read the raw byte at
  # the LTH offset so metadata cannot merely agree with a mis-decoded event.
  command = "dd if=\"" binlog_file "\" bs=1 skip=" (lth_offset + 4) " count=1 2>/dev/null | od -An -tu1"
  command | getline terminal_type
  close(command)
  gsub(/[[:space:]]/, "", terminal_type)

  # Check the promoted prefix is contiguous, the first GTID ends exactly at
  # the 64 KiB reserved boundary, the LTH points beyond that boundary, and
  # the stored type is a recovery-supported terminal event (Query, XID, or
  # XA PREPARE) that matches the raw event-header byte.
  if (!(previous_start >= 4 && previous_end == lth_start &&
        lth_end == gtid_start && gtid_end == 65536 &&
        lth_offset >= gtid_end && terminal_end > lth_offset &&
        lth_padding > 0 && (lth_type == 2 || lth_type == 16 || lth_type == 38) &&
        terminal_type == lth_type)) exit 1
}
EOF
--exec awk -v binlog_file=$MYSQLD_DATADIR/$bolt_header_file -f $bolt_validator_script $bolt_promoted_dump
--remove_file $bolt_promoted_dump
--remove_file $bolt_validator_script

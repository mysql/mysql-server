# ==== Purpose ====
#
# Read the checksum algorithm of the Format_description event at the start
# of an unencrypted relay-log or in-memory relay log spill file.
#
# ==== Usage ====
#
# --let $fde_file= <file path, or a glob pattern that matches one file>
# --source suite/rpl_gtid/t/rpl_csa_imr_fde_checksum_alg.inc
#
# Output variable:
#
# $fde_checksum_alg will be set with:
#   - OFF or CRC32: the checksum algorithm of the file's first event;
#   - Error: <message>: the file could not be read.

--let _FCA_PATTERN= $fde_file
--let _FCA_OUTPUT_FILE= `SELECT CONCAT('$MYSQLTEST_VARDIR/tmp/_fca-', UUID(), '.txt')`
perl;
  my @files= glob($ENV{'_FCA_PATTERN'});
  my $result;
  if (scalar(@files) != 1) {
    $result= "Error: " . scalar(@files) . " files match";
  } elsif (!open(LFILE, '<:raw', $files[0])) {
    $result= "Error: unable to open the file";
  } else {
    # 4-byte magic, then the event header: event_size is at offset 9.
    my $head;
    if (read(LFILE, $head, 4 + 19) != 4 + 19 ||
        substr($head, 0, 4) ne "\xfe\x62\x69\x6e") {
      $result= "Error: not an unencrypted log file";
    } else {
      my $event_size= unpack('V', substr($head, 4 + 9, 4));
      # The algorithm byte precedes the 4 checksum bytes at the end of the
      # Format_description event.
      my $alg;
      seek(LFILE, 4 + $event_size - 5, 0);
      if (read(LFILE, $alg, 1) != 1) {
        $result= "Error: unable to read the checksum algorithm";
      } else {
        my $value= ord($alg);
        $result= $value == 0 ? "OFF" : ($value == 1 ? "CRC32" : "Error: algorithm $value");
      }
    }
    close LFILE;
  }
  my $output_file= $ENV{'_FCA_OUTPUT_FILE'};
  open FILE, "> $output_file" or die "Error $? opening $output_file: $!";
  print FILE $result or die "Error $? writing to $output_file: $!";
  close FILE or die "Error $? writing to $output_file: $!";
EOF
--let $fde_checksum_alg= `SELECT LOAD_FILE('$_FCA_OUTPUT_FILE')`
--remove_file $_FCA_OUTPUT_FILE
--let _FCA_PATTERN=
--let _FCA_OUTPUT_FILE=

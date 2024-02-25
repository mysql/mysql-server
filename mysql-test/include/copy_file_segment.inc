# ==== Purpose ====
#
# Copy a segment from one file to another file.
#
# ==== Usage ====
#
# --let $in_filename = FILENAME1
# --let $out_filename = FILENAME2
# --let $file_start_pos = NUMBER1
# --let $file_stop_pos = NUMBER2
# --source include/copy_file_segment.inc
#
# Parameters:
#   $in_filename
#     Name of file to copy from.
#
#   $out_filename
#     Name of file to create.
#
#   $file_start_pos
#     Start offset in $in_filename.  0 means beginning of file.
#
#   $file_stop_pos
#     End offset in $in_filename.  This is exclusive.
#     Example: if $file_stop_pos=$file_start_pos+1, one byte is copied.

if (!$in_filename) {
  --die Please assign a file name to $in_filename!!
}

if (!$out_filename) {
  --die Please assign a file name to $out_filename!!
}

if ($file_start_pos == '') {
  --die Please assign position of segment start offset to $file_start_pos!!
}

if ($file_stop_pos == '') {
  --die Please assign position of segment stop offset to $file_stop_pos!!
}

--let IN_FILENAME_ENV = $in_filename
--let OUT_FILENAME_ENV = $out_filename
--let FILE_START_POS_ENV = $file_start_pos
--let FILE_STOP_POS_ENV = $file_stop_pos

--perl
  my $in_filename = $ENV{'IN_FILENAME_ENV'};
  my $out_filename = $ENV{'OUT_FILENAME_ENV'};
  my $file_start_pos = $ENV{'FILE_START_POS_ENV'};
  my $file_stop_pos = $ENV{'FILE_STOP_POS_ENV'};

  open(IN, '<:raw', $in_filename) || die("Could not open <$in_filename: $!");
  open(OUT, '>>:raw', $out_filename) || die("Could not open >>$out_filename: $!");

  my ($len_in, $len_out, $data);

  # skip $file_start_pos bytes
  sysseek(IN, $file_start_pos, SEEK_SET);

  # length to be read
  my $length = $file_stop_pos - $file_start_pos;

  # copy binary segment
  while ($length > 0) {
    $len_in = sysread(IN, my $data, $length);
    if (!defined($len_in)) {
      die("Could not sysread data: $!");
    }
    if ($len_in == 0) {
      die("Read zero bytes");
    }

    $length = $length - $len_in;

    my $offset = 0;
    while ($len_in > 0) {

      $len_out = syswrite(OUT, $data, $len_in, $offset);
      if (!defined($len_out)) {
        die("Could not syswrite data: $!");
      }
      $offset += $len_out;
      $len_in -= $len_out;
    }
  }
EOF

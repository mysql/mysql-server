perl;
use File::Spec;
use List::Util qw[min max];

sub log_directory {
  return File::Spec->catdir($ENV{'MYSQLD_DATADIR'}, '#innodb_redo');
}

sub log_get_all_files {
  my $log_dir = log_directory();
  return glob(File::Spec->catfile($log_dir, "#ib_redo*"));
}

sub log_get_non_tmp_files {
  return grep(/#ib_redo\d+$/, log_get_all_files());
}

sub log_get_newest_checkpoint {
  my $max_checkpoint_file = '';
  my $max_checkpoint_lsn = 0;
  foreach my $file_path (log_get_all_files()) {
    my $file_checkpoint_lsn = log_file_get_checkpoint_lsn($file_path);
    if ($file_checkpoint_lsn > $max_checkpoint_lsn) {
      $max_checkpoint_lsn = $file_checkpoint_lsn;
      $max_checkpoint_file = $file_path;
    }
  }
  return ($max_checkpoint_file, $max_checkpoint_lsn);
}

sub log_file_get_header_size { return 2*1024; }

sub log_file_get_offset_for_lsn {
  my ($file_start_lsn, $lsn) = @_;
  return log_file_get_header_size() + ($lsn - $file_start_lsn);
}

sub log_file_get_lsn_for_offset {
  my ($file_start_lsn, $offset) = @_;
  return $file_start_lsn + ($offset - log_file_get_header_size());
}

sub log_file_get_block_no {
  my ($lsn) = @_;
  # Adding 1 because the same is done by log_block_convert_lsn_to_hdr_no() inside InnoDB
  # which this function pretends to mimic
  return 1 + int($lsn / 512);
}

sub log_file_get_block_offset {
  my ($offset) = @_;
  return int($offset / 512) * 512;
}

sub log_file_get_id {
  my ($file_path) = @_;
  $file_path =~ /#ib_redo(\d+)$/;
  return $1;
}

sub log_file_get_checkpoint_lsn {
  my ($file_path) = @_;
  open my $fh, '<:raw', $file_path or die "open failed: $!\n";
  my $offset1 = 512 + 8;
  my $offset2 = 3*512 + 8;
  seek $fh, $offset1, 0;
  read $fh, my $packed_checkpoint_lsn1, 8 or die "read failed: $!\n";
  seek $fh, $offset2, 0;
  read $fh, my $packed_checkpoint_lsn2, 8 or die "read failed: $!\n";
  my $checkpoint_lsn1 = unpack('Q>', $packed_checkpoint_lsn1);
  my $checkpoint_lsn2 = unpack('Q>', $packed_checkpoint_lsn2);
  close $fh or die "close failed: $!\n";
  return max($checkpoint_lsn1, $checkpoint_lsn2);
}

sub log_file_set_checkpoint_lsn {
  my ($file_path, $checkpoint_lsn1, $checkpoint_lsn2) = @_;
  open my $fh, '+<:raw', $file_path or die "open failed: $!\n";
  my $offset1 = 512 + 8;
  my $offset2 = 3*512 + 8;
  my $packed_checkpoint_lsn1 = pack('Q>', $checkpoint_lsn1);
  my $packed_checkpoint_lsn2 = pack('Q>', $checkpoint_lsn2);
  seek $fh, $offset1, 0;
  syswrite($fh, $packed_checkpoint_lsn1, 8) == 8 or die "write failed: $!\n";
  seek $fh, $offset2, 0;
  syswrite($fh, $packed_checkpoint_lsn2, 8) == 8 or die "write failed: $!\n";
  close $fh or die "close failed: $!\n";
}

sub log_file_get_start_lsn {
  my ($file_path) = @_;
  open my $fh, '<:raw', $file_path or die "open failed: $!\n";
  seek $fh, 8, 0;
  read $fh, my $packed_start_lsn, 8 or die "read failed: $!\n";
  my $start_lsn = unpack('Q>', $packed_start_lsn);
  close $fh or die "close failed: $!\n";
  return $start_lsn;
}

sub log_file_set_start_lsn {
  my ($file_path, $start_lsn) = @_;
  open my $fh, '+<:raw', $file_path or die "open failed: $!\n";
  seek $fh, 8, 0;
  my $packed_start_lsn = pack('Q>', $start_lsn);
  syswrite($fh, $packed_start_lsn, 8) == 8 or die "write failed: $!\n";
  close $fh or die "close failed: $!\n";
}

sub log_file_get_flags {
  my ($file_path) = @_;
  open my $fh, '<:raw', $file_path or die "open of $file_path failed: $!\n";
  seek $fh, 48, 0;
  read $fh, my $packed_flags, 4 or die "read failed: $!\n";
  my $flags = unpack('L>', $packed_flags);
  close $fh or die "close failed: $!\n";
  return $flags;
}

sub log_file_set_flags {
  my ($file_path, $flags) = @_;
  open my $fh, '+<:raw', $file_path or die "open of $file_path failed: $!\n";
  seek $fh, 48, 0;
  my $packed_flags = pack('L>', $flags);
  syswrite($fh, $packed_flags, 4) == 4 or die "write failed: $!\n";
  close $fh or die "close failed: $!\n";
}

EOF

################################################################################
#
# Verify whether repeated single-character markers are visible as plaintext in
# raw redo files. The server must be stopped and MYSQLD_DATADIR must identify
# its data directory.
#
# Parameters:
#
#   REDO_MARKERS_NOT_VISIBLE_IN_PLAINTEXT
#     Comma-separated list of single-character markers whose repeated patterns
#     must not be visible in raw redo. For example: b,d. May be omitted or
#     empty when no such markers are expected.
#
#   REDO_MARKERS_VISIBLE_IN_PLAINTEXT
#     Comma-separated list of single-character markers whose repeated patterns
#     must be visible in raw redo. For example: a,c,e. May be omitted or empty
#     when no such markers are expected.
#
#   At least one marker must be provided across both lists.
#
################################################################################

perl;
  use strict;
  require 'include/ib_redo_log_files.inc';

  my $markers_not_visible_in_plaintext =
      $ENV{'REDO_MARKERS_NOT_VISIBLE_IN_PLAINTEXT'};
  $markers_not_visible_in_plaintext = ''
      unless defined $markers_not_visible_in_plaintext;

  my $markers_visible_in_plaintext =
      $ENV{'REDO_MARKERS_VISIBLE_IN_PLAINTEXT'};
  $markers_visible_in_plaintext = ''
      unless defined $markers_visible_in_plaintext;

  my @markers_not_visible_in_plaintext =
      split(/,/, $markers_not_visible_in_plaintext);
  my @markers_visible_in_plaintext =
      split(/,/, $markers_visible_in_plaintext);
  my @markers =
      (@markers_not_visible_in_plaintext, @markers_visible_in_plaintext);

  die "At least one redo log marker is required\n" unless @markers;

  for my $marker (@markers) {
    die "Redo log markers must be single characters\n"
        unless length($marker) == 1;
  }

  my $pattern_length = 8;
  my %patterns = map { $_ => $_ x $pattern_length } @markers;
  my %visible_in_plaintext;

  for my $file (log_get_non_tmp_files()) {
    open(my $file_handle, "<:raw", $file)
        or die "Unable to open $file: $!\n";

    while (my $line = <$file_handle>) {
      for my $marker (@markers) {
        if (index($line, $patterns{$marker}) >= 0) {
          $visible_in_plaintext{$marker} = 1;
        }
      }
    }

    close($file_handle);
  }

  for my $marker (@markers_not_visible_in_plaintext) {
    if (exists $visible_in_plaintext{$marker}) {
      print "Pattern $patterns{$marker} visible in plaintext\n";
    }
  }

  for my $marker (@markers_visible_in_plaintext) {
    unless (exists $visible_in_plaintext{$marker}) {
      print "Pattern $patterns{$marker}",
            " expected to be visible in plaintext, but not found\n";
    }
  }
EOF

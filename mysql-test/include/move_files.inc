# ==== Purpose ====
#
# Moves file from one directory to another directory.
# Pattern for file names must be provided.
#
# ==== Usage ====
# --let MOVE_FILES_FROM = dir/a/b/file*
# --let MOVE_FILES_TO = dir/c/d/
# --source include/move_files.inc
#
# Files matching dir/a/b/file* are moved to dir/c/d/
#

perl;
use strict;
use warnings;
use File::Copy;

sub move_files
{
  my ($dir_from, $dir_to) = @_;
  for my $file (glob $dir_from) {
    move ($file, $dir_to) or die $!;
  }
}

move_files ($ENV{MOVE_FILES_FROM}, $ENV{MOVE_FILES_TO});
EOF

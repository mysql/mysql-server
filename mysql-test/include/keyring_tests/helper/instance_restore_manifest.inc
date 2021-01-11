# ==== Purpose ====
#
# Restore manifest file for server instance from backup
#
# ==== Usage ====
#
# --let CURRENT_DATADIR= <path to current data directory>
# --source include/keyring_tests/instace_restore_manifest.inc

--echo # Restore local manifest file for MySQL server instance from backup
--perl
  use strict;
  use File::Basename;
  my $manifest_file_ext = ".my";
  my $backup_manifest_file_ext = ".my.backup";
  my ($exename, $path, $suffix) = fileparse($ENV{'MYSQLD'}, qr/\.[^.]*/);
  my $datadir_path = $ENV{'CURRENT_DATADIR'};
  my $manifest_file_path = $datadir_path.$exename.$manifest_file_ext;
  my $backup_manifest_file_path = $datadir_path.$exename.$backup_manifest_file_ext;
  rename($backup_manifest_file_path, $manifest_file_path) or die;
EOF

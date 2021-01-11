# ==== Purpose ====
#
# Restore manifest file for server binary
#
# ==== Usage ====
#
# --source include/keyring_tests/binary_restore_manifest.inc

--echo # Restore global manifest file for MySQL server from backup
--perl
  use strict;
  use File::Basename;
  my $manifest_file_ext = ".my";
  my $backup_manifest_file_ext = ".my.backup";
  my ($exename, $path, $suffix) = fileparse($ENV{'MYSQLD'}, qr/\.[^.]*/);
  my $manifest_file_path = $path.$exename.$manifest_file_ext;
  my $backup_manifest_file_path = $path.$exename.$backup_manifest_file_ext;
  rename($backup_manifest_file_path, $manifest_file_path) or die;
EOF

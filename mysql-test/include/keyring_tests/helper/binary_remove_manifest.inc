# ==== Purpose ====
#
# Remove manifest file for server binary
#
# ==== Usage ====
#
# --source include/keyring_tests/binary_remove_manifest.inc
#

--echo # Removing global manifest file for MySQL server
--perl
  use strict;
  use File::Basename;
  my $manifest_file_ext = ".my";
  my ($exename, $path, $suffix) = fileparse($ENV{'MYSQLD'}, qr/\.[^.]*/);
  my $manifest_file_path = $path.$exename.$manifest_file_ext;
  unlink $manifest_file_path;
EOF

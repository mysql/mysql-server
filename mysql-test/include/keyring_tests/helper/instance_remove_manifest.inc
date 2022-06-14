# ==== Purpose ====
#
# Remove manifest file for server instance
#
# ==== Usage ====
#
# --let CURRENT_DATADIR= <path to current data directory>
# --source include/keyring_tests/instance_remove_manifest.inc
#
# ==== Parameters ====
#
# $CURRENT_DATADIR
#    Current data directory path
#

--echo # Removing manifest file for current MySQL server instance
--perl
  use strict;
  use File::Basename;
  my $manifest_file_ext = ".my";
  my ($exename, $path, $suffix) = fileparse($ENV{'MYSQLD'}, qr/\.[^.]*/);
  my $datadir_path = $ENV{'CURRENT_DATADIR'};
  my $manifest_file_path = $datadir_path.$exename.$manifest_file_ext;
  unlink $manifest_file_path;
EOF

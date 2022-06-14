# ==== Purpose ====
#
# Create manifest file for server binary
#
# ==== Usage ====
#
# --source include/keyring_tests/binary_create_manifest.inc

--echo # Creating global manifest file for MySQL server
--perl
  use strict;
  use File::Basename;
  my $config_content = "{ \"read_local_manifest\": true }";
  my $manifest_file_ext = ".my";
  my ($exename, $path, $suffix) = fileparse($ENV{'MYSQLD'}, qr/\.[^.]*/);
  my $manifest_file_path = $path.$exename.$manifest_file_ext;
  open(my $mh, "> $manifest_file_path") or die;
  print $mh $config_content or die;
  close($mh);
EOF

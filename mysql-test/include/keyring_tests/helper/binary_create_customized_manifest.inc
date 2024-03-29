# ==== Purpose ====
#
# Create manifest file for server binary with customized
# content
#
# ==== Usage ====
#
# --let GLOBAL_MANIFEST_CONTENT=<content>
# --source include/keyring_tests/binary_create_customized_manifest.inc

--echo # Creating custom global manifest file for MySQL server
--perl
  use strict;
  use File::Basename;
  my $manifest_content = $ENV{'GLOBAL_MANIFEST_CONTENT'};
  my $manifest_file_ext = ".my";
  my ($exename, $path, $suffix) = fileparse($ENV{'MYSQLD'},qr/\.[^.]*/);
  my $manifest_file_path = $path.$exename.$manifest_file_ext;
  open(my $mh, "> $manifest_file_path") or die;
  print $mh $manifest_content or die;
  close($mh);
EOF

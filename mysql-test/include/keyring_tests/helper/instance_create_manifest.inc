# ==== Purpose ====
#
# Create manifest file for server
#
# ==== Usage ====
#
# --let LOCAL_MANIFEST_CONTENT= <string_content_to_be_stored_in_config_file>
# --let CURRENT_DATADIR= <path to current data directory>
# --source include/keyring_tests/instance_create_manifest.inc
#
# ==== Parameters ====
#
# $LOCAL_MANIFEST_CONTENT
#    Content of the configuration file.
# $CURRENT_DATADIR
#    Current data directory path
#

--echo # Creating manifest file for current MySQL server instance
--perl
  use strict;
  use File::Basename;
  my $manifest_content = $ENV{'LOCAL_MANIFEST_CONTENT'};
  my $manifest_file_ext = ".my";
  my ($exename, $path, $suffix) = fileparse($ENV{'MYSQLD'}, qr/\.[^.]*/);
  my $datadir_path = $ENV{'CURRENT_DATADIR'};
  my $manifest_file_path = $datadir_path.$exename.$manifest_file_ext;
  open(my $mh, "> $manifest_file_path") or die;
  print $mh $manifest_content or die;
  close($mh);
EOF

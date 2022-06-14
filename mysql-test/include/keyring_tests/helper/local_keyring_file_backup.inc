# ==== Purpose ====
#
# Take backup of keyring file for given keyring component
#
# ==== Usage ====
#
# --let COMPONENT_NAME = <name_of_keyring_component>
# --let KEYRING_FILE_PATH = <path_of_keyring_file>
# --source include/keyring_tests/local_keyring_file_backup.inc
#
# ==== Parameters ====
#
# $COMPONENT_NAME
#    Name of the keyring component.
#
# $KEYRING_FILE_PATH
#    Path to keyring file
#

--echo # Taking backup of local keyring file for keyring component: $COMPONENT_NAME
--perl
  use strict;
  use File::Basename;
  my $backup_ext = ".copy";
  my ($name, $path, $suffix) = fileparse($ENV{'KEYRING_FILE_PATH'});
  my $file_path = $ENV{'KEYRING_FILE_PATH'};
  my $backup_file_path = $path.$name.$backup_ext;
  rename($file_path, $backup_file_path) or die;
EOF

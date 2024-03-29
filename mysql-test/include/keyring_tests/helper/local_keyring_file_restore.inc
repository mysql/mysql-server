# ==== Purpose ====
#
# Restore keyring file for given keyring component from backup
#
# ==== Usage ====
#
# --let COMPONENT_NAME = <name_of_keyring_component>
# --let KEYRING_FILE_PATH = <path_of_keyring_file>
# --source include/keyring_tests/local_keyring_file_restore.inc
#
# ==== Parameters ====
#
# $COMPONENT_NAME
#    Name of the keyring component.
#
# $KEYRING_FILE_PATH
#    Path to keyring file
#

--echo # Restoring local keyring file from backup for keyring component: $COMPONENT_NAME
--perl
  use strict;
  use File::Basename;
  my $backup_ext = ".copy";
  my ($name, $path, $suffix) = fileparse($ENV{'KEYRING_FILE_PATH'});
  my $file_path = $ENV{'KEYRING_FILE_PATH'};
  my $backup_file_path = $path.$name.$backup_ext;
  rename($backup_file_path, $file_path) or die;
EOF

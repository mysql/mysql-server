# ==== Purpose ====
#
# Backup global configuration file for given keyring component
#
# ==== Usage ====
#
# --let COMPONENT_DIR= <keyring_component_directory>
# --let COMPONENT_NAME= <keyring_component_name_without_extension>
# --source include/keyring_tests/global_keyring_config_backup.inc
#
# ==== Parameters ====
#
# $COMPONENT_DIR
#    Directory where keyring component is located
#
# $COMPONENT_NAME
#    Name of the keyring component. Configuration file name
#    would be <$COMPONENT_NAME>.cnf .
#

--echo # Taking backup of global configuration file for keyring component: $component_name
--perl
  use strict;
  my $config_file_ext = ".cnf";
  my $config_file_path = $ENV{COMPONENT_DIR}."\/".$ENV{'COMPONENT_NAME'}.$config_file_ext;
  my $backup_config_file_ext = ".cnf.backup";
  my $backup_config_file_path = $ENV{COMPONENT_DIR}."\/".$ENV{'COMPONENT_NAME'}.$backup_config_file_ext;
  rename($config_file_path, $backup_config_file_path) or die;
EOF

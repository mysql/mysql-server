# ==== Purpose ====
#
# Remove global configuration file for given keyring component
#
# ==== Usage ====
#
# --let COMPONENT_DIR= <keyring_component_directory>
# --let COMPONENT_NAME= <keyring_component_name_without_extension>
# --source include/keyring_tests/global_keyring_remove_config.inc
#
# ==== Parameters ====
#
# $COMPONENT_DIR
#    Location of component directory
#
# $COMPONENT_NAME
#    Name of the keyring component. Configuration file name
#    would be <$component_name>.cnf .
#

--echo # Removing global configuration file for keyring component: $COMPONENT_NAME
--perl
  use strict;
  my $config_file_ext = ".cnf";
  my $config_file_path = $ENV{COMPONENT_DIR}."\/".$ENV{'COMPONENT_NAME'}.$config_file_ext;
  unlink $config_file_path;
EOF

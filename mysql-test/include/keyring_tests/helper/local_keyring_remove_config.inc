# ==== Purpose ====
#
# Remove configuration file for given keyring component from data dir
#
# ==== Usage ====
#
# --let CURRENT_DATADIR= <Current data directory path>
# --let COMPONENT_NAME= <keyring_component_name_without_extension>
# --source include/keyring_tests/local_keyring_remove_config.inc
#
# ==== Parameters ====
#
# $CURRENT_DATADIR
#    Current data directory
#
# $COMPONENT_NAME
#    Name of the keyring component. Configuration file name
#    would be <$component_name>.cnf .
#

--echo # Removing local configuration file for keyring component: $COMPONENT_NAME
--perl
  use strict;
  my $config_file_ext = ".cnf";
  my $config_file_path = $ENV{CURRENT_DATADIR}."\/".$ENV{'COMPONENT_NAME'}.$config_file_ext;
  unlink $config_file_path;
EOF

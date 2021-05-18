# ==== Purpose ====
#
# Create local configuration file for given keyring component in data dir
#
# ==== Usage ====
#
# --let CURRENT_DATADIR= <Path of current data directory>
# --let COMPONENT_NAME= <keyring_component_name_without_extension>
# --let KEYRING_CONFIG_CONTENT= <string_content_to_be_stored_in_config_file>
# --source include/keyring_tests/local_keyring_create_config.inc
#
# ==== Parameters ====
#
# $CURRENT_DATADIR
#    Data directory
#
# $COMPONENT_NAME
#    Name of the keyring component. Configuration file name
#    would be <$COMPONENT_NAME>.cnf .
#
# $KEYRING_CONFIG_CONTENT
#    Content of the configuration file.
#

--echo # Creating local configuration file for keyring component: $COMPONENT_NAME
--perl
  use strict;
  my $config_content = $ENV{'KEYRING_CONFIG_CONTENT'};
  my $config_file_ext = ".cnf";
  my $config_file_path = $ENV{CURRENT_DATADIR}."\/".$ENV{'COMPONENT_NAME'}.$config_file_ext;
  open(my $mh, "> $config_file_path") or die;
  print $mh $config_content or die;
  close($mh);
EOF

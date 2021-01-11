# ==== Purpose ====
#
# Create global configuration file for given keyring component
#
# ==== Usage ====
#
# --let COMPONENT_DIR= <keyring_component_directory>
# --let COMPONENT_NAME= <keyring_component_name_without_extension>
# --source include/keyring_tests/global_keyring_create_config.inc
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

--echo # Creating global configuration file for keyring component: $component_name
--perl
  use strict;
  my $config_content = "{ \"read_local_config\": true }";
  my $config_file_ext = ".cnf";
  my $config_file_path = $ENV{COMPONENT_DIR}."\/".$ENV{'COMPONENT_NAME'}.$config_file_ext;
  open(my $mh, "> $config_file_path") or die;
  print $mh $config_content or die;
  close($mh);
EOF

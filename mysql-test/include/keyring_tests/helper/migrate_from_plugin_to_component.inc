# Migrate keyring from plugin to component - Offline mode
#
# ==== Usage ====
# --let $SOURCE_KEYRING_PLUGIN=<plugin_name>
# --let $SOURCE_KEYRING_OPTIONS=<options to initialize source plugin>
# --let $DESTINATION_KEYRING_COMPONENT=<component_name_with_extension>
# --let $DESTINATION_KEYRING_CONFIG_CONTENT=<content_of_config_file>
# --let $COMPONENT_DIR=<plugin_dir>
# --let COMPONENT_NAME= <keyring_component_name_without_extension>
# --source include/keyring_tests/helper/migrate_from_plugin_to_component.inc
#

--source global_keyring_config_backup.inc

--echo # Applying migration config for keyring component: $component_name
--perl
  use strict;
  my $config_content = $ENV{'DESTINATION_KEYRING_CONFIG_CONTENT'};
  my $config_file_ext = ".cnf";
  my $config_file_path = $ENV{COMPONENT_DIR}."\/".$ENV{'COMPONENT_NAME'}.$config_file_ext;
  open(my $mh, "> $config_file_path") or die;
  print $mh $config_content or die;
  close($mh);
EOF

--echo # Performing keyring migration

--let KEYRING_MIGRATION_CMD = $MYSQLD --no-defaults --secure-file-priv="" --log_error_verbosity=1 --plugin-dir=$COMPONENT_DIR --keyring-migration-source="$SOURCE_KEYRING_PLUGIN" $SOURCE_KEYRING_OPTIONS --keyring-migration-to-component --keyring-migration-destination="$DESTINATION_KEYRING_COMPONENT"

--exec $KEYRING_MIGRATION_CMD

--source global_keyring_config_restore.inc

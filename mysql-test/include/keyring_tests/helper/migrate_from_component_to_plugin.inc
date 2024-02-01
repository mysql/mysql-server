# Migrate keyring from component to plugin
#
# ==== Usage ====
# --let $SOURCE_KEYRING_COMPONENT=<component_name_with_extension>
# --let $SOURCE_KEYRING_CONFIG_CONTENT=<content_of_config_file>
# --let $DESTINATION_KEYRING_PLUGIN=<plugin_name>
# --let DESTINATION_KEYRING_OPTIONS=<options to initialize destination plugin>
# --let $COMPONENT_DIR=<plugin_dir>
# --let COMPONENT_NAME= <keyring_component_name_without_extension>
# --source include/keyring_tests/helper/migrate_from_component_to_plugin.inc
#

--source global_keyring_config_backup.inc

--echo # Applying migration config for keyring component: $component_name
--perl
  use strict;
  my $config_content = $ENV{'SOURCE_KEYRING_CONFIG_CONTENT'};
  my $config_file_ext = ".cnf";
  my $config_file_path = $ENV{COMPONENT_DIR}."\/".$ENV{'COMPONENT_NAME'}.$config_file_ext;
  open(my $mh, "> $config_file_path") or die;
  print $mh $config_content or die;
  close($mh);
EOF

--echo # Using keyring-migration-from-component and keyring-migration-to-component system variables together, it should fail.

let $MIGRATION_SERVER_LOG= $MYSQL_TMP_DIR/migration_server_log;
--let KEYRING_MIGRATION_CMD = $MYSQLD --no-defaults --secure-file-priv="" --log_error_verbosity=1 --plugin-dir=$COMPONENT_DIR --keyring-migration-source="$SOURCE_KEYRING_COMPONENT" $DESTINATION_KEYRING_OPTIONS --keyring-migration-from-component --keyring-migration-to-component --keyring-migration-destination="$DESTINATION_KEYRING_PLUGIN" > $MIGRATION_SERVER_LOG 2>&1

--error 1
--exec $KEYRING_MIGRATION_CMD

--echo # Searching for message in server log
let SEARCH_FILE=  $MIGRATION_SERVER_LOG;
let SEARCH_PATTERN= Component to component migration cannot be performed using migration server;
--source include/search_pattern.inc
--echo # Search completed.

--echo # Performing keyring migration

--let KEYRING_MIGRATION_CMD = $MYSQLD --no-defaults --secure-file-priv="" --log_error_verbosity=1 --plugin-dir=$COMPONENT_DIR --keyring-migration-source="$SOURCE_KEYRING_COMPONENT" $DESTINATION_KEYRING_OPTIONS --keyring-migration-from-component --keyring-migration-destination="$DESTINATION_KEYRING_PLUGIN"

--exec $KEYRING_MIGRATION_CMD

--source global_keyring_config_restore.inc

--remove_file $MIGRATION_SERVER_LOG

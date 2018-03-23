# ==== Purpose ====
#
# To check if specified plugins exist in the same dir;
#
# ==== Usage ====
#
#	--let plugins= MYSQLXPLUGIN,KEYRING_PLUGIN
#	--source include/check_plugin_dir.inc
#
# parameters :
#	plugins
#		plugins list separated by comma (ENV variable).
#

--perl
# Copies the plugins list from the env variable set in the test.
my $plugins= $ENV{'plugins'};

# Splitting plugins and assigning into a plugin_list array
@plugin_list= split(',',$plugins);

# Check if no plugins are specified.
die "[ERROR]: No plugin specified\n" if (!@plugin_list);

# Getting the plugin directories from the Environment variables.
for my $plugin (@plugin_list) {
	$plugin=~ s/^\s+|\s+$//g;
        my $plugin_dir= $plugin."_DIR";
         if (!$ENV{"$plugin"}) {
           die "[ERROR]: Plugin requires the environment variable \$$plugin".
              " to be set (normally done by mtr)\n";
         }
         if (!($plugin_dir= $ENV{"$plugin_dir"})) {
           die "[ERROR]: Plugin directory for \'$plugin\' is not set\n";
         }
        $plugin=$plugin_dir;
}

# assigning the first plugin directory as the default directory to compare with
# all other plugins
my $default_dir= shift(@plugin_list);
die "[ERROR]: Plugin path not found\n" if(!$default_dir);

# File to write the skip status variable so that it can be used in the test.
open(SKIP,">$ENV{MYSQL_TMP_DIR}/skip_status.txt");

# Compare the plugin paths of all the plugins are same
# set $skip variable if the test has to be skipped

if(@plugin_list != grep { $_ eq $default_dir } @plugin_list)
{
	print SKIP "--let \$skip= 1";
}

# close the file handler.
close(SKIP);
EOF

--let $skip= 0
--source $MYSQL_TMP_DIR/skip_status.txt
if ($skip) {
	--skip Plugins present in different folders
}
--remove_file $MYSQL_TMP_DIR/skip_status.txt

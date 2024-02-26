#
# Loads the plugin when there is no query_rewrite database
#
SET sql_mode = 'PIPES_AS_CONCAT';
CALL mtr.add_suppression("Plugin rewriter reported: " ||
"'Wrong column count or names when loading rules.'");
INSTALL PLUGIN rewriter SONAME 'rewriter.xxx';
UNINSTALL PLUGIN rewriter;
Warnings:
Warning	1620	Plugin is busy and will be uninstalled on shutdown
SET sql_mode = DEFAULT;

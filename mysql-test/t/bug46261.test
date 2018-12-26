--source include/have_example_plugin.inc

--echo #
--echo # Bug#46261 Plugins can be installed with --skip-grant-tables
--echo #

--replace_regex /\.dll/.so/
eval INSTALL PLUGIN example SONAME '$EXAMPLE_PLUGIN';

SELECT PLUGIN_STATUS FROM INFORMATION_SCHEMA.plugins
  WHERE plugin_name='example';

--replace_regex /\.dll/.so/
--error ER_UDF_EXISTS
eval INSTALL PLUGIN example SONAME '$EXAMPLE_PLUGIN';

UNINSTALL PLUGIN example;

--echo End of 5.1 tests

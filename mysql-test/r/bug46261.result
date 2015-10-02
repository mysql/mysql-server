#
# Bug#46261 Plugins can be installed with --skip-grant-tables
#
INSTALL PLUGIN example SONAME 'ha_example.so';
SELECT PLUGIN_STATUS FROM INFORMATION_SCHEMA.plugins
WHERE plugin_name='example';
PLUGIN_STATUS
ACTIVE
INSTALL PLUGIN example SONAME 'ha_example.so';
ERROR HY000: Function 'example' already exists
UNINSTALL PLUGIN example;
End of 5.1 tests

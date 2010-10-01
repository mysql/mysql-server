if (!`SELECT COUNT(*) FROM INFORMATION_SCHEMA.PLUGINS
         WHERE PLUGIN_NAME = 'innodb' AND PLUGIN_STATUS = 'active' AND
              (PLUGIN_LIBRARY LIKE 'ha_innodb_plugin%' OR PLUGIN_DESCRIPTION LIKE '%xtradb%')`) {
  skip Need InnoDB plugin or XtraDB;
}

if (`SELECT count(plugin_status)=0 FROM information_schema.plugins WHERE plugin_name LIKE 'mysqlx'`)
{
  skip Mysqlx plugin not available.;
}

if (`SELECT plugin_status!='ACTIVE' FROM information_schema.plugins WHERE plugin_name LIKE 'mysqlx'`)
{
  skip Mysqlx plugin is disabled.;
}

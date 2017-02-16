#
# suite.pm will make sure that all tests including this file
# will be skipped unless xtradb is enabled
#
# The test below is redundant

if (!`SELECT count(*) FROM information_schema.plugins WHERE
      plugin_name = 'innodb' AND plugin_status = 'active' AND
      plugin_description LIKE '%xtradb%'`){
  skip Needs XtraDB engine;
}

if (!`SELECT count(*) FROM information_schema.plugins WHERE
      plugin_name = 'innodb' AND plugin_status = 'active' AND
      plugin_description LIKE '%xtradb%'`){
  skip Need XtraDB engine;
}

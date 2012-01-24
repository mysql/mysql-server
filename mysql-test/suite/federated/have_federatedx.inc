if (!`SELECT count(*) FROM information_schema.plugins WHERE
      plugin_name = 'federated' AND plugin_status = 'active' AND
      plugin_description LIKE '%FederatedX%'`){
  skip Need FederatedX engine;
}

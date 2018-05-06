#
# Show the Ndb_slave_max_replicated_epoch status variable
#
let $_smre =
  `select variable_value from performance_schema.global_status
     where variable_name = 'Ndb_slave_max_replicated_epoch'`;
echo Ndb_slave_max_replicated_epoch: $_smre;

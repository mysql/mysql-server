# Wait for index stats thread to reach state matching global
# ndb_index_stat_enable.  This avoids error 4721 after cluster
# initial restart or after enable was turned off and on again.

while(!`select count(*) from dual
        where @@global.ndb_index_stat_enable =
        (select count(*) from performance_schema.global_status
         where variable_name = 'NDB_INDEX_STAT_STATUS'
         and concat(',',variable_value,',') like '%,allow:1,%')`)
{
  sleep 0.1;
}

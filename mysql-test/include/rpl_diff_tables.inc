# #############################################################################
# Check whether the given table is consistent between different master and
# slaves
#
# Usage: 
# --let $diff_table= test.t1 
# --let $diff_server_list= master, slave, slave2 
# --source include/rpl_diff_tables.inc
# #############################################################################

if (`SELECT "XX$diff_table" = "XX"`)
{
  --die diff_table is null.
}

--let $_servers= master, slave
if (`SELECT "XX$diff_server_list" <> "XX"`)
{
  --let $_servers= $diff_server_list
}

--let $_master= `SELECT SUBSTRING_INDEX('$_servers', ',', 1)`
--let $_servers= `SELECT LTRIM(SUBSTRING('$_servers', LENGTH('$_master') + 2))`
connection $_master;
while (`SELECT "XX$_servers" <> "XX"`)
{
  --let $_slave= `SELECT SUBSTRING_INDEX('$_servers', ',', 1)`
  --let $_servers= `SELECT LTRIM(SUBSTRING('$_servers', LENGTH('$_slave') + 2))`

  --sync_slave_with_master $_slave
  --let $diff_table_1= $_master:$diff_table
  --let $diff_table_2= $_slave:$diff_table
  --source include/diff_tables.inc
  connection $_slave;
}
connection $_master;

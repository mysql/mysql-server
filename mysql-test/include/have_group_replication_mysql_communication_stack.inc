# ==== Purpose ====
#
# Ensure that group replication is running using MySQL protocol stack
#

--let $_have_mysql_stack= `SELECT @@group_replication_communication_stack LIKE 'MYSQL'`
if (!$_have_mysql_stack)
{
  --skip Test requires MySQL Protocol Stack configured in Group Replication
}
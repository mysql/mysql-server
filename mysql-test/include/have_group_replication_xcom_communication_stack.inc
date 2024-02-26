# ==== Purpose ====
#
# Ensure that group replication is running using MySQL protocol stack
#

--let $_have_xcom_stack= `SELECT @@group_replication_communication_stack LIKE 'XCOM'`
if (!$_have_xcom_stack)
{
  --skip Test requires XCom Protocol Stack configured in Group Replication
}
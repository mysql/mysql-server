# Within the WL#7158 records for the users root@127.0.0.1 and root@::1
# have been deleted from the script mysql-test/include/mtr_system_tables_data.sql
# that used to fill in system tables. Since this test relies on the presence of this
# records in the table mysql.user we add the users root@127.0.0.1 and root@::1
# into mysql.user explicitly.

--disable_warnings
--disable_query_log
DROP USER 'root'@'::1';
DROP USER 'root'@'127.0.0.1';

if ($extra_root_users_for_slave)
{
  --connection slave_init_con
  DROP USER 'root'@'127.0.0.1';
  DROP USER 'root'@'::1';
  RESET BINARY LOGS AND GTIDS;
  --connection default
  --disconnect slave_init_con
}
--enable_query_log
--enable_warnings

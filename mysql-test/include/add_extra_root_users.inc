# Within the WL#7158 records for the users root@127.0.0.1 and root@::1
# have been deleted from the script mysql-test/include/mtr_system_tables_data.sql
# that used to fill in system tables. Since this test relies on the presence of this
# records in the table mysql.user we add the users root@127.0.0.1 and root@::1
# into mysql.user explicitly.

--disable_warnings
--disable_query_log
call mtr.add_suppression("Unsafe statement written to the binary log using statement format since BINLOG_FORMAT = STATEMENT");

CREATE USER IF NOT EXISTS 'root'@'127.0.0.1';
CREATE USER IF NOT EXISTS 'root'@'::1';
GRANT ALL ON *.* TO 'root'@'127.0.0.1', 'root'@'::1' WITH GRANT OPTION;

if ($extra_root_users_for_slave)
{
  --echo # connecting to a slave to add accounts root@127.0.0.1, root@::1
  let $os_version = `SELECT CONVERT(@@version_compile_os using latin1) NOT IN ("Win32","Win64","Windows")`;
  if ($os_version)
  {
    connect (slave_init_con,localhost,root,,test,$SLAVE_MYPORT,$SLAVE_MYSOCK);
  }
  if (!$os_version)
  {
    connect (slave_init_con,localhost,root,,test,$SLAVE_MYPORT,$SLAVE_MYSOCK,PIPE);
  }

  CREATE USER IF NOT EXISTS 'root'@'127.0.0.1';
  CREATE USER IF NOT EXISTS 'root'@'::1';
  GRANT ALL ON *.* TO 'root'@'127.0.0.1', 'root'@'::1';
  --connection default
}
--enable_query_log
--enable_warnings

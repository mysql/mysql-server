# In general is should be equivalent to run 2 connections to 
# different servers and 2 connections to th same server...
# So in general we should be able to run "multi tests" also on embedded
-- let have_embedded=`select version() like '%embedded%'`
if ($have_embedded)
{
  # Setup 2 connections used in multi tests
  connect (server1,localhost,root,,test);
  connect (server2,localhost,root,,test);
}
if (!$have_embedded)
{
  # Setup connections to both MySQL Servers connected to the cluster
  connect (server1,127.0.0.1,root,,test,$MASTER_MYPORT,);
  connect (server2,127.0.0.1,root,,test,$MASTER_MYPORT1,);
}

# Check that server1 has NDB  support
connection server1;
let $engines_table= query_get_value(SHOW TABLES FROM information_schema LIKE 'ENGINES', Tables_in_information_schema (ENGINES), 1);
disable_query_log;
if (`SELECT 1 FROM dual WHERE '$engines_table' = 'engines'`)
{
--require r/true.require
SELECT (support = 'YES' or support = 'DEFAULT' or support = 'ENABLED') as `TRUE` FROM information_schema.engines WHERE engine = 'ndbcluster';
}
enable_query_log;

if (!$have_embedded)
{
# Check that server2 has NDB support
connection server2;
let $engines_table= query_get_value(SHOW TABLES FROM information_schema LIKE 'ENGINES', Tables_in_information_schema (ENGINES), 1);
disable_query_log;
if (`SELECT 1 FROM dual WHERE '$engines_table' = 'engines'`)
{
--require r/true.require
SELECT (support = 'YES' or support = 'DEFAULT' or support = 'ENABLED') as `TRUE` FROM information_schema.engines WHERE engine = 'ndbcluster';
}
enable_query_log;
}

# cleanup

connection server1;
disable_query_log;
disable_warnings;
--error 0,1051
drop table if exists t1,t2,t3,t4,t5,t6,t7,t8,t9,t10;
flush tables;
flush status;
enable_warnings;
enable_query_log;

if (!$have_embedded)
{
connection server2;
disable_query_log;
disable_warnings;
--error 0,1051
drop table if exists t1,t2,t3,t4,t5,t6,t7,t8,t9,t10;
flush tables;
flush status;
enable_warnings;
enable_query_log;
}

# Set the default connection
connection server1;

--let NDB_BACKUPS=$NDB_BACKUP_DIR/BACKUP/BACKUP

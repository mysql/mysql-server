--source include/not_parallel.inc
--source include/have_ndb.inc

--disable_query_log

# Recreate database if it already exist
if (`SELECT COUNT(*) FROM INFORMATION_SCHEMA.SCHEMATA
     WHERE SCHEMA_NAME = 'sbtest'`)
{
  DROP DATABASE sbtest;
}
CREATE DATABASE sbtest;

# Add user for sysbench to connect using plain old native password
CREATE USER sbuser IDENTIFIED WITH mysql_native_password;
GRANT ALL ON sbtest.* TO sbuser;

--enable_query_log


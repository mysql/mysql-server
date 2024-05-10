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

# Add user for sysbench to connect using default authentication plugin
CREATE USER sbuser;
GRANT ALL ON sbtest.* TO sbuser;

--enable_query_log


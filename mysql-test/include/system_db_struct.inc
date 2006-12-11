#
# This test must examine structure of current system database
#

-- replace_result Tables_in_mysql Tables_in_db Tables_in_test Tables_in_db
show tables;
show create table db;
show create table host;
show create table user;
show create table func;
show create table tables_priv;
show create table columns_priv;
show create table procs_priv;
show create table servers;
show create table proc;
show create table event;
show create table general_log;
show create table slow_log;

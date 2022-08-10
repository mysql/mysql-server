#
# This test must examine structure of current system database
#

-- replace_result Tables_in_mysql Tables_in_db Tables_in_test Tables_in_db
show tables where Tables_in_mysql != 'ndb_binlog_index';
#
# Remove output about character set, since other tests (like main.mysqldump) might
# make the output of 'show create table' vary between runs.
#
--replace_regex / CHARACTER SET utf8mb3//
show create table db;
--replace_regex / CHARACTER SET utf8mb3//
show create table user;
--replace_regex / CHARACTER SET utf8mb3//
show create table func;
--replace_regex / CHARACTER SET utf8mb3//
show create table tables_priv;
--replace_regex / CHARACTER SET utf8mb3//
show create table columns_priv;
--replace_regex / CHARACTER SET utf8mb3//
show create table procs_priv;
--replace_regex / CHARACTER SET utf8mb3//
show create table servers;
--replace_regex / CHARACTER SET utf8mb3//
show create table general_log;
--replace_regex / CHARACTER SET utf8mb3//
show create table slow_log;

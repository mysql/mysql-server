##### suite/funcs_1/include/create_database.inc

--disable_warnings
eval DROP DATABASE IF EXISTS $new_database;
--enable_warnings
eval CREATE DATABASE $new_database;

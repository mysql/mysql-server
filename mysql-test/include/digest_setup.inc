--echo ####################################
--echo # SETUP
--echo ####################################
# Database setup
--disable_warnings
create database statements_digest;
--enable_warnings
use statements_digest;

# Table set up for queries
--disable_warnings
create table t1(a int);
create table t2(a int);
create table t3(a int, b int);
create table t4(a int, b int);
create table t5(a int, b int, c int);
--enable_warnings


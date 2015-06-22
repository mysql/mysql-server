#
# Bug #19929435 DROP DATABASE HANGS WITH MALFORMED TABLE
#
create database `b`;
use `b`;
create table `#mysql50#q.q` (f1 int primary key) engine=innodb;
drop database `b`;

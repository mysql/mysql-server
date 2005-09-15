create database if not exists TEST_DB;
use TEST_DB;
drop table if exists T1;
create table T1 (KOL1 int unsigned not null,
                 KOL2 int unsigned not null,
	         KOL3 int unsigned not null,
	         KOL4 int unsigned not null,
	         KOL5 int unsigned not null,
	         primary key using hash(KOL1)) engine=ndb;

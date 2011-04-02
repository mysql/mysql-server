create database innodb_memcache;

use innodb_memcache;

CREATE  TABLE IF NOT EXISTS `containers` (
`name` varchar(50) not null primary key,
`db_schema` VARCHAR(250) NOT NULL,
`db_table` VARCHAR(250) NOT NULL,
`key_columns` VARCHAR(250) NOT NULL,
`value_columns` VARCHAR(250),
`flags` VARCHAR(250) NOT NULL DEFAULT "0",
`cas_column` VARCHAR(250),
`expire_time_column` VARCHAR(250),
`unique_idx_name_on_key` VARCHAR(250) NOT NULL
) ENGINE = InnoDB;

/* This is an example */
insert into containers values ("aaa", "test", "demo_test", "c1", "c2",  "c3", "c4", "c5", "idx");

use test

create table demo_test(cx varchar(10), cy int, c1 varchar(32), cz int, c2 varchar(1024), ca int, cb int, c3 int, cu int, c4 bigint unsigned, c5 int) engine = innodb;

insert into demo_test values ("9", 3 , "aa", 2, "hello, hello", 8, 8, 0, 1, 3, 0);

create unique index idx on demo_test(c1);


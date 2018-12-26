##### suite/funcs_1/include/memory_tb1.inc

--disable_warnings
drop table if exists tb1 ;
--enable_warnings
create table tb1 (
f1 char,
f2 char binary,
f3 char ascii,
f12 binary,
f13 tinyint,
f14 tinyint unsigned,
f15 tinyint zerofill,
f16 tinyint unsigned zerofill,
f17 smallint,
f18 smallint unsigned,
f19 smallint zerofill,
f20 smallint unsigned zerofill,
f21 mediumint,
f22 mediumint unsigned,
f23 mediumint zerofill,
f24 mediumint unsigned zerofill,
f25 int,
f26 int unsigned,
f27 int zerofill,
f28 int unsigned zerofill,
f29 bigint,
f30 bigint unsigned,
f31 bigint zerofill,
f32 bigint unsigned zerofill,
f33 decimal not null DEFAULT 9.9,
f34 decimal unsigned not null DEFAULT 9.9,
f35 decimal zerofill not null DEFAULT 9.9,
f36 decimal unsigned zerofill not null DEFAULT 9.9,
f37 decimal (0) not null DEFAULT 9.9,
f38 decimal (64) not null DEFAULT 9.9,
f39 decimal (0) unsigned not null DEFAULT 9.9,
f40 decimal (64) unsigned not null DEFAULT 9.9,
f41 decimal (0) zerofill not null DEFAULT 9.9,
f42 decimal (64) zerofill not null DEFAULT 9.9,
f43 decimal (0) unsigned zerofill not null DEFAULT 9.9,
f44 decimal (64) unsigned zerofill not null DEFAULT 9.9,
f45 decimal (0,0) not null DEFAULT 9.9,
f46 decimal (63,30) not null DEFAULT 9.9,
f47 decimal (0,0) unsigned not null DEFAULT 9.9,
f48 decimal (63,30) unsigned not null DEFAULT 9.9,
f49 decimal (0,0) zerofill not null DEFAULT 9.9,
f50 decimal (63,30) zerofill not null DEFAULT 9.9,
f51 decimal (0,0) unsigned zerofill not null DEFAULT 9.9,
f52 decimal (63,30) unsigned zerofill not null DEFAULT 9.9,
f53 numeric not null DEFAULT 99,
f54 numeric unsigned not null DEFAULT 99,
f55 numeric zerofill not null DEFAULT 99,
f56 numeric unsigned zerofill not null DEFAULT 99,
f57 numeric (0) not null DEFAULT 99,
f58 numeric (64) not null DEFAULT 99
) charset latin1 engine = memory;

--replace_result $MYSQLTEST_VARDIR <MYSQLTEST_VARDIR>
eval
load data infile '$MYSQLTEST_VARDIR/std_data/funcs_1/memory_tb1.txt'
into table tb1 character set latin1;

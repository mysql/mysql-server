##### suite/funcs_1/include/ndb_tb2.inc

--disable_warnings
drop table if exists tb2 ;
--enable_warnings
create table tb2 (
f59 numeric (0) unsigned,
f60 numeric (64) unsigned,
f61 numeric (0) zerofill,
f62 numeric (64) zerofill,
f63 numeric (0) unsigned zerofill,
f64 numeric (64) unsigned zerofill,
f65 numeric (0,0),
f66 numeric (63,30),
f67 numeric (0,0) unsigned,
f68 numeric (63,30) unsigned,
f69 numeric (0,0) zerofill,
f70 numeric (63,30) zerofill,
f71 numeric (0,0) unsigned zerofill,
f72 numeric (63,30) unsigned zerofill,
f73 real,
f74 real unsigned,
f75 real zerofill,
f76 real unsigned zerofill,
f77 double default 7.7,
f78 double unsigned default 7.7,
f79 double zerofill default 7.7,
f80 double unsigned zerofill default 8.8,
f81 float not null default 8.8,
f82 float unsigned not null default 8.8,
f83 float zerofill not null default 8.8,
f84 float unsigned zerofill not null default 8.8,
f85 float(0) not null default 8.8,
f86 float(23) not null default 8.8,
f87 float(0) unsigned not null default 8.8,
f88 float(23) unsigned not null default 8.8,
f89 float(0) zerofill not null default 8.8,
f90 float(23) zerofill not null default 8.8,
f91 float(0) unsigned zerofill not null default 8.8,
f92 float(23) unsigned zerofill not null default 8.8,
f93 float(24) not null default 8.8,
f94 float(53) not null default 8.8,
f95 float(24) unsigned not null default 8.8,
f96 float(53) unsigned not null default 8.8,
f97 float(24) zerofill not null default 8.8,
f98 float(53) zerofill not null default 8.8,
f99 float(24) unsigned zerofill not null default 8.8,
f100 float(53) unsigned zerofill not null default 8.8,
f101 date not null default '2000-01-01',
f102 time not null default 20,
f103 datetime not null default '2/2/2',
f104 timestamp not null default 20001231235959,
f105 year not null default 2000,
f106 year(3) not null default 2000,
f107 year(4) not null default 2000,
f108 enum("1enum","2enum") not null default "1enum",
f109 set("1set","2set") not null default "1set"
) engine = ndb;

--replace_result $MYSQLTEST_VARDIR <MYSQLTEST_VARDIR>
eval
load data infile '$MYSQLTEST_VARDIR/std_data/funcs_1/ndb_tb2.txt'
into table tb2 ;

##### suite/funcs_1/include/memory_tb3.inc

--disable_warnings
drop table if exists tb3;
--enable_warnings
create table tb3 (
f118 char not null DEFAULT 'a',
f119 char binary not null DEFAULT b'101',
f120 char ascii not null DEFAULT b'101',
f121 char(50),
f122 char(50),
f129 binary not null DEFAULT b'101',
f130 tinyint not null DEFAULT 99,
f131 tinyint unsigned not null DEFAULT 99,
f132 tinyint zerofill not null DEFAULT 99,
f133 tinyint unsigned zerofill not null DEFAULT 99,
f134 smallint not null DEFAULT 999,
f135 smallint unsigned not null DEFAULT 999,
f136 smallint zerofill not null DEFAULT 999,
f137 smallint unsigned zerofill not null DEFAULT 999,
f138 mediumint not null DEFAULT 9999,
f139 mediumint unsigned not null DEFAULT 9999,
f140 mediumint zerofill not null DEFAULT 9999,
f141 mediumint unsigned zerofill not null DEFAULT 9999,
f142 int not null DEFAULT 99999,
f143 int unsigned not null DEFAULT 99999,
f144 int zerofill not null DEFAULT 99999,
f145 int unsigned zerofill not null DEFAULT 99999,
f146 bigint not null DEFAULT 999999,
f147 bigint unsigned not null DEFAULT 999999,
f148 bigint zerofill not null DEFAULT 999999,
f149 bigint unsigned zerofill not null DEFAULT 999999,
f150 decimal not null DEFAULT 999.999,
f151 decimal unsigned not null DEFAULT 999.17,
f152 decimal zerofill not null DEFAULT 999.999,
f153 decimal unsigned zerofill,
f154 decimal (0),
f155 decimal (64),
f156 decimal (0) unsigned,
f157 decimal (64) unsigned,
f158 decimal (0) zerofill,
f159 decimal (64) zerofill,
f160 decimal (0) unsigned zerofill,
f161 decimal (64) unsigned zerofill,
f162 decimal (0,0),
f163 decimal (63,30),
f164 decimal (0,0) unsigned,
f165 decimal (63,30) unsigned,
f166 decimal (0,0) zerofill,
f167 decimal (63,30) zerofill,
f168 decimal (0,0) unsigned zerofill,
f169 decimal (63,30) unsigned zerofill,
f170 numeric,
f171 numeric unsigned,
f172 numeric zerofill,
f173 numeric unsigned zerofill,
f174 numeric (0),
f175 numeric (64)
) engine = memory;

--replace_result $MYSQLTEST_VARDIR <MYSQLTEST_VARDIR>
eval
load data infile '$MYSQLTEST_VARDIR/std_data/funcs_1/memory_tb3.txt'
into table tb3;

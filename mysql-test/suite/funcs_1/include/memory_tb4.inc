##### suite/funcs_1/include/memory_tb4.inc

--disable_warnings
drop table if exists tb4 ;
--enable_warnings
create table tb4 (
f176 numeric (0) unsigned not null DEFAULT 9,
f177 numeric (64) unsigned not null DEFAULT 9,
f178 numeric (0) zerofill not null DEFAULT 9,
f179 numeric (64) zerofill not null DEFAULT 9,
f180 numeric (0) unsigned zerofill not null DEFAULT 9,
f181 numeric (64) unsigned zerofill not null DEFAULT 9,
f182 numeric (0,0) not null DEFAULT 9,
f183 numeric (63,30) not null DEFAULT 9,
f184 numeric (0,0) unsigned not null DEFAULT 9,
f185 numeric (63,30) unsigned not null DEFAULT 9,
f186 numeric (0,0) zerofill not null DEFAULT 9,
f187 numeric (63,30) zerofill not null DEFAULT 9,
f188 numeric (0,0) unsigned zerofill not null DEFAULT 9,
f189 numeric (63,30) unsigned zerofill not null DEFAULT 9,
f190 real not null DEFAULT 88.8,
f191 real unsigned not null DEFAULT 88.8,
f192 real zerofill not null DEFAULT 88.8,
f193 real unsigned zerofill not null DEFAULT 88.8,
f194 double not null DEFAULT 55.5,
f195 double unsigned not null DEFAULT 55.5,
f196 double zerofill not null DEFAULT 55.5,
f197 double unsigned zerofill not null DEFAULT 55.5,
f198 float,
f199 float unsigned,
f200 float zerofill,
f201 float unsigned zerofill,
f202 float(0),
f203 float(23),
f204 float(0) unsigned,
f205 float(23) unsigned,
f206 float(0) zerofill,
f207 float(23) zerofill,
f208 float(0) unsigned zerofill,
f209 float(23) unsigned zerofill,
f210 float(24),
f211 float(53),
f212 float(24) unsigned,
f213 float(53) unsigned,
f214 float(24) zerofill,
f215 float(53) zerofill,
f216 float(24) unsigned zerofill,
f217 float(53) unsigned zerofill,
f218 date,
f219 time,
f220 datetime,
f221 timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
f222 year,
f223 year(4),
f224 year(4),
f225 enum("1enum","2enum"),
f226 set("1set","2set"),
f236 char(95),
f241 char(255),
f237 char(130) binary,
f238 varchar(25000) binary,
f239 varbinary(0),
f240 varchar(1200)
) engine = memory;

--replace_result $MYSQLTEST_VARDIR <MYSQLTEST_VARDIR>
eval
load data infile '$MYSQLTEST_VARDIR/std_data/funcs_1/memory_tb4.txt'
into table tb4;

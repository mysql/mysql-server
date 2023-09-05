#
# Tests from mix2.inc which require ucs2 character sets should go here
#

#
# BUG 14056 Column prefix index on UTF-8 primary key column causes: Can't find record..
#

eval create table t1 (
  a int, b char(10), c char(10), filler char(10), primary key(a, b(2)), unique key (a, c(2))
) character set utf8mb3 engine = $engine_type;
eval create table t2 (
  a int, b char(10), c char(10), filler char(10), primary key(a, b(2)), unique key (a, c(2))
) character set ucs2 engine = $engine_type;
insert into t1 values (1,'abcdefg','abcdefg','one');
insert into t1 values (2,'ijkilmn','ijkilmn','two');
insert into t1 values (3,'qrstuvw','qrstuvw','three');
insert into t1 values (4,_utf8mb3 0xe880bd,_utf8mb3 0xe880bd,'four');
insert into t1 values (4,_utf8mb3 0x5b,_utf8mb3 0x5b,'five');
insert into t1 values (4,_utf8mb3 0xe880bde880bd,_utf8mb3 0xe880bde880bd,'six');
insert into t1 values (4,_utf8mb3 0xe880bdD0B1e880bd,_utf8mb3 0xe880bdD0B1e880bd,'seven');
insert into t1 values (4,_utf8mb3 0xD0B1,_utf8mb3 0xD0B1,'eight');
insert into t2 values (1,'abcdefg','abcdefg','one');
insert into t2 values (2,'ijkilmn','ijkilmn','two');
insert into t2 values (3,'qrstuvw','qrstuvw','three');
insert into t2 values (4,_ucs2 0x00e400,_ucs2 0x00e400,'four');
insert into t2 values (4,_ucs2 0x00640065,_ucs2 0x00640065,'five');
insert into t2 values (4,_ucs2 0x00e400e50068,_ucs2 0x00e400e50068,'six');
insert into t2 values (4,_ucs2 0x01fc,_ucs2 0x01fc,'seven');
insert into t2 values (4,_ucs2 0x0120,_ucs2 0x0120,'eight');
insert into t2 values (4,_ucs2 0x0563,_ucs2 0x0563,'ten');
insert into t2 values (4,_ucs2 0x05630563,_ucs2 0x05630563,'eleven');
insert into t2 values (4,_ucs2 0x0563001fc0563,_ucs2 0x0563001fc0563,'point');
insert into t2 values (4,_ucs2 0x05612020,_ucs2 0x05612020,'taken');
update t1 set filler = 'boo' where a = 1;
update t2 set filler ='email' where a = 4;
select a,hex(b),hex(c),filler from t1 order by filler;
select a,hex(b),hex(c),filler from t2 order by filler, hex(b) desc;
drop table t1;
drop table t2;

eval create table t1 (
  a int, b varchar(10), c varchar(10), filler varchar(10), primary key(a, b(2)), unique key (a, c(2))
) character set utf8mb3 engine = $engine_type;
eval create table t2 (
  a int, b varchar(10), c varchar(10), filler varchar(10), primary key(a, b(2)), unique key (a, c(2))
) character set ucs2 engine = $engine_type;
insert into t1 values (1,'abcdefg','abcdefg','one');
insert into t1 values (2,'ijkilmn','ijkilmn','two');
insert into t1 values (3,'qrstuvw','qrstuvw','three');
insert into t1 values (4,_utf8mb3 0xe880bd,_utf8mb3 0xe880bd,'four');
insert into t1 values (4,_utf8mb3 0x5b,_utf8mb3 0x5b,'five');
insert into t1 values (4,_utf8mb3 0xe880bde880bd,_utf8mb3 0xe880bde880bd,'six');
insert into t1 values (4,_utf8mb3 0xe880bdD0B1e880bd,_utf8mb3 0xe880bdD0B1e880bd,'seven');
insert into t1 values (4,_utf8mb3 0xD0B1,_utf8mb3 0xD0B1,'eight');
insert into t2 values (1,'abcdefg','abcdefg','one');
insert into t2 values (2,'ijkilmn','ijkilmn','two');
insert into t2 values (3,'qrstuvw','qrstuvw','three');


insert into t2 values (4,_ucs2 0x00e400,_ucs2 0x00e400,'four');
insert into t2 values (4,_ucs2 0x00640065,_ucs2 0x00640065,'five');
insert into t2 values (4,_ucs2 0x00e400e50068,_ucs2 0x00e400e50068,'six');
insert into t2 values (4,_ucs2 0x01fc,_ucs2 0x01fc,'seven');
insert into t2 values (4,_ucs2 0x0120,_ucs2 0x0120,'eight');
insert into t2 values (4,_ucs2 0x0563,_ucs2 0x0563,'ten');
insert into t2 values (4,_ucs2 0x05630563,_ucs2 0x05630563,'eleven');
insert into t2 values (4,_ucs2 0x0563001fc0563,_ucs2 0x0563001fc0563,'point');
insert into t2 values (4,_ucs2 0x05612020,_ucs2 0x05612020,'taken');
update t1 set filler = 'boo' where a = 1;
update t2 set filler ='email' where a = 4;
select a,hex(b),hex(c),filler from t1 order by filler;
select a,hex(b),hex(c),filler from t2 order by filler, hex(b) desc;
drop table t1;
drop table t2;

eval create table t1 (
  a int, b text(10), c text(10), filler text(10), primary key(a, b(2)), unique key (a, c(2))
) character set utf8mb3 engine = $engine_type;
eval create table t2 (
  a int, b text(10), c text(10), filler text(10), primary key(a, b(2)), unique key (a, c(2))
) character set ucs2 engine = $engine_type;
insert into t1 values (1,'abcdefg','abcdefg','one');
insert into t1 values (2,'ijkilmn','ijkilmn','two');
insert into t1 values (3,'qrstuvw','qrstuvw','three');
insert into t1 values (4,_utf8mb3 0xe880bd,_utf8mb3 0xe880bd,'four');
insert into t1 values (4,_utf8mb3 0x5b,_utf8mb3 0x5b,'five');
insert into t1 values (4,_utf8mb3 0xe880bde880bd,_utf8mb3 0xe880bde880bd,'six');
insert into t1 values (4,_utf8mb3 0xe880bdD0B1e880bd,_utf8mb3 0xe880bdD0B1e880bd,'seven');
insert into t1 values (4,_utf8mb3 0xD0B1,_utf8mb3 0xD0B1,'eight');
insert into t2 values (1,'abcdefg','abcdefg','one');
insert into t2 values (2,'ijkilmn','ijkilmn','two');
insert into t2 values (3,'qrstuvw','qrstuvw','three');
insert into t2 values (4,_ucs2 0x00e400,_ucs2 0x00e400,'four');
insert into t2 values (4,_ucs2 0x00640065,_ucs2 0x00640065,'five');
insert into t2 values (4,_ucs2 0x00e400e50068,_ucs2 0x00e400e50068,'six');
insert into t2 values (4,_ucs2 0x01fc,_ucs2 0x01fc,'seven');
insert into t2 values (4,_ucs2 0x0120,_ucs2 0x0120,'eight');
insert into t2 values (4,_ucs2 0x0563,_ucs2 0x0563,'ten');
insert into t2 values (4,_ucs2 0x05630563,_ucs2 0x05630563,'eleven');
insert into t2 values (4,_ucs2 0x0563001fc0563,_ucs2 0x0563001fc0563,'point');
insert into t2 values (4,_ucs2 0x05612020,_ucs2 0x05612020,'taken');
update t1 set filler = 'boo' where a = 1;
update t2 set filler ='email' where a = 4;
select a,hex(b),hex(c),filler from t1 order by filler;
select a,hex(b),hex(c),filler from t2 order by filler, hex(b) desc;
drop table t1;
drop table t2;

eval create table t1 (
  a int, b blob(10), c blob(10), filler blob(10), primary key(a, b(2)), unique key (a, c(2))
) character set utf8mb3 engine = $engine_type;
eval create table t2 (
  a int, b blob(10), c blob(10), filler blob(10), primary key(a, b(2)), unique key (a, c(2))
) character set ucs2 engine = $engine_type;
insert into t1 values (1,'abcdefg','abcdefg','one');
insert into t1 values (2,'ijkilmn','ijkilmn','two');
insert into t1 values (3,'qrstuvw','qrstuvw','three');
insert into t1 values (4,_utf8mb3 0xe880bd,_utf8mb3 0xe880bd,'four');
insert into t1 values (4,_utf8mb3 0x5b,_utf8mb3 0x5b,'five');
insert into t1 values (4,_utf8mb3 0xD0B1,_utf8mb3 0xD0B1,'eight');
insert into t2 values (1,'abcdefg','abcdefg','one');
insert into t2 values (2,'ijkilmn','ijkilmn','two');
insert into t2 values (3,'qrstuvw','qrstuvw','three');
insert into t2 values (4,_ucs2 0x00e400,_ucs2 0x00e400,'four');
insert into t2 values (4,_ucs2 0x00640065,_ucs2 0x00640065,'five');
insert into t2 values (4,_ucs2 0x00e400e50068,_ucs2 0x00e400e50068,'six');
insert into t2 values (4,_ucs2 0x01fc,_ucs2 0x01fc,'seven');
insert into t2 values (4,_ucs2 0x0120,_ucs2 0x0120,'eight');
insert into t2 values (4,_ucs2 0x0563,_ucs2 0x0563,'ten');
insert into t2 values (4,_ucs2 0x05612020,_ucs2 0x05612020,'taken');
update t1 set filler = 'boo' where a = 1;
update t2 set filler ='email' where a = 4;
select a,hex(b),hex(c),filler from t1 order by filler;
select a,hex(b),hex(c),filler from t2 order by filler;
drop table t1;
drop table t2;
commit;

#
# Test cases for bug #15308 Problem of Order with Enum Column in Primary Key
#
eval CREATE TABLE t1 (
  ind enum('0','1','2') NOT NULL default '0',
  string1 varchar(250) NOT NULL,
  PRIMARY KEY  (ind)
) ENGINE=$engine_type DEFAULT CHARSET=utf8mb3;
eval CREATE TABLE t2 (
  ind enum('0','1','2') NOT NULL default '0',
  string1 varchar(250) NOT NULL,
  PRIMARY KEY  (ind)
) ENGINE=$engine_type DEFAULT CHARSET=ucs2;

INSERT INTO t1 VALUES ('1', ''),('2', '');
INSERT INTO t2 VALUES ('1', ''),('2', '');
SELECT hex(ind),hex(string1) FROM t1 ORDER BY string1;
SELECT hex(ind),hex(string1) FROM t2 ORDER BY string1;
drop table t1,t2;

eval CREATE TABLE t1 (
  ind set('0','1','2') NOT NULL default '0',
  string1 varchar(250) NOT NULL,
  PRIMARY KEY  (ind)
) ENGINE=$engine_type DEFAULT CHARSET=utf8mb3;
eval CREATE TABLE t2 (
  ind set('0','1','2') NOT NULL default '0',
  string1 varchar(250) NOT NULL,
  PRIMARY KEY  (ind)
) ENGINE=$engine_type DEFAULT CHARSET=ucs2;

INSERT INTO t1 VALUES ('1', ''),('2', '');
INSERT INTO t2 VALUES ('1', ''),('2', '');
SELECT hex(ind),hex(string1) FROM t1 ORDER BY string1;
SELECT hex(ind),hex(string1) FROM t2 ORDER BY string1;
drop table t1,t2;

eval CREATE TABLE t1 (
  ind bit not null,
  string1 varchar(250) NOT NULL,
  PRIMARY KEY  (ind)
) ENGINE=$engine_type DEFAULT CHARSET=utf8mb3;
eval CREATE TABLE t2 (
  ind bit not null,
  string1 varchar(250) NOT NULL,
  PRIMARY KEY  (ind)
) ENGINE=$engine_type DEFAULT CHARSET=ucs2;
insert into t1 values(0,''),(1,'');
insert into t2 values(0,''),(1,'');
select hex(ind),hex(string1) from t1 order by string1;
select hex(ind),hex(string1) from t2 order by string1;
drop table t1,t2;

# tests for bug #14056 Column prefix index on UTF-8 primary key column causes 'Can't find record..'

eval create table t2 (
  a int, b char(10), filler char(10), primary key(a, b(2))
) character set utf8mb3 engine = $engine_type;

insert into t2 values (1,'abcdefg','one');
insert into t2 values (2,'ijkilmn','two');
insert into t2 values (3, 'qrstuvw','three');
update t2 set a=5, filler='booo' where a=1;
drop table t2;
eval create table t2 (
  a int, b char(10), filler char(10), primary key(a, b(2))
) character set ucs2 engine = $engine_type;

insert into t2 values (1,'abcdefg','one');
insert into t2 values (2,'ijkilmn','two');
insert into t2 values (3, 'qrstuvw','three');
update t2 set a=5, filler='booo' where a=1;
drop table t2;

eval create table t1(a int not null, b char(110),primary key(a,b(100))) engine=$engine_type default charset=utf8mb3;
insert into t1 values(1,'abcdefg'),(2,'defghijk');
insert into t1 values(6,_utf8mb3 0xD0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1);
insert into t1 values(7,_utf8mb3 0xD0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B2);
select a,hex(b) from t1 order by b;
update t1 set b = 'three' where a = 6;
drop table t1;
eval create table t1(a int not null, b text(110),primary key(a,b(100))) engine=$engine_type default charset=utf8mb3;
insert into t1 values(1,'abcdefg'),(2,'defghijk');
insert into t1 values(6,_utf8mb3 0xD0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1);
insert into t1 values(7,_utf8mb3 0xD0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B1D0B2);
select a,hex(b) from t1 order by b;
update t1 set b = 'three' where a = 6;
drop table t1;


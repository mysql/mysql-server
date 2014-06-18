use test;
create table if not exists mysql56strings (
  id int not null PRIMARY KEY,
  str_fix_utf16le CHAR(20) character set utf16le,
  str_var_utf16le VARCHAR(20) character set utf16le,
  str_fix_utf8mb4 CHAR(20) character set utf8mb4,
  str_var_utf8mb4 VARCHAR(20) character set utf8mb4,
  str_fix_utf8mb3 CHAR(20) character set utf8mb3,
  str_var_utf8mb3 VARCHAR(20) character set utf8mb3
);

create table if not exists mysql56times ( 
  id int not null primary key,
  a time(1) null,
  b datetime(2) null,
  c timestamp(3) null,
  d time(4) null,
  e datetime(5) null,
  f timestamp(6) null
);

create table if not exists mysql56stringsWithText (
  id int not null PRIMARY KEY,
  str_var_utf16le VARCHAR(20) character set utf16le,
  str_var_utf8mb4 VARCHAR(20) character set utf8mb4,
  utf16le_text TEXT character set utf16le
);

delete from mysql56strings;
delete from mysql56times;
delete from mysql56stringsWithText;

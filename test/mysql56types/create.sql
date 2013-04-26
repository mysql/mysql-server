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


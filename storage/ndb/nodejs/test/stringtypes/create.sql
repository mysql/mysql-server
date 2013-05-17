use test;
create table if not exists charset_test (
  id int not null PRIMARY KEY,
  str_fix_latin1 CHAR(20) character set latin1,
  str_var_latin1 VARCHAR(20) character set latin1,
  str_fix_latin2 CHAR(20) character set latin2,
  str_var_latin2 VARCHAR(20) character set latin2,
  str_fix_utf8  CHAR(20) character set utf8,
  str_var_utf8  VARCHAR(20) character set utf8,
  str_fix_utf16 CHAR(20) character set utf16,
  str_var_utf16 VARCHAR(20) character set utf16,
  str_fix_ascii CHAR(20) character set ascii,
  str_var_ascii VARCHAR(20) character set ascii,
  str_fix_utf32 CHAR(20) character set utf32,
  str_var_utf32 VARCHAR(20) character set utf32  
);


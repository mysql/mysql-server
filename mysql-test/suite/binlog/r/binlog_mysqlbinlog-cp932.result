RESET MASTER;
create table t3 (f text character set utf8);
Warnings:
Warning	3719	'utf8' is currently an alias for the character set UTF8MB3, but will be an alias for UTF8MB4 in a future release. Please consider using UTF8MB4 in order to be unambiguous.
create table t4 (f text character set cp932);
flush logs;
rename table t3 to t03, t4 to t04;
RESET MASTER;
select HEX(f) from t03;
HEX(f)
E382BD
select HEX(f) from t3;
HEX(f)
E382BD
select HEX(f) from t04;
HEX(f)
835C
select HEX(f) from t4;
HEX(f)
835C
drop table t3, t4, t03, t04;
End of 5.0 tests

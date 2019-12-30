--error ER_UNKNOWN_SYSTEM_VARIABLE
set global log_bin_trust_routine_creators=1;
--error ER_UNKNOWN_SYSTEM_VARIABLE
set table_type='MyISAM';
--error ER_UNKNOWN_SYSTEM_VARIABLE
select @@table_type='MyISAM';
--error ER_PARSE_ERROR
backup table t1 to 'data.txt';
--error ER_PARSE_ERROR
restore table t1 from 'data.txt';
--error ER_PARSE_ERROR
show plugin;
--error ER_PARSE_ERROR
load table t1 from master;
--error ER_PARSE_ERROR
load data from master;
--error ER_PARSE_ERROR
SHOW INNODB STATUS;
--error ER_PARSE_ERROR
create table t1 (t6 timestamp) type=myisam;
--error ER_PARSE_ERROR
show table types;
--error ER_PARSE_ERROR
show mutex status;

--echo # WL#13070 Deprecate && as synonym for AND and || as synonym for OR in SQL statements
# Verify that || sends no warning if PIPES_AS_CONCAT

set sql_mode=pipes_as_concat;
select 2 || 3;
select 2 or 3;
select concat(2,3);
set sql_mode='';
select 2 || 3;
select 2 or 3;
set sql_mode=default;

--echo # WL#13068 Deprecate BINARY keyword for specifying _bin collations

--echo # (I) Those statements SHOULD WARN

# CREATE TABLE for column
create table t1 (v varchar(10) binary);
show create table t1;
drop table t1;
# "binary" after "character set" is one yacc rule:
create table t1 (v varchar(10) character set latin1 binary);
show create table t1;
drop table t1;
# and the reverse order is another rule:
create table t1 (v varchar(10) binary character set latin1);
show create table t1;
drop table t1;

# ASCII and UNICODE have dedicated yacc rules
create table t1 (v varchar(10) binary ascii);
show create table t1;
drop table t1;
create table t1 (v varchar(10) ascii binary);
show create table t1;
drop table t1;
create table t1 (v varchar(10) binary unicode);
show create table t1;
drop table t1;
create table t1 (v varchar(10) unicode binary);
show create table t1;
drop table t1;

# ALTER TABLE for column
create table t1 (v varchar(10));
show create table t1;
alter table t1 modify v varchar(10) binary character set latin1;
show create table t1;
alter table t1 modify v varchar(10) unicode binary;
show create table t1;
alter table t1 modify v varchar(10) binary ascii;
show create table t1;
drop table t1;

select collation(cast('a' as char(2))), collation(cast('a' as char(2) binary));
select collation(convert('a', char(2))), collation(convert('a', char(2) binary));
select collation(convert('a',char(2) ascii)), collation(convert('a',char(2) ascii binary));

--echo # (II) Those statements SHOULDN'T WARN, as they do make
--echo # "binary" charset, not just a "_bin" collation of another charset.

# A binary column:

create table t1 (v binary(10));
show create table t1;
drop table t1;

# table's charset:

create table t1 (v varchar(10)) character set binary;
show create table t1;
drop table t1;

create table t1 (v varchar(10));
show create table t1;
alter table t1 character set binary;
show create table t1;
drop table t1;

# database's charset:

create database mysqltest2 default character set = binary;
show create database mysqltest2 ;
drop database mysqltest2;
create database mysqltest2 default character set = latin1;
show create database mysqltest2 ;
alter database mysqltest2 default character set = binary;
show create database mysqltest2 ;
drop database mysqltest2;

# session variables:

select @@character_set_client;
set character set binary;
select @@character_set_client;
set character set default;
select @@character_set_client;
set names binary;
select @@character_set_client;
set names default;

# misc:

# gives binary charset
select convert("123" using binary);
select char(123 using binary);
select collation(char(123)), collation(char(123 using binary));

# creates varbinary
create table t1 (v varchar(10) byte);
show create table t1;

# LOAD DATA INFILE '$file' :
# and SELECT ... INTO OUTFILE:

# https://dev.mysql.com/doc/refman/8.0/en/load-data.html says:
# "If the contents of the input file use a character set that differs
# from the default, it is usually preferable to specify the character set
# of the file by using the CHARACTER SET clause. A character set of
# binary specifies "no conversion.""
# So it's not about implying a _bin collation of another charset:
# no warning.

insert into t1 values("xyz");
select * from t1 into outfile 'tmp1.txt' character set binary;
load data infile 'tmp1.txt' into table t1 character set binary;
select * from t1;
let $MYSQLD_DATADIR= `select @@datadir`;
remove_file $MYSQLD_DATADIR/test/tmp1.txt;
drop table t1;

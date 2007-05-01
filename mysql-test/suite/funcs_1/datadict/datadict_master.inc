#### suite/funcs_1/datadict/datadict_master.inc
#
--disable_abort_on_error
let $message=
.
. It is intended that the 3 <engine>__datadict.test files are named this way to be
. sure they are - in a *full run* of the suite - the first tests done for each
. storage engine. Using two _ and the order of processing in mysql-test-run.pl
. ensures this in an easy way.
.
. If needed a restart could be implemented later between the storage engines if
. values changes in the result depending from the position where the
. *__datadict.test are started. This can be a result of showing e.g. maximum
. values of the number of rows of tables.
.
. This .result file has been checked OK with Linux 5.0.23-bk,
. ChangeSet@1.2211, 2006-06-28 10:11:43-07:00.
.;
--source include/show_msg80.inc

################################################################################
#
   let $message= FIXME: There are subtests that are switched off due to known bugs:;
   --source include/show_msg.inc
   #set variable(s) here to be able to switch crashing sub tests with ONE change HERE.
   #change the variable(s) to enable / disable the crashing parts.

   # different 'logics' are used because sometimes codelines needs to be switched off
   # and otherwise some extra statements needs to be executed as long as the bug is not
   # fixed:
   let $have_bug_11589= 1;

   #seems not to work: --vertical_results
   eval SELECT $have_bug_11589 AS "have_bug_11589";
   #seems not to work: --horizontal_results

   # As long as
   # Bug#11589: mysqltest, --ps-protocol, strange output, float/double/real with zerofill
   # is not fixed, we must switch the ps-protocol for some statements off.
   # If this bug is fixed, please
   # 1. set the following variable to 0
   # 2. check, if the test passes
   # 3. remove the workarounds
   if ($have_bug_11589)
   {
      let $message= There are some statements where the ps-protocol is switched off.
                    This may come from the bug listed below, ir from other problems.
                    Bug#11589: mysqltest, --ps-protocol, strange output, float/double/real with zerofill;
      --source include/show_msg80.inc
   }
#
################################################################################


# loading the tables (data is not really needed in this test) is separated to
# make it easier in this file to show the message above.
--source suite/funcs_1/datadict/datadict_load.inc

#FIXME:  -  check for remaining change of object names to standards: db_, tb_, v_, u_, ...
#FIXME:  -  check warnings when data is loaded (Data truncated for column ...)
#FIXME:  -  change connect() to use less users / connections
#
#FIXME:  -  check for additional 'FIXME' here in the script

use information_schema;
--source suite/funcs_1/include/show_connection.inc


################################################################################
#
#  Data Dictionary
#
################################################################################

let $message= Testcase 3.2.1.1:;
--source include/show_msg80.inc

################################################################################
# Testcase 3.2.1.1: Ensure that every INFORMATION_SCHEMA table can be queried
#                   with a SELECT statement, just as if it were an ordinary
#                   user-defined table.
################################################################################

# create at least one object for all 'tables' to be checked
--disable_warnings
DROP DATABASE IF EXISTS db_datadict;
--enable_warnings
CREATE DATABASE db_datadict;
USE db_datadict;

CREATE VIEW v1 AS SELECT * FROM information_schema.tables;

# try to get the server's name to be able to clean-up the result from machine
#  specific stuff.
CREATE OR REPLACE VIEW db_datadict.vu1 as
SELECT grantee AS u
  FROM information_schema.user_privileges;
CREATE OR REPLACE VIEW db_datadict.vu as
SELECT DISTINCT u,
  SUBSTRING( u, LENGTH(SUBSTRING_INDEX(u,_utf8'@',1))+3 )
    AS server,
  SUBSTRING( u, LENGTH(SUBSTRING_INDEX(u,_utf8'@',1))+3,
    LENGTH( SUBSTRING( u,
    LENGTH( SUBSTRING_INDEX(u, _utf8'@',1)) +3 )) - 1 )
    AS Server_Clean
FROM db_datadict.vu1;
--replace_result $SERVER_NAME <SERVER_NAME>
SELECT * FROM db_datadict.vu order by u;

delimiter //;
CREATE PROCEDURE db_datadict.sp_1()
  BEGIN
    SELECT * FROM db_datadict.v1;
  END//
delimiter ;//

#FIXME 3.2.1.1: add missing objects of each type to have something to select
#FIXME 3.2.1.1: - FUNCTION
#FIXME 3.2.1.1: - TRIGGER

USE information_schema;
SHOW tables;

select * from schemata ORDER BY 2 DESC, 1 ASC;

if ($have_bug_11589)
{
--disable_ps_protocol
}
--vertical_results
#SELECT * FROM tables;
#
#FIXME 3.2.1.1: we split the "SELECT * FROM tables" in two parts until
#FIXME 3.2.1.1: Bug #12397: wrong values shown in column CREATE_OPTIONS of
#FIXME 3.2.1.1: INFORMATION_SCHEMA.TABLES is solved, one with 'more' and one
#FIXME 3.2.1.1: with 'less' replace
#  9 AVG_ROW_LENGTH
# 10 DATA_LENGTH
# 11 MAX_DATA_LENGTH
# 12 INDEX_LENGTH
# 13 DATA_FREE
# 15 CREATE_TIME
# 16 UPDATE_TIME
# 17 CHECK_TIME
# 20 CREATE_OPTIONS
--replace_column  9 "#ARL#"  10 "#DL#"  11 "#MDL#"  12 "#IL#"  13 "#DF#"  15 "YYYY-MM-DD hh:mm:ss"  16 "YYYY-MM-DD hh:mm:ss" 17 "YYYY-MM-DD hh:mm:ss"  20 "#CO#"
SELECT * FROM tables
 WHERE table_schema = 'information_schema';
#  9 AVG_ROW_LENGTH
# 10 DATA_LENGTH
# 11 MAX_DATA_LENGTH
# 12 INDEX_LENGTH
# 13 DATA_FREE
# 15 CREATE_TIME
# 16 UPDATE_TIME
# 17 CHECK_TIME
--replace_column  9 "#ARL#"  10 "#DL#"  11 "#MDL#"  12 "#IL#"  13 "#DF#"  15 "YYYY-MM-DD hh:mm:ss"  16 "YYYY-MM-DD hh:mm:ss" 17 "YYYY-MM-DD hh:mm:ss"
SELECT * FROM tables
 WHERE NOT( table_schema = 'information_schema');
--horizontal_results
--enable_ps_protocol

select s.catalog_name, s.schema_name, s.default_character_set_name,
       t.table_type, t.engine
  from schemata s inner join tables t
 ORDER BY s.schema_name, s.default_character_set_name, table_type, engine;
--source suite/funcs_1/datadict/datadict_bug_12777.inc
select * from columns;
select * from character_sets;
select sum(id) from collations;
select collation_name, character_set_name into @x,@y
  from collation_character_set_applicability limit 1;
  select @x, @y;

--replace_column 16 "YYYY-MM-DD hh:mm:ss" 17 "YYYY-MM-DD hh:mm:ss"
select * from routines;

select count(*) from routines;
select * from statistics;
select * from views;
--replace_result $SERVER_NAME <SERVER_NAME>
select * from user_privileges order by grantee, privilege_type;
select * from schema_privileges;
select * from table_privileges;
select * from column_privileges;
select * from table_constraints;
select * from key_column_usage;
select count(*) as max_recs from key_column_usage;

select max(cardinality) from statistics;

select concat("View '",
       table_name, "' is associated with the database '", table_schema, "'.")
       AS "Who is Who for the Views"
  from views;

select concat("Table or view '", table_name,
              "' is associated with the database '", table_schema, "'.") as "Who is Who"
  from tables;

--replace_result $SERVER_NAME <SERVER_NAME>
select grantee as "user's having select privilege",
       substring( grantee, length(SUBSTRING_INDEX(grantee,_utf8'@',1))+2 )
  from user_privileges where privilege_type = 'select'
  order by grantee;

select all table_schema from schema_privileges limit 0,5;

select distinct(privilege_type) from table_privileges;

select * from column_privileges
 group by table_schema having table_schema = 'db_datadict';

select * from table_constraints limit 0,5;
select count(*) as max_recs from key_column_usage limit 0,5;

select information_schema.tables.table_name as "table name",
       count(distinct(column_name)) as "no of columns in the table"
  from information_schema.tables left outer join information_schema.columns on
    information_schema.tables.table_name = information_schema.columns.table_name
 group by information_schema.tables.table_name;

# Reference Manual 22.1.16 - we will add more ...:
# select * from parameters;
# select * from referential_constraints;
# select * from triggers;

let $message= root: simple select to check all - and never forget some - tables;
let $dd_part1= SELECT * FROM;
let $dd_part2= LIMIT 1;
--source suite/funcs_1/datadict/datadict_tables.inc

# check again, but from different database (will fail due to missing database name)
use db_datadict;

--error 1146
select * from schemata;
--error 1146
select * from tables;
--error 1146
select s.catalog_name, s.schema_name, s.default_character_set_name,
       t.table_type, t.engine
  from schemata s inner join tables t
 ORDER BY s.catalog_name, s.schema_name, s.default_character_set_name;
--error 1146
select * from columns limit 0, 5;
--error 1146
select * from character_sets limit 0, 5;
--error 1146
select * from collations limit 0, 5;
--error 1146
select * from collation_character_set_applicability limit 0, 5;
--error 1146
select * from routines limit 0, 5;
--error 1146
select * from statistics limit 0, 5;
--error 1146
select * from views limit 0, 5;
--error 1146
select * from user_privileges limit 0, 5;
--error 1146
select * from schema_privileges limit 0, 5;
--error 1146
select * from table_privileges limit 0, 5;
--error 1146
select * from column_privileges limit 0, 5;
--error 1146
select * from table_constraints limit 0, 5;
--error 1146
select * from key_column_usage limit 0, 5;
# Reference Manual 22.1.16 - we will add more ...:
# --error 1146
# select * from parameters;
# --error 1146
# select * from referential_constraints;
# --error 1146
# select * from triggers;
let $message= will fail due to missing database name;
let $dd_part1= SELECT * FROM;
let $dd_part2=;
--source suite/funcs_1/datadict/datadict_tables_error_1146.inc

# now check from "other" database, but with database name
select * from information_schema.schemata ORDER BY 2 DESC;

#SELECT * FROM information_schema.tables;
#
#FIXME 3.2.1.1: we split the "SELECT * FROM tables" in two parts until
#FIXME 3.2.1.1: Bug #12397: wrong values shown in column CREATE_OPTIONS of
#FIXME 3.2.1.1: INFORMATION_SCHEMA.TABLES is solved, one with 'more' and one
#FIXME 3.2.1.1: with 'less' replace
#  9 AVG_ROW_LENGTH
# 10 DATA_LENGTH
# 11 MAX_DATA_LENGTH
# 12 INDEX_LENGTH
# 13 DATA_FREE
# 15 CREATE_TIME
# 16 UPDATE_TIME
# 17 CHRCK_TIME
# 20 CREATE_OPTIONS
if ($have_bug_11589)
{
--disable_ps_protocol
}
--vertical_results
--replace_column  9 "#ARL#"  10 "#DL#"  11 "#MDL#"  12 "#IL#"  13 "#DF#"  15 "YYYY-MM-DD hh:mm:ss"  16 "YYYY-MM-DD hh:mm:ss" 17 "YYYY-MM-DD hh:mm:ss" 20 "#CO#"
SELECT * FROM information_schema.tables
 WHERE table_schema = 'information_schema';
#  9 AVG_ROW_LENGTH
# 10 DATA_LENGTH
# 11 MAX_DATA_LENGTH
# 12 INDEX_LENGTH
# 13 DATA_FREE
# 15 CREATE_TIME
# 16 UPDATE_TIME
# 17 CHRCK_TIME
--replace_column  9 "#ARL#"  10 "#DL#"  11 "#MDL#"  12 "#IL#"  13 "#DF#"  15 "YYYY-MM-DD hh:mm:ss"  16 "YYYY-MM-DD hh:mm:ss" 17 "YYYY-MM-DD hh:mm:ss"
SELECT * FROM information_schema.tables
 WHERE NOT( table_schema = 'information_schema');
--horizontal_results
--enable_ps_protocol

select s.catalog_name, s.schema_name, s.default_character_set_name,
       t.table_type, t.engine
  from information_schema.schemata s inner join information_schema.tables t
 ORDER BY s.schema_name, s.default_character_set_name, table_type, engine;

--source suite/funcs_1/datadict/datadict_bug_12777.inc
select * from information_schema.columns limit 0, 5;
select * from information_schema.character_sets limit 0, 5;
select * from information_schema.collations limit 0, 5;
select * from information_schema.collation_character_set_applicability limit 0, 5;
--replace_column 16 "YYYY-MM-DD hh:mm:ss" 17 "YYYY-MM-DD hh:mm:ss"
select * from information_schema.routines limit 0, 5;
select * from information_schema.statistics limit 0, 5;
select * from information_schema.views limit 0, 5;
--replace_result $SERVER_NAME <SERVER_NAME>
select * from information_schema.user_privileges limit 0, 5;
select * from information_schema.schema_privileges limit 0, 5;
select * from information_schema.table_privileges limit 0, 5;
select * from information_schema.column_privileges limit 0, 5;
select * from information_schema.table_constraints limit 0, 5;
select * from information_schema.key_column_usage limit 0, 5;
select count(*) as max_recs from information_schema.key_column_usage limit 0, 5;

# Reference Manual 22.1.16 - we will add more ...:
# select * from information_schema.parameters;
# select * from information_schema.referential_constraints;
# select * from information_schema.triggers;

let $message= root: check with db name;
let $dd_part1= SELECT COUNT(*) FROM information_schema.;
let $dd_part2=;
--source suite/funcs_1/datadict/datadict_tables.inc

# cleanup
USE db_datadict;
DROP VIEW v1, vu1, vu;
DROP PROCEDURE db_datadict.sp_1;
USE information_schema;
# ------------------------------------------------------------------------------

let $message= Testcase 3.2.1.2:;
--source include/show_msg80.inc

################################################################################
# Testcase 3.2.1.2: Ensure that queries on an INFORMATION_SCHEMA table can
#                   accept all SELECT statement options and are always
#                   correctly evaluated.
################################################################################

# currently here only a subset of select statement options is checked, it's still
# not possible to check here all possible options
select catalog_name, schema_name, default_character_set_name
  from schemata where schema_name like '%s%';

select count(*) as tot_tabs from tables;
select count(*) as the_cols from columns;

select max(maxlen) as the_max from character_sets;
select * from collations order by id asc  limit 0, 5;
select * from collation_character_set_applicability
 order by character_set_name desc, collation_name limit 0, 5;

select routine_definition from routines;
select * from statistics group by index_name asc  limit 0, 5;
select concat(table_schema, ', ', table_name, ', ', view_definition) view_info
  from views;
select concat(table_schema, ', ', table_name) "Table_info"
  from tables ORDER BY 1;

--replace_result $SERVER_NAME <SERVER_NAME>
select distinct grantee from user_privileges order by grantee, privilege_type;
select * from schema_privileges where table_catalog is null limit 0, 5;
select * from table_privileges where grantee like '%r%'  limit 0, 5;

select * from column_privileges where table_catalog is not null limit 0, 5;
select HIGH_PRIORITY * from table_constraints
 group by constraint_name desc  limit 0, 5;
select sum(ordinal_position) from key_column_usage;

select * from schemata limit 0,5;
select * from schemata  limit 0,5;
--replace_result $SERVER_NAME <SERVER_NAME>
select distinct grantee from user_privileges;
--replace_result $SERVER_NAME <SERVER_NAME>
select all      grantee from user_privileges order by grantee, privilege_type;

select id , character_set_name from collations order by id asc limit 10;

select table_catalog from columns
 union all
select table_catalog from tables limit 0,5;
select table_catalog from columns
 union
select table_catalog from tables limit 0,5;

select all schema_name from information_schema.schemata;

# the $ENGINE_TYPE variable is used here ONLY to have 3 different file names in
# the three datadict testcases innodb_*, memory_* and myisam_*
eval SELECT *
  INTO OUTFILE '../tmp/out.$ENGINE_TYPE.file'
  FIELDS TERMINATED BY ',' OPTIONALLY ENCLOSED BY '"'
  LINES TERMINATED BY '\n'
  FROM schemata LIMIT 0, 5;

USE test;

eval SELECT *
  INTO OUTFILE '../tmp/out.$ENGINE_TYPE.db.file'
  FIELDS TERMINATED BY ',' OPTIONALLY ENCLOSED BY '"'
  LINES TERMINATED BY '\n'
  FROM information_schema.schemata
  WHERE schema_name LIKE 'db_%';

# check also with a 'simple' user
CREATE USER user_3212@localhost;
GRANT ALL ON db_datadict.* TO user_3212@localhost;

--replace_result $MASTER_MYPORT MYSQL_PORT $MASTER_MYSOCK MYSQL_SOCK
connect (u3212,localhost,user_3212,,db_datadict);
--source suite/funcs_1/include/show_connection.inc

# no db given --> db_datadict.schema does not exist
--error 1045
eval SELECT *
  INTO OUTFILE '../tmp/out.$ENGINE_TYPE.user.file'
  FIELDS TERMINATED BY ',' OPTIONALLY ENCLOSED BY '"'
  LINES TERMINATED BY '\n'
  FROM schemata LIMIT 0, 5;
# FIXME 3.2.1.2: why do we get different error numbers with and without OUTFILE ?
--error 1146
eval SELECT *
  FROM schemata LIMIT 0, 5;

eval SELECT *
  INTO OUTFILE '../tmp/out.$ENGINE_TYPE.user.db.file'
  FIELDS TERMINATED BY ',' OPTIONALLY ENCLOSED BY '"'
  LINES TERMINATED BY '\n'
  FROM information_schema.schemata
  WHERE schema_name LIKE 'db_%';
# The above error is bug 28181 - a regression in 5.0.42
eval SELECT *
  FROM information_schema.schemata
  WHERE schema_name LIKE 'db_%';

USE information_schema;

# no db given --> db_datadict.schema does not exist
eval SELECT *
  INTO OUTFILE '../tmp/out.$ENGINE_TYPE.user_2.file'
  FIELDS TERMINATED BY ',' OPTIONALLY ENCLOSED BY '"'
  LINES TERMINATED BY '\n'
  FROM schemata LIMIT 0, 5;
# The above error is bug 28181 - a regression in 5.0.42
eval SELECT *
  FROM schemata LIMIT 0, 5;

eval SELECT *
  INTO OUTFILE '../tmp/out.$ENGINE_TYPE.user_2.db.file'
  FIELDS TERMINATED BY ',' OPTIONALLY ENCLOSED BY '"'
  LINES TERMINATED BY '\n'
  FROM information_schema.schemata
  WHERE schema_name LIKE 'db_%';
# The above error is bug 28181 - a regression in 5.0.42
eval SELECT *
  FROM information_schema.schemata
  WHERE schema_name LIKE 'db_%';

disconnect u3212;
connection default;
USE information_schema;
--source suite/funcs_1/include/show_connection.inc

use db_datadict;
select table_catalog "1", table_schema "2", table_name "3", column_name "4"
  from information_schema.columns
union
select table_catalog, table_schema, table_name,
       concat( "*** type = ", table_type )
  from information_schema.tables
 order by 3, 4 desc, 1, 2 limit 30;

use information_schema;
--source suite/funcs_1/datadict/datadict_bug_12777.inc
select table_catalog "1", table_schema "2", table_name "3", column_name "4"
  from columns
union
select table_catalog, table_schema, table_name,
       concat( "*** type = ", table_type )
  from tables
 order by 3, 4 desc, 1, 2 limit 30;

# cleanup
DROP USER user_3212@localhost;

# ------------------------------------------------------------------------------

let $message= Testcase 3.2.1.3:;
--source include/show_msg80.inc

################################################################################
# Testcase 3.2.1.3:  Ensure that no user may execute an INSERT statement on any
#                    INFORMATION_SCHEMA table.
################################################################################

#FIXME: in this block we had --error 1288 until Mid Sep05, check the change!
--error 1044
insert into schemata (catalog_name, schema_name, default_character_set_name, sql_path)
  values ('null', 'db1', 'latin1', 'null');
--error 1044
insert into tables (table_schema, table_name)values('db_datadict', 't1');
--error 1044
insert into columns (table_name, column_name)values('t3', 'f2');
--error 1044
insert into character_sets (character_set_name, default_collate_name, description, maxlen)
  values('cp1251', 'cp1251_general_ci', 'windows  cyrillic', 1);
--error 1044
insert into collations ( collation_name, character_set_name, id, is_default, is_compiled, sortlen)
  values ('cp1251_bin', 'cp1251', 50, '', '', 0);
--error 1044
insert into collation_character_set_applicability (collation_name, character_set_name)
  values (' big5_chinese_ci', 'big6');
--error 1044
insert into routines(routine_name, routine_type ) values ('p2', 'procedure');
--error 1044
insert into statistics(table_schema, table_name, index_name)
  values ('mysql', 'db', 'primary');
--error 1044
insert into views(table_schema, table_name) values ('db2', 'v2');
--error 1044
insert into user_privileges (privilege_type, is_grantable) values ('select', 'yes');
--error 1044
insert into schema_privileges (table_schema, privilege_type) values('db2', 'insert');
--error 1044
insert into table_privileges (able_schema, table_name, privilege_type)
  values('db2', 'v2', 'insert');
--error 1044
insert into  column_privileges (table_name, column_name, privilege_type)
  values ('t3', 'f3', 'insert');
--error 1044
insert into table_constraints ( constraint_schema, constraint_name, table_schema)
  values ('primary', 'mysql', 'user');
--error 1044
insert into key_column_usage (constraint_schema, constraint_name, table_name)
  values ('mysql', 'primary', 'db');

# insert through a procedure
--disable_warnings
drop procedure if exists db_datadict.sp_4_1_3;
--enable_warnings

delimiter //;
create procedure db_datadict.sp_4_1_3()
begin
   insert into information_schema.schema_privileges (table_schema,privilege_type)
      values('db2','insert');
end//
delimiter ;//

#FIXME: check for the diffs Win ./. Linux
SELECT table_schema, privilege_type FROM information_schema.schema_privileges
 WHERE table_schema LIKE 'db%';

--error 1044
call db_datadict.sp_4_1_3();

#FIXME: check for the diffs Win ./. Linux
SELECT table_schema, privilege_type FROM information_schema.schema_privileges
 WHERE table_schema LIKE 'db%';

# cleanup
drop procedure db_datadict.sp_4_1_3;

# insert into information_schema as a limited user

CREATE USER user_4_1_3@localhost;

--replace_result $MASTER_MYPORT MYSQL_PORT $MASTER_MYSOCK MYSQL_SOCK
connect (u413,localhost,user_4_1_3,,test);
--source suite/funcs_1/include/show_connection.inc

use information_schema;

#FIXME: check later the change from 1288 to 1044 (since Mid Sep05)
--error 1044
insert into table_constraints ( constraint_schema, constraint_name,  table_schema)
  values ('primary', 'mysql', 'user');

connection default;
--source suite/funcs_1/include/show_connection.inc
# ------------------------------------------------------------------------------

let $message= Testcase 3.2.1.4:;
--source include/show_msg80.inc

################################################################################
# Testcase 3.2.1.4: Ensure that no user may execute an UPDATE statement on any
#                   INFORMATION_SCHEMA table.
################################################################################

use information_schema;
--source suite/funcs_1/include/show_connection.inc

#FIXME: check later the change from 1288 to 1044 (since Mid Sep05) in the whole next block
--error 1044
update schemata set schema_name = 'db5' where default_character_set_name = 'latin1';
--error 1044
update tables set table_schema = 'db_datadict1' where table_name = 't1';
--error 1044
update columns set table_name = 't4' where column_name = 'f2';
--error 1044
update character_sets set character_set_name = 'cp1252' where maxlen = 1;
--error 1044
update collations set collation_name = 'cp1253_bin'
 where character_set_name = 'cp1251';
--error 1044
update collation_character_set_applicability set collation_name = 'big6_chinese_ci'
  where character_set_name = 'big6';
--error 1044
update routines set routine_name = p2 where routine_body = 'sql';
--error 1044
update statistics set table_schema = 'mysql1' where table_name = 'db';
--error 1044
update views set table_schema = 'db3' where table_name = 'v1';
--error 1044
update  user_privileges set privilege_type = 'insert' where is_grantable = 'yes';
--error 1044
update schema_privileges set table_schema = 'db2' where privilege_type = 'select';
--error 1044
update table_privileges set table_name = 'v3' where privilege_type = 'select';
--error 1044
update column_privileges set table_name = 't4' where column_name = 'f3';
--error 1044
update  table_constraints set constraint_schema = 'primary'
 where table_schema = 'proc';
--error 1044
update key_column_usage set  table_name = 'db1' where constraint_name = 'primary';

# update through a procedure
--disable_warnings
drop procedure if exists db_datadict.sp_4_1_4;
--enable_warnings

delimiter //;
create procedure db_datadict.sp_4_1_4()
begin
  update information_schema.routines set routine_name = 'p2'
   where routine_name = 'sp_4_1_4';
end//
delimiter ;//

#FIXME: check for the diffs Win ./. Linux
--replace_column 16 "YYYY-MM-DD hh:mm:ss" 17 "YYYY-MM-DD hh:mm:ss"
select * from information_schema.routines;

--error 1044
call db_datadict.sp_4_1_4();

#FIXME: check for the diffs Win ./. Linux
--replace_column 16 "YYYY-MM-DD hh:mm:ss" 17 "YYYY-MM-DD hh:mm:ss"
select * from information_schema.routines;

# cleanup
drop procedure db_datadict.sp_4_1_4;

# update information_schema as a limited user

connection u413;

use information_schema;
--source suite/funcs_1/include/show_connection.inc

#FIXME: check later the change from 1288 to 1044 (since Mid Sep05)
--error 1044
update  user_privileges set privilege_type = 'insert' where is_grantable = 'yes';

connection default;
--source suite/funcs_1/include/show_connection.inc
#
# ------------------------------------------------------------------------------

let $message= Testcase 3.2.1.5:;
--source include/show_msg80.inc

################################################################################
# Testcase 3.2.1.5: Ensure that no user may execute a DELETE statement on any
#                   INFORMATION_SCHEMA table.
################################################################################

use information_schema;

let $message= root: DELETE FROM any table in IS;
let $dd_part1= DELETE FROM;
let $dd_part2=;
--source suite/funcs_1/datadict/datadict_tables_error_1044.inc

#FIXME: check later the change from 1288 to 1044 (since Mid Sep05) in the whole next block
--error 1044
delete from schemata where schema_name = 'mysql';
--error 1044
delete from tables where table_name = 'abc';
--error 1044
delete from columns;
--error 1044
delete from character_sets;
--error 1044
delete from collations;
--error 1044
delete from collation_character_set_applicability;
--error 1044
delete from routines;
--error 1044
delete from statistics;
--error 1044
delete from views;
--error 1044
delete from user_privileges;
--error 1044
delete from schema_privileges;
--error 1044
delete from table_privileges;
--error 1044
delete from column_privileges;
--error 1044
delete from table_constraints;
--error 1044
delete from key_column_usage;

# delete through a procedure
--disable_warnings
drop procedure if exists db_datadict.sp_4_1_5;
--enable_warnings

delimiter //;
create procedure db_datadict.sp_4_1_5()
begin
  delete from information_schema.column_privileges;
end//
delimiter ;//

--error 1044
call db_datadict.sp_4_1_5();

# cleanup
drop procedure db_datadict.sp_4_1_5;

# delete from information_schema as a limited user

connection u413;

use information_schema;
--source suite/funcs_1/include/show_connection.inc

#FIXME: check later the change from 1288 to 1044 (since Mid Sep05)
--error 1044
delete from tables where table_name = 'abc';

disconnect u413;

connection default;
--source suite/funcs_1/include/show_connection.inc

# cleanup
DROP USER user_4_1_3@localhost;
# ------------------------------------------------------------------------------

let $message= Testcase 3.2.1.6:;
--source include/show_msg80.inc

################################################################################
# Testcase 3.2.1.6: Ensure that no user may create an INFORMATION_SCHEMA base
#                   table.
################################################################################

use information_schema;

let $message= root: create a table with a name of an IS table directly in IS;
let $dd_part1= CREATE TABLE;
let $dd_part2= ( c1 INT );
--source suite/funcs_1/datadict/datadict_tables_error_1044.inc
#FIXME solved - 3.2.1.6: check for better error message - 42S02: Unknown table '<xxx>' in <db> ?
--error 1109
create table t1 (f1 int, f2 int, f3 int);

use db_datadict;

#FIXME: check correct error message - HY000 Can't create/write to file '.\information_schema\columns.frm' (Errcode: 2)
let $message= root: create a table with a name of an IS table from other db;
let $dd_part1= CREATE TABLE information_schema.;
let $dd_part2= ( c1 INT );
--source suite/funcs_1/datadict/datadict_tables_error_1044.inc
#FIXME solved - 3.2.1.6: check for better error message - 42S02: Unknown table '<xxx>' in <db> ?
--error 1109
create table information_schema.t1 (f1 int, f2 int, f3 int);

# create a table in information_schema as a limited user with sufficient permissions
CREATE USER user_4_1_6@localhost;

grant all on *.* to user_4_1_6@localhost;

FLUSH PRIVILEGES;

SHOW GRANTS FOR user_4_1_6@localhost;

--replace_result $MASTER_MYPORT MYSQL_PORT $MASTER_MYSOCK MYSQL_SOCK
connect (u2, localhost, user_4_1_6, , information_schema);
--source suite/funcs_1/include/show_connection.inc

use information_schema;

let $message= user: create a table with a name of an IS table directly in IS;
let $dd_part1= CREATE TABLE;
let $dd_part2= ( c1 INT );
--source suite/funcs_1/datadict/datadict_tables_error_1044.inc
#FIXME solved - 3.2.1.6: check for better error message - 42S02: Unknown table '<xxx>' in <db> ?
--error 1109
create table t1 (f1 int, f2 int, f3 int);

use test;

#FIXME 3.2.1.6: check correct error message - HY000 Can't create/write to file '.\information_schema\columns.frm' (Errcode: 2)
let $message= user: create a table with a name of an IS table from other db;
let $dd_part1= CREATE TABLE information_schema.;
let $dd_part2= ( c1 INT );
--source suite/funcs_1/datadict/datadict_tables_error_1044.inc
#FIXME solved - 3.2.1.6: check for better error message - 42S02: Unknown table '<xxx>' in <db> ?
--error 1109
create table information_schema.t1 (f1 int, f2 int, f3 int);

#cleanup
connection default;
--source suite/funcs_1/include/show_connection.inc
disconnect u2;
DROP USER user_4_1_6@localhost;
# ------------------------------------------------------------------------------

let $message= Testcase 3.2.1.7:;
--source include/show_msg80.inc

################################################################################
# Testcase 3.2.1.7: Ensure that no user may create an INFORMATION_SCHEMA view.
################################################################################

use information_schema;

let $message= root: create a view with a name of an IS table directly in IS;
let $dd_part1= CREATE VIEW ;
let $dd_part2= AS SELECT * FROM mysql.time_zone;
#FIXME: check change from error 1 to 1044
--source suite/funcs_1/datadict/datadict_tables_error_1044.inc

--error 1109
CREATE VIEW v1 AS SELECT * FROM information_schema.schemata;

USE db_datadict;

let $message= root: create a view with a name of an IS table from other db;
let $dd_part1= CREATE VIEW information_schema.;
let $dd_part2= AS SELECT * FROM mysql.time_zone;
#FIXME: check change from error 1 to 1044
--source suite/funcs_1/datadict/datadict_tables_error_1044.inc

# ... but creating a view that 'uses' the information_schema is allowed:
CREATE VIEW v1 AS SELECT * FROM information_schema.columns;

SELECT * FROM v1 LIMIT 5;

# create a view in information_schema as a limited user with sufficient permissions
CREATE USER user_4_1_7@localhost;

GRANT ALL ON db_datadict.*        TO user_4_1_7@localhost;
#FIXME: check that GRANT ON i_s is no longer allowed
--error 1044
GRANT ALL ON information_schema.* TO user_4_1_7@localhost;

FLUSH PRIVILEGES;

--replace_result $MASTER_MYPORT MYSQL_PORT $MASTER_MYSOCK MYSQL_SOCK
connect (u3, localhost, user_4_1_7, , db_datadict);
use information_schema;
--source suite/funcs_1/include/show_connection.inc

let $message= user: create a view with a name of an IS table directly in IS;
let $dd_part1= CREATE VIEW ;
let $dd_part2= AS SELECT * FROM db_datadict.v1;
#FIXME: check change from error 1 to 1044
--source suite/funcs_1/datadict/datadict_tables_error_1044.inc
--error 1109
create view v1 as select * from table_privileges;

use db_datadict;

let $message= user: create a view with a name of an IS table from other db;
let $dd_part1= CREATE VIEW information_schema.;
let $dd_part2= AS SELECT * FROM db_datadict.v1;
#FIXME: check change from error 1 to 1044
--source suite/funcs_1/datadict/datadict_tables_error_1044.inc
disconnect u3;

# cleanup
connection default;
--source suite/funcs_1/include/show_connection.inc
DROP USER user_4_1_7@localhost;
DROP VIEW db_datadict.v1;
# ------------------------------------------------------------------------------

let $message= Testcase 3.2.1.8:;
--source include/show_msg80.inc

################################################################################
# Testcase 3.2.1.8: Ensure that no user may create an index on an
#                   INFORMATION_SCHEMA table.
################################################################################

use information_schema;

#FIXME: check later the change from 1288 to 1044 (since Mid Sep05)
--error 1044
create index i1 on schemata(schema_name);
--error 1044
create index i2 on tables(table_schema);
--error 1044
create index i3 on columns(table_name);

--error 1044
create index i4 on character_sets(character_set_name);
--error 1044
create index i5 on collations( collation_name);
--error 1044
create index i6 on collation_character_set_applicability(collation_name);

--error 1044
create index i7 on routines(routine_name);
--error 1044
create index i8 on statistics(table_schema);
--error 1044
create index i9 on views(table_schema);

--error 1044
create index i10 on user_privileges(privilege_type);
--error 1044
create index i11 on schema_privileges(table_schema);
--error 1044
create index i12 on table_privileges(able_schema);

--error 1044
create index i13 on column_privileges(table_name);
--error 1044
create index i14 on table_constraints(constraint_schema);
--error 1044
create index i15 on key_column_usage(constraint_schema);
--error 1044
create index i16 on triggers(trigger_name);

use db_datadict;
--error 1044
create index i15 on information_schema.key_column_usage(constraint_schema);

use information_schema;

# create an index on an information_schema table as a limited user with sufficient permissions
CREATE USER user_4_1_8@localhost;

grant select, index on *.* to user_4_1_8@localhost;

FLUSH PRIVILEGES;

--replace_result $MASTER_MYPORT MYSQL_PORT $MASTER_MYSOCK MYSQL_SOCK
connect (u4, localhost, user_4_1_8, , test);
--source suite/funcs_1/include/show_connection.inc

use information_schema;

--error 1044
create index i1 on schemata(schema_name);
--error 1044
create index i2 on tables(table_schema);
--error 1044
create index i3 on columns(table_name);

--error 1044
create index i4 on character_sets(character_set_name);
--error 1044
create index i5 on collations( collation_name);
--error 1044
create index i6 on collation_character_set_applicability(collation_name);

--error 1044
create index i7 on routines(routine_name);
--error 1044
create index i8 on statistics(table_schema);
--error 1044
create index i9 on views(table_schema);

--error 1044
create index i10 on user_privileges(privilege_type);
--error 1044
create index i11 on schema_privileges(table_schema);
--error 1044
create index i12 on table_privileges(able_schema);

--error 1044
create index i13 on column_privileges(table_name);
--error 1044
create index i14 on table_constraints(constraint_schema);
--error 1044
create index i15 on key_column_usage(constraint_schema);
--error 1044
create index i16 on triggers(trigger_name);

use db_datadict;
--error 1044
create index i15 on information_schema.key_column_usage(constraint_schema);

disconnect u4;

connection default;
--source suite/funcs_1/include/show_connection.inc

# cleanup
DROP USER user_4_1_8@localhost;
# ------------------------------------------------------------------------------

let $message= Testcase 3.2.1.9:;
--source include/show_msg80.inc

################################################################################
# Testcase 3.2.1.9: Ensure that no user may alter the definition of an
#                   INFORMATION_SCHEMA table.
################################################################################

#FIXME: 3.2.1.9 check for better error message than ERROR 42S02: Table 'information_schema.schemata' doesn't exist

# first simple check all known - so we never forget one ...
let $message= root: alter a table from other db;
let $dd_part1= ALTER TABLE information_schema.;
let $dd_part2= ADD f1 INT;
#FIXME: check change from error 1146 to 1044
--source suite/funcs_1/datadict/datadict_tables_error_1044.inc
use information_schema;
let $message= root: alter a table from directly;
let $dd_part1= ALTER TABLE ;
let $dd_part2= ADD f1 INT;
#FIXME: check change from error 1146 to 1044
--source suite/funcs_1/datadict/datadict_tables_error_1044.inc
# now more detailed checks
--error 1044
alter table schemata add f1 int;
--error 1044
alter table tables drop primary key;
--error 1044
alter table columns add f1 int;
--error 1044
alter table character_sets disable keys;
--error 1044
alter table collations enable keys;
--error 1044
alter table collation_character_set_applicability add f1 int;
--error 1044
alter table routines discard tablespace;
--error 1044
alter table statistics import tablespace;
--error 1044
alter table views drop column table_name;
--error 1044
alter table user_privileges drop index privilege_type;
--error 1044
alter table schema_privileges drop column is_grantable;
--error 1044
alter table table_privileges order by constraint_type;
--error 1044
alter table column_privileges rename to aaxyz;
--error 1044
alter table table_constraints order by schema_name;
--error 1044
alter table key_column_usage rename to information_schema.aabxyz;
--error 1044
alter table triggers rename to information_schema.sql_mode;
# Alter an information_schema table as a limited user with sufficient permissions

CREATE USER user_4_1_9@localhost;

grant select, alter, create, insert on *.* to user_4_1_9@localhost;

FLUSH PRIVILEGES;

--replace_result $MASTER_MYPORT MYSQL_PORT $MASTER_MYSOCK MYSQL_SOCK
connect (u5, localhost, user_4_1_9, , db_datadict);
--source suite/funcs_1/include/show_connection.inc

use db_datadict;

let $message= user: alter a table from other db;
let $dd_part1= ALTER TABLE information_schema.;
let $dd_part2= ADD f1 INT;
#FIXME: check change from error 1146 to 1044
--source suite/funcs_1/datadict/datadict_tables_error_1044.inc
use information_schema;
let $message= user: alter a table from directly;
let $dd_part1= ALTER TABLE ;
let $dd_part2= ADD f1 INT;
#FIXME: check change from error 1146 to 1044
--source suite/funcs_1/datadict/datadict_tables_error_1044.inc
disconnect u5;

# cleanup
connection default;
--source suite/funcs_1/include/show_connection.inc
DROP USER user_4_1_9@localhost;
# ------------------------------------------------------------------------------

let $message= Testcase 3.2.1.10:;
--source include/show_msg80.inc

################################################################################
# Testcase 3.2.1.10: Ensure that no user may drop an INFORMATION_SCHEMA table.
################################################################################

use information_schema;

let $message= root: drop a table from IS;
let $dd_part1= DROP TABLE ;
let $dd_part2=;
#FIXME: check change from error 1051 to 1044
--source suite/funcs_1/datadict/datadict_tables_error_1044.inc
use db_datadict;

let $message= root: drop a table from other db;
let $dd_part1= DROP TABLE information_schema.;
let $dd_part2=;
#FIXME: check change from error 1051 to 1044
--source suite/funcs_1/datadict/datadict_tables_error_1044.inc
use information_schema;

# drop an information_schema table as a limited user with sufficient permissions

CREATE USER user_4_1_10@localhost;

GRANT SELECT, DROP ON *.* TO user_4_1_10@localhost;

FLUSH PRIVILEGES;

--replace_result $MASTER_MYPORT MYSQL_PORT $MASTER_MYSOCK MYSQL_SOCK
connect (u6,localhost,user_4_1_10,,db_datadict);

use information_schema;
--source suite/funcs_1/include/show_connection.inc

let $message= user: drop a table from IS;
let $dd_part1= DROP TABLE ;
let $dd_part2=;
#FIXME: check change from error 1051 to 1044
--source suite/funcs_1/datadict/datadict_tables_error_1044.inc
use db_datadict;

let $message= user: drop a table from other db;
let $dd_part1= DROP TABLE information_schema.;
let $dd_part2=;
#FIXME: check change from error 1051 to 1044
--source suite/funcs_1/datadict/datadict_tables_error_1044.inc
disconnect u6;

connection default;
--source suite/funcs_1/include/show_connection.inc

# cleanup
DROP USER user_4_1_10@localhost;

# Try to carry out information_schema modification operations with a user other than root having SUPER privileges
CREATE USER user_4_1_11@localhost;

GRANT SUPER ON *.* TO user_4_1_11@localhost;

FLUSH PRIVILEGES;

--replace_result $MASTER_MYPORT MYSQL_PORT $MASTER_MYSOCK MYSQL_SOCK
connect (u7,localhost,user_4_1_11, ,test);

use information_schema;
--source suite/funcs_1/include/show_connection.inc

--error 1044
drop table routines;

--error 1044
alter table collations enable keys;

--error 1044
create index i5 on collations( collation_name );

--error 1109
create view v1 as select * from schemata;

--error 1044
delete from columns;

--error 1044
update columns set table_name = 't4' where column_name = 'f2';

--error 1044
insert into collations ( collation_name, character_set_name, id, is_default,
  is_compiled, sortlen)
  values ('cp1251_bin', 'cp1251', 50, '', '', 0);

disconnect u7;

connection default;
--source suite/funcs_1/include/show_connection.inc
DROP USER user_4_1_11@localhost;
# ------------------------------------------------------------------------------

let $message= Testcase 3.2.1.11:;
--source include/show_msg80.inc

################################################################################
# Testcase 3.2.1.11: Ensure that no user may move an INFORMATION_SCHEMA table
#                    to any other database.
################################################################################

--disable_warnings
DROP DATABASE IF EXISTS db_datadict;
--enable_warnings

CREATE DATABASE db_datadict;
CREATE USER 'u_6_401011'@'localhost';

#FIXME: check that GRANT ON i_s is no longer allowed
--error 1044
GRANT ALL ON information_schema.* TO 'u_6_401011'@'localhost';
GRANT ALL ON db_datadict.* TO 'u_6_401011'@'localhost';

FLUSH PRIVILEGES;

--error 1044
ALTER TABLE information_schema.schemata RENAME db_datadict.schemata;

let $message= root: move table to other DB;
let $dd_part1= ALTER TABLE information_schema.;
let $dd_part2= RENAME db_datadict.tb_01;
#FIXME: check change from error 1146 to 1044
--source suite/funcs_1/datadict/datadict_tables_error_1044.inc
--replace_result $MASTER_MYPORT MYSQL_PORT $MASTER_MYSOCK MYSQL_SOCK
connect (u_6_401011, localhost, u_6_401011, , db_datadict);

USE information_schema;
--source suite/funcs_1/include/show_connection.inc

--error 1044
ALTER TABLE information_schema.schemata RENAME db_datadict.schemata;

let $message= user: move table to other DB;
let $dd_part1= ALTER TABLE information_schema.;
let $dd_part2= RENAME db_datadict.tb_01;
#FIXME: check change from error 1146 to 1044
--source suite/funcs_1/datadict/datadict_tables_error_1044.inc
disconnect u_6_401011;

connection default;
--source suite/funcs_1/include/show_connection.inc

#cleanup
--disable_warnings
#DROP DATABASE db_datadict;
DROP TABLE IF EXISTS db_datadict.schemata;
DROP USER 'u_6_401011'@'localhost';
--enable_warnings
# ------------------------------------------------------------------------------

let $message= Testcase 3.2.1.12:;
--source include/show_msg80.inc

################################################################################
# Testcase 3.2.1.12: Ensure that no user may directly add to, alter, or delete
#                    any data in an INFORMATION_SCHEMA table.
################################################################################

# first as root a DELETE check on all tables
let $message= root: delete from IS tables;
let $dd_part1= DELETE FROM information_schema.;
let $dd_part2=;
#FIXME: check change from error 1288 to 1044
--source suite/funcs_1/datadict/datadict_tables_error_1044.inc
# check UPDATE for all ...
--error 1044
UPDATE information_schema.tables SET table_name = 't_4711';
--error 1044
UPDATE information_schema.columns SET table_name = 't_4711';
--error 1044
UPDATE information_schema.statistics SET table_name = 't_4711';
--error 1044
UPDATE information_schema.views SET table_name = 't_4711';
--error 1044
UPDATE information_schema.table_privileges SET table_name = 't_4711';
--error 1044
UPDATE information_schema.column_privileges SET table_name = 't_4711';
--error 1044
UPDATE information_schema.table_constraints SET table_name = 't_4711';
--error 1044
UPDATE information_schema.key_column_usage SET table_name = 't_4711';
--error 1044
UPDATE information_schema.schemata SET catalog_name = 't_4711';
--error 1044
UPDATE information_schema.character_sets SET description = 't_4711';
--error 1044
UPDATE information_schema.collations SET character_set_name = 't_4711';
--error 1044
UPDATE information_schema.collation_character_set_applicability
   SET character_set_name = 't_4711';
--error 1044
UPDATE information_schema.routines SET routine_type = 't_4711';
--error 1044
UPDATE information_schema.user_privileges SET grantee = 't_4711';
--error 1044
UPDATE information_schema.schema_privileges SET grantee = 't_4711';
--error 1044
UPDATE information_schema.triggers SET sql_mode = 't_4711';

CREATE USER 'u_6_401012'@'localhost';

--replace_result $MASTER_MYPORT MYSQL_PORT $MASTER_MYSOCK MYSQL_SOCK
connect (u_6_401012, localhost, u_6_401012, , test);

use information_schema;

--error 1044
insert into information_schema.schemata  (catalog_name, schema_name,
  default_character_set_name, sql_path)
  values (null, information_schema1, utf16, null);
#FIXME: check later the change from 1142 to 1044 (since Mid Sep05)
--error 1044
alter table information_schema.schemata rename db_datadict1.schemata;
#FIXME: check later the change from 1146 to 1044 (since Mid Sep05)
--error 1044
alter table information_schema.tables drop column checksum;
--error 1044
alter table information_schema.statistics modify packed int;
--error 1044
alter table information_schema.routines modify created int not null;
--error 1044
alter table information_schema.key_column_usage drop column ordinal_position;
--error 1044
alter table information_schema.table_privileges
  change privilege_type rights_approved varchar(32);
--error 1044
update columns set table_name = 't4' where column_name = 'f2';
--error 1044
delete from information_schema.collations;

disconnect u_6_401012;

# cleanup
connection default;
--source suite/funcs_1/include/show_connection.inc

--disable_warnings
drop table if exists db_datadict1.schemata;
DROP USER 'u_6_401012'@'localhost';
--enable_warnings
#-------------------------------------------------------------------------------------------------

let $message= Testcase 3.2.1.13:;
--source include/show_msg80.inc

################################################################################
# Testcase 3.2.1.13: Ensure that the creation of any new database object
#                    (e.g. table or column) automatically inserts all relevant
#                    information on that object into every appropriate
#                    INFORMATION_SCHEMA table.
################################################################################

use information_schema;

let $dbname=db_datadict;
let $message= first check status >before< creating the objects ...;
--source suite/funcs_1/datadict/datadict_show_schema.inc

--disable_warnings
DROP DATABASE IF EXISTS db_datadict;
--enable_warnings
CREATE DATABASE db_datadict;
USE db_datadict;

eval create table res_t_401013(f1 char(10), f2 char(25), f3 int)
            engine = $ENGINE_TYPE;
create view res_v_401013 as select * from res_t_401013;
CREATE USER u_6_401013@localhost;
create procedure sp_6_401013() select 'db_datadict';
create function fn_6_401013() returns int return 0;
create index i_6_401013 on res_t_401013(f3);

use information_schema;

let $message= now check whether all new objects exists in IS ...;
--source suite/funcs_1/datadict/datadict_show_schema.inc

# cleanup objects
use db_datadict;
drop index i_6_401013 on res_t_401013;
drop table db_datadict.res_t_401013;
drop view  db_datadict.res_v_401013;
DROP USER u_6_401013@localhost;
drop procedure sp_6_401013;
drop function fn_6_401013;
drop database db_datadict;

use information_schema;

let $message= and now check whether all objects are removed from IS ...;
--source suite/funcs_1/datadict/datadict_show_schema.inc
# ------------------------------------------------------------------------------

let $message= Testcase 3.2.1.14:;
--source include/show_msg80.inc

################################################################################
# Testcase 3.2.1.14: Ensure that the alteration of any existing database object
#                    automatically updates all relevant information on that
#                    object in every appropriate INFORMATION_SCHEMA table.
################################################################################

--disable_warnings
DROP DATABASE IF EXISTS db_datadict;
--enable_warnings
CREATE DATABASE db_datadict;
USE db_datadict;

create table res_t_401014(f1 char(10), f2 text(25), f3 int);
create view res_v_401014 as select * from res_t_401014;
create procedure sp_6_401014() select 'db_datadict';
create function fn_6_401014() returns int return 0;

# check current information in information_schema

let $dbname=db_datadict;
let $message= show existing objects >before< changing them ...;
--source suite/funcs_1/datadict/datadict_show_schema.inc

# alter objects

use db_datadict;

alter table res_t_401014 change f1 ff1 int;
alter table res_t_401014 engine = innodb;
alter table res_t_401014 change f3 f3_new bigint;
alter view res_v_401014 as select ff1 from res_t_401014;
alter procedure sp_6_401014 sql security invoker;
alter function fn_6_401014 comment 'updated comments';
alter database db_datadict character set utf8;

# check updated information in information_schema
let $message= now check whether the changes are visible in IS ...;
--source suite/funcs_1/datadict/datadict_show_schema.inc


# cleanup

use db_datadict;
drop table db_datadict.res_t_401014;
drop view  db_datadict.res_v_401014;
drop procedure sp_6_401014;
drop function fn_6_401014;
drop database db_datadict;
# ------------------------------------------------------------------------------

let $message= Testcase 3.2.1.15:;
--source include/show_msg80.inc

################################################################################
# Testcase 3.2.1.15: Ensure that the dropping of any existing database object
#                    automatically deletes all relevant information on that
#                    object from every appropriate INFORMATION_SCHEMA table.
################################################################################

--disable_warnings
DROP DATABASE IF EXISTS db_datadict;
--enable_warnings
CREATE DATABASE db_datadict;
USE db_datadict;

create table res_t_401015(f1 char(10), f2 text(25), f3 int);
create view res_v_401015 as select * from res_t_401015;
CREATE USER u_6_401015@localhost;
create procedure sp_6_401015() select 'test';
create function fn_6_401015() returns int return 0;
create index i_6_401015 on res_t_401015(f3);

let $dbname=db_datadict;
let $message= show existing objects >before< dropping them ...;
--source suite/funcs_1/datadict/datadict_show_schema.inc

use db_datadict;
drop index i_6_401015 on res_t_401015;
drop table db_datadict.res_t_401015;
drop view  db_datadict.res_v_401015;
DROP USER u_6_401015@localhost;
drop procedure sp_6_401015;
drop function fn_6_401015;
#drop database db_datadict;

let $message= now check they are really gone ...;
--source suite/funcs_1/datadict/datadict_show_schema.inc
# ------------------------------------------------------------------------------

let $message= Testcase 3.2.1.16:;
--source include/show_msg80.inc

################################################################################
# Testcase 3.2.1.16: Ensure that no user may use any INFORMATION_SCHEMA table to
#                    determine any information on a database and/or its
#                    structure unless authorized to get that information.
################################################################################

--disable_warnings
DROP DATABASE IF EXISTS db_datadict;
--enable_warnings
CREATE DATABASE db_datadict;

CREATE DATABASE db_hidden;
USE db_hidden;
CREATE TABLE tb_hidden ( c1 TEXT );

USE db_datadict;

CREATE TABLE res_t_401016(f1 char(10),f2 text(25),f3 int);
CREATE TABLE res_t_401016_1(f1 char(10),f2 text(25),f3 int);

CREATE USER 'u_6_401016'@'localhost';
GRANT SELECT ON db_datadict.res_t_401016 TO 'u_6_401016'@'localhost';

FLUSH PRIVILEGES;

--replace_result $MASTER_MYPORT MYSQL_PORT $MASTER_MYSOCK MYSQL_SOCK
connect (u_6_401016, localhost, u_6_401016, , test);

USE information_schema;

SELECT table_schema, table_name, engine
  FROM TABLES;
# WHERE table_name LIKE 'res_t_401016%';

SHOW TABLES;
SELECT * FROM schemata;

disconnect u_6_401016;

connection default;
--source suite/funcs_1/include/show_connection.inc

#FIXME: check that GRANT ON i_s is no longer allowed
--error 1044
grant usage on information_schema.* to 'u_6_401016'@'localhost';
FLUSH PRIVILEGES;

--replace_result $MASTER_MYPORT MYSQL_PORT $MASTER_MYSOCK MYSQL_SOCK
connect (u_6_401016_1, localhost, u_6_401016, , db_datadict);

USE information_schema;
SHOW TABLES;
SELECT * FROM schemata;

disconnect u_6_401016_1;

# all tables are checked again later with permission tests

# cleanup
connection default;
use db_datadict;
--source suite/funcs_1/include/show_connection.inc
DROP USER 'u_6_401016'@'localhost';
drop table res_t_401016;
drop table res_t_401016_1;
DROP DATABASE db_hidden;
#drop database db_datadict;
# ------------------------------------------------------------------------------

let $message= Testcase 3.2.1.17:;
--source include/show_msg80.inc

################################################################################
# Testcase 3.2.1.17: Ensure that the SELECT privilege is granted TO PUBLIC WITH
#                    GRANT OPTION on every INFORMATION_SCHEMA table.
################################################################################

CREATE USER 'u_6_401017'@'localhost';

#FIXME: check that GRANT ON i_s is no longer allowed
--error 1044
grant select on information_schema.* to u_6_401017@localhost;

FLUSH PRIVILEGES;

--replace_result $MASTER_MYPORT MYSQL_PORT $MASTER_MYSOCK MYSQL_SOCK
connect (u_6_401017, localhost, u_6_401017, , test);

use information_schema;

select * from collation_character_set_applicability;
select * from schemata;
select table_name from tables;
--source suite/funcs_1/datadict/datadict_bug_12777.inc
select table_name, column_name, column_type from columns;
select character_set_name from character_sets;
select collation_name from collations;
select routine_name, routine_type from routines;
select table_name, index_name from statistics;
select table_name from views;
select privilege_type from user_privileges;
select grantee, privilege_type from schema_privileges;
select * from table_privileges;
select column_name, privilege_type from column_privileges;
select table_name,constraint_type from table_constraints;
select table_schema, table_name, column_name from key_column_usage;

disconnect u_6_401017;

# cleanup
connection default;
--source suite/funcs_1/include/show_connection.inc
DROP USER 'u_6_401017'@'localhost';
# ------------------------------------------------------------------------------

let $message= Testcase 3.2.1.18:;
--source include/show_msg80.inc

################################################################################
# Testcase 3.2.1.18: Ensure that the CREATE VIEW privilege on an
#                    INFORMATION_SCHEMA table may be granted to any user.
################################################################################

CREATE USER 'u_6_401018'@'localhost';

#FIXME: check GRANT on IS
--error 1044
GRANT CREATE VIEW ON information_schema.* TO 'u_6_401018'@'localhost';
GRANT ALL         ON db_datadict.*        TO 'u_6_401018'@'localhost';

SHOW GRANTS FOR 'u_6_401018'@'localhost';
FLUSH PRIVILEGES;

--disable_warnings
DROP DATABASE IF EXISTS db_datadict;
--enable_warnings
CREATE DATABASE db_datadict;

--replace_result $MASTER_MYPORT MYSQL_PORT $MASTER_MYSOCK MYSQL_SOCK
connect (u_6_401018, localhost, u_6_401018, , test);

USE db_datadict;

create view db_datadict.v_401018 as
 select * from information_schema.schemata;
SELECT * FROM v_401018 ORDER BY 2 DESC;

disconnect u_6_401018;

#cleanup
connection default;
--source suite/funcs_1/include/show_connection.inc
DROP USER 'u_6_401018'@'localhost';
DROP DATABASE db_datadict;
# ------------------------------------------------------------------------------

let $message= Testcase 3.2.1.19:;
--source include/show_msg80.inc

################################################################################
# Testcase 3.2.1.19: Ensure that no other privilege on an INFORMATION_SCHEMA
#                    table is granted, or may be granted, to any user.
################################################################################

CREATE USER 'u_6_401019'@'localhost';

#FIXME: check GRANT on IS
--error 1044
grant alter on information_schema.* to 'u_6_401019'@'localhost';

#FIXME: check GRANT on IS
--error 1044
grant alter routine on information_schema.* to 'u_6_401019'@'localhost';

#FIXME: check GRANT on IS
--error 1044
grant create on information_schema.* to 'u_6_401019'@'localhost';

#FIXME: check GRANT on IS
--error 1044
grant create routine on information_schema.* to 'u_6_401019'@'localhost';

#FIXME: check GRANT on IS
--error 1044
grant create temporary tables
   on information_schema.* to 'u_6_401019'@'localhost';

#FIXME: check GRANT on IS
--error 1044
grant delete on information_schema.* to 'u_6_401019'@'localhost';

#FIXME: check GRANT on IS
--error 1044
grant drop on information_schema.* to 'u_6_401019'@'localhost';

#FIXME: check GRANT on IS
--error 1044
grant execute on information_schema.* to 'u_6_401019'@'localhost';

#FIXME: check GRANT on IS
--error 1044
grant index on information_schema.* to 'u_6_401019'@'localhost';

#FIXME: check GRANT on IS
--error 1044
grant insert on information_schema.* to 'u_6_401019'@'localhost';

#FIXME: check GRANT on IS
--error 1044
grant lock tables on information_schema.* to 'u_6_401019'@'localhost';

#FIXME: check GRANT on IS
--error 1044
grant update on information_schema.* to 'u_6_401019'@'localhost';

SELECT * FROM information_schema.table_privileges
 WHERE table_schema = "information_schema";
SELECT * FROM information_schema.column_privileges
 WHERE table_schema = "information_schema";

# cleanup
DROP USER 'u_6_401019'@'localhost';
# ------------------------------------------------------------------------------

let $message= Testcase 3.2.1.20:;
--source include/show_msg80.inc

################################################################################
# Testcase 3.2.1.20: Ensure that USE INFORMATION_SCHEMA allows the user to
#                    switch to the INFORMATION_SCHEMA database, for query
#                    purposes only.
################################################################################

CREATE USER 'u_6_401020'@'localhost';

--replace_result $MASTER_MYPORT MYSQL_PORT $MASTER_MYSOCK MYSQL_SOCK
connect (u_6_401020, localhost, u_6_401020, , test);

USE information_schema;
SELECT * FROM schemata;

--error 1109
CREATE TABLE tb_not_allowed ( col TEXT );
#FIXME 3.2.1.20: bad message: ERROR 42S02: Unknown table 'tb_not_allowed' in information_schema
#FIXME 3.2.1.20: better:      ERROR 42000: Access denied for user 'u_6_401020'@'localhost' to database 'information_schema'

--error 1109
create view res_v1 as select * from information_schema.schemata;

--error 1044
alter table schemata modify catalog_name varchar(255);

--error 1044
update schemata set catalog_name = 'abc'
 where schema_name = 'information_schema';

delimiter //;
--error ER_BAD_DB_ERROR
CREATE PROCEDURE sp_3_2_1_20()
  BEGIN
    INSERT INTO information_schema.schema_privileges (table_schema,privilege_type)
      VALUES('db2','insert');
  END//
delimiter ;//

--error 1044
DELETE FROM schemata WHERE schema_name = 'information_schema';

disconnect u_6_401020;

# cleanup
connection default;
--source suite/funcs_1/include/show_connection.inc
DROP USER 'u_6_401020'@'localhost';
# ------------------------------------------------------------------------------

let $message= Testcase 3.2.2.1:;
--source include/show_msg80.inc

################################################################################
# Testcase 3.2.2.1:  Ensure that the INFORMATION_SCHEMA.CHARACTER_SETS
#                    table has the following columns, in the following order:
#
#                    CHARACTER_SET_NAME (shows a character set name),
#                    DEFAULT_COLLATE_NAME (shows the name of the default
#                          collation for that character set),
#                    DESCRIPTION (shows a descriptive name for that character
#                          set),
#                    MAXLEN (shows the number of bytes used to store each
#                          character supported by that character set).
################################################################################

let $is_table= character_sets;
--source suite/funcs_1/datadict/datadict_show_table_design.inc
# ------------------------------------------------------------------------------

let $message= Testcase 3.2.2.2:;
--source include/show_msg80.inc

################################################################################
# Testcase 3.2.2.2: Ensure that the table shows the relevant information on
#                   every character set for which the current user or PUBLIC
#                   have the USAGE privilege.
################################################################################

--source suite/funcs_1/include/show_connection.inc
SELECT * FROM information_schema.character_sets;
# ------------------------------------------------------------------------------

let $message= Testcase 3.2.2.3:;
--source include/show_msg80.inc

################################################################################
# Testcase 3.2.2.3:  Ensure that the table shows the relevant information on
#                    every character set for which the current user or PUBLIC
#                    have the USAGE privilege.
################################################################################

# Test requirement is erroneous... we cannot grant / revoke privilege for using
# a character set.
# ------------------------------------------------------------------------------

let $message= Testcase 3.2.3.1:;
--source include/show_msg80.inc

################################################################################
# Testcase 3.2.3.1:  Ensure that the INFORMATION_SCHEMA.COLLATIONS
#                    table has the following columns, in the following order:
#
#                    COLLATION_NAME (shows a collation name),
#                    CHARACTER_SET_NAME (shows the name of the character set to
#                          which the collation applies),
#                    ID (shows a numeric identifier for that collation/character
#                          set combination),
#                    IS_DEFAULT (shows whether the collation is the default
#                          collation for the character set shown),
#                    IS_COMPILED (indicates whether the collation is compiled
#                          into the MySQL server),
#                    SORTLEN (shows a value related to the amount of memory
#                          required to sort strings using this
#                          collation/character set combination).
################################################################################

let $is_table= collations;
--source suite/funcs_1/datadict/datadict_show_table_design.inc
# -------------------------------------------------------------------------------------------------------

let $message= Testcase 3.2.3.2:;
--source include/show_msg80.inc

################################################################################
# Testcase 3.2.3.2:  Ensure that the table shows the relevant information on
#                    every collation for which the current user or PUBLIC have
#                    the USAGE privilege.
################################################################################

SELECT * FROM collations;
# -------------------------------------------------------------------------------------------------------

let $message= Testcase 3.2.3.3:;
--source include/show_msg80.inc

###############################################################################
# Testcase 3.2.3.3:  Ensure that the table does not show any information on any
#                    collations for which the current user and PUBLIC have no
#                    USAGE privilege.
################################################################################

# Test requirement is erroneous... we cannot grant / revoke privilege for using
# a collation.
# -------------------------------------------------------------------------------------------------------

let $message= Testcase 3.2.4.1:;
--source include/show_msg80.inc

################################################################################
# Testcase 3.2.4.1:  Ensure that the
#                      INFORMATION_SCHEMA.COLLATION_CHARACTER_SET_APPLICABILITY
#                    table has the following columns, in the following order:
#
#                    COLLATION_NAME (shows the name of a collation),
#                    CHARACTER_SET_NAME (shows the name of a character set to
#                          which that collation applies).
################################################################################

let $is_table= collation_character_set_applicability;
--source suite/funcs_1/datadict/datadict_show_table_design.inc
# -------------------------------------------------------------------------------------------------------

let $message= Testcase 3.2.4.2:;
--source include/show_msg80.inc

################################################################################
# Testcase 3.2.4.2:  Ensure that the table shows the relevant information on
#                    every collation/character set combination for which the
#                    current user or PUBLIC have the USAGE privilege.
################################################################################

SELECT * FROM collation_character_set_applicability;
# -------------------------------------------------------------------------------------------------------

let $message= Testcase 3.2.4.3:;
--source include/show_msg80.inc

################################################################################
# Testcase 3.2.4.3:  Ensure that the table does not show any information on any
#                    collation/character set combinations for which the current
#                    user and PUBLIC have no USAGE privilege.
################################################################################

# Test requirement is erroneous... we cannot grant / revoke privilege for using a collation.
# -------------------------------------------------------------------------------------------------------

let $message= Testcase 3.2.5.1:;
--source include/show_msg80.inc

################################################################################
# Testcase 3.2.5.1:  Ensure that the INFORMATION_SCHEMA.COLUMN_PRIVILEGES
#                    table has the following columns, in the following order:
#
#                    GRANTEE (shows the name of a user who has either granted,
#                          or been granted a column privilege),
#                    TABLE_CATALOG (always shows NULL),
#                    TABLE_SCHEMA (shows the name of the schema, or database,
#                          in which the table for which a column privilege has
#                          been granted resides),
#                    TABLE_NAME (shows the name of the table),
#                    COLUMN_NAME (shows the name of the column on which a
#                          column privilege has been granted),
#                    PRIVILEGE_TYPE (shows the type of privilege that was
#                          granted; must be either SELECT, INSERT, UPDATE, or
#                          REFERENCES),
#                    IS_GRANTABLE (shows whether that privilege was granted
#                          WITH GRANT OPTION).
################################################################################

let $is_table= column_privileges;
--source suite/funcs_1/datadict/datadict_show_table_design.inc
# -------------------------------------------------------------------------------------------------------

let $message= Testcase 3.2.5.2 + 3.2.5.3 + 3.2.5.4:;
--source include/show_msg80.inc

################################################################################
# Testcase 3.2.5.2:  Ensure that the table shows the relevant information on
#                    every column privilege which has been granted to the
#                    current user or PUBLIC, or which was granted by the current
#                    user.
################################################################################
# Testcase 3.2.5.3:  Ensure that the table does not show any information on any
#                    column privilege which was granted to any user other than
#                    the current user or PUBLIC, or which was granted by any
#                    user other than the current user.
################################################################################
# Testcase 3.2.5.4:  Ensure that the table does not show any information on any
#                    privileges that are not column privileges for the current
#                    user.
################################################################################

CREATE USER 'user_1'@'localhost';
CREATE USER 'user_2'@'localhost';
CREATE USER 'user_3'@'localhost';

--disable_warnings
DROP DATABASE IF EXISTS db_datadict;
--enable_warnings
CREATE DATABASE db_datadict;
USE db_datadict;

CREATE TABLE db_datadict.res_t40502 (f1 INT, f2 DECIMAL, f3 TEXT);

GRANT SELECT(f1, f3) ON db_datadict.res_t40502 TO 'user_1'@'localhost';
GRANT INSERT(f1)     ON db_datadict.res_t40502 TO 'user_1'@'localhost';
GRANT UPDATE(f2)     ON db_datadict.res_t40502 TO 'user_1'@'localhost';
GRANT SELECT(f2)     ON db_datadict.res_t40502 TO 'user_2'@'localhost';
GRANT INSERT, SELECT ON db_datadict.res_t40502 TO 'user_3'@'localhost';
GRANT SELECT(f3)     ON db_datadict.res_t40502 TO 'user_3'@'localhost';

GRANT INSERT, SELECT ON db_datadict.res_t40502 TO 'user_3'@'localhost' WITH GRANT OPTION;
GRANT ALL            ON db_datadict.*          TO 'user_3'@'localhost';

let $select= SELECT * FROM information_schema.column_privileges
 WHERE grantee LIKE "'user%"
 ORDER BY grantee, table_name, column_name, privilege_type;
eval $select;

let $message= FIXME: Check it is correct that the following GRANT changes ALL privs that user_1 has;
--source include/show_msg.inc

GRANT UPDATE(f3)     ON db_datadict.res_t40502 TO 'user_1'@'localhost' WITH GRANT OPTION;

eval $select;

FLUSH PRIVILEGES;

--replace_result $MASTER_MYPORT MYSQL_PORT $MASTER_MYSOCK MYSQL_SOCK
connect (user_5_1, localhost, user_1, , db_datadict);
eval $select;

--replace_result $MASTER_MYPORT MYSQL_PORT $MASTER_MYSOCK MYSQL_SOCK
connect (user_5_2, localhost, user_2, , db_datadict);
eval $select;

--replace_result $MASTER_MYPORT MYSQL_PORT $MASTER_MYSOCK MYSQL_SOCK
connect (user_5_3, localhost, user_3, , db_datadict);

let $message= FIXME: check it is correct that granted TABLES doesn_t occur in COLUMN_PRIVILEGES;
--source include/show_msg.inc
SELECT * FROM information_schema.table_privileges WHERE grantee LIKE "'user%";
SELECT * FROM information_schema.schema_privileges WHERE grantee LIKE "'user%";
eval $select;
GRANT SELECT(f1, f3) ON db_datadict.res_t40502 TO 'user_2'@'localhost';

let $message= FIXME: check whether it is intended that *my* grants to others are *NOT* shown here;
--source include/show_msg.inc
eval $select;

connection user_5_2;
--source suite/funcs_1/include/show_connection.inc
eval $select;

disconnect user_5_1;
disconnect user_5_2;
disconnect user_5_3;

#cleanup
connection default;
--source suite/funcs_1/include/show_connection.inc
--disable_warnings
DROP TABLE IF EXISTS db_datadict.res_t40502;
DROP DATABASE IF EXISTS db_datadict;
--enable_warnings

DROP USER 'user_1'@'localhost';
DROP USER 'user_2'@'localhost';
DROP USER 'user_3'@'localhost';
# -------------------------------------------------------------------------------------------------------

let $message= Testcase 3.2.6.1:;
--source include/show_msg80.inc

################################################################################
# Testcase 3.2.6.1:  Ensure that the INFORMATION_SCHEMA.COLUMNS table has the
#                    following columns, in the following order:
#
#                    TABLE_CATALOG (always shows NULL),
#                    TABLE_SCHEMA (shows the name of the database, or schema,
#                          in which an accessible table resides),
#                    TABLE_NAME (shows the name of an accessible table),
#                    COLUMN_NAME (shows the name of a column within that
#                          table),
#                    ORDINAL_POSITION (shows the ordinal position of that
#                          column in that table),
#                    COLUMN_DEFAULT (shows the column's default value),
#                    IS_NULLABLE (shows whether the column may accept NULL
#                          values),
#                    DATA_TYPE (shows the column's defined data type; keyword
#                          only),
#                    CHARACTER_MAXIMUM_LENGTH (shows, for a string column, the
#                          column's defined maximum length in characters;
#                          otherwise NULL),
#                    CHARACTER_OCTET_LENGTH (shows, for a string column, the
#                          column's defined maximum length in octets;
#                          otherwise NULL),
#                    NUMERIC_PRECISION (shows, for a numeric column, the
#                          column's or data type's defined precision;
#                          otherwise NULL),
#                    NUMERIC_SCALE (shows, for a numeric column, the column's
#                          or data type's defined scale; otherwise NULL),
#                    CHARACTER_SET_NAME (shows, for a character string column,
#                          the column's default character set; otherwise NULL),
#                    COLLATION_NAME (shows, for a character string column, the
#                          column's default collation; otherwise NULL),
#                    COLUMN_TYPE (shows the column's complete, defined data
#                          type),
#                    COLUMN_KEY (shows whether the column is indexed; possible
#                          values are PRI if the column is part of a PRIMARY
#                          KEY, UNI if the column is part of a UNIQUE key, MUL
#                          if the column is part of an index key that allows
#                          duplicates),
#                    EXTRA (shows any additional column definition information,
#                          e.g. whether the column was defined with the
#                          AUTO_INCREMENT attribute),
#                    PRIVILEGES (shows the privileges available to the user on
#                          the column),
#                    COLUMN_COMMENT (shows the comment, if any, defined for the
#                          comment; otherwise NULL).
################################################################################

let $is_table= columns;
--source suite/funcs_1/datadict/datadict_show_table_design.inc
# -------------------------------------------------------------------------------------------------------

let $message= Testcase 3.2.6.2 + 3.2.6.3:;
--source include/show_msg80.inc

################################################################################
# Testcase 3.2.6.2:  Ensure that the table shows the relevant information on the
#                    columns of every table that is accessible to the current
#                    user or to PUBLIC.
################################################################################
# Testcase 3.2.6.3:  Ensure that the table does not show any information on the
#                    columns of any table which is not accessible to the current
#                    user or PUBLIC.
################################################################################

--disable_warnings
DROP DATABASE IF EXISTS db_datadict;
--enable_warnings
CREATE DATABASE db_datadict;

CREATE USER 'user_1'@'localhost';
CREATE USER 'user_2'@'localhost';

USE db_datadict;

create table t_6_406001(f1 char(10), f2 text, f3 date, f4 int);
grant select(f1, f2) on db_datadict.t_6_406001 to 'user_1'@'localhost';

create table t_6_406002(f1 char(10), f2 text, f3 date, f4 int);
GRANT INSERT(f1, f2) ON db_datadict.t_6_406002 TO 'user_2'@'localhost';

FLUSH PRIVILEGES;

let $select= SELECT * FROM information_schema.columns
 ORDER BY table_schema, table_name, ordinal_position;

# show view of user root
--source suite/funcs_1/datadict/datadict_bug_12777.inc
eval $select;

# reconnect to mysql with user credential of user u_6_406002_1.
--replace_result $MASTER_MYPORT MYSQL_PORT $MASTER_MYSOCK MYSQL_SOCK
connect (user_6_1, localhost, user_1, , db_datadict);
--source suite/funcs_1/datadict/datadict_bug_12777.inc
eval $select;

--replace_result $MASTER_MYPORT MYSQL_PORT $MASTER_MYSOCK MYSQL_SOCK
connect (user_6_2, localhost, user_2, , db_datadict);
--source suite/funcs_1/datadict/datadict_bug_12777.inc
eval $select;

disconnect user_6_1;
disconnect user_6_2;

connection default;
--source suite/funcs_1/include/show_connection.inc

let $message= Show the quotient of COL and CML for all COLUMNS;
--source include/show_msg.inc
SELECT DISTINCT
       CHARACTER_OCTET_LENGTH / CHARACTER_MAXIMUM_LENGTH AS COL_CML,
       DATA_TYPE,
       CHARACTER_SET_NAME,
       COLLATION_NAME
  FROM information_schema.columns
 WHERE CHARACTER_OCTET_LENGTH / CHARACTER_MAXIMUM_LENGTH = 1
 ORDER BY CHARACTER_SET_NAME, COLLATION_NAME, COL_CML;

#FIXME 3.2.6.2: check the value 2.0079 tinytext ucs2 ucs2_general_ci
SELECT DISTINCT
       CHARACTER_OCTET_LENGTH / CHARACTER_MAXIMUM_LENGTH AS COL_CML,
       DATA_TYPE,
       CHARACTER_SET_NAME,
       COLLATION_NAME
  FROM information_schema.columns
 WHERE CHARACTER_OCTET_LENGTH / CHARACTER_MAXIMUM_LENGTH <> 1
 ORDER BY CHARACTER_SET_NAME, COLLATION_NAME, COL_CML;

SELECT DISTINCT
       CHARACTER_OCTET_LENGTH / CHARACTER_MAXIMUM_LENGTH AS COL_CML,
       DATA_TYPE,
       CHARACTER_SET_NAME,
       COLLATION_NAME
  FROM information_schema.columns
 WHERE CHARACTER_OCTET_LENGTH / CHARACTER_MAXIMUM_LENGTH IS NULL
 ORDER BY CHARACTER_SET_NAME, COLLATION_NAME, COL_CML;

echo --> CHAR(0) is allowed (see manual), and here both CHARACHTER_* values;
echo --> are 0, which is intended behavior, and the result of 0 / 0 IS NULL;
--source suite/funcs_1/datadict/datadict_bug_12777.inc
SELECT CHARACTER_OCTET_LENGTH / CHARACTER_MAXIMUM_LENGTH AS COL_CML,
       TABLE_SCHEMA,
       TABLE_NAME,
       COLUMN_NAME,
       DATA_TYPE,
       CHARACTER_MAXIMUM_LENGTH,
       CHARACTER_OCTET_LENGTH,
       CHARACTER_SET_NAME,
       COLLATION_NAME,
       COLUMN_TYPE
  FROM information_schema.columns
 ORDER BY TABLE_SCHEMA, TABLE_NAME, ORDINAL_POSITION;

#cleanup
DROP USER 'user_1'@'localhost';
DROP USER 'user_2'@'localhost';
DROP TABLE IF EXISTS t_6_406001;
DROP TABLE IF EXISTS t_6_406002;
DROP DATABASE IF EXISTS db_datadict;
# -------------------------------------------------------------------------------------------------------

let $message= Testcase 3.2.7.1:;
--source include/show_msg80.inc

################################################################################
# Testcase 3.2.7.1:  Ensure that the INFORMATION_SCHEMA.KEY_COLUMN_USAGE
#                    table has the following columns, in the following order:
#
#                    CONSTRAINT_CATALOG (always shows NULL),
#                    CONSTRAINT_SCHEMA (shows the database, or schema, in which
#                          an accessible constraint, or index, resides),
#                    CONSTRAINT_NAME (shows the name of the accessible
#                          constraint),
#                    TABLE_CATALOG (always shows NULL),
#                    TABLE_SCHEMA (shows the database, or schema, in which the
#                          table constrained by that constraint resides),
#                    TABLE_NAME (shows the name of the table constrained by the
#                           constraint),
#                    COLUMN_NAME (shows the name of a column that is the index
#                          key, or part of the index key),
#                    ORDINAL_POSITION (shows the ordinal position of the column
#                          within the constraint index),
#                    POSITION_IN_UNIQUE_CONSTRAINT (shows, for a foreign key
#                          column, the ordinal position of the referenced
#                          column within the referenced unique index;
#                          otherwise NULL).
# added with 5.0.6:  REFERENCED_TABLE_SCHEMA,
#                    REFERENCED_TABLE_NAME,
#                    REFERENCED_COLUMN_NAME
################################################################################

let $is_table= key_column_usage;
--source suite/funcs_1/datadict/datadict_show_table_design.inc
# -------------------------------------------------------------------------------------------------------

let $message= Testcase 3.2.7.2 + 3.2.7.3:;
--source include/show_msg80.inc

################################################################################
# Testcase 3.2.7.2:  Ensure that the table shows the relevant information on
#                    every column, defined to be part of an index key, which is
#                    accessible to the current user or to PUBLIC.
################################################################################
# Testcase 3.2.7.3:  Ensure that the table does not show any information on any
#                    indexed column that is not accessible to the current user
#                    or PUBLIC.
################################################################################

--disable_warnings
DROP DATABASE IF EXISTS db_datadict;
--enable_warnings
CREATE DATABASE db_datadict;

CREATE USER 'user_1'@'localhost';
CREATE USER 'user_2'@'localhost';

USE db_datadict;

CREATE TABLE t_40701 (
      f1 INT NOT NULL, PRIMARY KEY(f1),
      f2 INT,          INDEX f2_ind(f2)
      );
GRANT SELECT ON t_40701 to 'user_1'@'localhost';

CREATE TABLE t_40702 (
      f1 INT NOT NULL, PRIMARY KEY(f1),
      f2 INT,          INDEX f2_ind(f2)
      );
GRANT SELECT ON t_40702 to 'user_2'@'localhost';
#FIXME: add foreign keys

FLUSH PRIVILEGES;

let $select= SELECT * FROM information_schema.key_column_usage
 ORDER BY constraint_catalog, constraint_schema, constraint_name,
          table_catalog, table_schema, table_name, ordinal_position;

# show view of user root
eval $select;

# reconnect to mysql with user credential of user u_6_406002_1.
--replace_result $MASTER_MYPORT MYSQL_PORT $MASTER_MYSOCK MYSQL_SOCK
connect (user_7_1, localhost, user_1, , db_datadict);
eval $select;

--replace_result $MASTER_MYPORT MYSQL_PORT $MASTER_MYSOCK MYSQL_SOCK
connect (user_7_2, localhost, user_2, , db_datadict);
eval $select;

disconnect user_7_1;
disconnect user_7_2;

#cleanup
connection default;
--source suite/funcs_1/include/show_connection.inc
DROP USER 'user_1'@'localhost';
DROP USER 'user_2'@'localhost';
DROP TABLE t_40701;
DROP TABLE t_40702;
DROP DATABASE IF EXISTS db_datadict;
# -------------------------------------------------------------------------------------------------------

let $message= Testcase 3.2.8.1:;
--source include/show_msg80.inc

################################################################################
# Testcase 3.2.8.1: Ensure that the INFORMATION_SCHEMA.ROUTINES
#                   table has the following columns, in the following order:
#
#                   SPECIFIC_NAME (shows the name of an accessible stored
#                          procedure, or routine),
#                   ROUTINE_CATALOG (always shows NULL),
#                   ROUTINE_SCHEMA (shows the database, or schema, in which
#                          the routine resides),
#                   ROUTINE_NAME (shows the same stored procedure name),
#                   ROUTINE_TYPE (shows whether the stored procedure is a
#                          procedure or a function),
#                   DTD_IDENTIFIER (shows, for a function, the complete
#                          data type definition of the value the function will
#                          return; otherwise NULL),
#                   ROUTINE_BODY (shows the language in which the stored
#                          procedure is written; currently always SQL),
#                   ROUTINE_DEFINITION (shows as much of the routine body as
#                          is possible in the allotted space),
#                   EXTERNAL_NAME (always shows NULL),
#                   EXTERNAL_LANGUAGE (always shows NULL),
#                   PARAMETER_STYLE (shows the routine's parameter style;
#                          always SQL),
#                   IS_DETERMINISTIC (shows whether the routine is
#                          deterministic),
#                   SQL_DATA_ACCESS (shows the routine's defined
#                          sql-data-access clause value),
#                   SQL_PATH (always shows NULL),
#                   SECURITY_TYPE (shows whether the routine's defined
#                          security_type is 'definer' or 'invoker'),
#                   CREATED (shows the timestamp of the time the routine was
#                          created),
#                   LAST_ALTERED (shows the timestamp of the time the routine
#                          was last altered),
#                   SQL_MODE (shows the sql_mode setting at the time the
#                          routine was created),
#                   ROUTINE_COMMENT (shows the comment, if any, defined for
#                          the routine; otherwise NULL),
#                   DEFINER (shows the user who created the routine).
################################################################################

let $is_table= routines;
--source suite/funcs_1/datadict/datadict_show_table_design.inc
# -------------------------------------------------------------------------------------------------------

let $message= Testcase 3.2.8.2 + 3.2.8.3:;
--source include/show_msg80.inc

################################################################################
# Testcase 3.2.8.2:  Ensure that the table shows the relevant information on
#                    every SQL-invoked routine (i.e. stored procedure) which is
#                    accessible to the current user or to PUBLIC.
################################################################################
# Testcase 3.2.8.3:  Ensure that the table does not show any information on any
#                    stored procedure that is not accessible to the current user
#                    or PUBLIC.;
##############################################################################

--disable_warnings
DROP DATABASE IF EXISTS db_datadict;
DROP DATABASE IF EXISTS db_datadict_2;
--enable_warnings

CREATE DATABASE db_datadict;
USE db_datadict;

CREATE USER 'user_1'@'localhost';
CREATE USER 'user_2'@'localhost';
CREATE USER 'user_3'@'localhost';

CREATE TABLE res_6_408002_1(f1 CHAR(3), f2 TEXT(25), f3 DATE, f4 INT);

INSERT INTO res_6_408002_1(f1, f2, f3, f4)
   VALUES('abc', 'xyz', '1989-11-09', 0815);

--disable_warnings
DROP PROCEDURE IF EXISTS sp_6_408002_1;
--enable_warnings

delimiter //;
CREATE PROCEDURE sp_6_408002_1()
BEGIN
   SELECT * FROM db_datadict.res_6_408002_1;
END//
delimiter ;//

CREATE DATABASE db_datadict_2;
USE db_datadict_2;

CREATE TABLE res_6_408002_2(f1 CHAR(3), f2 TEXT(25), f3 DATE, f4 INT);

INSERT INTO res_6_408002_2(f1, f2, f3, f4)
   VALUES('abc', 'xyz', '1990-10-03', 4711);

--disable_warnings
DROP PROCEDURE IF EXISTS sp_6_408002_2;
--enable_warnings

delimiter //;
CREATE PROCEDURE sp_6_408002_2()
BEGIN
   SELECT * FROM db_datadict_2.res_6_408002_2;
END//
delimiter ;//

GRANT SELECT  ON db_datadict_2.*         TO 'user_1'@'localhost';
GRANT EXECUTE ON db_datadict_2.*         TO 'user_1'@'localhost';

GRANT EXECUTE ON           db_datadict.*               TO 'user_1'@'localhost';
GRANT SELECT  ON           db_datadict.*               TO 'user_2'@'localhost';

GRANT EXECUTE ON PROCEDURE db_datadict_2.sp_6_408002_2 TO 'user_2'@'localhost';
GRANT EXECUTE ON           db_datadict_2.*             TO 'user_2'@'localhost';
FLUSH PRIVILEGES;

--replace_result $MASTER_MYPORT MYSQL_PORT $MASTER_MYSOCK MYSQL_SOCK
connect (user_8_1, localhost, user_1, , db_datadict);

--replace_column 16 "YYYY-MM-DD hh:mm:ss" 17 "YYYY-MM-DD hh:mm:ss"
SELECT * FROM information_schema.routines;
disconnect user_8_1;

--replace_result $MASTER_MYPORT MYSQL_PORT $MASTER_MYSOCK MYSQL_SOCK
connect (user_8_2, localhost, user_2, , db_datadict);

--replace_column 16 "YYYY-MM-DD hh:mm:ss" 17 "YYYY-MM-DD hh:mm:ss"
SELECT * FROM information_schema.routines;
disconnect user_8_2;

--replace_result $MASTER_MYPORT MYSQL_PORT $MASTER_MYSOCK MYSQL_SOCK
connect (user_8_3, localhost, user_3, , test);

--replace_column 16 "YYYY-MM-DD hh:mm:ss" 17 "YYYY-MM-DD hh:mm:ss"
SELECT * FROM information_schema.routines;
disconnect user_8_3;

# clean-up
connection default;
--source suite/funcs_1/include/show_connection.inc
DROP USER 'user_1'@'localhost';
DROP USER 'user_2'@'localhost';
DROP USER 'user_3'@'localhost';

use db_datadict;
DROP TABLE res_6_408002_1;
DROP PROCEDURE sp_6_408002_1;

USE db_datadict_2;
DROP TABLE res_6_408002_2;
DROP PROCEDURE sp_6_408002_2;

USE test;
DROP DATABASE db_datadict;
DROP DATABASE db_datadict_2;
# -------------------------------------------------------------------------------------------------------

let $message= Testcase 3.2.8.4:;
--source include/show_msg80.inc

################################################################################
# Testcase 3.2.8.4:  Ensure that a stored procedure with a routine body that is
#                    too large to fit into the
#                    INFORMATION_SCHEMA.ROUTINES.ROUTINE_DEFINITION column
#                    correctly shows as much of the information as is possible
#                    within the allotted size.
################################################################################

--disable_warnings
DROP DATABASE IF EXISTS db_datadict;
--enable_warnings

CREATE DATABASE db_datadict;
USE db_datadict;

create table res_6_408004_1(f1 longtext , f2 mediumint , f3 longblob , f4 real , f5 year);

insert into res_6_408004_1 values ('abc', 98765 , 99999999 , 98765, 10);

--disable_warnings
drop procedure if exists sp_6_408004;
--enable_warnings

create table res_6_408004_2(f1 longtext , f2 mediumint , f3 longblob , f4 real , f5 year);

insert into res_6_408004_2 values ('abc', 98765 , 99999999 , 98765, 10);

let $message= Checking the max. possible length of (currently) 4 GByte is not possible in this environment here.;
--source include/show_msg.inc

delimiter //;
create procedure sp_6_408004 ()
begin
   declare done integer default 0;
   declare variable_number_1 longtext;
   declare variable_number_2 mediumint;
   declare variable_number_3 longblob;
   declare variable_number_4 real;
   declare variable_number_5 year;
   declare cursor_number_1 cursor for select * from res_6_408004_1 limit 0, 10;
   declare cursor_number_2 cursor for select * from res_6_408004_1 limit 0, 10;
   declare cursor_number_3 cursor for select * from res_6_408004_1 limit 0, 10;
   declare cursor_number_4 cursor for select * from res_6_408004_1 limit 0, 10;
   declare cursor_number_5 cursor for select * from res_6_408004_1 limit 0, 10;
   declare continue handler for sqlstate '02000' set done = 1;
   begin
      open cursor_number_1;
      while done <> 1 do
         fetch cursor_number_1 into variable_number_1, variable_number_2, variable_number_3, variable_number_4, variable_number_5;
         if done <> 0 then
                 insert into res_6_408004_2 values (variable_number_1, variable_number_2, variable_number_3,
                                 variable_number_4, variable_number_5);
         end if;
      end while;
      begin
         begin
            set done = 0;
            open cursor_number_2;
            while done <> 1 do
               fetch cursor_number_2 into variable_number_1, variable_number_2, variable_number_3, variable_number_4, variable_number_5;
               if done <> 0 then
                  insert into res_6_408004_2 values(variable_number_1, variable_number_2, variable_number_3, variable_number_4, variable_number_5);
               end if;
            end while;
         end;
         set done = 0;
         open cursor_number_3;
         while done <> 1 do
            fetch cursor_number_3 into variable_number_1, variable_number_2, variable_number_3, variable_number_4, variable_number_5;
            if done <> 0 then
               insert into res_6_408004_2 values(variable_number_1, variable_number_2, variable_number_3, variable_number_4, variable_number_5);
            end if;
         end while;
      end;
   end;
   begin
      set done = 0;
      open cursor_number_4;
      while done <> 1 do
         fetch cursor_number_4 into variable_number_1, variable_number_2, variable_number_3, variable_number_4, variable_number_5;
         if done <> 0 then
            insert into res_6_408004_2 values (variable_number_1, variable_number_2, variable_number_3, variable_number_4, variable_number_5);
         end if;
      end while;
   end;
   begin
      set @a='test row';
      select @a;
      select @a;
      select @a;
   end;
   begin
      set done = 0;
      open cursor_number_5;
      while done <> 1 do
         fetch cursor_number_5 into variable_number_1, variable_number_2, variable_number_3, variable_number_4, variable_number_5;
         if done <> 0 then
            insert into res_6_408004_2 values (variable_number_1, variable_number_2, variable_number_3, variable_number_4, variable_number_5);
         end if;
      end while;
   end;
   begin
      set @a='test row';
      select @a;
      select @a;
      select @a;
   end;
end//
delimiter ;//

call sp_6_408004 ();
select * from res_6_408004_2;

--vertical_results
--replace_column 16 "YYYY-MM-DD hh:mm:ss" 17 "YYYY-MM-DD hh:mm:ss"
SELECT *, LENGTH(routine_definition)
  FROM information_schema.routines
 WHERE routine_schema = 'db_datadict';
--horizontal_results

# clean-up
use db_datadict;
drop procedure sp_6_408004;
drop table res_6_408004_1;
drop table res_6_408004_2;
use test;
drop database db_datadict;
# -------------------------------------------------------------------------------------------------------

let $message= Testcase 3.2.9.1:;
--source include/show_msg80.inc

################################################################################
# Testcase 3.2.9.1: Ensure that the INFORMATION_SCHEMA.SCHEMATA
#                   table has the following columns, in the following order:
#
#                   CATALOG_NAME (always shows NULL),
#                   SCHEMA_NAME (shows the name of a database, or schema, on
#                          which the current user or PUBLIC has privileges),
#                   DEFAULT_CHARACTER_SET_NAME (shows the name of that
#                          database's default character set),
#                   DEFAULT_COLLATION_NAME (shows the database default
#                          collation)
#                   SQL_PATH (always shows NULL).
################################################################################

let $is_table= schemata;
--source suite/funcs_1/datadict/datadict_show_table_design.inc
# -------------------------------------------------------------------------------------------------------

let $message= Testcase 3.2.9.2 + 3.2.9.3:;
--source include/show_msg80.inc

################################################################################
# Testcase 3.2.9.2:  Ensure that the table shows the relevant information for
#                    every database on which the current user or PUBLIC have
#                    privileges.
################################################################################
# Testcase 3.2.9.3:  Ensure that the table does not show any information on any
#                    databases on which the current user and PUBLIC have no
#                    privileges.
################################################################################

CREATE USER 'user_1'@'localhost';
CREATE USER 'user_2'@'localhost';
CREATE USER 'user_3'@'localhost';

--disable_warnings
DROP DATABASE IF EXISTS db_datadict_1;
DROP DATABASE IF EXISTS db_datadict_2;
--enable_warnings

CREATE DATABASE db_datadict_1;
CREATE DATABASE db_datadict_2;

GRANT SELECT ON db_datadict_1.* to 'user_1'@'localhost';
GRANT SELECT ON db_datadict_2.* to 'user_2'@'localhost';

FLUSH PRIVILEGES;

# shows db_1
--replace_result $MASTER_MYPORT MYSQL_PORT $MASTER_MYSOCK MYSQL_SOCK
connect (user_9_1, localhost, user_1, , db_datadict_1);

SELECT COUNT(*) FROM information_schema.schemata;
SELECT * FROM information_schema.schemata;
disconnect user_9_1;


# shows db_2
--replace_result $MASTER_MYPORT MYSQL_PORT $MASTER_MYSOCK MYSQL_SOCK
connect (user_9_2, localhost, user_2, , db_datadict_2);

SELECT COUNT(*) FROM information_schema.schemata;
SELECT * FROM information_schema.schemata;
disconnect user_9_2;


# shows neither db_1 nor db_2
--replace_result $MASTER_MYPORT MYSQL_PORT $MASTER_MYSOCK MYSQL_SOCK
connect (user_9_3, localhost, user_3, , test);

SELECT COUNT(*) FROM information_schema.schemata;
SELECT * FROM information_schema.schemata;
disconnect user_9_3;


# clean-up
connection default;
--source suite/funcs_1/include/show_connection.inc
DROP USER 'user_1'@'localhost';
DROP USER 'user_2'@'localhost';
DROP USER 'user_3'@'localhost';
DROP DATABASE db_datadict_1;
DROP DATABASE db_datadict_2;
# -------------------------------------------------------------------------------------------------------

let $message= Testcase 3.2.10.1:;
--source include/show_msg80.inc

################################################################################
# Testcase 3.2.10.1: Ensure that the INFORMATION_SCHEMA.TABLE_CONSTRAINTS
#                    table has the following columns, in the following order:
#
#                    CONSTRAINT_CATALOG (always shows NULL),
#                    CONSTRAINT_SCHEMA (shows the database, or schema, in which
#                          a constraint an accessible table resides),
#                    CONSTRAINT_NAME (shows the name of a constraint defined on
#                          an accessible table),
#                    TABLE_SCHEMA (shows the database, or schema, in which the
#                          table resides),
#                    TABLE_NAME (shows the name of the table),
#                    CONSTRAINT_TYPE (shows the type of the constraint; either
#                          'primary key', 'foreign key', 'unique', 'check').
################################################################################

let $is_table= table_constraints;
--source suite/funcs_1/datadict/datadict_show_table_design.inc
# -------------------------------------------------------------------------------------------------------

let $message= Testcase 3.2.10.2 + 3.2.10.3:;
--source include/show_msg80.inc

################################################################################
# Testcase 3.2.10.2: Ensure that the table shows the relevant information on all
#                    constraints defined on every table for which the current
#                    user or PUBLIC have privileges.
################################################################################
# Testcase 3.2.10.3: Ensure that the table does not show any information on
#                    constraints defined on any table for which the current user
#                    and PUBLIC have no privileges.
################################################################################

CREATE USER 'user_1'@'localhost';
CREATE USER 'user_2'@'localhost';

--disable_warnings
DROP DATABASE IF EXISTS db_datadict;
DROP DATABASE IF EXISTS db_datadict_2;
--enable_warnings

CREATE DATABASE db_datadict;
CREATE DATABASE db_datadict_2;

USE db_datadict;

CREATE TABLE res_6_401003_1(f1 INT NOT NULL, PRIMARY KEY(f1), f2 INT, INDEX f2_ind(f2));

USE db_datadict_2;

CREATE TABLE res_6_401003_2(f1 INT NOT NULL, PRIMARY KEY(f1), f2 INT, INDEX f2_ind(f2));

GRANT SELECT ON db_datadict.res_6_401003_1 TO 'user_1'@'localhost';
GRANT SELECT ON db_datadict_2.res_6_401003_2 TO 'user_2'@'localhost';

FLUSH PRIVILEGES;


--replace_result $MASTER_MYPORT MYSQL_PORT $MASTER_MYSOCK MYSQL_SOCK
connect (user_10_1, localhost, user_1, , db_datadict);

SELECT * FROM information_schema.table_constraints;
SELECT COUNT(*) FROM information_schema.table_constraints;
disconnect user_10_1;

--replace_result $MASTER_MYPORT MYSQL_PORT $MASTER_MYSOCK MYSQL_SOCK
connect (user_10_2, localhost, user_2, , db_datadict_2);

SELECT * FROM information_schema.table_constraints;
SELECT COUNT(*) FROM information_schema.table_constraints;
disconnect user_10_2;

# clean-up
connection default;
use db_datadict;
--source suite/funcs_1/include/show_connection.inc
DROP USER 'user_1'@'localhost';
DROP USER 'user_2'@'localhost';
DROP TABLE res_6_401003_1;
USE db_datadict_2;
DROP TABLE res_6_401003_2;
USE test;
DROP DATABASE db_datadict;
DROP DATABASE db_datadict_2;
# -------------------------------------------------------------------------------------------------------

let $message= Testcase 3.2.11.1:;
--source include/show_msg80.inc

################################################################################
# Testcase 3.2.11.1: Ensure that the INFORMATION_SCHEMA.TABLE_PRIVILEGES
#                    table has the following columns, in the following order:
#
#                    GRANTEE (shows the name of a user who has either granted,
#                          or been granted a table privilege),
#                    TABLE_CATALOG (always shows NULL),
#                    TABLE_SCHEMA (shows the name of the schema, or database,
#                          in which the table for which a privilege has been
#                          granted resides),
#                    TABLE_NAME (shows the name of the table),
#                    PRIVILEGE_TYPE (shows the type of privilege that was
#                          granted; must be either SELECT, INSERT, UPDATE,
#                          DELETE, REFERENCES, ALTER, INDEX, DROP, or CREATE
#                          VIEW),
#                    IS_GRANTABLE (shows whether that privilege was granted
#                          WITH GRANT OPTION).
################################################################################

let $is_table= table_privileges;
--source suite/funcs_1/datadict/datadict_show_table_design.inc
# -------------------------------------------------------------------------------------------------------

let $message= Testcase 3.2.11.2 + 3.2.11.3 + 3.2.11.4:;
--source include/show_msg80.inc

################################################################################
# Testcase 3.2.11.2: Ensure that the table shows the relevant information on
#                    every table privilege which has been granted to the current
#                    user or PUBLIC, or which was granted by the current user.
################################################################################
# Testcase 3.2.11.3: Ensure that the table does not show any information on any
#                    table privilege which was granted to any user other than
#                    the current user or PUBLIC, or which was granted by any
#                    user other than the current user.
################################################################################
# Testcase 3.2.11.4: Ensure that the table does not show any information on any
#                    privileges that are not table privileges for the current
#                    user.
################################################################################

--disable_warnings
DROP DATABASE IF EXISTS db_datadict;
--enable_warnings

create database db_datadict;

CREATE USER 'user_1'@'localhost';
GRANT CREATE, SELECT ON db_datadict.* TO 'user_1'@'localhost' WITH GRANT OPTION;
CREATE USER 'user_2'@'localhost';
CREATE USER 'user_3'@'localhost';

use db_datadict;

create table tb1(f1 int, f2 int, f3 int);

grant select on db_datadict.tb1 to 'user_1'@'localhost';
GRANT ALL    on db_datadict.tb1 to 'user_2'@'localhost' WITH GRANT OPTION;

FLUSH PRIVILEGES;

--replace_result $MASTER_MYPORT MYSQL_PORT $MASTER_MYSOCK MYSQL_SOCK
connect (user_11_1, localhost, user_1, , db_datadict);

CREATE TABLE tb3 (f1 TEXT);
GRANT SELECT ON db_datadict.tb3 to 'user_3'@'localhost';

SELECT * FROM information_schema.table_privileges
 WHERE table_name LIKE 'tb%';

--replace_result $MASTER_MYPORT MYSQL_PORT $MASTER_MYSOCK MYSQL_SOCK
connect (user_11_2, localhost, user_2, , db_datadict);

# we see only table privileges for this user, and not any other privileges
SELECT * FROM information_schema.table_privileges;

SELECT USER(), COUNT(*)
  FROM information_schema.table_privileges
 WHERE grantee = USER();

SELECT USER(), COUNT(*)
  FROM information_schema.table_privileges
 WHERE grantee = "'user_2'@'localhost'";

--replace_result $MASTER_MYPORT MYSQL_PORT $MASTER_MYSOCK MYSQL_SOCK
connect (user_11_3, localhost, user_3, , db_datadict);

# we see only table privileges for this user, and not any other privileges
SELECT * FROM information_schema.table_privileges;

connection default;
--source suite/funcs_1/include/show_connection.inc

# we see only 'public' table privileges
SELECT * FROM information_schema.table_privileges;

# clean-up
connection default;
--source suite/funcs_1/include/show_connection.inc
disconnect user_11_1;
disconnect user_11_2;
disconnect user_11_3;
DROP USER 'user_1'@'localhost';
DROP USER 'user_2'@'localhost';
DROP USER 'user_3'@'localhost';
drop table db_datadict.tb1;
drop table db_datadict.tb3;
use test;
drop database db_datadict;
# -------------------------------------------------------------------------------------------------------

let $message= Testcase 3.2.12.1:;
--source include/show_msg80.inc

################################################################################
# Testcase 3.2.12.1: Ensure that the INFORMATION_SCHEMA.TABLES
#                    table has the following columns, in the following order:
#
#                    TABLE_CATALOG (always shows NULL),
#                    TABLE_SCHEMA (shows the name of the database, or schema,
#                          in which an accessible table resides),
#                    TABLE_NAME (shows the name of a table which the current
#                          user may access),
#                    TABLE_TYPE (shows whether the table is a BASE TABLE, a
#                          TEMPORARY table, or a VIEW),
#                    ENGINE (shows the storage engine used for the table),
#                    VERSION (shows the version number of the table's .frm
#                          file),
#                    ROW_FORMAT (shows the table's row storage format; either
#                          FIXED, DYNAMIC or COMPRESSED),
#                    TABLE_ROWS (shows the number of rows in the table),
#                    AVG_ROW_LENGTH (shows the average length of the table's
#                          rows),
#                    DATA_LENGTH (shows the length of the table's data file),
#                    MAX_DATA_LENGTH (shows the maximum length of the table's
#                          data file),
#                    INDEX_LENGTH (shows the length of the index file
#                          associated with the table),
#                    DATA_FREE (shows the number of allocated, unused bytes),
#                    AUTO_INCREMENT (shows the next AUTO_INCREMENT value, where
#                          applicable),
#                    CREATE_TIME (shows the timestamp of the time the table was
#                          created),
#                    UPDATE_TIME (shows the timestamp of the time the table's
#                          data file was last updated),
#                    CHECK_TIME (shows the timestamp of the time the table was
#                          last checked),
#                    TABLE_COLLATION (shows the table's default collation),
#                    CHECKSUM (shows the live checksum value for the table, if
#                          any; otherwise NULL),
#                    CREATE_OPTIONS (shows any additional options used in the
#                          table's definition; otherwise NULL),
#                    TABLE_COMMENT (shows the comment added to the table's
#                          definition; otherwise NULL).
################################################################################

let $is_table= tables;
--source suite/funcs_1/datadict/datadict_show_table_design.inc
# -------------------------------------------------------------------------------------------------------

let $message= Testcase 3.2.12.2 + 3.2.12.3:;
--source include/show_msg80.inc

################################################################################
# Testcase 3.2.12.2: Ensure that the table shows the relevant information on
#                    every base table and view on which the current user or
#                    PUBLIC has privileges.
################################################################################
# Testcase 3.2.12.3: Ensure that the table does not show any information on any
#                    tables on which the current user and public have no
#                    privileges.
################################################################################

--disable_warnings
DROP DATABASE IF EXISTS db_datadict;
--enable_warnings

create database db_datadict;

CREATE USER 'user_1'@'localhost';
GRANT CREATE, CREATE VIEW, INSERT, SELECT ON db_datadict.*
   TO 'user_1'@'localhost' WITH GRANT OPTION;
CREATE USER 'user_2'@'localhost';
CREATE USER 'user_3'@'localhost';

use db_datadict;

create table tb1(f1 int, f2 int, f3 int);

grant select on db_datadict.tb1 to 'user_1'@'localhost';
GRANT ALL    on db_datadict.tb1 to 'user_2'@'localhost' WITH GRANT OPTION;

FLUSH PRIVILEGES;

--replace_result $MASTER_MYPORT MYSQL_PORT $MASTER_MYSOCK MYSQL_SOCK
connect (user_12_1, localhost, user_1, , db_datadict);

# tb2 is not granted to anyone
CREATE TABLE tb2 (f1 DECIMAL);
CREATE TABLE tb3 (f1 TEXT);
GRANT SELECT ON db_datadict.tb3 to 'user_3'@'localhost';
GRANT INSERT ON db_datadict.tb3 to 'user_2'@'localhost';

CREATE VIEW v3 AS SELECT * FROM tb3;
GRANT SELECT ON db_datadict.v3 to 'user_3'@'localhost';

#FIXME 3.2.12: we split the "SELECT * FROM tables" in two parts until
#FIXME 3.2.12: Bug #12397: wrong values shown in column CREATE_OPTIONS of
#FIXME 3.2.12: INFORMATION_SCHEMA.TABLES is solved, one with 'more' and one
#FIXME 3.2.12: with 'less' replace
#  9 AVG_ROW_LENGTH
# 10 DATA_LENGTH
# 11 MAX_DATA_LENGTH
# 12 INDEX_LENGTH
# 13 DATA_FREE
# 15 CREATE_TIME
# 16 UPDATE_TIME
# 17 CHECK_TIME
# 20 CREATE_OPTIONS
if ($have_bug_11589)
{
--disable_ps_protocol
}
--replace_column  9 "#ARL#"  10 "#DL#"  11 "#MDL#"  12 "#IL#"  13 "#DF#"  15 "YYYY-MM-DD hh:mm:ss"  16 "YYYY-MM-DD hh:mm:ss" 17 "YYYY-MM-DD hh:mm:ss" 20 "#CO#"
SELECT * FROM information_schema.tables
 WHERE table_schema = 'information_schema';
#  9 AVG_ROW_LENGTH
# 10 DATA_LENGTH
# 11 MAX_DATA_LENGTH
# 12 INDEX_LENGTH
# 13 DATA_FREE
# 15 CREATE_TIME
# 16 UPDATE_TIME
# 17 CHECK_TIME
--replace_column  9 "#ARL#"  10 "#DL#"  11 "#MDL#"  12 "#IL#"  13 "#DF#"  15 "YYYY-MM-DD hh:mm:ss"  16 "YYYY-MM-DD hh:mm:ss" 17 "YYYY-MM-DD hh:mm:ss"
SELECT * FROM information_schema.tables
 WHERE NOT( table_schema = 'information_schema');
--enable_ps_protocol

--replace_result $MASTER_MYPORT MYSQL_PORT $MASTER_MYSOCK MYSQL_SOCK
connect (user_12_2, localhost, user_2, , db_datadict);

# we see only tables for this user, and not any other
#FIXME 3.2.12: we split the "SELECT * FROM tables" in two parts until
#FIXME 3.2.12: Bug #12397: wrong values shown in column CREATE_OPTIONS of
#FIXME 3.2.12: INFORMATION_SCHEMA.TABLES is solved, one with 'more' and one
#FIXME 3.2.12: with 'less' replace
#  9 AVG_ROW_LENGTH
# 10 DATA_LENGTH
# 11 MAX_DATA_LENGTH
# 12 INDEX_LENGTH
# 13 DATA_FREE
# 15 CREATE_TIME
# 16 UPDATE_TIME
# 17 CHECK_TIME
# 20 CREATE_OPTIONS
if ($have_bug_11589)
{
--disable_ps_protocol
}
--replace_column  9 "#ARL#"  10 "#DL#"  11 "#MDL#"  12 "#IL#"  13 "#DF#"  15 "YYYY-MM-DD hh:mm:ss"  16 "YYYY-MM-DD hh:mm:ss" 17 "YYYY-MM-DD hh:mm:ss" 20 "#CO#"
SELECT * FROM information_schema.tables
 WHERE table_schema = 'information_schema';
#  9 AVG_ROW_LENGTH
# 10 DATA_LENGTH
# 11 MAX_DATA_LENGTH
# 12 INDEX_LENGTH
# 13 DATA_FREE
# 15 CREATE_TIME
# 16 UPDATE_TIME
# 17 CHECK_TIME
--replace_column  9 "#ARL#"  10 "#DL#"  11 "#MDL#"  12 "#IL#"  13 "#DF#"  15 "YYYY-MM-DD hh:mm:ss"  16 "YYYY-MM-DD hh:mm:ss" 17 "YYYY-MM-DD hh:mm:ss"
SELECT * FROM information_schema.tables
 WHERE NOT( table_schema = 'information_schema');
--enable_ps_protocol

--replace_result $MASTER_MYPORT MYSQL_PORT $MASTER_MYSOCK MYSQL_SOCK
connect (user_12_3, localhost, user_3, , db_datadict);

# we see only tables for this user, and not any other
#
#FIXME 3.2.12: we split the "SELECT * FROM tables" in two parts until
#FIXME 3.2.12: Bug #12397: wrong values shown in column CREATE_OPTIONS of
#FIXME 3.2.12: INFORMATION_SCHEMA.TABLES is solved, one with 'more' and one
#FIXME 3.2.12: with 'less' replace
#  9 AVG_ROW_LENGTH
# 10 DATA_LENGTH
# 11 MAX_DATA_LENGTH
# 12 INDEX_LENGTH
# 13 DATA_FREE
# 15 CREATE_TIME
# 16 UPDATE_TIME
# 17 CHECK_TIME
# 20 CREATE_OPTIONS
if ($have_bug_11589)
{
--disable_ps_protocol
}
--replace_column  9 "#ARL#"  10 "#DL#"  11 "#MDL#"  12 "#IL#"  13 "#DF#"  15 "YYYY-MM-DD hh:mm:ss"  16 "YYYY-MM-DD hh:mm:ss" 17 "YYYY-MM-DD hh:mm:ss"  20 "#CO#"
SELECT * FROM information_schema.tables
 WHERE table_schema = 'information_schema';
#  9 AVG_ROW_LENGTH
# 10 DATA_LENGTH
# 11 MAX_DATA_LENGTH
# 12 INDEX_LENGTH
# 13 DATA_FREE
# 15 CREATE_TIME
# 16 UPDATE_TIME
# 17 CHECK_TIME
--replace_column  9 "#ARL#"  10 "#DL#"  11 "#MDL#"  12 "#IL#"  13 "#DF#"  15 "YYYY-MM-DD hh:mm:ss"  16 "YYYY-MM-DD hh:mm:ss" 17 "YYYY-MM-DD hh:mm:ss"
SELECT * FROM information_schema.tables
 WHERE NOT( table_schema = 'information_schema');
--enable_ps_protocol

connection default;
--source suite/funcs_1/include/show_connection.inc

# we see only 'public' tables
#
#FIXME 3.2.12: we split the "SELECT * FROM tables" in two parts until
#FIXME 3.2.12: Bug #12397: wrong values shown in column CREATE_OPTIONS of
#FIXME 3.2.12: INFORMATION_SCHEMA.TABLES is solved, one with 'more' and one
#FIXME 3.2.12: with 'less' replace
#  9 AVG_ROW_LENGTH
# 10 DATA_LENGTH
# 11 MAX_DATA_LENGTH
# 12 INDEX_LENGTH
# 13 DATA_FREE
# 15 CREATE_TIME
# 16 UPDATE_TIME
# 17 CHECK_TIME
# 20 CREATE_OPTIONS
if ($have_bug_11589)
{
--disable_ps_protocol
}
--replace_column  9 "#ARL#"  10 "#DL#"  11 "#MDL#"  12 "#IL#"  13 "#DF#"  15 "YYYY-MM-DD hh:mm:ss"  16 "YYYY-MM-DD hh:mm:ss" 17 "YYYY-MM-DD hh:mm:ss"  20 "#CO#"
SELECT * FROM information_schema.tables
 WHERE table_schema = 'information_schema';
#  9 AVG_ROW_LENGTH
# 10 DATA_LENGTH
# 11 MAX_DATA_LENGTH
# 12 INDEX_LENGTH
# 13 DATA_FREE
# 15 CREATE_TIME
# 16 UPDATE_TIME
# 17 CHECK_TIME
--replace_column  9 "#ARL#"  10 "#DL#"  11 "#MDL#"  12 "#IL#"  13 "#DF#"  15 "YYYY-MM-DD hh:mm:ss"  16 "YYYY-MM-DD hh:mm:ss" 17 "YYYY-MM-DD hh:mm:ss"
SELECT * FROM information_schema.tables
 WHERE NOT( table_schema = 'information_schema');
--enable_ps_protocol

# clean-up
disconnect user_12_1;
disconnect user_12_2;
disconnect user_12_3;
DROP USER 'user_1'@'localhost';
DROP USER 'user_2'@'localhost';
DROP USER 'user_3'@'localhost';
DROP TABLE db_datadict.tb1;
DROP TABLE db_datadict.tb3;
DROP VIEW db_datadict.v3;
USE test;
DROP DATABASE db_datadict;
# -------------------------------------------------------------------------------------------------------

let $message= Testcase 3.2.13.1:;
--source include/show_msg80.inc

################################################################################
# Testcase 3.2.13.1: Ensure that the INFORMATION_SCHEMA.VIEWS
#                    table has the following columns, in the following order:
#
#                    TABLE_CATALOG (always shows NULL),
#                    TABLE_SCHEMA (shows the database, or schema, in which an
#                          accessible view resides),
#                    TABLE_NAME (shows the name of a view accessible to the
#                          current user),
#                    VIEW_DEFINITION (shows the SELECT statement that makes
#                          up the view's definition),
#                    CHECK_OPTION (shows the value of the WITH CHECK OPTION
#                          clause used to define the view, either NONE, LOCAL
#                          or CASCADED),
#                    IS_UPDATABLE (shows whether the view is an updatable
#                          view),
#                    DEFINER (added with 5.0.14),
#                    SECURITY_TYPE (added with 5.0.14).
################################################################################

let $is_table= views;
--source suite/funcs_1/datadict/datadict_show_table_design.inc
# -------------------------------------------------------------------------------------------------------

let $message= Testcase 3.2.13.2 + 3.2.13.3:;
--source include/show_msg80.inc

################################################################################
# Testcase 3.2.13.2: Ensure that the table shows the relevant information on
#                    every view for which the current user or PUBLIC has the
#                    SHOW CREATE VIEW privilege.
################################################################################
# Testcase 3.2.13.3: Ensure that the table does not show any information on any
#                    views for which the current user and PUBLIC have no SHOW
#                    CREATE VIEW privilege.
################################################################################

--disable_warnings
DROP DATABASE IF EXISTS db_datadict;
--enable_warnings

CREATE DATABASE db_datadict;

CREATE USER 'user_1'@'localhost';
CREATE USER 'user_2'@'localhost';
CREATE USER 'user_no_views'@'localhost';
USE db_datadict;

CREATE TABLE tb_401302(f1 INT, f2 INT, f3 INT);
CREATE VIEW v_granted_to_1 AS SELECT * FROM tb_401302;
CREATE VIEW v_granted_glob AS SELECT f2, f3 FROM tb_401302;

GRANT SELECT ON db_datadict.tb_401302 TO 'user_1'@'localhost';
GRANT SELECT ON db_datadict.v_granted_to_1 TO 'user_1'@'localhost';
GRANT SHOW VIEW, CREATE VIEW ON db_datadict.* TO 'user_2'@'localhost';

FLUSH PRIVILEGES;

SELECT * FROM information_schema.views;

--replace_result $MASTER_MYPORT MYSQL_PORT $MASTER_MYSOCK MYSQL_SOCK
connect (user_13_1, localhost, user_1, , test);

SELECT * FROM information_schema.views;

--replace_result $MASTER_MYPORT MYSQL_PORT $MASTER_MYSOCK MYSQL_SOCK
connect (user_13_2, localhost, user_2, , test);

SELECT * FROM information_schema.views;

--replace_result $MASTER_MYPORT MYSQL_PORT $MASTER_MYSOCK MYSQL_SOCK
connect (user_no_views, localhost, user_no_views, , test);

SELECT * FROM information_schema.views;

# clean-up
connection default;
--source suite/funcs_1/include/show_connection.inc
disconnect user_13_1;
disconnect user_13_2;
disconnect user_no_views;
USE db_datadict;
DROP USER 'user_1'@'localhost';
DROP USER 'user_2'@'localhost';
DROP USER 'user_no_views'@'localhost';
DROP VIEW v_granted_to_1;
DROP TABLE tb_401302;
DROP VIEW v_granted_glob;
USE test;
DROP DATABASE db_datadict;
# -------------------------------------------------------------------------------------------------------

let $message= Testcase 3.2.14.1:;
--source include/show_msg80.inc

################################################################################
# Testcase 3.2.14.1: Ensure that the INFORMATION_SCHEMA.STATISTICS
#                    table has the following columns, in the following order:
#
#                    TABLE_CATALOG (always shows NULL),
#                    TABLE_SCHEMA (shows the database, or schema, in which a
#                          table indexed by an accessible index resides),
#                    TABLE_NAME (shows the name of  the indexed table),
#                    NON_UNIQUE (shows whether the index may contain duplicate
#                          values; 0 if it cannot, 1 if it can),
#                    INDEX_SCHEMA (shows the database, or schema, in which an
#                          accessible index resides),
#                    INDEX_NAME (shows the name of an index which the current
#                          user may access),
#                    SEQ_IN_INDEX (shows the ordinal position of an indexed
#                          column within the index),
#                    COLUMN_NAME (shows the name of a column that comprises
#                          some, or all, of an index key),
#                    COLLATION (shows how the column is sorted in the index;
#                          either A for ascending or NULL for unsorted
#                          columns),
#                    CARDINALITY (shows the number of unique values in the
#                          index),
#                    SUB_PART (shows the number of indexed characters if the
#                          index is a prefix index),
#                    PACKED (shows how the index key is packed),
#                    NULLABLE (shows whether the index column may contain NULL
#                          values),
#                    INDEX_TYPE (shows the index type; either BTREE, FULLTEXT,
#                          HASH or RTREE),
#                    COMMENT (shows a comment on the index, if any).
################################################################################

let $is_table= statistics;
--source suite/funcs_1/datadict/datadict_show_table_design.inc
# -------------------------------------------------------------------------------------------------------

let $message= Testcase 3.2.14.2 + 3.2.14.3:;
--source include/show_msg80.inc

################################################################################
# Testcase 3.2.14.2: Ensure that the table shows the relevant information on
#                    every index which the current user or PUBLIC may access
#                    (usually because privileges on the indexed table have been
#                    granted).
################################################################################
# Testcase 3.2.14.3: Ensure that the table does not show any information on any
#                    indexes which the current user and PUBLIC may not access.
################################################################################

--disable_warnings
DROP DATABASE IF EXISTS db_datadict;
DROP DATABASE IF EXISTS db_datadict_2;
--enable_warnings

CREATE DATABASE db_datadict;
CREATE DATABASE db_datadict_2;

CREATE USER 'user_1'@'localhost';
CREATE USER 'user_2'@'localhost';
USE db_datadict;

create table tb_6_401402_1(f1 int not null, primary key(f1), f2 int, index f2_ind(f2));
create table tb_6_401402_2(f1 int not null, primary key(f1), f2 int, index f2_ind(f2));
grant select on db_datadict.tb_6_401402_1 to 'user_1'@'localhost' WITH GRANT OPTION;

USE db_datadict_2;

create table tb_2_1(f1 int not null, primary key(f1), f2 int, index f2_ind(f2));
create table tb_2_2(f1 int not null, primary key(f1), f2 int, index f2_ind(f2));
grant select on db_datadict_2.tb_2_1 to 'user_1'@'localhost';

FLUSH PRIVILEGES;

--replace_result $MASTER_MYPORT MYSQL_PORT $MASTER_MYSOCK MYSQL_SOCK
connect (user_14_1, localhost, user_1, , test);
SELECT * FROM information_schema.statistics;

--replace_result $MASTER_MYPORT MYSQL_PORT $MASTER_MYSOCK MYSQL_SOCK
connect (user_14_2, localhost, user_2, , test);
SELECT * FROM information_schema.statistics;

connection default;
--source suite/funcs_1/include/show_connection.inc
REVOKE SELECT ON db_datadict.tb_6_401402_1 FROM 'user_1'@'localhost';
SELECT * FROM information_schema.statistics;

# nothing visible for user_1
connection user_14_1;
--source suite/funcs_1/include/show_connection.inc
SELECT * FROM information_schema.statistics;

# no changes visible for user_2
connection user_14_2;
--source suite/funcs_1/include/show_connection.inc
SELECT * FROM information_schema.statistics;

# cleanup
connection default;
--source suite/funcs_1/include/show_connection.inc
disconnect user_14_1;
disconnect user_14_2;
USE db_datadict;
DROP USER 'user_1'@'localhost';
DROP USER 'user_2'@'localhost';
DROP TABLE tb_6_401402_1;
DROP TABLE tb_6_401402_2;
USE test;
DROP DATABASE db_datadict;
# -------------------------------------------------------------------------------------------------------

let $message= Testcase 3.2.15.1:;
--source include/show_msg80.inc

################################################################################
# Testcase 3.2.15.1: Ensure that the INFORMATION_SCHEMA.SCHEMA_PRIVILEGES
#                    table has the following columns, in the following order:
#
#                    GRANTEE (shows a user to whom a schema privilege has been
#                          granted),
#                    TABLE_CATALOG (always shows NULL),
#                    TABLE_SCHEMA (shows the name of the database, or schema,
#                          on which the privilege has been granted),
#                    PRIVILEGE_TYPE (shows the granted privilege),
#                    IS_GRANTABLE (shows whether the privilege was granted WITH
#                          GRANT OPTION).
################################################################################

let $is_table= schema_privileges;
--source suite/funcs_1/datadict/datadict_show_table_design.inc
# -------------------------------------------------------------------------------------------------------

let $message= Testcase 3.2.15.2:;
--source include/show_msg80.inc

################################################################################
# Testcase 3.2.15.2: Ensure that the table shows the relevant information on
#                    every schema-level privilege which has been granted to the
#                    current user or to PUBLIC, or has been granted by the
#                    current user.
################################################################################

--disable_warnings
DROP DATABASE IF EXISTS db_datadict;
DROP DATABASE IF EXISTS db_datadict_2;
--enable_warnings

create database db_datadict;
create database db_datadict_2;

CREATE USER 'u_6_401502'@'localhost';

use db_datadict;

create table res_6_401502(f1 int, f2 int, f3 int);
grant insert on db_datadict.* to 'u_6_401502'@'localhost';

FLUSH PRIVILEGES;

SELECT * FROM information_schema.schema_privileges;

--replace_result $MASTER_MYPORT MYSQL_PORT $MASTER_MYSOCK MYSQL_SOCK
connect (u_6_401502, localhost, u_6_401502, , test);

SELECT * FROM information_schema.schema_privileges;

disconnect u_6_401502;

# clean-up

connection default;
use db_datadict;
--source suite/funcs_1/include/show_connection.inc
DROP USER 'u_6_401502'@'localhost';
drop table res_6_401502;
use test;
drop database db_datadict;
drop database db_datadict_2;
# -------------------------------------------------------------------------------------------------------

let $message= Testcase 3.2.15.3 + 3.2.15.4:;
--source include/show_msg80.inc

################################################################################
# Testcase 3.2.15.3: Ensure that the table does not show any information on any
#                    schema-level privileges which have been granted to users
#                    other than the current user or to PUBLIC, or that have been
#                    granted by any user other than the current user.
################################################################################
# Testcase 3.2.15.4: Ensure that the table does not show any information on any
#                    privileges that are not schema-level privileges for the
#                    current user.
################################################################################

--disable_warnings
DROP DATABASE IF EXISTS db_datadict;
DROP DATABASE IF EXISTS db_datadict_2;
--enable_warnings

create database db_datadict;
create database db_datadict_2;

CREATE USER 'u_6_401503_1'@'localhost';
CREATE USER 'u_6_401503_2'@'localhost';
CREATE USER 'u_6_401503_3'@'localhost';

use db_datadict;

create table res_6_401503_1(f1 int, f2 int, f3 int);

use db_datadict_2;

create table res_6_401503_2(f1 int, f2 int, f3 int);

grant update on db_datadict.* to 'u_6_401503_1'@'localhost';
grant delete on db_datadict_2.* to 'u_6_401503_2'@'localhost';

FLUSH PRIVILEGES;

SELECT * FROM information_schema.schema_privileges;

--replace_result $MASTER_MYPORT MYSQL_PORT $MASTER_MYSOCK MYSQL_SOCK
connect (u_6_401503_1, localhost, u_6_401503_1, , test);

SELECT * FROM information_schema.schema_privileges;

--replace_result $MASTER_MYPORT MYSQL_PORT $MASTER_MYSOCK MYSQL_SOCK
connect (u_6_401503_2, localhost, u_6_401503_2, , test);

SELECT * FROM information_schema.schema_privileges;

# should not show anything
--replace_result $MASTER_MYPORT MYSQL_PORT $MASTER_MYSOCK MYSQL_SOCK
connect (u_6_401503_3, localhost, u_6_401503_3, , test);

SELECT * FROM information_schema.schema_privileges;

# clean-up

connection default;
--source suite/funcs_1/include/show_connection.inc
disconnect u_6_401503_1;
disconnect u_6_401503_2;
disconnect u_6_401503_3;
use db_datadict;
DROP USER 'u_6_401503_1'@'localhost';
DROP USER 'u_6_401503_2'@'localhost';
DROP USER 'u_6_401503_3'@'localhost';
drop table res_6_401503_1;
use db_datadict_2;
drop table res_6_401503_2;
use test;
drop database db_datadict;
drop database db_datadict_2;
# -------------------------------------------------------------------------------------------------------

let $message= Testcase 3.2.16.1:;
--source include/show_msg80.inc

################################################################################
# Testcase 3.2.16.1: Ensure that the INFORMATION_SCHEMA.USER_PRIVILEGES
#                    table has the following columns, in the following order:
#
#                    GRANTEE (shows a user to whom a user privilege has been
#                          granted),
#                    TABLE_CATALOG (always shows NULL),
#                    PRIVILEGE_TYPE (shows the granted privilege),
#                    IS_GRANTABLE (shows whether the privilege was granted WITH
#                          GRANT OPTION).
################################################################################

#-----------
# Bug #12063 column 'TABLE_SCHEMA' is missing in table
#                 INFORMATION_SCHEMA.USER_PRIVILEGE
# ... is not a bug, it has been added by mistake in the TP requirement document.
#-----------

let $is_table= user_privileges;
--source suite/funcs_1/datadict/datadict_show_table_design.inc
# -------------------------------------------------------------------------------------------------------

let $message= Testcase 3.2.16.2 + 3.2.16.3 + 3.2.16.4:;
--source include/show_msg80.inc

################################################################################
# Testcase 3.2.16.2: Ensure that the table shows the relevant information on
#                    every user privilege which has been granted to the current
#                    user or to PUBLIC, or has been granted by the current user.
################################################################################
# Testcase 3.2.16.3: Ensure that the table does not show any information on any
#                    user privileges which have been granted to users other than
#                    the current user or  have been granted by any user other
#                    than the current user.
################################################################################
# Testcase 3.2.16.4: Ensure that the table does not show any information on any
#                    privileges that are not user privileges for the current
#                    user.
################################################################################

#FIXME 3.2.16: - when Bug #12269 is fixed a some of the outputs here may be
#FIXME 3.2.16:   deleted as I added them for checking where / which information
#FIXME 3.2.16:   is shown.

--disable_warnings
DROP DATABASE IF EXISTS db_datadict;
--enable_warnings

let $cmd1= SELECT * FROM information_schema.user_privileges
            WHERE grantee LIKE "%user%"
            ORDER BY grantee, table_catalog, privilege_type;
let $cmd2= SELECT * FROM mysql.user WHERE user LIKE "%user%" ORDER BY host, user;
let $cmd3= SHOW GRANTS;

CREATE DATABASE db_datadict;

CREATE USER 'user_1'@'localhost';
CREATE USER 'user_2'@'localhost';
CREATE USER 'user_3'@'localhost';

GRANT SELECT ON db_datadict.* TO 'user_1'@'localhost';
GRANT SELECT ON mysql.user TO 'user_1'@'localhost';

GRANT INSERT ON *.* TO 'user_2'@'localhost';
GRANT UPDATE ON *.* TO 'user_2'@'localhost';

FLUSH PRIVILEGES;

let $message= FIXME (see Bug 12269) Here we expect more than only <USAGE> for user_1;
--source include/show_msg.inc
eval $cmd1;
eval $cmd2;
eval $cmd3;

let $message= add GRANT OPTION db_datadict.* to user_1;
--source include/show_msg.inc
GRANT UPDATE ON db_datadict.* TO 'user_1'@'localhost' WITH GRANT OPTION;

let $message= FIXME (see Bug 12269) Here the <YES> is missing for the GRANT OPTION for user_1;
--source include/show_msg.inc
eval $cmd1;
eval $cmd2;
eval $cmd3;

FLUSH PRIVILEGES;


--replace_result $MASTER_MYPORT MYSQL_PORT $MASTER_MYSOCK MYSQL_SOCK
connect (user_16_1, localhost, user_1, , db_datadict);
eval $cmd1;
eval $cmd2;
eval $cmd3;

# add SELECT on *.* to user_1
let $message= Now add SELECT on *.* to user_1;
--source include/show_msg.inc

connection default;
--source suite/funcs_1/include/show_connection.inc
GRANT SELECT ON *.* TO 'user_1'@'localhost';
let $message= Here <SELECT NO> is shown correctly for user_1;
--source include/show_msg.inc
eval $cmd1;
eval $cmd2;
eval $cmd3;

GRANT SELECT ON *.* TO 'user_1'@'localhost' WITH GRANT OPTION;
let $message= Here <SELECT YES> is shown correctly for user_1;
--source include/show_msg.inc
eval $cmd1;
eval $cmd2;
eval $cmd3;

FLUSH PRIVILEGES;
eval $cmd1;
eval $cmd2;
eval $cmd3;

# check that this appears
connection user_16_1;
--source suite/funcs_1/include/show_connection.inc
eval $cmd1;
eval $cmd2;
eval $cmd3;

--replace_result $MASTER_MYPORT MYSQL_PORT $MASTER_MYSOCK MYSQL_SOCK
connect (user_16_2, localhost, user_2, , db_datadict);
eval $cmd1;
--error 1142
eval $cmd2;
eval $cmd3;

--replace_result $MASTER_MYPORT MYSQL_PORT $MASTER_MYSOCK MYSQL_SOCK
connect (user_16_3, localhost, user_3, , test);
eval $cmd1;
--error 1142
eval $cmd2;
eval $cmd3;

let $message= revoke privileges from user_1;
--source include/show_msg.inc

connection default;
--source suite/funcs_1/include/show_connection.inc
REVOKE ALL PRIVILEGES, GRANT OPTION FROM 'user_1'@'localhost';
eval $cmd1;
eval $cmd2;
eval $cmd3;

# check for changes
connection user_16_1;
--source suite/funcs_1/include/show_connection.inc
eval $cmd1;
--error 1142
eval $cmd2;
eval $cmd3;

--source suite/funcs_1/include/show_connection.inc
# checks entered before bug #12269 was reported
# OK, user_1 has no privs here
--error 1142
CREATE TABLE db_datadict.tb_55 ( c1 TEXT );
--source suite/funcs_1/include/show_connection.inc
eval $cmd1;
--error 1142
eval $cmd2;
eval $cmd3;
# OK, user_1 has no privs here
--error 1142
CREATE TABLE db_datadict.tb_66 ( c1 TEXT );

let $message= add ALL on db_datadict.* (and select on mysql.user) to user_1;
--source include/show_msg.inc

connection default;
--source suite/funcs_1/include/show_connection.inc

GRANT ALL ON db_datadict.* TO 'user_1'@'localhost' WITH GRANT OPTION;
GRANT SELECT ON mysql.user TO 'user_1'@'localhost';
eval $cmd1;
eval $cmd2;
eval $cmd3;

# check for changes
connection user_16_1;
--source suite/funcs_1/include/show_connection.inc
eval $cmd1;
eval $cmd2;
eval $cmd3;

# OK, user_1 has no privs here
--error 1142
CREATE TABLE db_datadict.tb_56 ( c1 TEXT );

# using 'USE' lets the server read the privileges new, so now the CREATE works
USE db_datadict;
--source suite/funcs_1/include/show_connection.inc
eval $cmd1;
eval $cmd2;
eval $cmd3;
CREATE TABLE tb_57 ( c1 TEXT );

let $message= revoke privileges from user_1;
--source include/show_msg.inc

connection default;
--source suite/funcs_1/include/show_connection.inc
REVOKE ALL PRIVILEGES, GRANT OPTION FROM 'user_1'@'localhost';
FLUSH PRIVILEGES;
eval $cmd1;
eval $cmd2;
eval $cmd3;

# check for changes
connection user_16_1;
--source suite/funcs_1/include/show_connection.inc
eval $cmd1;
--error 1142
eval $cmd2;
eval $cmd3;
# WORKS, as the existing old privileges are used!
CREATE TABLE db_datadict.tb_58 ( c1 TEXT );
# existing privileges are "read" new when USE is called, user has no priviliges
--error 1044
USE db_datadict;
#FIXME 3.2.16: check that it is correct that this now 'works': --error 1142
CREATE TABLE db_datadict.tb_59 ( c1 TEXT );


# clean-up
connection default;
--source suite/funcs_1/include/show_connection.inc
disconnect user_16_1;
disconnect user_16_2;
disconnect user_16_3;
DROP USER 'user_1'@'localhost';
DROP USER 'user_2'@'localhost';
DROP USER 'user_3'@'localhost';
DROP DATABASE IF EXISTS db_datadict;
# -------------------------------------------------------------------------------------------------------

let $message=  Testcase 3.2.17: Checks on Performance - not here in this script!;
--source include/show_msg80.inc

################################################################################
# Testcase 3.2.17.1: Ensure that every INFORMATION_SCHEMA table shows all the
#                    correct information, and no incorrect information, for a
#                    database to which 100 different users, each of which has a
#                     randomly issued set of privileges and access to a
#                    randomly chosen set of database objects, have access.
#                    The database should contain a mixture of all types of
#                    database objects (i.e. tables, views, stored procedures,
#                    triggers).
################################################################################

################################################################################
# Testcase 3.2.17.2: Ensure that every INFORMATION_SCHEMA table shows all the
#                    correct information, and no incorrect information, for 10
#                    different databases to which 50 different users, each of
#                    which has a randomly issued set of privileges and access
#                    to a randomly chosen set of database objects in two or
#                    more of the databases, have access. The databases should
#                    each contain a mixture of all types of database objects
#                    (i.e. tables, views, stored procedures, triggers).
################################################################################

# -------------------------------------------------------------------------------------------------------

let $message= Testcase 3.2.18.1:;
--source include/show_msg80.inc

################################################################################
# Testcase 3.2.18.1: Ensure that the INFORMATION_SCHEMA.TRIGGERS
#                    table has the following columns, in the following order:
#
#
#                    (FIXME - list copied from WL#1996)
#
#                    TRIGGER_CATALOG               NULL
#                    TRIGGER_SCHEMA
#                    TRIGGER_NAME
#                    EVENT_MANIPULATION
#                    EVENT_OBJECT_CATALOG          NULL
#                    EVENT_OBJECT_SCHEMA
#                    EVENT_OBJECT_TABLE
#                    ACTION_ORDER                  NULL
#                    ACTION_CONDITION              NULL
#                    ACTION_STATEMENT
#                    ACTION_ORIENTATION
#                    ACTION_TIMING
#                    ACTION_REFERENCE_OLD_TABLE    NULL
#                    ACTION_REFERENCE_NEW_TABLE    NULL
#                    ACTION_REFERENCE_OLD_ROW
#                    ACTION_REFERENCE_NEW_ROW
#                    CREATED
#                    SQL_MODE
#
################################################################################

let $is_table= triggers;
--source suite/funcs_1/datadict/datadict_show_table_design.inc
# -------------------------------------------------------------------------------------------------------

let $message= Testcase 3.2.18.2 + 3.2.18.3:;
--source include/show_msg80.inc

################################################################################
# Testcase 3.2.18.2: Ensure that the table shows the relevant information on
#                    every trigger on which the current user or PUBLIC has
#                    privileges.
################################################################################
# Testcase 3.2.18.3: Ensure that the table does not show any information on any
#                    trigger on which the current user and public have no
#                    privileges.
################################################################################

#FIXME 3.2.18.2: to be added.
#FIXME 3.2.18.2: don't forget to add the test description to QATestPlanV50func

# -------------------------------------------------------------------------------------------------------

let $message= Testcase 3.2.19.1:;
--source include/show_msg80.inc

################################################################################
# Testcase 3.2.19.1: Ensure that the INFORMATION_SCHEMA.PARAMETERS
#                    table has the following columns, in the following order:
#
################################################################################

let $is_table= parameters;
# when table is implemented remove this and the next 4 lines and "enable" 5th line:
# and don't forget to add the test description to QATestPlanV50func
let $message= checking a table that will be implemented later;
--source include/show_msg.inc
--error 1109
eval DESC $is_table;
#--source suite/funcs_1/datadict/datadict_show_table_design.inc
# -------------------------------------------------------------------------------------------------------

let $message= Testcase 3.2.20.1:;
--source include/show_msg80.inc

################################################################################
# Testcase 3.2.20.1: Ensure that the INFORMATION_SCHEMA.REFERENTIAL_CONSTRAINTS
#                    table has the following columns, in the following order:
#
################################################################################

let $is_table= referential_constraints;
# when table is implemented remove this and the next 4 lines and "enable" 5th line:
# and don't forget to add the test description to QATestPlanV50func
let $message= checking a table that will be implemented later;
--source include/show_msg.inc
--error 1109
eval DESC $is_table;
#--source suite/funcs_1/datadict/datadict_show_table_design.inc
# -------------------------------------------------------------------------------------------------------


################################################################################
#
let $message= *** End of Data Dictionary Tests ***;
--source include/show_msg80.inc
#
################################################################################


# some cleanup to be sure nothing remains
--disable_warnings
DROP TABLE IF EXISTS test.tb1;
DROP TABLE IF EXISTS test.tb2;
DROP TABLE IF EXISTS test.tb3;
DROP TABLE IF EXISTS test.tb4;
DROP TABLE IF EXISTS test.t1;
DROP TABLE IF EXISTS test.t2;
DROP TABLE IF EXISTS test.t3;
DROP TABLE IF EXISTS test.t4;
DROP TABLE IF EXISTS test.t7;
DROP TABLE IF EXISTS test.t8;
DROP TABLE IF EXISTS test.t9;
DROP TABLE IF EXISTS test.t10;
DROP TABLE IF EXISTS test.t11;
DROP DATABASE IF EXISTS test1;
DROP DATABASE IF EXISTS test4;
DROP DATABASE IF EXISTS db_datadict;
DROP DATABASE IF EXISTS db_datadict_1;
DROP DATABASE IF EXISTS db_datadict_2;
--enable_warnings

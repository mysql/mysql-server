
#
# Test how DROP TABLE works if the index or data file doesn't exists
# myisam specific test
--source include/force_myisam_default.inc
--source include/have_myisam.inc

# Initialise
--disable_warnings
drop table if exists t1,t2;
--enable_warnings

create table t1 (a int) engine=myisam;
let $MYSQLD_DATADIR= `select @@datadir`;
--remove_file $MYSQLD_DATADIR/test/t1.MYI
# DROP TABLE sans IF EXISTS clause no longer drops such table.
--error ER_ENGINE_CANT_DROP_MISSING_TABLE
drop table t1;
--error ER_FILE_NOT_FOUND
select * from t1;
# DROP TABLE IF EXISTS should drop such table successfully.
drop table if exists t1;
create table t1 (a int) engine=myisam;
--remove_file $MYSQLD_DATADIR/test/t1.MYI
# DROP TABLE sans IF EXISTS clause no longer drops such table.
--error ER_ENGINE_CANT_DROP_MISSING_TABLE
drop table t1;
--error ER_FILE_NOT_FOUND
select * from t1;
# DROP TABLE IF EXISTS should drop such table successfully.
drop table if exists t1;
--error ER_BAD_TABLE_ERROR
drop table t1;
--remove_file $MYSQLD_DATADIR/test/t1.MYD 

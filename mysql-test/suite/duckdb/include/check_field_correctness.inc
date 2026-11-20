# Test the compatibility of duckdb engine fields and compare with innodb_db engine
--disable_query_log
CREATE DATABASE IF NOT EXISTS innodb_db;
CREATE DATABASE IF NOT EXISTS duckdb_db;
CREATE DATABASE IF NOT EXISTS duckdb_db_batch_insert;
CREATE DATABASE IF NOT EXISTS innotoduck;
CREATE DATABASE IF NOT EXISTS ducktoinno;

# create table with engine innodb and duckdb.
USE innodb_db;
--eval $create_sql engine = innodb;

USE duckdb_db;
--eval $create_sql engine = duckdb;

USE duckdb_db_batch_insert;
--eval $create_sql engine = duckdb;

use innotoduck;
--eval $create_sql engine = innodb;

use ducktoinno;
--eval $create_sql engine = duckdb;

--echo # display innodb table structure.
--eval desc innodb_db.$table_name

--echo # display duckdb table structure.
--eval desc duckdb_db.$table_name

--enable_query_log

--let $db_name=duckdb_db
--let $table_name=$table_name
--source suite/duckdb/include/show_duckdb_table_structure.inc
--source suite/duckdb/include/check_mysql_dd_structure.inc

--disable_query_log

# insert data
USE innodb_db;
--eval $insert_sql

USE innotoduck;
--eval $insert_sql

USE ducktoinno;
--eval $insert_sql

USE duckdb_db;
SET GLOBAL duckdb_dml_in_batch = OFF;
--eval $insert_sql

USE duckdb_db_batch_insert;
SET GLOBAL duckdb_dml_in_batch = ON;
--eval $insert_sql

--if ($config_sql) {
--eval $config_sql
}

USE innotoduck;
--eval ALTER TABLE $table_name ENGINE = duckdb;

USE ducktoinno;
--if($table_name != "t_decimal"){
--eval ALTER TABLE $table_name ENGINE = innodb;
}

--echo # check inserted data
# select data
USE innodb_db;
--let $innodb_result=`$select_sql`
--replace_regex /[a-z_]+//
--let $innodb_checksum=`checksum table $table_name`

USE duckdb_db;
--let $duckdb_result_1=`$select_sql`
--replace_regex /[a-z_]+//
--let $duckdb_checksum=`checksum table $table_name`

USE duckdb_db_batch_insert;
--let $duckdb_result_2=`$select_sql`
--replace_regex /[a-z_]+//
--let $duckdb_batch_insert_checksum=`checksum table $table_name`


USE innotoduck;
--let $innotoduck_result=`$select_sql`
--replace_regex /[a-z_]+//
--let $innotoduck_checksum=`checksum table $table_name`

USE ducktoinno;
--let $ducktoinno_result=`$select_sql`
--eval ALTER TABLE $table_name ENGINE = duckdb;
--let $ducktoinnotoduck_result=`$select_sql`
--replace_regex /[a-z_]+//
--let $ducktoinno_checksum=`checksum table $table_name`

--enable_query_log

--echo innodb_result  : $innodb_result
--echo duckdb_result_1: $duckdb_result_1
--echo duckdb_result_2: $duckdb_result_2
--echo innotoduck_result: $innotoduck_result
--echo ducktoinno_result: $ducktoinno_result
--echo ducktoinnotoduck_result: $ducktoinnotoduck_result

--echo innodb_checksum: $innodb_checksum
--echo duckdb_checksum  : $duckdb_checksum
--echo duckdb_batch_insert_checksum: $duckdb_batch_insert_checksum
--echo innotoduck_checksum: $innotoduck_checksum
--echo ducktoinno_checksum: $ducktoinno_checksum


--if ($innodb_checksum != $duckdb_checksum) {
--let $error=Results are not same, innodb_checksum: $innodb_checksum, duckdb_checksum: $duckdb_checksum
--echo $error
--if ($die_on_error) {
--die $error
}
}

--if ($innodb_checksum != $duckdb_batch_insert_checksum) {
--let $error=Results are not same, innodb_checksum: $innodb_checksum, duckdb_batch_insert_checksum: $duckdb_batch_insert_checksum
--echo $error
--if ($die_on_error) {
--die $error
}
}

--if ($innodb_checksum != $innotoduck_checksum) {
--let $error=Results are not same, innodb_checksum: $innodb_checksum, innotoduck_checksum: $innotoduck_checksum
--echo $error
--if ($die_on_error) {
--die $error
}
}

--if ($innodb_checksum != $ducktoinno_checksum) {
--let $error=Results are not same, innodb_checksum: $innodb_checksum, ducktoinno_checksum: $ducktoinno_checksum
--echo $error
--if ($die_on_error) {
--die $error
}
}

--disable_query_log
USE test;

--echo # cleanup
--eval DROP TABLE duckdb_db.$TABLE_NAME
--eval DROP TABLE innodb_db.$TABLE_NAME
--eval DROP TABLE innotoduck.$TABLE_NAME
--eval DROP TABLE ducktoinno.$TABLE_NAME


DROP DATABASE innodb_db;
DROP DATABASE duckdb_db;
DROP DATABASE duckdb_db_batch_insert;
DROP DATABASE innotoduck;
DROP DATABASE ducktoinno;


--enable_query_log

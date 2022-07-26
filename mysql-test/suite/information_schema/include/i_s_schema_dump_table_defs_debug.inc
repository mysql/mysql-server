--source include/have_debug.inc
SET debug = '+d,fetch_system_view_definition';

--echo ########################################################################
--echo # Verify all the CREATE VIEW statements of the I_S system view.
--echo # Mask collations that depend on the lower case table names setting.
--echo ########################################################################

# The debug variable 'fetch_system_view_definition' enables TABLE_COMMENT
# column of INFORMATION_SCHEMA.TABLES to return the CREATE VIEW command
# definition that the server uses to create system view during bootstrap.
#
# Note that we cannot use system view definition stored in DD column
# mysql.tables.view_definition(_utf8), because the SELECT query
# representing the view is re-written by optimizer. This causes difference
# in view definition syntax from 8.0 vs trunk. And the this difference
# would increase as optimizer implementation re-write SELECT query. In
# order to avoid these diffierences, we read system view definition that is
# hardcoded in source.

let $SELECT_CMD = SELECT TABLE_COMMENT
                  FROM INFORMATION_SCHEMA.TABLES
                  WHERE TABLE_SCHEMA='information_schema';
let $INSERT_CMD = INSERT INTO I_S_check_table(t) $SELECT_CMD;

let $WHERE_COND = AND TABLE_NAME='CHARACTER_SETS';
let $str = `$SELECT_CMD $WHERE_COND`;
echo $str;
eval INSERT INTO I_S_check_table(t) VALUES ("$str");

let $WHERE_COND = AND TABLE_NAME='CHECK_CONSTRAINTS';
replace_regex /(cat|sch).name COLLATE utf8mb3_tolower_ci/\1.name/;
let $str = `$SELECT_CMD $WHERE_COND`;
echo $str;
eval INSERT INTO I_S_check_table(t) VALUES ("$str");

let $WHERE_COND = AND TABLE_NAME='COLLATIONS';
let $str = `$SELECT_CMD $WHERE_COND`;
echo $str;
eval INSERT INTO I_S_check_table(t) VALUES ("$str");

let $WHERE_COND = AND TABLE_NAME='COLLATION_CHARACTER_SET_APPLICABILITY';
let $str = `$SELECT_CMD $WHERE_COND`;
echo $str;
eval INSERT INTO I_S_check_table(t) VALUES ("$str");

let $WHERE_COND = AND TABLE_NAME='COLUMNS';
replace_regex /(cat|sch|tbl).name COLLATE utf8mb3_tolower_ci/\1.name/;
let $str = `$SELECT_CMD $WHERE_COND`;
echo $str;
eval INSERT INTO I_S_check_table(t) VALUES ("$str");

let $WHERE_COND = AND TABLE_NAME='COLUMNS_EXTENSIONS';
#replace_regex /(cat|sch|tbl).name COLLATE utf8mb3_tolower_ci/\1.name/;
let $str = `$SELECT_CMD $WHERE_COND`;
echo $str;
eval INSERT INTO I_S_check_table(t) VALUES ("$str");

let $WHERE_COND = AND TABLE_NAME='COLUMN_STATISTICS';
let $str = `$SELECT_CMD $WHERE_COND`;
echo $str;
eval INSERT INTO I_S_check_table(t) VALUES ("$str");

let $WHERE_COND = AND TABLE_NAME='EVENTS';
replace_regex /(cat|sch).name COLLATE utf8mb3_tolower_ci/\1.name/;
let $str = `$SELECT_CMD $WHERE_COND`;
echo $str;
eval INSERT INTO I_S_check_table(t) VALUES ("$str");

let $WHERE_COND = AND TABLE_NAME='FILES';
let $str = `$SELECT_CMD $WHERE_COND`;
echo $str;
eval INSERT INTO I_S_check_table(t) VALUES ("$str");

let $WHERE_COND = AND TABLE_NAME='INNODB_DATAFILES';
let $str = `$SELECT_CMD $WHERE_COND`;
echo $str;
eval INSERT INTO I_S_check_table(t) VALUES ("$str");

let $WHERE_COND = AND TABLE_NAME='INNODB_FOREIGN';
replace_regex /(fk.name\)) COLLATE utf8mb3_tolower_ci/\1/;
let $str = `$SELECT_CMD $WHERE_COND`;
echo $str;
eval INSERT INTO I_S_check_table(t) VALUES ("$str");

let $WHERE_COND = AND TABLE_NAME='INNODB_FOREIGN_COLS';
replace_regex /(fk.name\)) COLLATE utf8mb3_tolower_ci/\1/;
let $str = `$SELECT_CMD $WHERE_COND`;
echo $str;
eval INSERT INTO I_S_check_table(t) VALUES ("$str");

let $WHERE_COND = AND TABLE_NAME='INNODB_FIELDS';
let $str = `$SELECT_CMD $WHERE_COND`;
echo $str;
eval INSERT INTO I_S_check_table(t) VALUES ("$str");

let $WHERE_COND = AND TABLE_NAME='INNODB_TABLESPACES_BRIEF';
let $str = `$SELECT_CMD $WHERE_COND`;
echo $str;
eval INSERT INTO I_S_check_table(t) VALUES ("$str");

let $WHERE_COND = AND TABLE_NAME='KEY_COLUMN_USAGE';
replace_regex /(cat|sch|tbl).name COLLATE utf8mb3_tolower_ci/\1.name/;
let $str = `$SELECT_CMD $WHERE_COND`;
echo $str;
eval INSERT INTO I_S_check_table(t) VALUES ("$str");

let $WHERE_COND = AND TABLE_NAME='KEYWORDS';
replace_regex /JSON_TABLE\(.*\)/JSON_TABLE(<elements masked>)/;
let $str = `$SELECT_CMD $WHERE_COND`;
echo $str;
eval INSERT INTO I_S_check_table(t) VALUES ("$str");

let $WHERE_COND = AND TABLE_NAME='PARAMETERS';
replace_regex /(cat|sch).name COLLATE utf8mb3_tolower_ci/\1.name/;
let $str = `$SELECT_CMD $WHERE_COND`;
echo $str;
eval INSERT INTO I_S_check_table(t) VALUES ("$str");

let $WHERE_COND = AND TABLE_NAME='PARTITIONS';
replace_regex /(cat|sch).name COLLATE utf8mb3_tolower_ci/\1.name/;
let $str = `$SELECT_CMD $WHERE_COND`;
echo $str;
eval INSERT INTO I_S_check_table(t) VALUES ("$str");

let $WHERE_COND = AND TABLE_NAME='REFERENTIAL_CONSTRAINTS';
let $str = `$SELECT_CMD $WHERE_COND`;
echo $str;
eval INSERT INTO I_S_check_table(t) VALUES ("$str");

let $WHERE_COND = AND TABLE_NAME='RESOURCE_GROUPS';
replace_regex /(cat|sch).name COLLATE utf8mb3_tolower_ci/\1.name/;
let $str = `$SELECT_CMD $WHERE_COND`;
echo $str;
eval INSERT INTO I_S_check_table(t) VALUES ("$str");

let $WHERE_COND = AND TABLE_NAME='ROUTINES';
replace_regex /(cat|sch).name COLLATE utf8mb3_tolower_ci/\1.name/;
let $str = `$SELECT_CMD $WHERE_COND`;
echo $str;
eval INSERT INTO I_S_check_table(t) VALUES ("$str");

let $WHERE_COND = AND TABLE_NAME='SHOW_STATISTICS';
replace_regex /(cat|sch|tbl).name COLLATE utf8mb3_tolower_ci/\1.name/;
let $str = `$SELECT_CMD $WHERE_COND`;
echo $str;
eval INSERT INTO I_S_check_table(t) VALUES ("$str");

let $WHERE_COND = AND TABLE_NAME='SCHEMATA';
replace_regex /(cat|sch).name COLLATE utf8mb3_tolower_ci/\1.name/
              /JSON_TABLE\(.*\)/JSON_TABLE(<elements masked>)/;
let $str = `$SELECT_CMD $WHERE_COND`;
echo $str;
eval INSERT INTO I_S_check_table(t) VALUES ("$str");

let $WHERE_COND = AND TABLE_NAME='SCHEMATA_EXTENSIONS';
replace_regex /(cat|sch).name COLLATE utf8mb3_tolower_ci/\1.name/
              /JSON_TABLE\(.*\)/JSON_TABLE(<elements masked>)/;
let $str = `$SELECT_CMD $WHERE_COND`;
echo $str;
eval INSERT INTO I_S_check_table(t) VALUES ("$str");

let $WHERE_COND = AND TABLE_NAME='ST_SPATIAL_REFERENCE_SYSTEMS';
let $str = `$SELECT_CMD $WHERE_COND`;
echo $str;
eval INSERT INTO I_S_check_table(t) VALUES ("$str");

let $WHERE_COND = AND TABLE_NAME='ST_UNITS_OF_MEASURE';
replace_regex /JSON_TABLE\(.*\)/JSON_TABLE(<elements masked>)/;
let $str = `$SELECT_CMD $WHERE_COND`;
echo $str;
eval INSERT INTO I_S_check_table(t) VALUES ("$str");

let $WHERE_COND = AND TABLE_NAME='ST_GEOMETRY_COLUMNS';
let $str = `$SELECT_CMD $WHERE_COND`;
echo $str;
eval INSERT INTO I_S_check_table(t) VALUES ("$str");

let $WHERE_COND = AND TABLE_NAME='STATISTICS';
replace_regex /(cat|sch|tbl).name COLLATE utf8mb3_tolower_ci/\1.name/;
let $str = `$SELECT_CMD $WHERE_COND`;
echo $str;
eval INSERT INTO I_S_check_table(t) VALUES ("$str");

let $WHERE_COND = AND TABLE_NAME='TABLE_CONSTRAINTS';
replace_regex /(cat|sch|tbl).name COLLATE utf8mb3_tolower_ci/\1.name/;
let $str = `$SELECT_CMD $WHERE_COND`;
echo $str;
eval INSERT INTO I_S_check_table(t) VALUES ("$str");

let $WHERE_COND = AND TABLE_NAME='TABLE_CONSTRAINTS_EXTENSIONS';
#replace_regex /(cat|sch|tbl).name COLLATE utf8mb3_tolower_ci/\1.name/;
let $str = `$SELECT_CMD $WHERE_COND`;
echo $str;
eval INSERT INTO I_S_check_table(t) VALUES ("$str");

let $WHERE_COND = AND TABLE_NAME='TABLES';
replace_regex /(cat|sch|tbl).name COLLATE utf8mb3_tolower_ci/\1.name/;
let $str = `$SELECT_CMD $WHERE_COND`;
echo $str;
eval INSERT INTO I_S_check_table(t) VALUES ("$str");

let $WHERE_COND = AND TABLE_NAME='TABLES_EXTENSIONS';
#replace_regex /(cat|sch|tbl).name COLLATE utf8mb3_tolower_ci/\1.name/;
let $str = `$SELECT_CMD $WHERE_COND`;
echo $str;
eval INSERT INTO I_S_check_table(t) VALUES ("$str");

let $WHERE_COND = AND TABLE_NAME='TABLESPACES_EXTENSIONS';
#replace_regex /(cat|sch|tbl).name COLLATE utf8mb3_tolower_ci/\1.name/;
let $str = `$SELECT_CMD $WHERE_COND`;
echo $str;
eval INSERT INTO I_S_check_table(t) VALUES ("$str");

let $WHERE_COND = AND TABLE_NAME='TRIGGERS';
replace_regex /(cat|sch|tbl).name COLLATE utf8mb3_tolower_ci/\1.name/;
let $str = `$SELECT_CMD $WHERE_COND`;
echo $str;
eval INSERT INTO I_S_check_table(t) VALUES ("$str");

let $WHERE_COND = AND TABLE_NAME='VIEW_ROUTINE_USAGE';
replace_regex /(cat|sch|vw).name COLLATE utf8mb3_tolower_ci/\1.name/
              /vru.routine_(catalog|schema) COLLATE utf8mb3_tolower_ci/vru.routine_\1/;
let $str = `$SELECT_CMD $WHERE_COND`;
echo $str;
eval INSERT INTO I_S_check_table(t) VALUES ("$str");

let $WHERE_COND = AND TABLE_NAME='VIEW_TABLE_USAGE';
replace_regex /(cat|sch|vw).name COLLATE utf8mb3_tolower_ci/\1.name/
              /vtu.table_(catalog|schema|name) COLLATE utf8mb3_tolower_ci/vtu.table_\1/;
let $str = `$SELECT_CMD $WHERE_COND`;
echo $str;
eval INSERT INTO I_S_check_table(t) VALUES ("$str");

let $WHERE_COND = AND TABLE_NAME='VIEWS';
replace_regex /(cat|sch|vw).name COLLATE utf8mb3_tolower_ci/\1.name/;
let $str = `$SELECT_CMD $WHERE_COND`;
echo $str;
eval INSERT INTO I_S_check_table(t) VALUES ("$str");

let $WHERE_COND = AND TABLE_NAME='APPLICABLE_ROLES';
let $str = `$SELECT_CMD $WHERE_COND`;
echo $str;
eval INSERT INTO I_S_check_table(t) VALUES ("$str");

let $WHERE_COND = AND TABLE_NAME='ADMINISTRABLE_ROLE_AUTHORIZATIONS';
let $str = `$SELECT_CMD $WHERE_COND`;
echo $str;
eval INSERT INTO I_S_check_table(t) VALUES ("$str");

let $WHERE_COND = AND TABLE_NAME='ENABLED_ROLES';
let $str = `$SELECT_CMD $WHERE_COND`;
echo $str;
eval INSERT INTO I_S_check_table(t) VALUES ("$str");

let $WHERE_COND = AND TABLE_NAME='ROLE_TABLE_GRANTS';
let $str = `$SELECT_CMD $WHERE_COND`;
echo $str;
eval INSERT INTO I_S_check_table(t) VALUES ("$str");

let $WHERE_COND = AND TABLE_NAME='ROLE_COLUMN_GRANTS';
let $str = `$SELECT_CMD $WHERE_COND`;
echo $str;
eval INSERT INTO I_S_check_table(t) VALUES ("$str");

let $WHERE_COND = AND TABLE_NAME='ROLE_ROUTINE_GRANTS';
let $str = `$SELECT_CMD $WHERE_COND`;
echo $str;
eval INSERT INTO I_S_check_table(t) VALUES ("$str");

let $WHERE_COND = AND TABLE_NAME='USER_ATTRIBUTES';
let $str = `$SELECT_CMD $WHERE_COND`;
echo $str;
eval INSERT INTO I_S_check_table(t) VALUES ('$str');

SET debug = '-d,fetch_system_view_definition';

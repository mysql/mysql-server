########################################################################
#
# Verify definitions of I_S system views.
# --------------------------------------
# This test has three parts:
#
# 1. Verify the existence of the expected I_S system views, neither less
#    nor more.
# 2. Verify the I_S_VERSION and the CREATE VIEW statements of the
#    I_S system views.
# 3. Verify the checksum of the generated meta data associated with the
#    current I_S version number.
#
########################################################################
#
# A note regarding the checksum usage.
# ------------------------------------
# This is based on a test by Marc Alff (perfschema.dd_version_check).
# The idea is to keep a record of each I_S version released with the
# I_S version number and a checksum created based on the meta data for
# the I_S views. It is important not to change the expected checksum
# once a release has been published.
#
# If there was an intentional change, leading to a new checksum, then:
#
# 1. Uncomment the debug output showing the low level meta data and
#    verify that the changes are intentional.
# 2. Create a new target I_S version (unless the existing target IS
#    version is not publicly released yet).
# 3. Add a new row in test.I_S_published_schema with the new target
#    I_S version and the new checksum (or update the expected
#    checksum if the target I_S version is not publicly released yet).
#
# Note that a failure to do 2, when doing 3, will result in a broken
# upgrade where the server code is expecting a certain I_S system view
# definition, while the actual persisted I_S system view definition is
# different (because it was created by a previous server binary, and
# I_S upgrade handling was not triggered).
#
# Thus, this part of the test is designed to be a reminder to do 2.
# by detecting differences in 3.
#
########################################################################
#
# How to handle test failures.
# ----------------------------
# In general, if there is a change leading to a failure in this test,
# then we must first consider whether the change is intentional, i.e.,
# whether there is a change in a I_S system view definition that
# that we actually want to do, or whether the change is by mistake
# or not needed.
#
# Then, if the change is intentional, we must make sure the following
# scenarios are handled:
#
# 1. Plain initialization of the server. This is most likely handled,
#    otherwise we wouldn't even get to the point where this test starts
#    failing.
# 2. Plain server restart using a data directory initialized by this
#    server.
# 3. Upgrade from any supported I_S version, or server restart using
#    a data directory from a different server version using the same
#    I_S version.
#
# Failures in the different parts of the test must be handled as
# follows:
#
# *  A failure in part 1 means that a I_S system view is added or removed.
#    In this case, please make sure the tables 'I_S_view_names'
#    below are kept up to date. Also make sure that
#    the list of SHOW CREATE VIEW statements is up to date.
# *  A failure in part 2 or 3 means that there is a change in the I_S
#    system view definitions, and that we have a new I_S version. So
#    if the current target I_S version number is released publicly,
#    then we must bump the target I_S version. If the I_S version is
#    not yet released, and has already been bumped, then it should
#    stay the same. This test must have its results re-recorded. A
#    failure in part 1 or 2 will most likely also lead to a failure
#    in part 3.
# *  A failure in part 3 means that the generated meta data is
#    different than it used to be. If there is a change in part 1 or
#    2, this is to be expected. In that case, the test results must
#    be re-recorded, and we must either update the expected checksum
#    (if the I_S version stays the same) or add the new I_S version
#    along with the expected checksum. However, if there is not a
#    change in part 1 or 2, then we have changes in the SQL DDL
#    semantics. This should be handled by changing the I_S initialization
#    procedure to either tweak the view statements before execution
#    by tweaking the generated low level meta data. Either way, the
#    result of the changes to the server code should be that
#    this test stops failing.
########################################################################
SET debug = '+d,skip_dd_table_access_check';
########################################################################
# PART 1
# Verify the existence of the expected I_S system views, neither less
########################################################################
CREATE TABLE test.I_S_view_names (name VARCHAR(64) PRIMARY KEY);
########################################################################
# Verify the existence of the expected I_S system views, neither
# less nor more. Fill help table with the I_S system view names.
########################################################################
########################################################################
# The number of I_S system views must be as expected.
########################################################################
include/assert.inc [There are 44 system views.]
include/assert.inc [There are 44 I_S system views in total.]
########################################################################
# No unexpected I_S tables must be present.
########################################################################
include/assert.inc [No unexpected I_S system view are present.]
########################################################################
# All expected I_S system views should be present.
########################################################################
include/assert.inc [All expected I_S system views are present.]
########################################################################
# PART 2. Verify the I_S_VERSION and the CREATE VIEW statements of the
#         I_S system views.
########################################################################
# Print the actual I_S version stored on disk.
Current I_S_VERSION=80030
CREATE TABLE I_S_check_table (id INT NOT NULL AUTO_INCREMENT PRIMARY KEY,
t TEXT NOT NULL,
row_hash VARCHAR(64) DEFAULT NULL);
SET debug = '+d,fetch_system_view_definition';
########################################################################
# Verify all the CREATE VIEW statements of the I_S system view.
# Mask collations that depend on the lower case table names setting.
########################################################################
CREATE OR REPLACE DEFINER=`mysql.infoschema`@`localhost` VIEW information_schema.CHARACTER_SETS AS SELECT 
  cs.name AS CHARACTER_SET_NAME,
  col.name AS DEFAULT_COLLATE_NAME,
  cs.comment AS DESCRIPTION,
  cs.mb_max_length AS MAXLEN FROM 
  mysql.character_sets cs
  JOIN mysql.collations col ON cs.default_collation_id=col.id 

INSERT INTO I_S_check_table(t) VALUES ("CREATE OR REPLACE DEFINER=`mysql.infoschema`@`localhost` VIEW information_schema.CHARACTER_SETS AS SELECT 
  cs.name AS CHARACTER_SET_NAME,
  col.name AS DEFAULT_COLLATE_NAME,
  cs.comment AS DESCRIPTION,
  cs.mb_max_length AS MAXLEN FROM 
  mysql.character_sets cs
  JOIN mysql.collations col ON cs.default_collation_id=col.id 
");
CREATE OR REPLACE DEFINER=`mysql.infoschema`@`localhost` VIEW information_schema.CHECK_CONSTRAINTS AS SELECT 
  cat.name AS CONSTRAINT_CATALOG,
  sch.name AS CONSTRAINT_SCHEMA,
  cc.name AS CONSTRAINT_NAME,
  cc.check_clause_utf8 AS CHECK_CLAUSE FROM 
  mysql.check_constraints cc
  JOIN mysql.tables tbl ON cc.table_id=tbl.id
  JOIN mysql.schemata sch ON tbl.schema_id=sch.id
  JOIN mysql.catalogs cat ON cat.id=sch.catalog_id WHERE 
  CAN_ACCESS_TABLE(sch.name, tbl.name)
  AND IS_VISIBLE_DD_OBJECT(tbl.hidden)

INSERT INTO I_S_check_table(t) VALUES ("CREATE OR REPLACE DEFINER=`mysql.infoschema`@`localhost` VIEW information_schema.CHECK_CONSTRAINTS AS SELECT 
  cat.name AS CONSTRAINT_CATALOG,
  sch.name AS CONSTRAINT_SCHEMA,
  cc.name AS CONSTRAINT_NAME,
  cc.check_clause_utf8 AS CHECK_CLAUSE FROM 
  mysql.check_constraints cc
  JOIN mysql.tables tbl ON cc.table_id=tbl.id
  JOIN mysql.schemata sch ON tbl.schema_id=sch.id
  JOIN mysql.catalogs cat ON cat.id=sch.catalog_id WHERE 
  CAN_ACCESS_TABLE(sch.name, tbl.name)
  AND IS_VISIBLE_DD_OBJECT(tbl.hidden)
");
CREATE OR REPLACE DEFINER=`mysql.infoschema`@`localhost` VIEW information_schema.COLLATIONS AS SELECT 
  col.name AS COLLATION_NAME,
  cs.name AS CHARACTER_SET_NAME,
  col.id AS ID,
  IF(EXISTS(SELECT * FROM mysql.character_sets           WHERE mysql.character_sets.default_collation_id= col.id),   'Yes','') AS IS_DEFAULT,
  IF(col.is_compiled,'Yes','') AS IS_COMPILED,
  col.sort_length AS SORTLEN,
  col.pad_attribute AS PAD_ATTRIBUTE FROM 
  mysql.collations col
  JOIN mysql.character_sets cs ON col.character_set_id=cs.id 

INSERT INTO I_S_check_table(t) VALUES ("CREATE OR REPLACE DEFINER=`mysql.infoschema`@`localhost` VIEW information_schema.COLLATIONS AS SELECT 
  col.name AS COLLATION_NAME,
  cs.name AS CHARACTER_SET_NAME,
  col.id AS ID,
  IF(EXISTS(SELECT * FROM mysql.character_sets           WHERE mysql.character_sets.default_collation_id= col.id),   'Yes','') AS IS_DEFAULT,
  IF(col.is_compiled,'Yes','') AS IS_COMPILED,
  col.sort_length AS SORTLEN,
  col.pad_attribute AS PAD_ATTRIBUTE FROM 
  mysql.collations col
  JOIN mysql.character_sets cs ON col.character_set_id=cs.id 
");
CREATE OR REPLACE DEFINER=`mysql.infoschema`@`localhost` VIEW information_schema.COLLATION_CHARACTER_SET_APPLICABILITY AS SELECT 
  col.name AS COLLATION_NAME,
  cs.name AS CHARACTER_SET_NAME FROM 
  mysql.character_sets cs
  JOIN mysql.collations col ON cs.id = col.character_set_id 

INSERT INTO I_S_check_table(t) VALUES ("CREATE OR REPLACE DEFINER=`mysql.infoschema`@`localhost` VIEW information_schema.COLLATION_CHARACTER_SET_APPLICABILITY AS SELECT 
  col.name AS COLLATION_NAME,
  cs.name AS CHARACTER_SET_NAME FROM 
  mysql.character_sets cs
  JOIN mysql.collations col ON cs.id = col.character_set_id 
");
CREATE OR REPLACE DEFINER=`mysql.infoschema`@`localhost` VIEW information_schema.COLUMNS AS SELECT 
  cat.name AS TABLE_CATALOG,
  sch.name AS TABLE_SCHEMA,
  tbl.name AS TABLE_NAME,
  col.name COLLATE utf8mb3_tolower_ci AS COLUMN_NAME,
  col.ordinal_position AS ORDINAL_POSITION,
  col.default_value_utf8 AS COLUMN_DEFAULT,
  IF (col.is_nullable = 1, 'YES','NO') AS IS_NULLABLE,
  SUBSTRING_INDEX(SUBSTRING_INDEX(col.column_type_utf8, '(', 1),' ', 1) AS DATA_TYPE,
  INTERNAL_DD_CHAR_LENGTH(col.type, col.char_length, coll.name, 0) AS CHARACTER_MAXIMUM_LENGTH,
  INTERNAL_DD_CHAR_LENGTH(col.type, col.char_length, coll.name, 1) AS CHARACTER_OCTET_LENGTH,
  IF (col.numeric_precision = 0, NULL, col.numeric_precision) AS NUMERIC_PRECISION,
  IF (col.numeric_scale = 0 && col.numeric_precision = 0,    NULL, col.numeric_scale) AS NUMERIC_SCALE,
  col.datetime_precision AS DATETIME_PRECISION,
  CASE col.type  WHEN 'MYSQL_TYPE_STRING' THEN (IF (cs.name='binary',NULL, cs.name))  WHEN 'MYSQL_TYPE_VAR_STRING' THEN (IF (cs.name='binary',NULL, cs.name))  WHEN 'MYSQL_TYPE_VARCHAR' THEN (IF (cs.name='binary',NULL, cs.name))  WHEN 'MYSQL_TYPE_TINY_BLOB' THEN (IF (cs.name='binary',NULL, cs.name))  WHEN 'MYSQL_TYPE_MEDIUM_BLOB' THEN (IF (cs.name='binary',NULL, cs.name))  WHEN 'MYSQL_TYPE_BLOB' THEN (IF (cs.name='binary',NULL, cs.name))  WHEN 'MYSQL_TYPE_LONG_BLOB' THEN (IF (cs.name='binary',NULL, cs.name))  WHEN 'MYSQL_TYPE_ENUM' THEN (IF (cs.name='binary',NULL, cs.name))  WHEN 'MYSQL_TYPE_SET' THEN (IF (cs.name='binary',NULL, cs.name))  ELSE NULL END AS CHARACTER_SET_NAME,
  CASE col.type  WHEN 'MYSQL_TYPE_STRING' THEN (IF (cs.name='binary',NULL, coll.name))  WHEN 'MYSQL_TYPE_VAR_STRING' THEN     (IF (cs.name='binary',NULL, coll.name))  WHEN 'MYSQL_TYPE_VARCHAR' THEN (IF (cs.name='binary',NULL, coll.name))  WHEN 'MYSQL_TYPE_TINY_BLOB' THEN (IF (cs.name='binary',NULL, coll.name))  WHEN 'MYSQL_TYPE_MEDIUM_BLOB' THEN     (IF (cs.name='binary',NULL, coll.name))  WHEN 'MYSQL_TYPE_BLOB' THEN (IF (cs.name='binary',NULL, coll.name))  WHEN 'MYSQL_TYPE_LONG_BLOB' THEN (IF (cs.name='binary',NULL, coll.name))  WHEN 'MYSQL_TYPE_ENUM' THEN (IF (cs.name='binary',NULL, coll.name))  WHEN 'MYSQL_TYPE_SET' THEN (IF (cs.name='binary',NULL, coll.name))  ELSE NULL END AS COLLATION_NAME,
  col.column_type_utf8 AS COLUMN_TYPE,
  col.column_key AS COLUMN_KEY,
  INTERNAL_GET_DD_COLUMN_EXTRA(ISNULL(col.generation_expression_utf8),  col.is_virtual, col.is_auto_increment, col.update_option,   IF(LENGTH(col.default_option), TRUE, FALSE), col.options,   col.hidden, tbl.type) AS EXTRA,
  GET_DD_COLUMN_PRIVILEGES(sch.name, tbl.name, col.name) AS PRIVILEGES,
  IFNULL(col.comment, '') AS COLUMN_COMMENT,
  IFNULL(col.generation_expression_utf8, '') AS GENERATION_EXPRESSION,
  col.srs_id AS SRS_ID FROM 
  mysql.columns col
  JOIN mysql.tables tbl ON col.table_id=tbl.id
  JOIN mysql.schemata sch ON tbl.schema_id=sch.id
  JOIN mysql.catalogs cat ON cat.id=sch.catalog_id
  JOIN mysql.collations coll ON col.collation_id=coll.id
  JOIN mysql.character_sets cs ON coll.character_set_id= cs.id WHERE 
  INTERNAL_GET_VIEW_WARNING_OR_ERROR(sch.name,tbl.name, tbl.type, tbl.options)
  AND CAN_ACCESS_COLUMN(sch.name, tbl.name, col.name)
  AND IS_VISIBLE_DD_OBJECT(tbl.hidden, col.hidden NOT IN ('Visible', 'User'), col.options)

INSERT INTO I_S_check_table(t) VALUES ("CREATE OR REPLACE DEFINER=`mysql.infoschema`@`localhost` VIEW information_schema.COLUMNS AS SELECT 
  cat.name AS TABLE_CATALOG,
  sch.name AS TABLE_SCHEMA,
  tbl.name AS TABLE_NAME,
  col.name COLLATE utf8mb3_tolower_ci AS COLUMN_NAME,
  col.ordinal_position AS ORDINAL_POSITION,
  col.default_value_utf8 AS COLUMN_DEFAULT,
  IF (col.is_nullable = 1, 'YES','NO') AS IS_NULLABLE,
  SUBSTRING_INDEX(SUBSTRING_INDEX(col.column_type_utf8, '(', 1),' ', 1) AS DATA_TYPE,
  INTERNAL_DD_CHAR_LENGTH(col.type, col.char_length, coll.name, 0) AS CHARACTER_MAXIMUM_LENGTH,
  INTERNAL_DD_CHAR_LENGTH(col.type, col.char_length, coll.name, 1) AS CHARACTER_OCTET_LENGTH,
  IF (col.numeric_precision = 0, NULL, col.numeric_precision) AS NUMERIC_PRECISION,
  IF (col.numeric_scale = 0 && col.numeric_precision = 0,    NULL, col.numeric_scale) AS NUMERIC_SCALE,
  col.datetime_precision AS DATETIME_PRECISION,
  CASE col.type  WHEN 'MYSQL_TYPE_STRING' THEN (IF (cs.name='binary',NULL, cs.name))  WHEN 'MYSQL_TYPE_VAR_STRING' THEN (IF (cs.name='binary',NULL, cs.name))  WHEN 'MYSQL_TYPE_VARCHAR' THEN (IF (cs.name='binary',NULL, cs.name))  WHEN 'MYSQL_TYPE_TINY_BLOB' THEN (IF (cs.name='binary',NULL, cs.name))  WHEN 'MYSQL_TYPE_MEDIUM_BLOB' THEN (IF (cs.name='binary',NULL, cs.name))  WHEN 'MYSQL_TYPE_BLOB' THEN (IF (cs.name='binary',NULL, cs.name))  WHEN 'MYSQL_TYPE_LONG_BLOB' THEN (IF (cs.name='binary',NULL, cs.name))  WHEN 'MYSQL_TYPE_ENUM' THEN (IF (cs.name='binary',NULL, cs.name))  WHEN 'MYSQL_TYPE_SET' THEN (IF (cs.name='binary',NULL, cs.name))  ELSE NULL END AS CHARACTER_SET_NAME,
  CASE col.type  WHEN 'MYSQL_TYPE_STRING' THEN (IF (cs.name='binary',NULL, coll.name))  WHEN 'MYSQL_TYPE_VAR_STRING' THEN     (IF (cs.name='binary',NULL, coll.name))  WHEN 'MYSQL_TYPE_VARCHAR' THEN (IF (cs.name='binary',NULL, coll.name))  WHEN 'MYSQL_TYPE_TINY_BLOB' THEN (IF (cs.name='binary',NULL, coll.name))  WHEN 'MYSQL_TYPE_MEDIUM_BLOB' THEN     (IF (cs.name='binary',NULL, coll.name))  WHEN 'MYSQL_TYPE_BLOB' THEN (IF (cs.name='binary',NULL, coll.name))  WHEN 'MYSQL_TYPE_LONG_BLOB' THEN (IF (cs.name='binary',NULL, coll.name))  WHEN 'MYSQL_TYPE_ENUM' THEN (IF (cs.name='binary',NULL, coll.name))  WHEN 'MYSQL_TYPE_SET' THEN (IF (cs.name='binary',NULL, coll.name))  ELSE NULL END AS COLLATION_NAME,
  col.column_type_utf8 AS COLUMN_TYPE,
  col.column_key AS COLUMN_KEY,
  INTERNAL_GET_DD_COLUMN_EXTRA(ISNULL(col.generation_expression_utf8),  col.is_virtual, col.is_auto_increment, col.update_option,   IF(LENGTH(col.default_option), TRUE, FALSE), col.options,   col.hidden, tbl.type) AS EXTRA,
  GET_DD_COLUMN_PRIVILEGES(sch.name, tbl.name, col.name) AS PRIVILEGES,
  IFNULL(col.comment, '') AS COLUMN_COMMENT,
  IFNULL(col.generation_expression_utf8, '') AS GENERATION_EXPRESSION,
  col.srs_id AS SRS_ID FROM 
  mysql.columns col
  JOIN mysql.tables tbl ON col.table_id=tbl.id
  JOIN mysql.schemata sch ON tbl.schema_id=sch.id
  JOIN mysql.catalogs cat ON cat.id=sch.catalog_id
  JOIN mysql.collations coll ON col.collation_id=coll.id
  JOIN mysql.character_sets cs ON coll.character_set_id= cs.id WHERE 
  INTERNAL_GET_VIEW_WARNING_OR_ERROR(sch.name,tbl.name, tbl.type, tbl.options)
  AND CAN_ACCESS_COLUMN(sch.name, tbl.name, col.name)
  AND IS_VISIBLE_DD_OBJECT(tbl.hidden, col.hidden NOT IN ('Visible', 'User'), col.options)
");
CREATE OR REPLACE DEFINER=`mysql.infoschema`@`localhost` VIEW information_schema.COLUMNS_EXTENSIONS AS SELECT 
  cat.name AS TABLE_CATALOG,
  sch.name AS TABLE_SCHEMA,
  tbl.name AS TABLE_NAME,
  col.name COLLATE utf8mb3_tolower_ci AS COLUMN_NAME,
  col.engine_attribute AS ENGINE_ATTRIBUTE,
  col.secondary_engine_attribute AS SECONDARY_ENGINE_ATTRIBUTE FROM 
  mysql.columns col
  JOIN mysql.tables tbl ON col.table_id=tbl.id
  JOIN mysql.schemata sch ON tbl.schema_id=sch.id
  JOIN mysql.catalogs cat ON cat.id=sch.catalog_id WHERE 
  INTERNAL_GET_VIEW_WARNING_OR_ERROR(sch.name,tbl.name, tbl.type, tbl.options)
  AND CAN_ACCESS_COLUMN(sch.name, tbl.name, col.name)
  AND IS_VISIBLE_DD_OBJECT(tbl.hidden, col.hidden NOT IN ('Visible', 'User'), col.options)

INSERT INTO I_S_check_table(t) VALUES ("CREATE OR REPLACE DEFINER=`mysql.infoschema`@`localhost` VIEW information_schema.COLUMNS_EXTENSIONS AS SELECT 
  cat.name AS TABLE_CATALOG,
  sch.name AS TABLE_SCHEMA,
  tbl.name AS TABLE_NAME,
  col.name COLLATE utf8mb3_tolower_ci AS COLUMN_NAME,
  col.engine_attribute AS ENGINE_ATTRIBUTE,
  col.secondary_engine_attribute AS SECONDARY_ENGINE_ATTRIBUTE FROM 
  mysql.columns col
  JOIN mysql.tables tbl ON col.table_id=tbl.id
  JOIN mysql.schemata sch ON tbl.schema_id=sch.id
  JOIN mysql.catalogs cat ON cat.id=sch.catalog_id WHERE 
  INTERNAL_GET_VIEW_WARNING_OR_ERROR(sch.name,tbl.name, tbl.type, tbl.options)
  AND CAN_ACCESS_COLUMN(sch.name, tbl.name, col.name)
  AND IS_VISIBLE_DD_OBJECT(tbl.hidden, col.hidden NOT IN ('Visible', 'User'), col.options)
");
CREATE OR REPLACE DEFINER=`mysql.infoschema`@`localhost` VIEW information_schema.COLUMN_STATISTICS AS SELECT 
  SCHEMA_NAME AS SCHEMA_NAME,
  TABLE_NAME AS TABLE_NAME,
  COLUMN_NAME AS COLUMN_NAME,
  HISTOGRAM AS HISTOGRAM FROM 
  mysql.column_statistics WHERE 
  CAN_ACCESS_TABLE(SCHEMA_NAME, TABLE_NAME)

INSERT INTO I_S_check_table(t) VALUES ("CREATE OR REPLACE DEFINER=`mysql.infoschema`@`localhost` VIEW information_schema.COLUMN_STATISTICS AS SELECT 
  SCHEMA_NAME AS SCHEMA_NAME,
  TABLE_NAME AS TABLE_NAME,
  COLUMN_NAME AS COLUMN_NAME,
  HISTOGRAM AS HISTOGRAM FROM 
  mysql.column_statistics WHERE 
  CAN_ACCESS_TABLE(SCHEMA_NAME, TABLE_NAME)
");
CREATE OR REPLACE DEFINER=`mysql.infoschema`@`localhost` VIEW information_schema.EVENTS AS SELECT 
  cat.name AS EVENT_CATALOG,
  sch.name AS EVENT_SCHEMA,
  evt.name AS EVENT_NAME,
  evt.definer AS DEFINER,
  evt.time_zone AS TIME_ZONE,
  'SQL' AS EVENT_BODY,
  evt.definition_utf8 AS EVENT_DEFINITION,
  IF (ISNULL(evt.interval_value),'ONE TIME','RECURRING') AS EVENT_TYPE,
  CONVERT_TZ(evt.execute_at,'+00:00', evt.time_zone) AS EXECUTE_AT,
  CONVERT_INTERVAL_TO_USER_INTERVAL(evt.interval_value,evt.interval_field) AS INTERVAL_VALUE,
  evt.interval_field AS INTERVAL_FIELD,
  evt.sql_mode AS SQL_MODE,
  CONVERT_TZ(evt.starts,'+00:00', evt.time_zone) AS STARTS,
  CONVERT_TZ(evt.ends,'+00:00', evt.time_zone) AS ENDS,
  evt.status AS STATUS,
  IF (evt.on_completion='DROP', 'NOT PRESERVE', 'PRESERVE') AS ON_COMPLETION,
  evt.created AS CREATED,
  evt.last_altered AS LAST_ALTERED,
  CONVERT_TZ(evt.last_executed,'+00:00', evt.time_zone) AS LAST_EXECUTED,
  evt.comment AS EVENT_COMMENT,
  evt.originator AS ORIGINATOR,
  cs_client.name AS CHARACTER_SET_CLIENT,
  coll_conn.name AS COLLATION_CONNECTION,
  coll_db.name AS DATABASE_COLLATION FROM 
  mysql.events evt
  JOIN mysql.schemata sch ON evt.schema_id=sch.id
  JOIN mysql.catalogs cat ON cat.id=sch.catalog_id
  JOIN mysql.collations coll_client ON coll_client.id=evt.client_collation_id
  JOIN mysql.character_sets cs_client ON cs_client.id=coll_client.character_set_id
  JOIN mysql.collations coll_conn ON coll_conn.id=evt.connection_collation_id
  JOIN mysql.collations coll_db ON coll_db.id=evt.schema_collation_id WHERE 
  CAN_ACCESS_EVENT(sch.name)

INSERT INTO I_S_check_table(t) VALUES ("CREATE OR REPLACE DEFINER=`mysql.infoschema`@`localhost` VIEW information_schema.EVENTS AS SELECT 
  cat.name AS EVENT_CATALOG,
  sch.name AS EVENT_SCHEMA,
  evt.name AS EVENT_NAME,
  evt.definer AS DEFINER,
  evt.time_zone AS TIME_ZONE,
  'SQL' AS EVENT_BODY,
  evt.definition_utf8 AS EVENT_DEFINITION,
  IF (ISNULL(evt.interval_value),'ONE TIME','RECURRING') AS EVENT_TYPE,
  CONVERT_TZ(evt.execute_at,'+00:00', evt.time_zone) AS EXECUTE_AT,
  CONVERT_INTERVAL_TO_USER_INTERVAL(evt.interval_value,evt.interval_field) AS INTERVAL_VALUE,
  evt.interval_field AS INTERVAL_FIELD,
  evt.sql_mode AS SQL_MODE,
  CONVERT_TZ(evt.starts,'+00:00', evt.time_zone) AS STARTS,
  CONVERT_TZ(evt.ends,'+00:00', evt.time_zone) AS ENDS,
  evt.status AS STATUS,
  IF (evt.on_completion='DROP', 'NOT PRESERVE', 'PRESERVE') AS ON_COMPLETION,
  evt.created AS CREATED,
  evt.last_altered AS LAST_ALTERED,
  CONVERT_TZ(evt.last_executed,'+00:00', evt.time_zone) AS LAST_EXECUTED,
  evt.comment AS EVENT_COMMENT,
  evt.originator AS ORIGINATOR,
  cs_client.name AS CHARACTER_SET_CLIENT,
  coll_conn.name AS COLLATION_CONNECTION,
  coll_db.name AS DATABASE_COLLATION FROM 
  mysql.events evt
  JOIN mysql.schemata sch ON evt.schema_id=sch.id
  JOIN mysql.catalogs cat ON cat.id=sch.catalog_id
  JOIN mysql.collations coll_client ON coll_client.id=evt.client_collation_id
  JOIN mysql.character_sets cs_client ON cs_client.id=coll_client.character_set_id
  JOIN mysql.collations coll_conn ON coll_conn.id=evt.connection_collation_id
  JOIN mysql.collations coll_db ON coll_db.id=evt.schema_collation_id WHERE 
  CAN_ACCESS_EVENT(sch.name)
");
CREATE OR REPLACE DEFINER=`mysql.infoschema`@`localhost` VIEW information_schema.FILES AS SELECT 
  INTERNAL_TABLESPACE_ID(ts.name, tsf.file_name, ts.engine, ts.se_private_data) AS FILE_ID,
  REPLACE(IF( INSTR('./', LEFT(tsf.file_name,1)) = 0 AND             SUBSTRING(tsf.file_name,2,1) != ':', CONCAT('./', tsf.file_name), tsf.file_name), '\\', '/') AS FILE_NAME,
  INTERNAL_TABLESPACE_TYPE(ts.name, tsf.file_name, ts.engine, ts.se_private_data) AS FILE_TYPE,
  ts.name AS TABLESPACE_NAME,
  '' AS TABLE_CATALOG,
  NULL AS TABLE_SCHEMA,
  NULL AS TABLE_NAME,
  INTERNAL_TABLESPACE_LOGFILE_GROUP_NAME(ts.name,tsf.file_name, ts.engine, ts.se_private_data) AS LOGFILE_GROUP_NAME,
  INTERNAL_TABLESPACE_LOGFILE_GROUP_NUMBER(ts.name,tsf.file_name, ts.engine, ts.se_private_data) AS LOGFILE_GROUP_NUMBER,
  ts.engine AS ENGINE,
  NULL AS FULLTEXT_KEYS,
  NULL AS DELETED_ROWS,
  NULL AS UPDATE_COUNT,
  INTERNAL_TABLESPACE_FREE_EXTENTS(ts.name, tsf.file_name, ts.engine, ts.se_private_data) AS FREE_EXTENTS,
  INTERNAL_TABLESPACE_TOTAL_EXTENTS(ts.name, tsf.file_name, ts.engine, ts.se_private_data) AS TOTAL_EXTENTS,
  INTERNAL_TABLESPACE_EXTENT_SIZE(ts.name, tsf.file_name, ts.engine, ts.se_private_data) AS EXTENT_SIZE,
  INTERNAL_TABLESPACE_INITIAL_SIZE(ts.name, tsf.file_name, ts.engine, ts.se_private_data) AS INITIAL_SIZE,
  INTERNAL_TABLESPACE_MAXIMUM_SIZE(ts.name, tsf.file_name, ts.engine, ts.se_private_data) AS MAXIMUM_SIZE,
  INTERNAL_TABLESPACE_AUTOEXTEND_SIZE(ts.name, tsf.file_name, ts.engine, ts.se_private_data) AS AUTOEXTEND_SIZE,
  NULL AS CREATION_TIME,
  NULL AS LAST_UPDATE_TIME,
  NULL AS LAST_ACCESS_TIME,
  NULL AS RECOVER_TIME,
  NULL AS TRANSACTION_COUNTER,
  INTERNAL_TABLESPACE_VERSION(ts.name,tsf.file_name, ts.engine, ts.se_private_data) AS VERSION,
  INTERNAL_TABLESPACE_ROW_FORMAT(ts.name,tsf.file_name, ts.engine, ts.se_private_data) AS ROW_FORMAT,
  NULL AS TABLE_ROWS,
  NULL AS AVG_ROW_LENGTH,
  NULL AS DATA_LENGTH,
  NULL AS MAX_DATA_LENGTH,
  NULL AS INDEX_LENGTH,
  INTERNAL_TABLESPACE_DATA_FREE(ts.name, tsf.file_name, ts.engine, ts.se_private_data) AS DATA_FREE,
  NULL AS CREATE_TIME,
  NULL AS UPDATE_TIME,
  NULL AS CHECK_TIME,
  NULL AS CHECKSUM,
  INTERNAL_TABLESPACE_STATUS(ts.name, tsf.file_name, ts.engine, ts.se_private_data) AS STATUS,
  INTERNAL_TABLESPACE_EXTRA(ts.name, tsf.file_name, ts.engine, ts.se_private_data) AS EXTRA FROM 
  mysql.tablespaces ts
  JOIN mysql.tablespace_files tsf ON ts.id=tsf.tablespace_id

INSERT INTO I_S_check_table(t) VALUES ("CREATE OR REPLACE DEFINER=`mysql.infoschema`@`localhost` VIEW information_schema.FILES AS SELECT 
  INTERNAL_TABLESPACE_ID(ts.name, tsf.file_name, ts.engine, ts.se_private_data) AS FILE_ID,
  REPLACE(IF( INSTR('./', LEFT(tsf.file_name,1)) = 0 AND             SUBSTRING(tsf.file_name,2,1) != ':', CONCAT('./', tsf.file_name), tsf.file_name), '\\', '/') AS FILE_NAME,
  INTERNAL_TABLESPACE_TYPE(ts.name, tsf.file_name, ts.engine, ts.se_private_data) AS FILE_TYPE,
  ts.name AS TABLESPACE_NAME,
  '' AS TABLE_CATALOG,
  NULL AS TABLE_SCHEMA,
  NULL AS TABLE_NAME,
  INTERNAL_TABLESPACE_LOGFILE_GROUP_NAME(ts.name,tsf.file_name, ts.engine, ts.se_private_data) AS LOGFILE_GROUP_NAME,
  INTERNAL_TABLESPACE_LOGFILE_GROUP_NUMBER(ts.name,tsf.file_name, ts.engine, ts.se_private_data) AS LOGFILE_GROUP_NUMBER,
  ts.engine AS ENGINE,
  NULL AS FULLTEXT_KEYS,
  NULL AS DELETED_ROWS,
  NULL AS UPDATE_COUNT,
  INTERNAL_TABLESPACE_FREE_EXTENTS(ts.name, tsf.file_name, ts.engine, ts.se_private_data) AS FREE_EXTENTS,
  INTERNAL_TABLESPACE_TOTAL_EXTENTS(ts.name, tsf.file_name, ts.engine, ts.se_private_data) AS TOTAL_EXTENTS,
  INTERNAL_TABLESPACE_EXTENT_SIZE(ts.name, tsf.file_name, ts.engine, ts.se_private_data) AS EXTENT_SIZE,
  INTERNAL_TABLESPACE_INITIAL_SIZE(ts.name, tsf.file_name, ts.engine, ts.se_private_data) AS INITIAL_SIZE,
  INTERNAL_TABLESPACE_MAXIMUM_SIZE(ts.name, tsf.file_name, ts.engine, ts.se_private_data) AS MAXIMUM_SIZE,
  INTERNAL_TABLESPACE_AUTOEXTEND_SIZE(ts.name, tsf.file_name, ts.engine, ts.se_private_data) AS AUTOEXTEND_SIZE,
  NULL AS CREATION_TIME,
  NULL AS LAST_UPDATE_TIME,
  NULL AS LAST_ACCESS_TIME,
  NULL AS RECOVER_TIME,
  NULL AS TRANSACTION_COUNTER,
  INTERNAL_TABLESPACE_VERSION(ts.name,tsf.file_name, ts.engine, ts.se_private_data) AS VERSION,
  INTERNAL_TABLESPACE_ROW_FORMAT(ts.name,tsf.file_name, ts.engine, ts.se_private_data) AS ROW_FORMAT,
  NULL AS TABLE_ROWS,
  NULL AS AVG_ROW_LENGTH,
  NULL AS DATA_LENGTH,
  NULL AS MAX_DATA_LENGTH,
  NULL AS INDEX_LENGTH,
  INTERNAL_TABLESPACE_DATA_FREE(ts.name, tsf.file_name, ts.engine, ts.se_private_data) AS DATA_FREE,
  NULL AS CREATE_TIME,
  NULL AS UPDATE_TIME,
  NULL AS CHECK_TIME,
  NULL AS CHECKSUM,
  INTERNAL_TABLESPACE_STATUS(ts.name, tsf.file_name, ts.engine, ts.se_private_data) AS STATUS,
  INTERNAL_TABLESPACE_EXTRA(ts.name, tsf.file_name, ts.engine, ts.se_private_data) AS EXTRA FROM 
  mysql.tablespaces ts
  JOIN mysql.tablespace_files tsf ON ts.id=tsf.tablespace_id
");
CREATE OR REPLACE DEFINER=`mysql.infoschema`@`localhost` VIEW information_schema.INNODB_DATAFILES AS SELECT 
  GET_DD_TABLESPACE_PRIVATE_DATA(ts.se_private_data, 'id') AS SPACE,
  ts_files.file_name AS PATH FROM 
  mysql.tablespace_files ts_files
  JOIN mysql.tablespaces ts ON ts.id=ts_files.tablespace_id WHERE 
  ts.se_private_data IS NOT NULL
  AND ts.engine='InnoDB'
  AND ts.name<>'mysql'
  AND ts.name<>'innodb_temporary'

INSERT INTO I_S_check_table(t) VALUES ("CREATE OR REPLACE DEFINER=`mysql.infoschema`@`localhost` VIEW information_schema.INNODB_DATAFILES AS SELECT 
  GET_DD_TABLESPACE_PRIVATE_DATA(ts.se_private_data, 'id') AS SPACE,
  ts_files.file_name AS PATH FROM 
  mysql.tablespace_files ts_files
  JOIN mysql.tablespaces ts ON ts.id=ts_files.tablespace_id WHERE 
  ts.se_private_data IS NOT NULL
  AND ts.engine='InnoDB'
  AND ts.name<>'mysql'
  AND ts.name<>'innodb_temporary'
");
CREATE OR REPLACE DEFINER=`mysql.infoschema`@`localhost` VIEW information_schema.INNODB_FOREIGN AS SELECT 
  CONCAT(sch.name, '/', fk.name) AS ID,
  CONCAT(sch.name, '/', tbl.name) AS FOR_NAME,
  CONCAT(fk.referenced_table_schema, '/', fk.referenced_table_name) AS REF_NAME,
  COUNT(*) AS N_COLS,
  IF(fk.delete_rule='CASCADE',1,0)|IF(fk.delete_rule='SET NULL',2,0)|IF(fk.update_rule='CASCADE',4,0)|IF(fk.update_rule='SET NULL',8,0)|IF(fk.delete_rule='NO ACTION',16,0)|IF(fk.update_rule='NO ACTION',32,0) AS TYPE FROM 
  mysql.foreign_keys fk
  JOIN mysql.tables tbl ON fk.table_id=tbl.id
  JOIN mysql.schemata sch ON fk.schema_id=sch.id
  JOIN mysql.foreign_key_column_usage col ON fk.id=col.foreign_key_id WHERE 
  NOT tbl.type = 'VIEW'
  AND tbl.hidden = 'Visible'
  AND tbl.se_private_id IS NOT NULL
  AND tbl.engine='INNODB'
  GROUP BY fk.id

INSERT INTO I_S_check_table(t) VALUES ("CREATE OR REPLACE DEFINER=`mysql.infoschema`@`localhost` VIEW information_schema.INNODB_FOREIGN AS SELECT 
  CONCAT(sch.name, '/', fk.name) AS ID,
  CONCAT(sch.name, '/', tbl.name) AS FOR_NAME,
  CONCAT(fk.referenced_table_schema, '/', fk.referenced_table_name) AS REF_NAME,
  COUNT(*) AS N_COLS,
  IF(fk.delete_rule='CASCADE',1,0)|IF(fk.delete_rule='SET NULL',2,0)|IF(fk.update_rule='CASCADE',4,0)|IF(fk.update_rule='SET NULL',8,0)|IF(fk.delete_rule='NO ACTION',16,0)|IF(fk.update_rule='NO ACTION',32,0) AS TYPE FROM 
  mysql.foreign_keys fk
  JOIN mysql.tables tbl ON fk.table_id=tbl.id
  JOIN mysql.schemata sch ON fk.schema_id=sch.id
  JOIN mysql.foreign_key_column_usage col ON fk.id=col.foreign_key_id WHERE 
  NOT tbl.type = 'VIEW'
  AND tbl.hidden = 'Visible'
  AND tbl.se_private_id IS NOT NULL
  AND tbl.engine='INNODB'
  GROUP BY fk.id
");
CREATE OR REPLACE DEFINER=`mysql.infoschema`@`localhost` VIEW information_schema.INNODB_FOREIGN_COLS AS SELECT 
  CONCAT(sch.name, '/', fk.name) AS ID,
  col.name AS FOR_COL_NAME,
  referenced_column_name AS REF_COL_NAME,
  fk_col.ordinal_position AS POS FROM 
  mysql.foreign_key_column_usage fk_col
  JOIN mysql.foreign_keys fk ON fk.id=fk_col.foreign_key_id
  JOIN mysql.tables tbl ON fk.table_id=tbl.id
  JOIN mysql.schemata sch ON fk.schema_id=sch.id
  JOIN mysql.columns col ON tbl.id=col.table_id AND fk_col.column_id=col.id WHERE 
  NOT tbl.type = 'VIEW'
  AND tbl.hidden = 'Visible'
  AND tbl.se_private_id IS NOT NULL
  AND tbl.engine='INNODB'

INSERT INTO I_S_check_table(t) VALUES ("CREATE OR REPLACE DEFINER=`mysql.infoschema`@`localhost` VIEW information_schema.INNODB_FOREIGN_COLS AS SELECT 
  CONCAT(sch.name, '/', fk.name) AS ID,
  col.name AS FOR_COL_NAME,
  referenced_column_name AS REF_COL_NAME,
  fk_col.ordinal_position AS POS FROM 
  mysql.foreign_key_column_usage fk_col
  JOIN mysql.foreign_keys fk ON fk.id=fk_col.foreign_key_id
  JOIN mysql.tables tbl ON fk.table_id=tbl.id
  JOIN mysql.schemata sch ON fk.schema_id=sch.id
  JOIN mysql.columns col ON tbl.id=col.table_id AND fk_col.column_id=col.id WHERE 
  NOT tbl.type = 'VIEW'
  AND tbl.hidden = 'Visible'
  AND tbl.se_private_id IS NOT NULL
  AND tbl.engine='INNODB'
");
CREATE OR REPLACE DEFINER=`mysql.infoschema`@`localhost` VIEW information_schema.INNODB_FIELDS AS SELECT 
  GET_DD_INDEX_PRIVATE_DATA(idx.se_private_data, 'id') AS INDEX_ID,
  col.name AS NAME,
  fld.ordinal_position - 1 AS POS FROM 
  mysql.index_column_usage fld
  JOIN mysql.columns col ON fld.column_id=col.id
  JOIN mysql.indexes idx ON fld.index_id=idx.id
  JOIN mysql.tables tbl ON tbl.id=idx.table_id WHERE 
  NOT tbl.type = 'VIEW'
  AND tbl.hidden = 'Visible'
  AND NOT fld.hidden
  AND tbl.se_private_id IS NOT NULL
  AND tbl.engine='INNODB'

INSERT INTO I_S_check_table(t) VALUES ("CREATE OR REPLACE DEFINER=`mysql.infoschema`@`localhost` VIEW information_schema.INNODB_FIELDS AS SELECT 
  GET_DD_INDEX_PRIVATE_DATA(idx.se_private_data, 'id') AS INDEX_ID,
  col.name AS NAME,
  fld.ordinal_position - 1 AS POS FROM 
  mysql.index_column_usage fld
  JOIN mysql.columns col ON fld.column_id=col.id
  JOIN mysql.indexes idx ON fld.index_id=idx.id
  JOIN mysql.tables tbl ON tbl.id=idx.table_id WHERE 
  NOT tbl.type = 'VIEW'
  AND tbl.hidden = 'Visible'
  AND NOT fld.hidden
  AND tbl.se_private_id IS NOT NULL
  AND tbl.engine='INNODB'
");
CREATE OR REPLACE DEFINER=`mysql.infoschema`@`localhost` VIEW information_schema.INNODB_TABLESPACES_BRIEF AS SELECT 
  GET_DD_TABLESPACE_PRIVATE_DATA(ts.se_private_data, 'id') AS SPACE,
  ts.name AS NAME,
  ts_files.file_name AS PATH,
  GET_DD_TABLESPACE_PRIVATE_DATA(ts.se_private_data, 'flags') AS FLAG,
  IF(GET_DD_TABLESPACE_PRIVATE_DATA(ts.se_private_data, 'id')=0, 'System',    IF((GET_DD_TABLESPACE_PRIVATE_DATA(ts.se_private_data,                                       'flags')&2048)>>11 != 0,       'General', 'Single')) AS SPACE_TYPE FROM 
  mysql.tablespace_files ts_files
  JOIN mysql.tablespaces ts ON ts.id=ts_files.tablespace_id WHERE 
  ts.se_private_data IS NOT NULL
  AND ts.engine='InnoDB'
  AND ts.name<>'mysql'
  AND ts.name<>'innodb_temporary'

INSERT INTO I_S_check_table(t) VALUES ("CREATE OR REPLACE DEFINER=`mysql.infoschema`@`localhost` VIEW information_schema.INNODB_TABLESPACES_BRIEF AS SELECT 
  GET_DD_TABLESPACE_PRIVATE_DATA(ts.se_private_data, 'id') AS SPACE,
  ts.name AS NAME,
  ts_files.file_name AS PATH,
  GET_DD_TABLESPACE_PRIVATE_DATA(ts.se_private_data, 'flags') AS FLAG,
  IF(GET_DD_TABLESPACE_PRIVATE_DATA(ts.se_private_data, 'id')=0, 'System',    IF((GET_DD_TABLESPACE_PRIVATE_DATA(ts.se_private_data,                                       'flags')&2048)>>11 != 0,       'General', 'Single')) AS SPACE_TYPE FROM 
  mysql.tablespace_files ts_files
  JOIN mysql.tablespaces ts ON ts.id=ts_files.tablespace_id WHERE 
  ts.se_private_data IS NOT NULL
  AND ts.engine='InnoDB'
  AND ts.name<>'mysql'
  AND ts.name<>'innodb_temporary'
");
CREATE OR REPLACE DEFINER=`mysql.infoschema`@`localhost` VIEW information_schema.KEY_COLUMN_USAGE AS SELECT 
  cat.name AS CONSTRAINT_CATALOG,
  sch.name AS CONSTRAINT_SCHEMA,
  constraints.CONSTRAINT_NAME AS CONSTRAINT_NAME,
  cat.name AS TABLE_CATALOG,
  sch.name AS TABLE_SCHEMA,
  tbl.name AS TABLE_NAME,
  col.name COLLATE utf8mb3_tolower_ci AS COLUMN_NAME,
  constraints.ordinal_position AS ORDINAL_POSITION,
  constraints.POSITION_IN_UNIQUE_CONSTRAINT AS POSITION_IN_UNIQUE_CONSTRAINT,
  constraints.REFERENCED_TABLE_SCHEMA AS REFERENCED_TABLE_SCHEMA,
  constraints.REFERENCED_TABLE_NAME AS REFERENCED_TABLE_NAME,
  constraints.REFERENCED_COLUMN_NAME AS REFERENCED_COLUMN_NAME FROM 
  mysql.tables tbl
  JOIN mysql.schemata sch ON tbl.schema_id=sch.id
  JOIN mysql.catalogs cat ON cat.id=sch.catalog_id
  , LATERAL (SELECT    idx.name AS CONSTRAINT_NAME,    icu.ordinal_position AS ORDINAL_POSITION,    NULL AS POSITION_IN_UNIQUE_CONSTRAINT,    NULL AS REFERENCED_TABLE_SCHEMA,    NULL AS REFERENCED_TABLE_NAME,    NULL AS REFERENCED_COLUMN_NAME,    icu.column_id,    idx.hidden OR icu.hidden AS HIDDEN    FROM mysql.indexes idx    JOIN mysql.index_column_usage icu ON icu.index_id = idx.id    WHERE idx.table_id = tbl.id      AND idx.type IN ('PRIMARY', 'UNIQUE')  UNION ALL SELECT    fk.name COLLATE utf8mb3_tolower_ci AS CONSTRAINT_NAME,    fkcu.ordinal_position AS ORDINAL_POSITION,    fkcu.ordinal_position AS POSITION_IN_UNIQUE_CONSTRAINT,    fk.referenced_table_schema AS REFERENCED_TABLE_SCHEMA,    fk.referenced_table_name AS REFERENCED_TABLE_NAME,    fkcu.referenced_column_name AS REFERENCED_COLUMN_NAME,    fkcu.column_id,    FALSE AS HIDDEN    FROM mysql.foreign_keys fk    JOIN mysql.foreign_key_column_usage fkcu ON fkcu.foreign_key_id = fk.id    WHERE fk.table_id = tbl.id) constraints
  JOIN mysql.columns col ON constraints.COLUMN_ID=col.id WHERE 
  CAN_ACCESS_COLUMN(sch.name, tbl.name, col.name)
  AND IS_VISIBLE_DD_OBJECT(tbl.hidden, col.hidden NOT IN ('Visible', 'User') OR constraints.HIDDEN, col.options)

INSERT INTO I_S_check_table(t) VALUES ("CREATE OR REPLACE DEFINER=`mysql.infoschema`@`localhost` VIEW information_schema.KEY_COLUMN_USAGE AS SELECT 
  cat.name AS CONSTRAINT_CATALOG,
  sch.name AS CONSTRAINT_SCHEMA,
  constraints.CONSTRAINT_NAME AS CONSTRAINT_NAME,
  cat.name AS TABLE_CATALOG,
  sch.name AS TABLE_SCHEMA,
  tbl.name AS TABLE_NAME,
  col.name COLLATE utf8mb3_tolower_ci AS COLUMN_NAME,
  constraints.ordinal_position AS ORDINAL_POSITION,
  constraints.POSITION_IN_UNIQUE_CONSTRAINT AS POSITION_IN_UNIQUE_CONSTRAINT,
  constraints.REFERENCED_TABLE_SCHEMA AS REFERENCED_TABLE_SCHEMA,
  constraints.REFERENCED_TABLE_NAME AS REFERENCED_TABLE_NAME,
  constraints.REFERENCED_COLUMN_NAME AS REFERENCED_COLUMN_NAME FROM 
  mysql.tables tbl
  JOIN mysql.schemata sch ON tbl.schema_id=sch.id
  JOIN mysql.catalogs cat ON cat.id=sch.catalog_id
  , LATERAL (SELECT    idx.name AS CONSTRAINT_NAME,    icu.ordinal_position AS ORDINAL_POSITION,    NULL AS POSITION_IN_UNIQUE_CONSTRAINT,    NULL AS REFERENCED_TABLE_SCHEMA,    NULL AS REFERENCED_TABLE_NAME,    NULL AS REFERENCED_COLUMN_NAME,    icu.column_id,    idx.hidden OR icu.hidden AS HIDDEN    FROM mysql.indexes idx    JOIN mysql.index_column_usage icu ON icu.index_id = idx.id    WHERE idx.table_id = tbl.id      AND idx.type IN ('PRIMARY', 'UNIQUE')  UNION ALL SELECT    fk.name COLLATE utf8mb3_tolower_ci AS CONSTRAINT_NAME,    fkcu.ordinal_position AS ORDINAL_POSITION,    fkcu.ordinal_position AS POSITION_IN_UNIQUE_CONSTRAINT,    fk.referenced_table_schema AS REFERENCED_TABLE_SCHEMA,    fk.referenced_table_name AS REFERENCED_TABLE_NAME,    fkcu.referenced_column_name AS REFERENCED_COLUMN_NAME,    fkcu.column_id,    FALSE AS HIDDEN    FROM mysql.foreign_keys fk    JOIN mysql.foreign_key_column_usage fkcu ON fkcu.foreign_key_id = fk.id    WHERE fk.table_id = tbl.id) constraints
  JOIN mysql.columns col ON constraints.COLUMN_ID=col.id WHERE 
  CAN_ACCESS_COLUMN(sch.name, tbl.name, col.name)
  AND IS_VISIBLE_DD_OBJECT(tbl.hidden, col.hidden NOT IN ('Visible', 'User') OR constraints.HIDDEN, col.options)
");
CREATE OR REPLACE DEFINER=`mysql.infoschema`@`localhost` VIEW information_schema.KEYWORDS AS SELECT 
  j.word AS WORD,
  j.reserved AS RESERVED FROM 
  JSON_TABLE(<elements masked>) AS j

INSERT INTO I_S_check_table(t) VALUES ("CREATE OR REPLACE DEFINER=`mysql.infoschema`@`localhost` VIEW information_schema.KEYWORDS AS SELECT 
  j.word AS WORD,
  j.reserved AS RESERVED FROM 
  JSON_TABLE(<elements masked>) AS j
");
CREATE OR REPLACE DEFINER=`mysql.infoschema`@`localhost` VIEW information_schema.PARAMETERS AS SELECT 
  cat.name AS SPECIFIC_CATALOG,
  sch.name AS SPECIFIC_SCHEMA,
  rtn.name AS SPECIFIC_NAME,
  IF (rtn.type = 'FUNCTION', prm.ordinal_position-1, prm.ordinal_position) AS ORDINAL_POSITION,
  IF (rtn.type = 'FUNCTION' AND prm.ordinal_position = 1, NULL, prm.mode) AS PARAMETER_MODE,
  IF (rtn.type = 'FUNCTION' AND prm.ordinal_position = 1, NULL, prm.name) AS PARAMETER_NAME,
  SUBSTRING_INDEX(SUBSTRING_INDEX(prm.data_type_utf8, '(', 1), ' ', 1) AS DATA_TYPE,
  INTERNAL_DD_CHAR_LENGTH(prm.data_type, prm.char_length, col.name, 0) AS CHARACTER_MAXIMUM_LENGTH,
  INTERNAL_DD_CHAR_LENGTH(prm.data_type, prm.char_length, col.name, 1) AS CHARACTER_OCTET_LENGTH,
  prm.numeric_precision AS NUMERIC_PRECISION,
  IF(ISNULL(prm.numeric_precision), NULL, IFNULL(prm.numeric_scale, 0)) AS NUMERIC_SCALE,
  prm.datetime_precision AS DATETIME_PRECISION,
  CASE prm.data_type  WHEN 'MYSQL_TYPE_STRING' THEN     (IF (cs.name='binary',NULL, cs.name))  WHEN 'MYSQL_TYPE_VAR_STRING' THEN     (IF (cs.name='binary',NULL, cs.name))  WHEN 'MYSQL_TYPE_VARCHAR' THEN     (IF (cs.name='binary',NULL, cs.name))  WHEN 'MYSQL_TYPE_TINY_BLOB' THEN     (IF (cs.name='binary',NULL, cs.name))  WHEN 'MYSQL_TYPE_MEDIUM_BLOB' THEN     (IF (cs.name='binary',NULL, cs.name))  WHEN 'MYSQL_TYPE_BLOB' THEN     (IF (cs.name='binary',NULL, cs.name))  WHEN 'MYSQL_TYPE_LONG_BLOB' THEN     (IF (cs.name='binary',NULL, cs.name))  WHEN 'MYSQL_TYPE_ENUM' THEN     (IF (cs.name='binary',NULL, cs.name))  WHEN 'MYSQL_TYPE_SET' THEN     (IF (cs.name='binary',NULL, cs.name))  ELSE NULL END AS CHARACTER_SET_NAME,
  CASE prm.data_type  WHEN 'MYSQL_TYPE_STRING' THEN     (IF (cs.name='binary',NULL, col.name))  WHEN 'MYSQL_TYPE_VAR_STRING' THEN     (IF (cs.name='binary',NULL, col.name))  WHEN 'MYSQL_TYPE_VARCHAR' THEN     (IF (cs.name='binary',NULL, col.name))  WHEN 'MYSQL_TYPE_TINY_BLOB' THEN     (IF (cs.name='binary',NULL, col.name))  WHEN 'MYSQL_TYPE_MEDIUM_BLOB' THEN     (IF (cs.name='binary',NULL, col.name))  WHEN 'MYSQL_TYPE_BLOB' THEN     (IF (cs.name='binary',NULL, col.name))  WHEN 'MYSQL_TYPE_LONG_BLOB' THEN     (IF (cs.name='binary',NULL, col.name))  WHEN 'MYSQL_TYPE_ENUM' THEN     (IF (cs.name='binary',NULL, col.name))  WHEN 'MYSQL_TYPE_SET' THEN     (IF (cs.name='binary',NULL, col.name))  ELSE NULL END AS COLLATION_NAME,
  prm.data_type_utf8 AS DTD_IDENTIFIER,
  rtn.type AS ROUTINE_TYPE FROM 
  mysql.parameters prm JOIN mysql.routines rtn ON prm.routine_id=rtn.id
  JOIN mysql.schemata sch ON rtn.schema_id=sch.id
  JOIN mysql.catalogs cat ON cat.id=sch.catalog_id
  JOIN mysql.collations col ON prm.collation_id=col.id
  JOIN mysql.character_sets cs ON col.character_set_id=cs.id WHERE 
  CAN_ACCESS_ROUTINE(sch.name, rtn.name, rtn.type, rtn.definer, FALSE)

INSERT INTO I_S_check_table(t) VALUES ("CREATE OR REPLACE DEFINER=`mysql.infoschema`@`localhost` VIEW information_schema.PARAMETERS AS SELECT 
  cat.name AS SPECIFIC_CATALOG,
  sch.name AS SPECIFIC_SCHEMA,
  rtn.name AS SPECIFIC_NAME,
  IF (rtn.type = 'FUNCTION', prm.ordinal_position-1, prm.ordinal_position) AS ORDINAL_POSITION,
  IF (rtn.type = 'FUNCTION' AND prm.ordinal_position = 1, NULL, prm.mode) AS PARAMETER_MODE,
  IF (rtn.type = 'FUNCTION' AND prm.ordinal_position = 1, NULL, prm.name) AS PARAMETER_NAME,
  SUBSTRING_INDEX(SUBSTRING_INDEX(prm.data_type_utf8, '(', 1), ' ', 1) AS DATA_TYPE,
  INTERNAL_DD_CHAR_LENGTH(prm.data_type, prm.char_length, col.name, 0) AS CHARACTER_MAXIMUM_LENGTH,
  INTERNAL_DD_CHAR_LENGTH(prm.data_type, prm.char_length, col.name, 1) AS CHARACTER_OCTET_LENGTH,
  prm.numeric_precision AS NUMERIC_PRECISION,
  IF(ISNULL(prm.numeric_precision), NULL, IFNULL(prm.numeric_scale, 0)) AS NUMERIC_SCALE,
  prm.datetime_precision AS DATETIME_PRECISION,
  CASE prm.data_type  WHEN 'MYSQL_TYPE_STRING' THEN     (IF (cs.name='binary',NULL, cs.name))  WHEN 'MYSQL_TYPE_VAR_STRING' THEN     (IF (cs.name='binary',NULL, cs.name))  WHEN 'MYSQL_TYPE_VARCHAR' THEN     (IF (cs.name='binary',NULL, cs.name))  WHEN 'MYSQL_TYPE_TINY_BLOB' THEN     (IF (cs.name='binary',NULL, cs.name))  WHEN 'MYSQL_TYPE_MEDIUM_BLOB' THEN     (IF (cs.name='binary',NULL, cs.name))  WHEN 'MYSQL_TYPE_BLOB' THEN     (IF (cs.name='binary',NULL, cs.name))  WHEN 'MYSQL_TYPE_LONG_BLOB' THEN     (IF (cs.name='binary',NULL, cs.name))  WHEN 'MYSQL_TYPE_ENUM' THEN     (IF (cs.name='binary',NULL, cs.name))  WHEN 'MYSQL_TYPE_SET' THEN     (IF (cs.name='binary',NULL, cs.name))  ELSE NULL END AS CHARACTER_SET_NAME,
  CASE prm.data_type  WHEN 'MYSQL_TYPE_STRING' THEN     (IF (cs.name='binary',NULL, col.name))  WHEN 'MYSQL_TYPE_VAR_STRING' THEN     (IF (cs.name='binary',NULL, col.name))  WHEN 'MYSQL_TYPE_VARCHAR' THEN     (IF (cs.name='binary',NULL, col.name))  WHEN 'MYSQL_TYPE_TINY_BLOB' THEN     (IF (cs.name='binary',NULL, col.name))  WHEN 'MYSQL_TYPE_MEDIUM_BLOB' THEN     (IF (cs.name='binary',NULL, col.name))  WHEN 'MYSQL_TYPE_BLOB' THEN     (IF (cs.name='binary',NULL, col.name))  WHEN 'MYSQL_TYPE_LONG_BLOB' THEN     (IF (cs.name='binary',NULL, col.name))  WHEN 'MYSQL_TYPE_ENUM' THEN     (IF (cs.name='binary',NULL, col.name))  WHEN 'MYSQL_TYPE_SET' THEN     (IF (cs.name='binary',NULL, col.name))  ELSE NULL END AS COLLATION_NAME,
  prm.data_type_utf8 AS DTD_IDENTIFIER,
  rtn.type AS ROUTINE_TYPE FROM 
  mysql.parameters prm JOIN mysql.routines rtn ON prm.routine_id=rtn.id
  JOIN mysql.schemata sch ON rtn.schema_id=sch.id
  JOIN mysql.catalogs cat ON cat.id=sch.catalog_id
  JOIN mysql.collations col ON prm.collation_id=col.id
  JOIN mysql.character_sets cs ON col.character_set_id=cs.id WHERE 
  CAN_ACCESS_ROUTINE(sch.name, rtn.name, rtn.type, rtn.definer, FALSE)
");
CREATE OR REPLACE DEFINER=`mysql.infoschema`@`localhost` VIEW information_schema.PARTITIONS AS SELECT 
  cat.name AS TABLE_CATALOG,
  sch.name AS TABLE_SCHEMA,
  tbl.name AS TABLE_NAME,
  part.name AS PARTITION_NAME,
  sub_part.name AS SUBPARTITION_NAME,
  part.number+1 AS PARTITION_ORDINAL_POSITION,
  sub_part.number+1 AS SUBPARTITION_ORDINAL_POSITION,
  CASE tbl.partition_type  WHEN 'HASH' THEN 'HASH'   WHEN 'RANGE' THEN 'RANGE'   WHEN 'LIST' THEN 'LIST'   WHEN 'AUTO' THEN 'AUTO'   WHEN 'KEY_51' THEN 'KEY'   WHEN 'KEY_55' THEN 'KEY'   WHEN 'LINEAR_KEY_51' THEN 'LINEAR KEY'   WHEN 'LINEAR_KEY_55' THEN 'LINEAR KEY'   WHEN 'LINEAR_HASH' THEN 'LINEAR HASH'   WHEN 'RANGE_COLUMNS' THEN 'RANGE COLUMNS'   WHEN 'LIST_COLUMNS' THEN 'LIST COLUMNS'   ELSE NULL END AS PARTITION_METHOD,
  CASE tbl.subpartition_type  WHEN 'HASH' THEN 'HASH'   WHEN 'RANGE' THEN 'RANGE'   WHEN 'LIST' THEN 'LIST'   WHEN 'AUTO' THEN 'AUTO'   WHEN 'KEY_51' THEN 'KEY'   WHEN 'KEY_55' THEN 'KEY'   WHEN 'LINEAR_KEY_51' THEN 'LINEAR KEY'   WHEN 'LINEAR_KEY_55' THEN 'LINEAR KEY'   WHEN 'LINEAR_HASH' THEN 'LINEAR HASH'   WHEN 'RANGE_COLUMNS' THEN 'RANGE COLUMNS'   WHEN 'LIST_COLUMNS' THEN 'LIST COLUMNS'   ELSE NULL END AS SUBPARTITION_METHOD,
  tbl.partition_expression_utf8 AS PARTITION_EXPRESSION,
  tbl.subpartition_expression_utf8 AS SUBPARTITION_EXPRESSION,
  part.description_utf8 AS PARTITION_DESCRIPTION,
  INTERNAL_TABLE_ROWS(sch.name, tbl.name,  IF(ISNULL(tbl.partition_type), tbl.engine, ''),  tbl.se_private_id, tbl.hidden != 'Visible',   IF(sub_part.name IS NULL,      IF(part.name IS NULL, tbl.se_private_data, part_ts.se_private_data),     sub_part_ts.se_private_data), 0, 0,  IFNULL(sub_part.name, part.name)) AS TABLE_ROWS,
  INTERNAL_AVG_ROW_LENGTH(sch.name, tbl.name,  IF(ISNULL(tbl.partition_type), tbl.engine, ''),  tbl.se_private_id, tbl.hidden != 'Visible',   IF(sub_part.name IS NULL,      IF(part.name IS NULL, tbl.se_private_data, part_ts.se_private_data),     sub_part_ts.se_private_data), 0, 0,  IFNULL(sub_part.name, part.name)) AS AVG_ROW_LENGTH,
  INTERNAL_DATA_LENGTH(sch.name, tbl.name,  IF(ISNULL(tbl.partition_type), tbl.engine, ''),  tbl.se_private_id, tbl.hidden != 'Visible',   IF(sub_part.name IS NULL,      IF(part.name IS NULL, tbl.se_private_data, part_ts.se_private_data),     sub_part_ts.se_private_data), 0, 0,  IFNULL(sub_part.name, part.name)) AS DATA_LENGTH,
  INTERNAL_MAX_DATA_LENGTH(sch.name, tbl.name,  IF(ISNULL(tbl.partition_type), tbl.engine, ''),  tbl.se_private_id, tbl.hidden != 'Visible',   IF(sub_part.name IS NULL,      IF(part.name IS NULL, tbl.se_private_data, part_ts.se_private_data),     sub_part_ts.se_private_data), 0, 0,  IFNULL(sub_part.name, part.name)) AS MAX_DATA_LENGTH,
  INTERNAL_INDEX_LENGTH(sch.name, tbl.name,  IF(ISNULL(tbl.partition_type), tbl.engine, ''),  tbl.se_private_id, tbl.hidden != 'Visible',   IF(sub_part.name IS NULL,      IF(part.name IS NULL, tbl.se_private_data, part_ts.se_private_data),     sub_part_ts.se_private_data), 0, 0,  IFNULL(sub_part.name, part.name)) AS INDEX_LENGTH,
  INTERNAL_DATA_FREE(sch.name, tbl.name,  IF(ISNULL(tbl.partition_type), tbl.engine, ''),  tbl.se_private_id, tbl.hidden != 'Visible',   IF(sub_part.name IS NULL,      IF(part.name IS NULL, tbl.se_private_data, part_ts.se_private_data),     sub_part_ts.se_private_data), 0, 0,  IFNULL(sub_part.name, part.name)) AS DATA_FREE,
  tbl.created AS CREATE_TIME,
  INTERNAL_UPDATE_TIME(sch.name, tbl.name,  IF(ISNULL(tbl.partition_type), tbl.engine, ''),  tbl.se_private_id, tbl.hidden != 'Visible',   IF(sub_part.name IS NULL,      IF(part.name IS NULL, tbl.se_private_data, part_ts.se_private_data),     sub_part_ts.se_private_data), 0, 0,  IFNULL(sub_part.name, part.name)) AS UPDATE_TIME,
  INTERNAL_CHECK_TIME(sch.name, tbl.name,  IF(ISNULL(tbl.partition_type), tbl.engine, ''),  tbl.se_private_id, tbl.hidden != 'Visible',   IF(sub_part.name IS NULL,      IF(part.name IS NULL, tbl.se_private_data, part_ts.se_private_data),     sub_part_ts.se_private_data), 0, 0,  IFNULL(sub_part.name, part.name)) AS CHECK_TIME,
  INTERNAL_CHECKSUM(sch.name, tbl.name,  IF(ISNULL(tbl.partition_type), tbl.engine, ''),  tbl.se_private_id, tbl.hidden != 'Visible',   IF(sub_part.name IS NULL,      IF(part.name IS NULL, tbl.se_private_data, part_ts.se_private_data),     sub_part_ts.se_private_data), 0, 0,  IFNULL(sub_part.name, part.name)) AS CHECKSUM,
  IF(sub_part.name IS NULL,IFNULL(part.comment,''), IFNULL(sub_part.comment,'')) AS PARTITION_COMMENT,
  IF(part.name IS NULL, '', INTERNAL_GET_PARTITION_NODEGROUP( IF(sub_part.name IS NULL, part.options, sub_part.options))) AS NODEGROUP,
  IFNULL(sub_part_ts.name, part_ts.name) AS TABLESPACE_NAME FROM 
  mysql.tables tbl
  JOIN mysql.schemata sch ON sch.id=tbl.schema_id
  JOIN mysql.catalogs cat ON cat.id=sch.catalog_id
  LEFT JOIN mysql.table_partitions part ON part.table_id=tbl.id
  LEFT JOIN mysql.table_partitions sub_part ON sub_part.parent_partition_id=part.id
  LEFT JOIN mysql.tablespaces part_ts ON part_ts.id=part.tablespace_id
  LEFT JOIN mysql.tablespaces sub_part_ts ON sub_part.tablespace_id IS NOT NULL AND sub_part_ts.id=sub_part.tablespace_id WHERE 
  CAN_ACCESS_TABLE(sch.name, tbl.name)
  AND IS_VISIBLE_DD_OBJECT(tbl.hidden)
  AND part.parent_partition_id IS NULL

INSERT INTO I_S_check_table(t) VALUES ("CREATE OR REPLACE DEFINER=`mysql.infoschema`@`localhost` VIEW information_schema.PARTITIONS AS SELECT 
  cat.name AS TABLE_CATALOG,
  sch.name AS TABLE_SCHEMA,
  tbl.name AS TABLE_NAME,
  part.name AS PARTITION_NAME,
  sub_part.name AS SUBPARTITION_NAME,
  part.number+1 AS PARTITION_ORDINAL_POSITION,
  sub_part.number+1 AS SUBPARTITION_ORDINAL_POSITION,
  CASE tbl.partition_type  WHEN 'HASH' THEN 'HASH'   WHEN 'RANGE' THEN 'RANGE'   WHEN 'LIST' THEN 'LIST'   WHEN 'AUTO' THEN 'AUTO'   WHEN 'KEY_51' THEN 'KEY'   WHEN 'KEY_55' THEN 'KEY'   WHEN 'LINEAR_KEY_51' THEN 'LINEAR KEY'   WHEN 'LINEAR_KEY_55' THEN 'LINEAR KEY'   WHEN 'LINEAR_HASH' THEN 'LINEAR HASH'   WHEN 'RANGE_COLUMNS' THEN 'RANGE COLUMNS'   WHEN 'LIST_COLUMNS' THEN 'LIST COLUMNS'   ELSE NULL END AS PARTITION_METHOD,
  CASE tbl.subpartition_type  WHEN 'HASH' THEN 'HASH'   WHEN 'RANGE' THEN 'RANGE'   WHEN 'LIST' THEN 'LIST'   WHEN 'AUTO' THEN 'AUTO'   WHEN 'KEY_51' THEN 'KEY'   WHEN 'KEY_55' THEN 'KEY'   WHEN 'LINEAR_KEY_51' THEN 'LINEAR KEY'   WHEN 'LINEAR_KEY_55' THEN 'LINEAR KEY'   WHEN 'LINEAR_HASH' THEN 'LINEAR HASH'   WHEN 'RANGE_COLUMNS' THEN 'RANGE COLUMNS'   WHEN 'LIST_COLUMNS' THEN 'LIST COLUMNS'   ELSE NULL END AS SUBPARTITION_METHOD,
  tbl.partition_expression_utf8 AS PARTITION_EXPRESSION,
  tbl.subpartition_expression_utf8 AS SUBPARTITION_EXPRESSION,
  part.description_utf8 AS PARTITION_DESCRIPTION,
  INTERNAL_TABLE_ROWS(sch.name, tbl.name,  IF(ISNULL(tbl.partition_type), tbl.engine, ''),  tbl.se_private_id, tbl.hidden != 'Visible',   IF(sub_part.name IS NULL,      IF(part.name IS NULL, tbl.se_private_data, part_ts.se_private_data),     sub_part_ts.se_private_data), 0, 0,  IFNULL(sub_part.name, part.name)) AS TABLE_ROWS,
  INTERNAL_AVG_ROW_LENGTH(sch.name, tbl.name,  IF(ISNULL(tbl.partition_type), tbl.engine, ''),  tbl.se_private_id, tbl.hidden != 'Visible',   IF(sub_part.name IS NULL,      IF(part.name IS NULL, tbl.se_private_data, part_ts.se_private_data),     sub_part_ts.se_private_data), 0, 0,  IFNULL(sub_part.name, part.name)) AS AVG_ROW_LENGTH,
  INTERNAL_DATA_LENGTH(sch.name, tbl.name,  IF(ISNULL(tbl.partition_type), tbl.engine, ''),  tbl.se_private_id, tbl.hidden != 'Visible',   IF(sub_part.name IS NULL,      IF(part.name IS NULL, tbl.se_private_data, part_ts.se_private_data),     sub_part_ts.se_private_data), 0, 0,  IFNULL(sub_part.name, part.name)) AS DATA_LENGTH,
  INTERNAL_MAX_DATA_LENGTH(sch.name, tbl.name,  IF(ISNULL(tbl.partition_type), tbl.engine, ''),  tbl.se_private_id, tbl.hidden != 'Visible',   IF(sub_part.name IS NULL,      IF(part.name IS NULL, tbl.se_private_data, part_ts.se_private_data),     sub_part_ts.se_private_data), 0, 0,  IFNULL(sub_part.name, part.name)) AS MAX_DATA_LENGTH,
  INTERNAL_INDEX_LENGTH(sch.name, tbl.name,  IF(ISNULL(tbl.partition_type), tbl.engine, ''),  tbl.se_private_id, tbl.hidden != 'Visible',   IF(sub_part.name IS NULL,      IF(part.name IS NULL, tbl.se_private_data, part_ts.se_private_data),     sub_part_ts.se_private_data), 0, 0,  IFNULL(sub_part.name, part.name)) AS INDEX_LENGTH,
  INTERNAL_DATA_FREE(sch.name, tbl.name,  IF(ISNULL(tbl.partition_type), tbl.engine, ''),  tbl.se_private_id, tbl.hidden != 'Visible',   IF(sub_part.name IS NULL,      IF(part.name IS NULL, tbl.se_private_data, part_ts.se_private_data),     sub_part_ts.se_private_data), 0, 0,  IFNULL(sub_part.name, part.name)) AS DATA_FREE,
  tbl.created AS CREATE_TIME,
  INTERNAL_UPDATE_TIME(sch.name, tbl.name,  IF(ISNULL(tbl.partition_type), tbl.engine, ''),  tbl.se_private_id, tbl.hidden != 'Visible',   IF(sub_part.name IS NULL,      IF(part.name IS NULL, tbl.se_private_data, part_ts.se_private_data),     sub_part_ts.se_private_data), 0, 0,  IFNULL(sub_part.name, part.name)) AS UPDATE_TIME,
  INTERNAL_CHECK_TIME(sch.name, tbl.name,  IF(ISNULL(tbl.partition_type), tbl.engine, ''),  tbl.se_private_id, tbl.hidden != 'Visible',   IF(sub_part.name IS NULL,      IF(part.name IS NULL, tbl.se_private_data, part_ts.se_private_data),     sub_part_ts.se_private_data), 0, 0,  IFNULL(sub_part.name, part.name)) AS CHECK_TIME,
  INTERNAL_CHECKSUM(sch.name, tbl.name,  IF(ISNULL(tbl.partition_type), tbl.engine, ''),  tbl.se_private_id, tbl.hidden != 'Visible',   IF(sub_part.name IS NULL,      IF(part.name IS NULL, tbl.se_private_data, part_ts.se_private_data),     sub_part_ts.se_private_data), 0, 0,  IFNULL(sub_part.name, part.name)) AS CHECKSUM,
  IF(sub_part.name IS NULL,IFNULL(part.comment,''), IFNULL(sub_part.comment,'')) AS PARTITION_COMMENT,
  IF(part.name IS NULL, '', INTERNAL_GET_PARTITION_NODEGROUP( IF(sub_part.name IS NULL, part.options, sub_part.options))) AS NODEGROUP,
  IFNULL(sub_part_ts.name, part_ts.name) AS TABLESPACE_NAME FROM 
  mysql.tables tbl
  JOIN mysql.schemata sch ON sch.id=tbl.schema_id
  JOIN mysql.catalogs cat ON cat.id=sch.catalog_id
  LEFT JOIN mysql.table_partitions part ON part.table_id=tbl.id
  LEFT JOIN mysql.table_partitions sub_part ON sub_part.parent_partition_id=part.id
  LEFT JOIN mysql.tablespaces part_ts ON part_ts.id=part.tablespace_id
  LEFT JOIN mysql.tablespaces sub_part_ts ON sub_part.tablespace_id IS NOT NULL AND sub_part_ts.id=sub_part.tablespace_id WHERE 
  CAN_ACCESS_TABLE(sch.name, tbl.name)
  AND IS_VISIBLE_DD_OBJECT(tbl.hidden)
  AND part.parent_partition_id IS NULL
");
CREATE OR REPLACE DEFINER=`mysql.infoschema`@`localhost` VIEW information_schema.REFERENTIAL_CONSTRAINTS AS SELECT 
  cat.name AS CONSTRAINT_CATALOG,
  sch.name AS CONSTRAINT_SCHEMA,
  fk.name COLLATE utf8mb3_tolower_ci AS CONSTRAINT_NAME,
  fk.referenced_table_catalog AS UNIQUE_CONSTRAINT_CATALOG,
  fk.referenced_table_schema AS UNIQUE_CONSTRAINT_SCHEMA,
  fk.unique_constraint_name AS UNIQUE_CONSTRAINT_NAME,
  fk.match_option AS MATCH_OPTION,
  fk.update_rule AS UPDATE_RULE,
  fk.delete_rule AS DELETE_RULE,
  tbl.name AS TABLE_NAME,
  fk.referenced_table_name AS REFERENCED_TABLE_NAME FROM 
  mysql.foreign_keys fk
  JOIN mysql.tables tbl ON fk.table_id = tbl.id
  JOIN mysql.schemata sch ON fk.schema_id= sch.id
  JOIN mysql.catalogs cat ON cat.id=sch.catalog_id WHERE 
  CAN_ACCESS_TABLE(sch.name, tbl.name)
  AND IS_VISIBLE_DD_OBJECT(tbl.hidden)

INSERT INTO I_S_check_table(t) VALUES ("CREATE OR REPLACE DEFINER=`mysql.infoschema`@`localhost` VIEW information_schema.REFERENTIAL_CONSTRAINTS AS SELECT 
  cat.name AS CONSTRAINT_CATALOG,
  sch.name AS CONSTRAINT_SCHEMA,
  fk.name COLLATE utf8mb3_tolower_ci AS CONSTRAINT_NAME,
  fk.referenced_table_catalog AS UNIQUE_CONSTRAINT_CATALOG,
  fk.referenced_table_schema AS UNIQUE_CONSTRAINT_SCHEMA,
  fk.unique_constraint_name AS UNIQUE_CONSTRAINT_NAME,
  fk.match_option AS MATCH_OPTION,
  fk.update_rule AS UPDATE_RULE,
  fk.delete_rule AS DELETE_RULE,
  tbl.name AS TABLE_NAME,
  fk.referenced_table_name AS REFERENCED_TABLE_NAME FROM 
  mysql.foreign_keys fk
  JOIN mysql.tables tbl ON fk.table_id = tbl.id
  JOIN mysql.schemata sch ON fk.schema_id= sch.id
  JOIN mysql.catalogs cat ON cat.id=sch.catalog_id WHERE 
  CAN_ACCESS_TABLE(sch.name, tbl.name)
  AND IS_VISIBLE_DD_OBJECT(tbl.hidden)
");
CREATE OR REPLACE DEFINER=`mysql.infoschema`@`localhost` VIEW information_schema.RESOURCE_GROUPS AS SELECT 
  res.resource_group_name AS RESOURCE_GROUP_NAME,
  res.resource_group_type AS RESOURCE_GROUP_TYPE,
  res.resource_group_enabled AS RESOURCE_GROUP_ENABLED,
  CONVERT_CPU_ID_MASK(res.CPU_ID_MASK) AS VCPU_IDS,
  res.THREAD_PRIORITY AS THREAD_PRIORITY FROM 
  mysql.resource_groups res WHERE 
  CAN_ACCESS_RESOURCE_GROUP(res.resource_group_name)

INSERT INTO I_S_check_table(t) VALUES ("CREATE OR REPLACE DEFINER=`mysql.infoschema`@`localhost` VIEW information_schema.RESOURCE_GROUPS AS SELECT 
  res.resource_group_name AS RESOURCE_GROUP_NAME,
  res.resource_group_type AS RESOURCE_GROUP_TYPE,
  res.resource_group_enabled AS RESOURCE_GROUP_ENABLED,
  CONVERT_CPU_ID_MASK(res.CPU_ID_MASK) AS VCPU_IDS,
  res.THREAD_PRIORITY AS THREAD_PRIORITY FROM 
  mysql.resource_groups res WHERE 
  CAN_ACCESS_RESOURCE_GROUP(res.resource_group_name)
");
CREATE OR REPLACE DEFINER=`mysql.infoschema`@`localhost` VIEW information_schema.ROUTINES AS SELECT 
  rtn.name AS SPECIFIC_NAME,
  cat.name AS ROUTINE_CATALOG,
  sch.name AS ROUTINE_SCHEMA,
  rtn.name AS ROUTINE_NAME,
  rtn.type AS ROUTINE_TYPE,
  IF(rtn.type = 'PROCEDURE', '',    SUBSTRING_INDEX(SUBSTRING_INDEX(     rtn.result_data_type_utf8, '(', 1), ' ', 1)) AS DATA_TYPE,
  INTERNAL_DD_CHAR_LENGTH(rtn.result_data_type,  rtn.result_char_length, coll_result.name, 0) AS CHARACTER_MAXIMUM_LENGTH,
  INTERNAL_DD_CHAR_LENGTH(rtn.result_data_type,  rtn.result_char_length, coll_result.name, 1) AS CHARACTER_OCTET_LENGTH,
  rtn.result_numeric_precision AS NUMERIC_PRECISION,
  rtn.result_numeric_scale AS NUMERIC_SCALE,
  rtn.result_datetime_precision AS DATETIME_PRECISION,
  CASE rtn.result_data_type  WHEN 'MYSQL_TYPE_STRING' THEN     (IF (cs_result.name='binary',NULL, cs_result.name))  WHEN 'MYSQL_TYPE_VAR_STRING' THEN     (IF (cs_result.name='binary',NULL, cs_result.name))  WHEN 'MYSQL_TYPE_VARCHAR' THEN     (IF (cs_result.name='binary',NULL, cs_result.name))  WHEN 'MYSQL_TYPE_TINY_BLOB' THEN     (IF (cs_result.name='binary',NULL, cs_result.name))  WHEN 'MYSQL_TYPE_MEDIUM_BLOB' THEN     (IF (cs_result.name='binary',NULL, cs_result.name))  WHEN 'MYSQL_TYPE_BLOB' THEN     (IF (cs_result.name='binary',NULL, cs_result.name))  WHEN 'MYSQL_TYPE_LONG_BLOB' THEN     (IF (cs_result.name='binary',NULL, cs_result.name))  WHEN 'MYSQL_TYPE_ENUM' THEN     (IF (cs_result.name='binary',NULL, cs_result.name))  WHEN 'MYSQL_TYPE_SET' THEN     (IF (cs_result.name='binary',NULL, cs_result.name))  ELSE NULL END AS CHARACTER_SET_NAME,
  CASE rtn.result_data_type  WHEN 'MYSQL_TYPE_STRING' THEN     (IF (cs_result.name='binary',NULL, coll_result.name))  WHEN 'MYSQL_TYPE_VAR_STRING' THEN     (IF (cs_result.name='binary',NULL, coll_result.name))  WHEN 'MYSQL_TYPE_VARCHAR' THEN     (IF (cs_result.name='binary',NULL, coll_result.name))  WHEN 'MYSQL_TYPE_TINY_BLOB' THEN     (IF (cs_result.name='binary',NULL, coll_result.name))  WHEN 'MYSQL_TYPE_MEDIUM_BLOB' THEN     (IF (cs_result.name='binary',NULL, coll_result.name))  WHEN 'MYSQL_TYPE_BLOB' THEN     (IF (cs_result.name='binary',NULL, coll_result.name))  WHEN 'MYSQL_TYPE_LONG_BLOB' THEN     (IF (cs_result.name='binary',NULL, coll_result.name))  WHEN 'MYSQL_TYPE_ENUM' THEN     (IF (cs_result.name='binary',NULL, coll_result.name))  WHEN 'MYSQL_TYPE_SET' THEN     (IF (cs_result.name='binary',NULL, coll_result.name))  ELSE NULL END AS COLLATION_NAME,
  IF(rtn.type = 'PROCEDURE', NULL, rtn.result_data_type_utf8) AS DTD_IDENTIFIER,
  'SQL' AS ROUTINE_BODY,
  IF (CAN_ACCESS_ROUTINE(sch.name, rtn.name, rtn.type, rtn.definer, TRUE),    rtn.definition_utf8, NULL) AS ROUTINE_DEFINITION,
  NULL AS EXTERNAL_NAME,
  rtn.external_language AS EXTERNAL_LANGUAGE,
  'SQL' AS PARAMETER_STYLE,
  IF(rtn.is_deterministic=0, 'NO', 'YES') AS IS_DETERMINISTIC,
  rtn.sql_data_access AS SQL_DATA_ACCESS,
  NULL AS SQL_PATH,
  rtn.security_type AS SECURITY_TYPE,
  rtn.created AS CREATED,
  rtn.last_altered AS LAST_ALTERED,
  rtn.sql_mode AS SQL_MODE,
  rtn.comment AS ROUTINE_COMMENT,
  rtn.definer AS DEFINER,
  cs_client.name AS CHARACTER_SET_CLIENT,
  coll_conn.name AS COLLATION_CONNECTION,
  coll_db.name AS DATABASE_COLLATION FROM 
  mysql.routines rtn
  JOIN mysql.schemata sch ON rtn.schema_id=sch.id
  JOIN mysql.catalogs cat ON cat.id=sch.catalog_id
  JOIN mysql.collations coll_client ON coll_client.id=rtn.client_collation_id
  JOIN mysql.character_sets cs_client ON cs_client.id=coll_client.character_set_id
  JOIN mysql.collations coll_conn ON coll_conn.id=rtn.connection_collation_id
  JOIN mysql.collations coll_db ON coll_db.id=rtn.schema_collation_id
  LEFT JOIN mysql.collations coll_result ON coll_result.id=rtn.result_collation_id
  LEFT JOIN mysql.character_sets cs_result ON cs_result.id=coll_result.character_set_id WHERE 
  CAN_ACCESS_ROUTINE(sch.name, rtn.name, rtn.type, rtn.definer, FALSE)

INSERT INTO I_S_check_table(t) VALUES ("CREATE OR REPLACE DEFINER=`mysql.infoschema`@`localhost` VIEW information_schema.ROUTINES AS SELECT 
  rtn.name AS SPECIFIC_NAME,
  cat.name AS ROUTINE_CATALOG,
  sch.name AS ROUTINE_SCHEMA,
  rtn.name AS ROUTINE_NAME,
  rtn.type AS ROUTINE_TYPE,
  IF(rtn.type = 'PROCEDURE', '',    SUBSTRING_INDEX(SUBSTRING_INDEX(     rtn.result_data_type_utf8, '(', 1), ' ', 1)) AS DATA_TYPE,
  INTERNAL_DD_CHAR_LENGTH(rtn.result_data_type,  rtn.result_char_length, coll_result.name, 0) AS CHARACTER_MAXIMUM_LENGTH,
  INTERNAL_DD_CHAR_LENGTH(rtn.result_data_type,  rtn.result_char_length, coll_result.name, 1) AS CHARACTER_OCTET_LENGTH,
  rtn.result_numeric_precision AS NUMERIC_PRECISION,
  rtn.result_numeric_scale AS NUMERIC_SCALE,
  rtn.result_datetime_precision AS DATETIME_PRECISION,
  CASE rtn.result_data_type  WHEN 'MYSQL_TYPE_STRING' THEN     (IF (cs_result.name='binary',NULL, cs_result.name))  WHEN 'MYSQL_TYPE_VAR_STRING' THEN     (IF (cs_result.name='binary',NULL, cs_result.name))  WHEN 'MYSQL_TYPE_VARCHAR' THEN     (IF (cs_result.name='binary',NULL, cs_result.name))  WHEN 'MYSQL_TYPE_TINY_BLOB' THEN     (IF (cs_result.name='binary',NULL, cs_result.name))  WHEN 'MYSQL_TYPE_MEDIUM_BLOB' THEN     (IF (cs_result.name='binary',NULL, cs_result.name))  WHEN 'MYSQL_TYPE_BLOB' THEN     (IF (cs_result.name='binary',NULL, cs_result.name))  WHEN 'MYSQL_TYPE_LONG_BLOB' THEN     (IF (cs_result.name='binary',NULL, cs_result.name))  WHEN 'MYSQL_TYPE_ENUM' THEN     (IF (cs_result.name='binary',NULL, cs_result.name))  WHEN 'MYSQL_TYPE_SET' THEN     (IF (cs_result.name='binary',NULL, cs_result.name))  ELSE NULL END AS CHARACTER_SET_NAME,
  CASE rtn.result_data_type  WHEN 'MYSQL_TYPE_STRING' THEN     (IF (cs_result.name='binary',NULL, coll_result.name))  WHEN 'MYSQL_TYPE_VAR_STRING' THEN     (IF (cs_result.name='binary',NULL, coll_result.name))  WHEN 'MYSQL_TYPE_VARCHAR' THEN     (IF (cs_result.name='binary',NULL, coll_result.name))  WHEN 'MYSQL_TYPE_TINY_BLOB' THEN     (IF (cs_result.name='binary',NULL, coll_result.name))  WHEN 'MYSQL_TYPE_MEDIUM_BLOB' THEN     (IF (cs_result.name='binary',NULL, coll_result.name))  WHEN 'MYSQL_TYPE_BLOB' THEN     (IF (cs_result.name='binary',NULL, coll_result.name))  WHEN 'MYSQL_TYPE_LONG_BLOB' THEN     (IF (cs_result.name='binary',NULL, coll_result.name))  WHEN 'MYSQL_TYPE_ENUM' THEN     (IF (cs_result.name='binary',NULL, coll_result.name))  WHEN 'MYSQL_TYPE_SET' THEN     (IF (cs_result.name='binary',NULL, coll_result.name))  ELSE NULL END AS COLLATION_NAME,
  IF(rtn.type = 'PROCEDURE', NULL, rtn.result_data_type_utf8) AS DTD_IDENTIFIER,
  'SQL' AS ROUTINE_BODY,
  IF (CAN_ACCESS_ROUTINE(sch.name, rtn.name, rtn.type, rtn.definer, TRUE),    rtn.definition_utf8, NULL) AS ROUTINE_DEFINITION,
  NULL AS EXTERNAL_NAME,
  rtn.external_language AS EXTERNAL_LANGUAGE,
  'SQL' AS PARAMETER_STYLE,
  IF(rtn.is_deterministic=0, 'NO', 'YES') AS IS_DETERMINISTIC,
  rtn.sql_data_access AS SQL_DATA_ACCESS,
  NULL AS SQL_PATH,
  rtn.security_type AS SECURITY_TYPE,
  rtn.created AS CREATED,
  rtn.last_altered AS LAST_ALTERED,
  rtn.sql_mode AS SQL_MODE,
  rtn.comment AS ROUTINE_COMMENT,
  rtn.definer AS DEFINER,
  cs_client.name AS CHARACTER_SET_CLIENT,
  coll_conn.name AS COLLATION_CONNECTION,
  coll_db.name AS DATABASE_COLLATION FROM 
  mysql.routines rtn
  JOIN mysql.schemata sch ON rtn.schema_id=sch.id
  JOIN mysql.catalogs cat ON cat.id=sch.catalog_id
  JOIN mysql.collations coll_client ON coll_client.id=rtn.client_collation_id
  JOIN mysql.character_sets cs_client ON cs_client.id=coll_client.character_set_id
  JOIN mysql.collations coll_conn ON coll_conn.id=rtn.connection_collation_id
  JOIN mysql.collations coll_db ON coll_db.id=rtn.schema_collation_id
  LEFT JOIN mysql.collations coll_result ON coll_result.id=rtn.result_collation_id
  LEFT JOIN mysql.character_sets cs_result ON cs_result.id=coll_result.character_set_id WHERE 
  CAN_ACCESS_ROUTINE(sch.name, rtn.name, rtn.type, rtn.definer, FALSE)
");
CREATE OR REPLACE DEFINER=`mysql.infoschema`@`localhost` VIEW information_schema.SHOW_STATISTICS AS SELECT 
  cat.name AS TABLE_CATALOG,
  sch.name AS TABLE_SCHEMA,
  tbl.name AS TABLE_NAME,
  IF (idx.type = 'PRIMARY' OR idx.type = 'UNIQUE',0,1) AS NON_UNIQUE,
  sch.name AS INDEX_SCHEMA,
  idx.name COLLATE utf8mb3_tolower_ci AS INDEX_NAME,
  icu.ordinal_position AS SEQ_IN_INDEX,
  IF (col.hidden = 'SQL', NULL, col.name COLLATE utf8mb3_tolower_ci) AS COLUMN_NAME,
  CASE WHEN icu.order = 'DESC' THEN 'D' WHEN icu.order = 'ASC'  THEN 'A' ELSE NULL END AS COLLATION,
  INTERNAL_INDEX_COLUMN_CARDINALITY(sch.name, tbl.name, idx.name,col.name, idx.ordinal_position,icu.ordinal_position,IF(ISNULL(tbl.partition_type), tbl.engine, ''),tbl.se_private_id,tbl.hidden != 'Visible' OR idx.hidden OR icu.hidden,COALESCE(stat.cardinality, CAST(-1 AS UNSIGNED)),COALESCE(CAST(stat.cached_time as UNSIGNED), 0)) AS CARDINALITY,
  GET_DD_INDEX_SUB_PART_LENGTH(icu.length,col.type, col.char_length, col.collation_id,idx.type) AS SUB_PART,
  NULL AS PACKED,
  IF (col.is_nullable = 1, 'YES','') AS NULLABLE,
  CASE WHEN idx.type = 'SPATIAL' THEN 'SPATIAL' WHEN idx.algorithm = 'SE_PRIVATE' THEN '' ELSE idx.algorithm END  AS INDEX_TYPE,
  IF (idx.type = 'PRIMARY' OR idx.type = 'UNIQUE',     '',IF(INTERNAL_KEYS_DISABLED(tbl.options),'disabled', '')) AS COMMENT,
  idx.comment AS INDEX_COMMENT,
  IF (idx.is_visible, 'YES', 'NO') AS IS_VISIBLE,
  idx.ordinal_position AS INDEX_ORDINAL_POSITION,
  icu.ordinal_position AS COLUMN_ORDINAL_POSITION,
  IF (col.hidden = 'SQL', col.generation_expression_utf8, NULL) AS EXPRESSION FROM 
  mysql.index_column_usage icu
  JOIN mysql.indexes idx ON idx.id=icu.index_id
  JOIN mysql.tables tbl ON idx.table_id=tbl.id
  JOIN mysql.columns col ON icu.column_id=col.id
  JOIN mysql.schemata sch ON tbl.schema_id=sch.id
  JOIN mysql.catalogs cat ON cat.id=sch.catalog_id
  JOIN mysql.collations coll ON tbl.collation_id=coll.id
  LEFT JOIN mysql.index_stats stat  ON tbl.name=stat.table_name AND sch.name=stat.schema_name AND idx.name=stat.index_name AND col.name=stat.column_name WHERE 
  CAN_ACCESS_TABLE(sch.name, tbl.name)
  AND IS_VISIBLE_DD_OBJECT(tbl.hidden, idx.hidden OR icu.hidden, idx.options)

INSERT INTO I_S_check_table(t) VALUES ("CREATE OR REPLACE DEFINER=`mysql.infoschema`@`localhost` VIEW information_schema.SHOW_STATISTICS AS SELECT 
  cat.name AS TABLE_CATALOG,
  sch.name AS TABLE_SCHEMA,
  tbl.name AS TABLE_NAME,
  IF (idx.type = 'PRIMARY' OR idx.type = 'UNIQUE',0,1) AS NON_UNIQUE,
  sch.name AS INDEX_SCHEMA,
  idx.name COLLATE utf8mb3_tolower_ci AS INDEX_NAME,
  icu.ordinal_position AS SEQ_IN_INDEX,
  IF (col.hidden = 'SQL', NULL, col.name COLLATE utf8mb3_tolower_ci) AS COLUMN_NAME,
  CASE WHEN icu.order = 'DESC' THEN 'D' WHEN icu.order = 'ASC'  THEN 'A' ELSE NULL END AS COLLATION,
  INTERNAL_INDEX_COLUMN_CARDINALITY(sch.name, tbl.name, idx.name,col.name, idx.ordinal_position,icu.ordinal_position,IF(ISNULL(tbl.partition_type), tbl.engine, ''),tbl.se_private_id,tbl.hidden != 'Visible' OR idx.hidden OR icu.hidden,COALESCE(stat.cardinality, CAST(-1 AS UNSIGNED)),COALESCE(CAST(stat.cached_time as UNSIGNED), 0)) AS CARDINALITY,
  GET_DD_INDEX_SUB_PART_LENGTH(icu.length,col.type, col.char_length, col.collation_id,idx.type) AS SUB_PART,
  NULL AS PACKED,
  IF (col.is_nullable = 1, 'YES','') AS NULLABLE,
  CASE WHEN idx.type = 'SPATIAL' THEN 'SPATIAL' WHEN idx.algorithm = 'SE_PRIVATE' THEN '' ELSE idx.algorithm END  AS INDEX_TYPE,
  IF (idx.type = 'PRIMARY' OR idx.type = 'UNIQUE',     '',IF(INTERNAL_KEYS_DISABLED(tbl.options),'disabled', '')) AS COMMENT,
  idx.comment AS INDEX_COMMENT,
  IF (idx.is_visible, 'YES', 'NO') AS IS_VISIBLE,
  idx.ordinal_position AS INDEX_ORDINAL_POSITION,
  icu.ordinal_position AS COLUMN_ORDINAL_POSITION,
  IF (col.hidden = 'SQL', col.generation_expression_utf8, NULL) AS EXPRESSION FROM 
  mysql.index_column_usage icu
  JOIN mysql.indexes idx ON idx.id=icu.index_id
  JOIN mysql.tables tbl ON idx.table_id=tbl.id
  JOIN mysql.columns col ON icu.column_id=col.id
  JOIN mysql.schemata sch ON tbl.schema_id=sch.id
  JOIN mysql.catalogs cat ON cat.id=sch.catalog_id
  JOIN mysql.collations coll ON tbl.collation_id=coll.id
  LEFT JOIN mysql.index_stats stat  ON tbl.name=stat.table_name AND sch.name=stat.schema_name AND idx.name=stat.index_name AND col.name=stat.column_name WHERE 
  CAN_ACCESS_TABLE(sch.name, tbl.name)
  AND IS_VISIBLE_DD_OBJECT(tbl.hidden, idx.hidden OR icu.hidden, idx.options)
");
CREATE OR REPLACE DEFINER=`mysql.infoschema`@`localhost` VIEW information_schema.SCHEMATA AS SELECT 
  cat.name AS CATALOG_NAME,
  sch.name AS SCHEMA_NAME,
  cs.name AS DEFAULT_CHARACTER_SET_NAME,
  col.name AS DEFAULT_COLLATION_NAME,
  NULL AS SQL_PATH,
  sch.default_encryption AS DEFAULT_ENCRYPTION FROM 
  mysql.schemata sch
  JOIN mysql.catalogs cat ON cat.id=sch.catalog_id
  JOIN mysql.collations col ON sch.default_collation_id = col.id
  JOIN mysql.character_sets cs ON col.character_set_id= cs.id WHERE 
  CAN_ACCESS_DATABASE(sch.name)

INSERT INTO I_S_check_table(t) VALUES ("CREATE OR REPLACE DEFINER=`mysql.infoschema`@`localhost` VIEW information_schema.SCHEMATA AS SELECT 
  cat.name AS CATALOG_NAME,
  sch.name AS SCHEMA_NAME,
  cs.name AS DEFAULT_CHARACTER_SET_NAME,
  col.name AS DEFAULT_COLLATION_NAME,
  NULL AS SQL_PATH,
  sch.default_encryption AS DEFAULT_ENCRYPTION FROM 
  mysql.schemata sch
  JOIN mysql.catalogs cat ON cat.id=sch.catalog_id
  JOIN mysql.collations col ON sch.default_collation_id = col.id
  JOIN mysql.character_sets cs ON col.character_set_id= cs.id WHERE 
  CAN_ACCESS_DATABASE(sch.name)
");
CREATE OR REPLACE DEFINER=`mysql.infoschema`@`localhost` VIEW information_schema.SCHEMATA_EXTENSIONS AS SELECT 
  cat.name AS CATALOG_NAME,
  sch.name AS SCHEMA_NAME,
  GET_DD_SCHEMA_OPTIONS(sch.options) AS OPTIONS FROM 
  mysql.schemata sch
  JOIN mysql.catalogs cat ON cat.id=sch.catalog_id WHERE 
  CAN_ACCESS_DATABASE(sch.name)

INSERT INTO I_S_check_table(t) VALUES ("CREATE OR REPLACE DEFINER=`mysql.infoschema`@`localhost` VIEW information_schema.SCHEMATA_EXTENSIONS AS SELECT 
  cat.name AS CATALOG_NAME,
  sch.name AS SCHEMA_NAME,
  GET_DD_SCHEMA_OPTIONS(sch.options) AS OPTIONS FROM 
  mysql.schemata sch
  JOIN mysql.catalogs cat ON cat.id=sch.catalog_id WHERE 
  CAN_ACCESS_DATABASE(sch.name)
");
CREATE OR REPLACE DEFINER=`mysql.infoschema`@`localhost` VIEW information_schema.ST_SPATIAL_REFERENCE_SYSTEMS AS SELECT 
  name AS SRS_NAME,
  id AS SRS_ID,
  organization AS ORGANIZATION,
  organization_coordsys_id AS ORGANIZATION_COORDSYS_ID,
  definition AS DEFINITION,
  description AS DESCRIPTION FROM 
  mysql.st_spatial_reference_systems

INSERT INTO I_S_check_table(t) VALUES ("CREATE OR REPLACE DEFINER=`mysql.infoschema`@`localhost` VIEW information_schema.ST_SPATIAL_REFERENCE_SYSTEMS AS SELECT 
  name AS SRS_NAME,
  id AS SRS_ID,
  organization AS ORGANIZATION,
  organization_coordsys_id AS ORGANIZATION_COORDSYS_ID,
  definition AS DEFINITION,
  description AS DESCRIPTION FROM 
  mysql.st_spatial_reference_systems
");
CREATE OR REPLACE DEFINER=`mysql.infoschema`@`localhost` VIEW information_schema.ST_UNITS_OF_MEASURE AS SELECT 
  UNIT_NAME AS UNIT_NAME,
  UNIT_TYPE AS UNIT_TYPE,
  CONVERSION_FACTOR AS CONVERSION_FACTOR,
  DESCRIPTION AS DESCRIPTION FROM 
  JSON_TABLE(<elements masked>) AS ST_UNITS_OF_MEASURE

INSERT INTO I_S_check_table(t) VALUES ("CREATE OR REPLACE DEFINER=`mysql.infoschema`@`localhost` VIEW information_schema.ST_UNITS_OF_MEASURE AS SELECT 
  UNIT_NAME AS UNIT_NAME,
  UNIT_TYPE AS UNIT_TYPE,
  CONVERSION_FACTOR AS CONVERSION_FACTOR,
  DESCRIPTION AS DESCRIPTION FROM 
  JSON_TABLE(<elements masked>) AS ST_UNITS_OF_MEASURE
");
CREATE OR REPLACE DEFINER=`mysql.infoschema`@`localhost` VIEW information_schema.ST_GEOMETRY_COLUMNS AS SELECT 
  cols.TABLE_CATALOG AS TABLE_CATALOG,
  cols.TABLE_SCHEMA AS TABLE_SCHEMA,
  cols.TABLE_NAME AS TABLE_NAME,
  cols.COLUMN_NAME AS COLUMN_NAME,
  srs.SRS_NAME AS SRS_NAME,
  cols.SRS_ID AS SRS_ID,
  cols.DATA_TYPE AS GEOMETRY_TYPE_NAME FROM 
  INFORMATION_SCHEMA.COLUMNS cols
  LEFT JOIN   INFORMATION_SCHEMA.ST_SPATIAL_REFERENCE_SYSTEMS srs   ON (cols.SRS_ID = srs.SRS_ID) WHERE 
  DATA_TYPE IN ('geometry','point','linestring','polygon', 'multipoint',              'multilinestring', 'multipolygon','geomcollection')

INSERT INTO I_S_check_table(t) VALUES ("CREATE OR REPLACE DEFINER=`mysql.infoschema`@`localhost` VIEW information_schema.ST_GEOMETRY_COLUMNS AS SELECT 
  cols.TABLE_CATALOG AS TABLE_CATALOG,
  cols.TABLE_SCHEMA AS TABLE_SCHEMA,
  cols.TABLE_NAME AS TABLE_NAME,
  cols.COLUMN_NAME AS COLUMN_NAME,
  srs.SRS_NAME AS SRS_NAME,
  cols.SRS_ID AS SRS_ID,
  cols.DATA_TYPE AS GEOMETRY_TYPE_NAME FROM 
  INFORMATION_SCHEMA.COLUMNS cols
  LEFT JOIN   INFORMATION_SCHEMA.ST_SPATIAL_REFERENCE_SYSTEMS srs   ON (cols.SRS_ID = srs.SRS_ID) WHERE 
  DATA_TYPE IN ('geometry','point','linestring','polygon', 'multipoint',              'multilinestring', 'multipolygon','geomcollection')
");
CREATE OR REPLACE DEFINER=`mysql.infoschema`@`localhost` VIEW information_schema.STATISTICS AS SELECT 
  cat.name AS TABLE_CATALOG,
  sch.name AS TABLE_SCHEMA,
  tbl.name AS TABLE_NAME,
  IF (idx.type = 'PRIMARY' OR idx.type = 'UNIQUE',0,1) AS NON_UNIQUE,
  sch.name AS INDEX_SCHEMA,
  idx.name COLLATE utf8mb3_tolower_ci AS INDEX_NAME,
  icu.ordinal_position AS SEQ_IN_INDEX,
  IF (col.hidden = 'SQL', NULL, col.name COLLATE utf8mb3_tolower_ci) AS COLUMN_NAME,
  CASE WHEN icu.order = 'DESC' THEN 'D' WHEN icu.order = 'ASC'  THEN 'A' ELSE NULL END AS COLLATION,
  INTERNAL_INDEX_COLUMN_CARDINALITY(sch.name, tbl.name, idx.name,col.name, idx.ordinal_position,icu.ordinal_position,IF(ISNULL(tbl.partition_type), tbl.engine, ''),tbl.se_private_id,tbl.hidden != 'Visible' OR idx.hidden OR icu.hidden,COALESCE(stat.cardinality, CAST(-1 AS UNSIGNED)),COALESCE(CAST(stat.cached_time as UNSIGNED), 0)) AS CARDINALITY,
  GET_DD_INDEX_SUB_PART_LENGTH(icu.length,col.type, col.char_length, col.collation_id,idx.type) AS SUB_PART,
  NULL AS PACKED,
  IF (col.is_nullable = 1, 'YES','') AS NULLABLE,
  CASE WHEN idx.type = 'SPATIAL' THEN 'SPATIAL' WHEN idx.algorithm = 'SE_PRIVATE' THEN '' ELSE idx.algorithm END  AS INDEX_TYPE,
  IF (idx.type = 'PRIMARY' OR idx.type = 'UNIQUE',     '',IF(INTERNAL_KEYS_DISABLED(tbl.options),'disabled', '')) AS COMMENT,
  idx.comment AS INDEX_COMMENT,
  IF (idx.is_visible, 'YES', 'NO') AS IS_VISIBLE,
  IF (col.hidden = 'SQL', col.generation_expression_utf8, NULL) AS EXPRESSION FROM 
  mysql.index_column_usage icu
  JOIN mysql.indexes idx ON idx.id=icu.index_id
  JOIN mysql.tables tbl ON idx.table_id=tbl.id
  JOIN mysql.columns col ON icu.column_id=col.id
  JOIN mysql.schemata sch ON tbl.schema_id=sch.id
  JOIN mysql.catalogs cat ON cat.id=sch.catalog_id
  JOIN mysql.collations coll ON tbl.collation_id=coll.id
  LEFT JOIN mysql.index_stats stat  ON tbl.name=stat.table_name AND sch.name=stat.schema_name AND idx.name=stat.index_name AND col.name=stat.column_name WHERE 
  CAN_ACCESS_TABLE(sch.name, tbl.name)
  AND IS_VISIBLE_DD_OBJECT(tbl.hidden, idx.hidden OR icu.hidden, idx.options)

INSERT INTO I_S_check_table(t) VALUES ("CREATE OR REPLACE DEFINER=`mysql.infoschema`@`localhost` VIEW information_schema.STATISTICS AS SELECT 
  cat.name AS TABLE_CATALOG,
  sch.name AS TABLE_SCHEMA,
  tbl.name AS TABLE_NAME,
  IF (idx.type = 'PRIMARY' OR idx.type = 'UNIQUE',0,1) AS NON_UNIQUE,
  sch.name AS INDEX_SCHEMA,
  idx.name COLLATE utf8mb3_tolower_ci AS INDEX_NAME,
  icu.ordinal_position AS SEQ_IN_INDEX,
  IF (col.hidden = 'SQL', NULL, col.name COLLATE utf8mb3_tolower_ci) AS COLUMN_NAME,
  CASE WHEN icu.order = 'DESC' THEN 'D' WHEN icu.order = 'ASC'  THEN 'A' ELSE NULL END AS COLLATION,
  INTERNAL_INDEX_COLUMN_CARDINALITY(sch.name, tbl.name, idx.name,col.name, idx.ordinal_position,icu.ordinal_position,IF(ISNULL(tbl.partition_type), tbl.engine, ''),tbl.se_private_id,tbl.hidden != 'Visible' OR idx.hidden OR icu.hidden,COALESCE(stat.cardinality, CAST(-1 AS UNSIGNED)),COALESCE(CAST(stat.cached_time as UNSIGNED), 0)) AS CARDINALITY,
  GET_DD_INDEX_SUB_PART_LENGTH(icu.length,col.type, col.char_length, col.collation_id,idx.type) AS SUB_PART,
  NULL AS PACKED,
  IF (col.is_nullable = 1, 'YES','') AS NULLABLE,
  CASE WHEN idx.type = 'SPATIAL' THEN 'SPATIAL' WHEN idx.algorithm = 'SE_PRIVATE' THEN '' ELSE idx.algorithm END  AS INDEX_TYPE,
  IF (idx.type = 'PRIMARY' OR idx.type = 'UNIQUE',     '',IF(INTERNAL_KEYS_DISABLED(tbl.options),'disabled', '')) AS COMMENT,
  idx.comment AS INDEX_COMMENT,
  IF (idx.is_visible, 'YES', 'NO') AS IS_VISIBLE,
  IF (col.hidden = 'SQL', col.generation_expression_utf8, NULL) AS EXPRESSION FROM 
  mysql.index_column_usage icu
  JOIN mysql.indexes idx ON idx.id=icu.index_id
  JOIN mysql.tables tbl ON idx.table_id=tbl.id
  JOIN mysql.columns col ON icu.column_id=col.id
  JOIN mysql.schemata sch ON tbl.schema_id=sch.id
  JOIN mysql.catalogs cat ON cat.id=sch.catalog_id
  JOIN mysql.collations coll ON tbl.collation_id=coll.id
  LEFT JOIN mysql.index_stats stat  ON tbl.name=stat.table_name AND sch.name=stat.schema_name AND idx.name=stat.index_name AND col.name=stat.column_name WHERE 
  CAN_ACCESS_TABLE(sch.name, tbl.name)
  AND IS_VISIBLE_DD_OBJECT(tbl.hidden, idx.hidden OR icu.hidden, idx.options)
");
CREATE OR REPLACE DEFINER=`mysql.infoschema`@`localhost` VIEW information_schema.TABLE_CONSTRAINTS AS SELECT 
  cat.name AS CONSTRAINT_CATALOG,
  sch.name AS CONSTRAINT_SCHEMA,
  constraints.CONSTRAINT_NAME AS CONSTRAINT_NAME,
  sch.name AS TABLE_SCHEMA,
  tbl.name AS TABLE_NAME,
  constraints.CONSTRAINT_TYPE AS CONSTRAINT_TYPE,
  constraints.ENFORCED AS ENFORCED FROM 
  mysql.tables tbl
  JOIN mysql.schemata sch ON tbl.schema_id=sch.id
  JOIN mysql.catalogs cat ON cat.id = sch.catalog_id
  , LATERAL ( SELECT    idx.name AS CONSTRAINT_NAME,    IF (idx.type='PRIMARY', 'PRIMARY KEY', idx.type) as CONSTRAINT_TYPE,    'YES' as ENFORCED  FROM mysql.indexes idx    WHERE idx.table_id=tbl.id AND idx.type IN ('PRIMARY', 'UNIQUE')          AND IS_VISIBLE_DD_OBJECT(tbl.hidden, idx.hidden, idx.options) UNION ALL  SELECT    fk.name COLLATE utf8mb3_tolower_ci AS CONSTRAINT_NAME,    'FOREIGN KEY' as CONSTRAINT_TYPE,    'YES' as ENFORCED  FROM mysql.foreign_keys fk WHERE fk.table_id=tbl.id UNION ALL  SELECT    cc.name AS CONSTRAINT_NAME,    'CHECK' as CONSTRAINT_TYPE,    cc.enforced as ENFORCED  FROM mysql.check_constraints cc WHERE cc.table_id=tbl.id) constraints WHERE 
  CAN_ACCESS_TABLE(sch.name, tbl.name)
  AND IS_VISIBLE_DD_OBJECT(tbl.hidden)

INSERT INTO I_S_check_table(t) VALUES ("CREATE OR REPLACE DEFINER=`mysql.infoschema`@`localhost` VIEW information_schema.TABLE_CONSTRAINTS AS SELECT 
  cat.name AS CONSTRAINT_CATALOG,
  sch.name AS CONSTRAINT_SCHEMA,
  constraints.CONSTRAINT_NAME AS CONSTRAINT_NAME,
  sch.name AS TABLE_SCHEMA,
  tbl.name AS TABLE_NAME,
  constraints.CONSTRAINT_TYPE AS CONSTRAINT_TYPE,
  constraints.ENFORCED AS ENFORCED FROM 
  mysql.tables tbl
  JOIN mysql.schemata sch ON tbl.schema_id=sch.id
  JOIN mysql.catalogs cat ON cat.id = sch.catalog_id
  , LATERAL ( SELECT    idx.name AS CONSTRAINT_NAME,    IF (idx.type='PRIMARY', 'PRIMARY KEY', idx.type) as CONSTRAINT_TYPE,    'YES' as ENFORCED  FROM mysql.indexes idx    WHERE idx.table_id=tbl.id AND idx.type IN ('PRIMARY', 'UNIQUE')          AND IS_VISIBLE_DD_OBJECT(tbl.hidden, idx.hidden, idx.options) UNION ALL  SELECT    fk.name COLLATE utf8mb3_tolower_ci AS CONSTRAINT_NAME,    'FOREIGN KEY' as CONSTRAINT_TYPE,    'YES' as ENFORCED  FROM mysql.foreign_keys fk WHERE fk.table_id=tbl.id UNION ALL  SELECT    cc.name AS CONSTRAINT_NAME,    'CHECK' as CONSTRAINT_TYPE,    cc.enforced as ENFORCED  FROM mysql.check_constraints cc WHERE cc.table_id=tbl.id) constraints WHERE 
  CAN_ACCESS_TABLE(sch.name, tbl.name)
  AND IS_VISIBLE_DD_OBJECT(tbl.hidden)
");
CREATE OR REPLACE DEFINER=`mysql.infoschema`@`localhost` VIEW information_schema.TABLE_CONSTRAINTS_EXTENSIONS AS SELECT 
  cat.name AS CONSTRAINT_CATALOG,
  sch.name AS CONSTRAINT_SCHEMA,
  idx.name AS CONSTRAINT_NAME,
  tbl.name AS TABLE_NAME,
  idx.engine_attribute AS ENGINE_ATTRIBUTE,
  idx.secondary_engine_attribute AS SECONDARY_ENGINE_ATTRIBUTE FROM 
  mysql.indexes idx
  JOIN mysql.tables tbl ON idx.table_id=tbl.id
  JOIN mysql.schemata sch ON tbl.schema_id=sch.id
  JOIN mysql.catalogs cat ON cat.id=sch.catalog_id WHERE 
  CAN_ACCESS_TABLE(sch.name, tbl.name)
  AND IS_VISIBLE_DD_OBJECT(tbl.hidden, FALSE, idx.options)

INSERT INTO I_S_check_table(t) VALUES ("CREATE OR REPLACE DEFINER=`mysql.infoschema`@`localhost` VIEW information_schema.TABLE_CONSTRAINTS_EXTENSIONS AS SELECT 
  cat.name AS CONSTRAINT_CATALOG,
  sch.name AS CONSTRAINT_SCHEMA,
  idx.name AS CONSTRAINT_NAME,
  tbl.name AS TABLE_NAME,
  idx.engine_attribute AS ENGINE_ATTRIBUTE,
  idx.secondary_engine_attribute AS SECONDARY_ENGINE_ATTRIBUTE FROM 
  mysql.indexes idx
  JOIN mysql.tables tbl ON idx.table_id=tbl.id
  JOIN mysql.schemata sch ON tbl.schema_id=sch.id
  JOIN mysql.catalogs cat ON cat.id=sch.catalog_id WHERE 
  CAN_ACCESS_TABLE(sch.name, tbl.name)
  AND IS_VISIBLE_DD_OBJECT(tbl.hidden, FALSE, idx.options)
");
CREATE OR REPLACE DEFINER=`mysql.infoschema`@`localhost` VIEW information_schema.TABLES AS SELECT 
  cat.name AS TABLE_CATALOG,
  sch.name AS TABLE_SCHEMA,
  tbl.name AS TABLE_NAME,
  tbl.type AS TABLE_TYPE,
  IF(tbl.type = 'BASE TABLE', tbl.engine, NULL) AS ENGINE,
  IF(tbl.type = 'VIEW', NULL, 10 /* FRM_VER_TRUE_VARCHAR */) AS VERSION,
  tbl.row_format AS ROW_FORMAT,
  IF (tbl.type = 'VIEW', NULL,INTERNAL_TABLE_ROWS(sch.name, tbl.name,  IF(ISNULL(tbl.partition_type), tbl.engine, ''),  tbl.se_private_id, tbl.hidden != 'Visible',   ts.se_private_data,  COALESCE(stat.table_rows, 0),  COALESCE(CAST(stat.cached_time as UNSIGNED), 0))) AS TABLE_ROWS,
  IF (tbl.type = 'VIEW', NULL,INTERNAL_AVG_ROW_LENGTH(sch.name, tbl.name,  IF(ISNULL(tbl.partition_type), tbl.engine, ''),  tbl.se_private_id, tbl.hidden != 'Visible',   ts.se_private_data,  COALESCE(stat.avg_row_length, 0),  COALESCE(CAST(stat.cached_time as UNSIGNED), 0))) AS AVG_ROW_LENGTH,
  IF (tbl.type = 'VIEW', NULL,INTERNAL_DATA_LENGTH(sch.name, tbl.name,  IF(ISNULL(tbl.partition_type), tbl.engine, ''),  tbl.se_private_id, tbl.hidden != 'Visible',   ts.se_private_data,  COALESCE(stat.data_length, 0),  COALESCE(CAST(stat.cached_time as UNSIGNED), 0))) AS DATA_LENGTH,
  IF (tbl.type = 'VIEW', NULL,INTERNAL_MAX_DATA_LENGTH(sch.name, tbl.name,  IF(ISNULL(tbl.partition_type), tbl.engine, ''),  tbl.se_private_id, tbl.hidden != 'Visible',   ts.se_private_data,  COALESCE(stat.max_data_length, 0),  COALESCE(CAST(stat.cached_time as UNSIGNED), 0))) AS MAX_DATA_LENGTH,
  IF (tbl.type = 'VIEW', NULL,INTERNAL_INDEX_LENGTH(sch.name, tbl.name,  IF(ISNULL(tbl.partition_type), tbl.engine, ''),  tbl.se_private_id, tbl.hidden != 'Visible',   ts.se_private_data,  COALESCE(stat.index_length, 0),  COALESCE(CAST(stat.cached_time as UNSIGNED), 0))) AS INDEX_LENGTH,
  IF (tbl.type = 'VIEW', NULL,INTERNAL_DATA_FREE(sch.name, tbl.name,  IF(ISNULL(tbl.partition_type), tbl.engine, ''),  tbl.se_private_id, tbl.hidden != 'Visible',   ts.se_private_data,  COALESCE(stat.data_free, 0),  COALESCE(CAST(stat.cached_time as UNSIGNED), 0))) AS DATA_FREE,
  IF (tbl.type = 'VIEW', NULL,INTERNAL_AUTO_INCREMENT(sch.name, tbl.name,  IF(ISNULL(tbl.partition_type), tbl.engine, ''),  tbl.se_private_id,   IS_VISIBLE_DD_OBJECT(tbl.hidden, FALSE,    tbl.options) IS FALSE,  ts.se_private_data,  COALESCE(stat.auto_increment, 0),  COALESCE(CAST(stat.cached_time as UNSIGNED), 0), tbl.se_private_data)) AS AUTO_INCREMENT,
  tbl.created AS CREATE_TIME,
  IF (tbl.type = 'VIEW', NULL,INTERNAL_UPDATE_TIME(sch.name, tbl.name,  IF(ISNULL(tbl.partition_type), tbl.engine, ''),  tbl.se_private_id, tbl.hidden != 'Visible',   ts.se_private_data,  COALESCE(CAST(stat.update_time as UNSIGNED), 0),  COALESCE(CAST(stat.cached_time as UNSIGNED), 0))) AS UPDATE_TIME,
  IF (tbl.type = 'VIEW', NULL,INTERNAL_CHECK_TIME(sch.name, tbl.name,  IF(ISNULL(tbl.partition_type), tbl.engine, ''),  tbl.se_private_id, tbl.hidden != 'Visible',   ts.se_private_data,  COALESCE(CAST(stat.check_time as UNSIGNED), 0),  COALESCE(CAST(stat.cached_time as UNSIGNED), 0))) AS CHECK_TIME,
  col.name AS TABLE_COLLATION,
  IF (tbl.type = 'VIEW', NULL,INTERNAL_CHECKSUM(sch.name, tbl.name,  IF(ISNULL(tbl.partition_type), tbl.engine, ''),  tbl.se_private_id, tbl.hidden != 'Visible',   ts.se_private_data,  COALESCE(stat.checksum, 0),  COALESCE(CAST(stat.cached_time as UNSIGNED), 0))) AS CHECKSUM,
  IF (tbl.type = 'VIEW', NULL,  GET_DD_CREATE_OPTIONS(tbl.options,  IF(IFNULL(tbl.partition_expression, 'NOT_PART_TBL')='NOT_PART_TBL',     0, 1), IF(sch.default_encryption='YES',1,0))) AS CREATE_OPTIONS,
  INTERNAL_GET_COMMENT_OR_ERROR(sch.name, tbl.name, tbl.type, tbl.options, tbl.comment) AS TABLE_COMMENT FROM 
  mysql.tables tbl
  JOIN mysql.schemata sch ON tbl.schema_id=sch.id
  JOIN mysql.catalogs cat ON cat.id=sch.catalog_id
  LEFT JOIN mysql.collations col ON tbl.collation_id=col.id
  LEFT JOIN mysql.tablespaces ts ON tbl.tablespace_id=ts.id
  LEFT JOIN mysql.table_stats stat ON tbl.name=stat.table_name AND sch.name=stat.schema_name WHERE 
  CAN_ACCESS_TABLE(sch.name, tbl.name)
  AND IS_VISIBLE_DD_OBJECT(tbl.hidden)

INSERT INTO I_S_check_table(t) VALUES ("CREATE OR REPLACE DEFINER=`mysql.infoschema`@`localhost` VIEW information_schema.TABLES AS SELECT 
  cat.name AS TABLE_CATALOG,
  sch.name AS TABLE_SCHEMA,
  tbl.name AS TABLE_NAME,
  tbl.type AS TABLE_TYPE,
  IF(tbl.type = 'BASE TABLE', tbl.engine, NULL) AS ENGINE,
  IF(tbl.type = 'VIEW', NULL, 10 /* FRM_VER_TRUE_VARCHAR */) AS VERSION,
  tbl.row_format AS ROW_FORMAT,
  IF (tbl.type = 'VIEW', NULL,INTERNAL_TABLE_ROWS(sch.name, tbl.name,  IF(ISNULL(tbl.partition_type), tbl.engine, ''),  tbl.se_private_id, tbl.hidden != 'Visible',   ts.se_private_data,  COALESCE(stat.table_rows, 0),  COALESCE(CAST(stat.cached_time as UNSIGNED), 0))) AS TABLE_ROWS,
  IF (tbl.type = 'VIEW', NULL,INTERNAL_AVG_ROW_LENGTH(sch.name, tbl.name,  IF(ISNULL(tbl.partition_type), tbl.engine, ''),  tbl.se_private_id, tbl.hidden != 'Visible',   ts.se_private_data,  COALESCE(stat.avg_row_length, 0),  COALESCE(CAST(stat.cached_time as UNSIGNED), 0))) AS AVG_ROW_LENGTH,
  IF (tbl.type = 'VIEW', NULL,INTERNAL_DATA_LENGTH(sch.name, tbl.name,  IF(ISNULL(tbl.partition_type), tbl.engine, ''),  tbl.se_private_id, tbl.hidden != 'Visible',   ts.se_private_data,  COALESCE(stat.data_length, 0),  COALESCE(CAST(stat.cached_time as UNSIGNED), 0))) AS DATA_LENGTH,
  IF (tbl.type = 'VIEW', NULL,INTERNAL_MAX_DATA_LENGTH(sch.name, tbl.name,  IF(ISNULL(tbl.partition_type), tbl.engine, ''),  tbl.se_private_id, tbl.hidden != 'Visible',   ts.se_private_data,  COALESCE(stat.max_data_length, 0),  COALESCE(CAST(stat.cached_time as UNSIGNED), 0))) AS MAX_DATA_LENGTH,
  IF (tbl.type = 'VIEW', NULL,INTERNAL_INDEX_LENGTH(sch.name, tbl.name,  IF(ISNULL(tbl.partition_type), tbl.engine, ''),  tbl.se_private_id, tbl.hidden != 'Visible',   ts.se_private_data,  COALESCE(stat.index_length, 0),  COALESCE(CAST(stat.cached_time as UNSIGNED), 0))) AS INDEX_LENGTH,
  IF (tbl.type = 'VIEW', NULL,INTERNAL_DATA_FREE(sch.name, tbl.name,  IF(ISNULL(tbl.partition_type), tbl.engine, ''),  tbl.se_private_id, tbl.hidden != 'Visible',   ts.se_private_data,  COALESCE(stat.data_free, 0),  COALESCE(CAST(stat.cached_time as UNSIGNED), 0))) AS DATA_FREE,
  IF (tbl.type = 'VIEW', NULL,INTERNAL_AUTO_INCREMENT(sch.name, tbl.name,  IF(ISNULL(tbl.partition_type), tbl.engine, ''),  tbl.se_private_id,   IS_VISIBLE_DD_OBJECT(tbl.hidden, FALSE,    tbl.options) IS FALSE,  ts.se_private_data,  COALESCE(stat.auto_increment, 0),  COALESCE(CAST(stat.cached_time as UNSIGNED), 0), tbl.se_private_data)) AS AUTO_INCREMENT,
  tbl.created AS CREATE_TIME,
  IF (tbl.type = 'VIEW', NULL,INTERNAL_UPDATE_TIME(sch.name, tbl.name,  IF(ISNULL(tbl.partition_type), tbl.engine, ''),  tbl.se_private_id, tbl.hidden != 'Visible',   ts.se_private_data,  COALESCE(CAST(stat.update_time as UNSIGNED), 0),  COALESCE(CAST(stat.cached_time as UNSIGNED), 0))) AS UPDATE_TIME,
  IF (tbl.type = 'VIEW', NULL,INTERNAL_CHECK_TIME(sch.name, tbl.name,  IF(ISNULL(tbl.partition_type), tbl.engine, ''),  tbl.se_private_id, tbl.hidden != 'Visible',   ts.se_private_data,  COALESCE(CAST(stat.check_time as UNSIGNED), 0),  COALESCE(CAST(stat.cached_time as UNSIGNED), 0))) AS CHECK_TIME,
  col.name AS TABLE_COLLATION,
  IF (tbl.type = 'VIEW', NULL,INTERNAL_CHECKSUM(sch.name, tbl.name,  IF(ISNULL(tbl.partition_type), tbl.engine, ''),  tbl.se_private_id, tbl.hidden != 'Visible',   ts.se_private_data,  COALESCE(stat.checksum, 0),  COALESCE(CAST(stat.cached_time as UNSIGNED), 0))) AS CHECKSUM,
  IF (tbl.type = 'VIEW', NULL,  GET_DD_CREATE_OPTIONS(tbl.options,  IF(IFNULL(tbl.partition_expression, 'NOT_PART_TBL')='NOT_PART_TBL',     0, 1), IF(sch.default_encryption='YES',1,0))) AS CREATE_OPTIONS,
  INTERNAL_GET_COMMENT_OR_ERROR(sch.name, tbl.name, tbl.type, tbl.options, tbl.comment) AS TABLE_COMMENT FROM 
  mysql.tables tbl
  JOIN mysql.schemata sch ON tbl.schema_id=sch.id
  JOIN mysql.catalogs cat ON cat.id=sch.catalog_id
  LEFT JOIN mysql.collations col ON tbl.collation_id=col.id
  LEFT JOIN mysql.tablespaces ts ON tbl.tablespace_id=ts.id
  LEFT JOIN mysql.table_stats stat ON tbl.name=stat.table_name AND sch.name=stat.schema_name WHERE 
  CAN_ACCESS_TABLE(sch.name, tbl.name)
  AND IS_VISIBLE_DD_OBJECT(tbl.hidden)
");
CREATE OR REPLACE DEFINER=`mysql.infoschema`@`localhost` VIEW information_schema.TABLES_EXTENSIONS AS SELECT 
  cat.name AS TABLE_CATALOG,
  sch.name AS TABLE_SCHEMA,
  tbl.name AS TABLE_NAME,
  tbl.engine_attribute AS ENGINE_ATTRIBUTE,
  tbl.secondary_engine_attribute AS SECONDARY_ENGINE_ATTRIBUTE FROM 
  mysql.tables tbl
  JOIN mysql.schemata sch ON tbl.schema_id=sch.id
  JOIN mysql.catalogs cat ON cat.id=sch.catalog_id WHERE 
  CAN_ACCESS_TABLE(sch.name, tbl.name)
  AND IS_VISIBLE_DD_OBJECT(tbl.hidden)

INSERT INTO I_S_check_table(t) VALUES ("CREATE OR REPLACE DEFINER=`mysql.infoschema`@`localhost` VIEW information_schema.TABLES_EXTENSIONS AS SELECT 
  cat.name AS TABLE_CATALOG,
  sch.name AS TABLE_SCHEMA,
  tbl.name AS TABLE_NAME,
  tbl.engine_attribute AS ENGINE_ATTRIBUTE,
  tbl.secondary_engine_attribute AS SECONDARY_ENGINE_ATTRIBUTE FROM 
  mysql.tables tbl
  JOIN mysql.schemata sch ON tbl.schema_id=sch.id
  JOIN mysql.catalogs cat ON cat.id=sch.catalog_id WHERE 
  CAN_ACCESS_TABLE(sch.name, tbl.name)
  AND IS_VISIBLE_DD_OBJECT(tbl.hidden)
");
CREATE OR REPLACE DEFINER=`mysql.infoschema`@`localhost` VIEW information_schema.TABLESPACES_EXTENSIONS AS SELECT 
  tsps.name AS TABLESPACE_NAME,
  tsps.engine_attribute AS ENGINE_ATTRIBUTE FROM 
  mysql.tablespaces tsps

INSERT INTO I_S_check_table(t) VALUES ("CREATE OR REPLACE DEFINER=`mysql.infoschema`@`localhost` VIEW information_schema.TABLESPACES_EXTENSIONS AS SELECT 
  tsps.name AS TABLESPACE_NAME,
  tsps.engine_attribute AS ENGINE_ATTRIBUTE FROM 
  mysql.tablespaces tsps
");
CREATE OR REPLACE DEFINER=`mysql.infoschema`@`localhost` VIEW information_schema.TRIGGERS AS SELECT 
  cat.name AS TRIGGER_CATALOG,
  sch.name AS TRIGGER_SCHEMA,
  trg.name AS TRIGGER_NAME,
  trg.event_type AS EVENT_MANIPULATION,
  cat.name AS EVENT_OBJECT_CATALOG,
  sch.name AS EVENT_OBJECT_SCHEMA,
  tbl.name AS EVENT_OBJECT_TABLE,
  trg.action_order AS ACTION_ORDER,
  NULL AS ACTION_CONDITION,
  trg.action_statement_utf8 AS ACTION_STATEMENT,
  'ROW' AS ACTION_ORIENTATION,
  trg.action_timing AS ACTION_TIMING,
  NULL AS ACTION_REFERENCE_OLD_TABLE,
  NULL AS ACTION_REFERENCE_NEW_TABLE,
  'OLD' AS ACTION_REFERENCE_OLD_ROW,
  'NEW' AS ACTION_REFERENCE_NEW_ROW,
  trg.created AS CREATED,
  trg.sql_mode AS SQL_MODE,
  trg.definer AS DEFINER,
  cs_client.name AS CHARACTER_SET_CLIENT,
  coll_conn.name AS COLLATION_CONNECTION,
  coll_db.name AS DATABASE_COLLATION FROM 
  mysql.triggers trg JOIN mysql.tables tbl ON tbl.id=trg.table_id
  JOIN mysql.schemata sch ON tbl.schema_id=sch.id
  JOIN mysql.catalogs cat ON cat.id=sch.catalog_id
  JOIN mysql.collations coll_client ON coll_client.id=trg.client_collation_id
  JOIN mysql.character_sets cs_client ON cs_client.id=coll_client.character_set_id
  JOIN mysql.collations coll_conn ON coll_conn.id=trg.connection_collation_id
  JOIN mysql.collations coll_db ON coll_db.id=trg.schema_collation_id WHERE 
  tbl.type != 'VIEW'
  AND CAN_ACCESS_TRIGGER(sch.name, tbl.name)
  AND IS_VISIBLE_DD_OBJECT(tbl.hidden)

INSERT INTO I_S_check_table(t) VALUES ("CREATE OR REPLACE DEFINER=`mysql.infoschema`@`localhost` VIEW information_schema.TRIGGERS AS SELECT 
  cat.name AS TRIGGER_CATALOG,
  sch.name AS TRIGGER_SCHEMA,
  trg.name AS TRIGGER_NAME,
  trg.event_type AS EVENT_MANIPULATION,
  cat.name AS EVENT_OBJECT_CATALOG,
  sch.name AS EVENT_OBJECT_SCHEMA,
  tbl.name AS EVENT_OBJECT_TABLE,
  trg.action_order AS ACTION_ORDER,
  NULL AS ACTION_CONDITION,
  trg.action_statement_utf8 AS ACTION_STATEMENT,
  'ROW' AS ACTION_ORIENTATION,
  trg.action_timing AS ACTION_TIMING,
  NULL AS ACTION_REFERENCE_OLD_TABLE,
  NULL AS ACTION_REFERENCE_NEW_TABLE,
  'OLD' AS ACTION_REFERENCE_OLD_ROW,
  'NEW' AS ACTION_REFERENCE_NEW_ROW,
  trg.created AS CREATED,
  trg.sql_mode AS SQL_MODE,
  trg.definer AS DEFINER,
  cs_client.name AS CHARACTER_SET_CLIENT,
  coll_conn.name AS COLLATION_CONNECTION,
  coll_db.name AS DATABASE_COLLATION FROM 
  mysql.triggers trg JOIN mysql.tables tbl ON tbl.id=trg.table_id
  JOIN mysql.schemata sch ON tbl.schema_id=sch.id
  JOIN mysql.catalogs cat ON cat.id=sch.catalog_id
  JOIN mysql.collations coll_client ON coll_client.id=trg.client_collation_id
  JOIN mysql.character_sets cs_client ON cs_client.id=coll_client.character_set_id
  JOIN mysql.collations coll_conn ON coll_conn.id=trg.connection_collation_id
  JOIN mysql.collations coll_db ON coll_db.id=trg.schema_collation_id WHERE 
  tbl.type != 'VIEW'
  AND CAN_ACCESS_TRIGGER(sch.name, tbl.name)
  AND IS_VISIBLE_DD_OBJECT(tbl.hidden)
");
CREATE OR REPLACE DEFINER=`mysql.infoschema`@`localhost` VIEW information_schema.VIEW_ROUTINE_USAGE AS SELECT 
  cat.name AS TABLE_CATALOG,
  sch.name AS TABLE_SCHEMA,
  vw.name AS TABLE_NAME,
  vru.routine_catalog AS SPECIFIC_CATALOG,
  vru.routine_schema AS SPECIFIC_SCHEMA,
  vru.routine_name AS SPECIFIC_NAME FROM 
  mysql.tables vw
  JOIN mysql.schemata sch ON vw.schema_id=sch.id
  JOIN mysql.catalogs cat ON cat.id=sch.catalog_id
  JOIN mysql.view_routine_usage vru ON vru.view_id=vw.id
  JOIN mysql.routines rtn ON vru.routine_catalog= cat.name AND vru.routine_schema= sch.name AND vru.routine_name= rtn.name WHERE 
  vw.type = 'VIEW'
  AND CAN_ACCESS_ROUTINE(vru.routine_schema, vru.routine_name, rtn.type, rtn.definer, FALSE)
  AND CAN_ACCESS_VIEW(sch.name, vw.name, vw.view_definer, vw.options)

INSERT INTO I_S_check_table(t) VALUES ("CREATE OR REPLACE DEFINER=`mysql.infoschema`@`localhost` VIEW information_schema.VIEW_ROUTINE_USAGE AS SELECT 
  cat.name AS TABLE_CATALOG,
  sch.name AS TABLE_SCHEMA,
  vw.name AS TABLE_NAME,
  vru.routine_catalog AS SPECIFIC_CATALOG,
  vru.routine_schema AS SPECIFIC_SCHEMA,
  vru.routine_name AS SPECIFIC_NAME FROM 
  mysql.tables vw
  JOIN mysql.schemata sch ON vw.schema_id=sch.id
  JOIN mysql.catalogs cat ON cat.id=sch.catalog_id
  JOIN mysql.view_routine_usage vru ON vru.view_id=vw.id
  JOIN mysql.routines rtn ON vru.routine_catalog= cat.name AND vru.routine_schema= sch.name AND vru.routine_name= rtn.name WHERE 
  vw.type = 'VIEW'
  AND CAN_ACCESS_ROUTINE(vru.routine_schema, vru.routine_name, rtn.type, rtn.definer, FALSE)
  AND CAN_ACCESS_VIEW(sch.name, vw.name, vw.view_definer, vw.options)
");
CREATE OR REPLACE DEFINER=`mysql.infoschema`@`localhost` VIEW information_schema.VIEW_TABLE_USAGE AS SELECT 
  cat.name AS VIEW_CATALOG,
  sch.name AS VIEW_SCHEMA,
  vw.name AS VIEW_NAME,
  vtu.table_catalog AS TABLE_CATALOG,
  vtu.table_schema AS TABLE_SCHEMA,
  vtu.table_name AS TABLE_NAME FROM 
  mysql.tables vw
  JOIN mysql.schemata sch ON vw.schema_id=sch.id
  JOIN mysql.catalogs cat ON cat.id=sch.catalog_id
  JOIN mysql.view_table_usage vtu ON vtu.view_id=vw.id WHERE 
  CAN_ACCESS_TABLE(vtu.table_schema, vtu.table_name)
  AND vw.type = 'VIEW'
  AND CAN_ACCESS_VIEW(sch.name, vw.name, vw.view_definer, vw.options)

INSERT INTO I_S_check_table(t) VALUES ("CREATE OR REPLACE DEFINER=`mysql.infoschema`@`localhost` VIEW information_schema.VIEW_TABLE_USAGE AS SELECT 
  cat.name AS VIEW_CATALOG,
  sch.name AS VIEW_SCHEMA,
  vw.name AS VIEW_NAME,
  vtu.table_catalog AS TABLE_CATALOG,
  vtu.table_schema AS TABLE_SCHEMA,
  vtu.table_name AS TABLE_NAME FROM 
  mysql.tables vw
  JOIN mysql.schemata sch ON vw.schema_id=sch.id
  JOIN mysql.catalogs cat ON cat.id=sch.catalog_id
  JOIN mysql.view_table_usage vtu ON vtu.view_id=vw.id WHERE 
  CAN_ACCESS_TABLE(vtu.table_schema, vtu.table_name)
  AND vw.type = 'VIEW'
  AND CAN_ACCESS_VIEW(sch.name, vw.name, vw.view_definer, vw.options)
");
CREATE OR REPLACE DEFINER=`mysql.infoschema`@`localhost` VIEW information_schema.VIEWS AS SELECT 
  cat.name AS TABLE_CATALOG,
  sch.name AS TABLE_SCHEMA,
  vw.name AS TABLE_NAME,
  IF(CAN_ACCESS_VIEW(sch.name, vw.name, vw.view_definer, vw.options)=TRUE,   vw.view_definition_utf8, '') AS VIEW_DEFINITION,
  vw.view_check_option AS CHECK_OPTION,
  vw.view_is_updatable AS IS_UPDATABLE,
  vw.view_definer AS DEFINER,
  IF (vw.view_security_type='DEFAULT', 'DEFINER', vw.view_security_type) AS SECURITY_TYPE,
  cs.name AS CHARACTER_SET_CLIENT,
  conn_coll.name AS COLLATION_CONNECTION FROM 
  mysql.tables vw
  JOIN mysql.schemata sch ON vw.schema_id=sch.id
  JOIN mysql.catalogs cat ON cat.id=sch.catalog_id
  JOIN mysql.collations conn_coll ON conn_coll.id= vw.view_connection_collation_id
  JOIN mysql.collations client_coll ON client_coll.id= vw.view_client_collation_id
  JOIN mysql.character_sets cs ON cs.id= client_coll.character_set_id WHERE 
  CAN_ACCESS_TABLE(sch.name, vw.name)
  AND vw.type = 'VIEW'

INSERT INTO I_S_check_table(t) VALUES ("CREATE OR REPLACE DEFINER=`mysql.infoschema`@`localhost` VIEW information_schema.VIEWS AS SELECT 
  cat.name AS TABLE_CATALOG,
  sch.name AS TABLE_SCHEMA,
  vw.name AS TABLE_NAME,
  IF(CAN_ACCESS_VIEW(sch.name, vw.name, vw.view_definer, vw.options)=TRUE,   vw.view_definition_utf8, '') AS VIEW_DEFINITION,
  vw.view_check_option AS CHECK_OPTION,
  vw.view_is_updatable AS IS_UPDATABLE,
  vw.view_definer AS DEFINER,
  IF (vw.view_security_type='DEFAULT', 'DEFINER', vw.view_security_type) AS SECURITY_TYPE,
  cs.name AS CHARACTER_SET_CLIENT,
  conn_coll.name AS COLLATION_CONNECTION FROM 
  mysql.tables vw
  JOIN mysql.schemata sch ON vw.schema_id=sch.id
  JOIN mysql.catalogs cat ON cat.id=sch.catalog_id
  JOIN mysql.collations conn_coll ON conn_coll.id= vw.view_connection_collation_id
  JOIN mysql.collations client_coll ON client_coll.id= vw.view_client_collation_id
  JOIN mysql.character_sets cs ON cs.id= client_coll.character_set_id WHERE 
  CAN_ACCESS_TABLE(sch.name, vw.name)
  AND vw.type = 'VIEW'
");
CREATE OR REPLACE DEFINER=`mysql.infoschema`@`localhost` VIEW information_schema.APPLICABLE_ROLES AS WITH RECURSIVE role_graph (c_parent_user, c_parent_host,                                 c_from_user, c_from_host,                                 c_to_user, c_to_host, role_path,                                 c_with_admin, c_enabled) AS   ((SELECT       INTERNAL_GET_USERNAME(), INTERNAL_GET_HOSTNAME(),       INTERNAL_GET_USERNAME(), INTERNAL_GET_HOSTNAME(),       CAST('' as CHAR(64) CHARSET utf8mb4),       CAST('' as CHAR(255) CHARSET utf8mb4),       CAST(SHA2(CONCAT(QUOTE(INTERNAL_GET_USERNAME()),'@',                        QUOTE(INTERNAL_GET_HOSTNAME())), 256)            AS CHAR(17000) CHARSET utf8mb4),       CAST('N' as CHAR(1) CHARSET utf8mb4),       FALSE     UNION       SELECT         INTERNAL_GET_USERNAME(), INTERNAL_GET_HOSTNAME(),         ROLE_NAME, ROLE_HOST,         INTERNAL_GET_USERNAME(), INTERNAL_GET_HOSTNAME(),         CAST(SHA2(CONCAT(QUOTE(ROLE_NAME),'@',                   CONVERT(QUOTE(ROLE_HOST) using utf8mb4)), 256)              AS CHAR(17000) CHARSET utf8mb4),         CAST('N' as CHAR(1) CHARSET utf8mb4),         FALSE       FROM JSON_TABLE(INTERNAL_GET_MANDATORY_ROLES_JSON(),            '$[*]' COLUMNS (            ROLE_NAME VARCHAR(255) CHARSET utf8mb4 PATH '$.ROLE_NAME',             ROLE_HOST VARCHAR(255) CHARSET utf8mb4 PATH '$.ROLE_HOST')             ) mandatory_roles       WHERE CONCAT(QUOTE(ROLE_NAME),'@',                    CONVERT(QUOTE(ROLE_HOST) using utf8mb4)) NOT IN             (SELECT CONCAT(QUOTE(FROM_USER),'@',                        CONVERT(QUOTE(FROM_HOST) using utf8mb4))              FROM mysql.role_edges              WHERE TO_USER = INTERNAL_GET_USERNAME() AND              CONVERT(TO_HOST using utf8mb4) = INTERNAL_GET_HOSTNAME())   )   UNION     SELECT c_parent_user, c_parent_host,       FROM_USER, FROM_HOST, TO_USER, TO_HOST,       IF(LOCATE(SHA2(CONCAT(QUOTE(FROM_USER),'@',                      CONVERT(QUOTE(FROM_HOST) using utf8mb4)), 256),                 role_path) = 0,          CONCAT(role_path,'->', SHA2(CONCAT(QUOTE(FROM_USER),'@',           CONVERT(QUOTE(FROM_HOST) using utf8mb4)), 256)), NULL),       WITH_ADMIN_OPTION,       IF(c_enabled OR        INTERNAL_IS_ENABLED_ROLE(FROM_USER, FROM_HOST), TRUE, FALSE)     FROM mysql.role_edges, role_graph     WHERE TO_USER = c_from_user AND           CONVERT(TO_HOST using utf8mb4)= c_from_host AND           role_path IS NOT NULL)
 SELECT DISTINCT 
  c_parent_user AS USER,
  c_parent_host AS HOST,
  c_to_user AS GRANTEE,
  c_to_host AS GRANTEE_HOST,
  c_from_user AS ROLE_NAME,
  c_from_host AS ROLE_HOST,
  IF(c_with_admin = 'N', 'NO', 'YES') AS IS_GRANTABLE,
   (SELECT IF(COUNT(*), 'YES', 'NO')    FROM mysql.default_roles    WHERE DEFAULT_ROLE_USER = c_from_user AND          CONVERT(DEFAULT_ROLE_HOST using utf8mb4)= c_from_host AND          USER = c_parent_user AND          CONVERT(HOST using utf8mb4) = c_parent_host)  AS IS_DEFAULT,
  IF(INTERNAL_IS_MANDATORY_ROLE(c_from_user, c_from_host), 'YES', 'NO')  AS IS_MANDATORY FROM 
  role_graph WHERE 
  c_to_user != ''

INSERT INTO I_S_check_table(t) VALUES ("CREATE OR REPLACE DEFINER=`mysql.infoschema`@`localhost` VIEW information_schema.APPLICABLE_ROLES AS WITH RECURSIVE role_graph (c_parent_user, c_parent_host,                                 c_from_user, c_from_host,                                 c_to_user, c_to_host, role_path,                                 c_with_admin, c_enabled) AS   ((SELECT       INTERNAL_GET_USERNAME(), INTERNAL_GET_HOSTNAME(),       INTERNAL_GET_USERNAME(), INTERNAL_GET_HOSTNAME(),       CAST('' as CHAR(64) CHARSET utf8mb4),       CAST('' as CHAR(255) CHARSET utf8mb4),       CAST(SHA2(CONCAT(QUOTE(INTERNAL_GET_USERNAME()),'@',                        QUOTE(INTERNAL_GET_HOSTNAME())), 256)            AS CHAR(17000) CHARSET utf8mb4),       CAST('N' as CHAR(1) CHARSET utf8mb4),       FALSE     UNION       SELECT         INTERNAL_GET_USERNAME(), INTERNAL_GET_HOSTNAME(),         ROLE_NAME, ROLE_HOST,         INTERNAL_GET_USERNAME(), INTERNAL_GET_HOSTNAME(),         CAST(SHA2(CONCAT(QUOTE(ROLE_NAME),'@',                   CONVERT(QUOTE(ROLE_HOST) using utf8mb4)), 256)              AS CHAR(17000) CHARSET utf8mb4),         CAST('N' as CHAR(1) CHARSET utf8mb4),         FALSE       FROM JSON_TABLE(INTERNAL_GET_MANDATORY_ROLES_JSON(),            '$[*]' COLUMNS (            ROLE_NAME VARCHAR(255) CHARSET utf8mb4 PATH '$.ROLE_NAME',             ROLE_HOST VARCHAR(255) CHARSET utf8mb4 PATH '$.ROLE_HOST')             ) mandatory_roles       WHERE CONCAT(QUOTE(ROLE_NAME),'@',                    CONVERT(QUOTE(ROLE_HOST) using utf8mb4)) NOT IN             (SELECT CONCAT(QUOTE(FROM_USER),'@',                        CONVERT(QUOTE(FROM_HOST) using utf8mb4))              FROM mysql.role_edges              WHERE TO_USER = INTERNAL_GET_USERNAME() AND              CONVERT(TO_HOST using utf8mb4) = INTERNAL_GET_HOSTNAME())   )   UNION     SELECT c_parent_user, c_parent_host,       FROM_USER, FROM_HOST, TO_USER, TO_HOST,       IF(LOCATE(SHA2(CONCAT(QUOTE(FROM_USER),'@',                      CONVERT(QUOTE(FROM_HOST) using utf8mb4)), 256),                 role_path) = 0,          CONCAT(role_path,'->', SHA2(CONCAT(QUOTE(FROM_USER),'@',           CONVERT(QUOTE(FROM_HOST) using utf8mb4)), 256)), NULL),       WITH_ADMIN_OPTION,       IF(c_enabled OR        INTERNAL_IS_ENABLED_ROLE(FROM_USER, FROM_HOST), TRUE, FALSE)     FROM mysql.role_edges, role_graph     WHERE TO_USER = c_from_user AND           CONVERT(TO_HOST using utf8mb4)= c_from_host AND           role_path IS NOT NULL)
 SELECT DISTINCT 
  c_parent_user AS USER,
  c_parent_host AS HOST,
  c_to_user AS GRANTEE,
  c_to_host AS GRANTEE_HOST,
  c_from_user AS ROLE_NAME,
  c_from_host AS ROLE_HOST,
  IF(c_with_admin = 'N', 'NO', 'YES') AS IS_GRANTABLE,
   (SELECT IF(COUNT(*), 'YES', 'NO')    FROM mysql.default_roles    WHERE DEFAULT_ROLE_USER = c_from_user AND          CONVERT(DEFAULT_ROLE_HOST using utf8mb4)= c_from_host AND          USER = c_parent_user AND          CONVERT(HOST using utf8mb4) = c_parent_host)  AS IS_DEFAULT,
  IF(INTERNAL_IS_MANDATORY_ROLE(c_from_user, c_from_host), 'YES', 'NO')  AS IS_MANDATORY FROM 
  role_graph WHERE 
  c_to_user != ''
");
CREATE OR REPLACE DEFINER=`mysql.infoschema`@`localhost` VIEW information_schema.ADMINISTRABLE_ROLE_AUTHORIZATIONS AS SELECT 
* FROM 
  INFORMATION_SCHEMA.APPLICABLE_ROLES WHERE 
  IS_GRANTABLE='YES'

INSERT INTO I_S_check_table(t) VALUES ("CREATE OR REPLACE DEFINER=`mysql.infoschema`@`localhost` VIEW information_schema.ADMINISTRABLE_ROLE_AUTHORIZATIONS AS SELECT 
* FROM 
  INFORMATION_SCHEMA.APPLICABLE_ROLES WHERE 
  IS_GRANTABLE='YES'
");
CREATE OR REPLACE DEFINER=`mysql.infoschema`@`localhost` VIEW information_schema.ENABLED_ROLES AS SELECT 
  ROLE_NAME AS ROLE_NAME,
  ROLE_HOST AS ROLE_HOST,
   (SELECT IF(COUNT(*), 'YES', 'NO')    FROM mysql.default_roles    WHERE DEFAULT_ROLE_USER = ROLE_NAME AND          CONVERT(DEFAULT_ROLE_HOST using utf8mb4) = ROLE_HOST AND          USER = INTERNAL_GET_USERNAME() AND          CONVERT(HOST using utf8mb4) = INTERNAL_GET_HOSTNAME())  AS IS_DEFAULT,
  IF(INTERNAL_IS_MANDATORY_ROLE(ROLE_NAME, ROLE_HOST), 'YES', 'NO')  AS IS_MANDATORY FROM 
  JSON_TABLE(INTERNAL_GET_ENABLED_ROLE_JSON(), '$[*]' COLUMNS ( ROLE_NAME VARCHAR(255) CHARSET utf8mb4 PATH '$.ROLE_NAME',  ROLE_HOST VARCHAR(255) CHARSET utf8mb4 PATH '$.ROLE_HOST')  ) current_user_enabled_roles

INSERT INTO I_S_check_table(t) VALUES ("CREATE OR REPLACE DEFINER=`mysql.infoschema`@`localhost` VIEW information_schema.ENABLED_ROLES AS SELECT 
  ROLE_NAME AS ROLE_NAME,
  ROLE_HOST AS ROLE_HOST,
   (SELECT IF(COUNT(*), 'YES', 'NO')    FROM mysql.default_roles    WHERE DEFAULT_ROLE_USER = ROLE_NAME AND          CONVERT(DEFAULT_ROLE_HOST using utf8mb4) = ROLE_HOST AND          USER = INTERNAL_GET_USERNAME() AND          CONVERT(HOST using utf8mb4) = INTERNAL_GET_HOSTNAME())  AS IS_DEFAULT,
  IF(INTERNAL_IS_MANDATORY_ROLE(ROLE_NAME, ROLE_HOST), 'YES', 'NO')  AS IS_MANDATORY FROM 
  JSON_TABLE(INTERNAL_GET_ENABLED_ROLE_JSON(), '$[*]' COLUMNS ( ROLE_NAME VARCHAR(255) CHARSET utf8mb4 PATH '$.ROLE_NAME',  ROLE_HOST VARCHAR(255) CHARSET utf8mb4 PATH '$.ROLE_HOST')  ) current_user_enabled_roles
");
CREATE OR REPLACE DEFINER=`mysql.infoschema`@`localhost` VIEW information_schema.ROLE_TABLE_GRANTS AS WITH RECURSIVE role_graph (c_parent_user, c_parent_host,                                 c_from_user, c_from_host,                                 c_to_user, c_to_host, role_path,                                 c_with_admin, c_enabled) AS   ((SELECT       INTERNAL_GET_USERNAME(), INTERNAL_GET_HOSTNAME(),       INTERNAL_GET_USERNAME(), INTERNAL_GET_HOSTNAME(),       CAST('' as CHAR(64) CHARSET utf8mb4),       CAST('' as CHAR(255) CHARSET utf8mb4),       CAST(SHA2(CONCAT(QUOTE(INTERNAL_GET_USERNAME()),'@',                        QUOTE(INTERNAL_GET_HOSTNAME())), 256)            AS CHAR(17000) CHARSET utf8mb4),       CAST('N' as CHAR(1) CHARSET utf8mb4),       FALSE     UNION       SELECT         INTERNAL_GET_USERNAME(), INTERNAL_GET_HOSTNAME(),         ROLE_NAME, ROLE_HOST,         INTERNAL_GET_USERNAME(), INTERNAL_GET_HOSTNAME(),         CAST(SHA2(CONCAT(QUOTE(ROLE_NAME),'@',                   CONVERT(QUOTE(ROLE_HOST) using utf8mb4)), 256)              AS CHAR(17000) CHARSET utf8mb4),         CAST('N' as CHAR(1) CHARSET utf8mb4),         FALSE       FROM JSON_TABLE(INTERNAL_GET_MANDATORY_ROLES_JSON(),            '$[*]' COLUMNS (            ROLE_NAME VARCHAR(255) CHARSET utf8mb4 PATH '$.ROLE_NAME',             ROLE_HOST VARCHAR(255) CHARSET utf8mb4 PATH '$.ROLE_HOST')             ) mandatory_roles       WHERE CONCAT(QUOTE(ROLE_NAME),'@',                    CONVERT(QUOTE(ROLE_HOST) using utf8mb4)) NOT IN             (SELECT CONCAT(QUOTE(FROM_USER),'@',                        CONVERT(QUOTE(FROM_HOST) using utf8mb4))              FROM mysql.role_edges              WHERE TO_USER = INTERNAL_GET_USERNAME() AND              CONVERT(TO_HOST using utf8mb4) = INTERNAL_GET_HOSTNAME())   )   UNION     SELECT c_parent_user, c_parent_host,       FROM_USER, FROM_HOST, TO_USER, TO_HOST,       IF(LOCATE(SHA2(CONCAT(QUOTE(FROM_USER),'@',                      CONVERT(QUOTE(FROM_HOST) using utf8mb4)), 256),                 role_path) = 0,          CONCAT(role_path,'->', SHA2(CONCAT(QUOTE(FROM_USER),'@',           CONVERT(QUOTE(FROM_HOST) using utf8mb4)), 256)), NULL),       WITH_ADMIN_OPTION,       IF(c_enabled OR        INTERNAL_IS_ENABLED_ROLE(FROM_USER, FROM_HOST), TRUE, FALSE)     FROM mysql.role_edges, role_graph     WHERE TO_USER = c_from_user AND           CONVERT(TO_HOST using utf8mb4)= c_from_host AND           role_path IS NOT NULL)
 SELECT DISTINCT 
  INTERNAL_GET_USERNAME(Grantor) AS GRANTOR,
  INTERNAL_GET_HOSTNAME(Grantor) AS GRANTOR_HOST,
  tp.User AS GRANTEE,
  tp.Host AS GRANTEE_HOST,
  'def' AS TABLE_CATALOG,
  Db AS TABLE_SCHEMA,
  Table_name AS TABLE_NAME,
  Table_priv AS PRIVILEGE_TYPE,
  IF(FIND_IN_SET('Grant',Table_priv)>0, 'YES', 'NO') AS IS_GRANTABLE FROM 
  mysql.tables_priv tp
   JOIN role_graph rg ON  tp.User = rg.c_from_user AND   CONVERT(tp.Host using utf8mb4) = rg.c_from_host WHERE 
  Table_priv > 0
   AND c_to_user != ''
   AND c_enabled = TRUE

INSERT INTO I_S_check_table(t) VALUES ("CREATE OR REPLACE DEFINER=`mysql.infoschema`@`localhost` VIEW information_schema.ROLE_TABLE_GRANTS AS WITH RECURSIVE role_graph (c_parent_user, c_parent_host,                                 c_from_user, c_from_host,                                 c_to_user, c_to_host, role_path,                                 c_with_admin, c_enabled) AS   ((SELECT       INTERNAL_GET_USERNAME(), INTERNAL_GET_HOSTNAME(),       INTERNAL_GET_USERNAME(), INTERNAL_GET_HOSTNAME(),       CAST('' as CHAR(64) CHARSET utf8mb4),       CAST('' as CHAR(255) CHARSET utf8mb4),       CAST(SHA2(CONCAT(QUOTE(INTERNAL_GET_USERNAME()),'@',                        QUOTE(INTERNAL_GET_HOSTNAME())), 256)            AS CHAR(17000) CHARSET utf8mb4),       CAST('N' as CHAR(1) CHARSET utf8mb4),       FALSE     UNION       SELECT         INTERNAL_GET_USERNAME(), INTERNAL_GET_HOSTNAME(),         ROLE_NAME, ROLE_HOST,         INTERNAL_GET_USERNAME(), INTERNAL_GET_HOSTNAME(),         CAST(SHA2(CONCAT(QUOTE(ROLE_NAME),'@',                   CONVERT(QUOTE(ROLE_HOST) using utf8mb4)), 256)              AS CHAR(17000) CHARSET utf8mb4),         CAST('N' as CHAR(1) CHARSET utf8mb4),         FALSE       FROM JSON_TABLE(INTERNAL_GET_MANDATORY_ROLES_JSON(),            '$[*]' COLUMNS (            ROLE_NAME VARCHAR(255) CHARSET utf8mb4 PATH '$.ROLE_NAME',             ROLE_HOST VARCHAR(255) CHARSET utf8mb4 PATH '$.ROLE_HOST')             ) mandatory_roles       WHERE CONCAT(QUOTE(ROLE_NAME),'@',                    CONVERT(QUOTE(ROLE_HOST) using utf8mb4)) NOT IN             (SELECT CONCAT(QUOTE(FROM_USER),'@',                        CONVERT(QUOTE(FROM_HOST) using utf8mb4))              FROM mysql.role_edges              WHERE TO_USER = INTERNAL_GET_USERNAME() AND              CONVERT(TO_HOST using utf8mb4) = INTERNAL_GET_HOSTNAME())   )   UNION     SELECT c_parent_user, c_parent_host,       FROM_USER, FROM_HOST, TO_USER, TO_HOST,       IF(LOCATE(SHA2(CONCAT(QUOTE(FROM_USER),'@',                      CONVERT(QUOTE(FROM_HOST) using utf8mb4)), 256),                 role_path) = 0,          CONCAT(role_path,'->', SHA2(CONCAT(QUOTE(FROM_USER),'@',           CONVERT(QUOTE(FROM_HOST) using utf8mb4)), 256)), NULL),       WITH_ADMIN_OPTION,       IF(c_enabled OR        INTERNAL_IS_ENABLED_ROLE(FROM_USER, FROM_HOST), TRUE, FALSE)     FROM mysql.role_edges, role_graph     WHERE TO_USER = c_from_user AND           CONVERT(TO_HOST using utf8mb4)= c_from_host AND           role_path IS NOT NULL)
 SELECT DISTINCT 
  INTERNAL_GET_USERNAME(Grantor) AS GRANTOR,
  INTERNAL_GET_HOSTNAME(Grantor) AS GRANTOR_HOST,
  tp.User AS GRANTEE,
  tp.Host AS GRANTEE_HOST,
  'def' AS TABLE_CATALOG,
  Db AS TABLE_SCHEMA,
  Table_name AS TABLE_NAME,
  Table_priv AS PRIVILEGE_TYPE,
  IF(FIND_IN_SET('Grant',Table_priv)>0, 'YES', 'NO') AS IS_GRANTABLE FROM 
  mysql.tables_priv tp
   JOIN role_graph rg ON  tp.User = rg.c_from_user AND   CONVERT(tp.Host using utf8mb4) = rg.c_from_host WHERE 
  Table_priv > 0
   AND c_to_user != ''
   AND c_enabled = TRUE
");
CREATE OR REPLACE DEFINER=`mysql.infoschema`@`localhost` VIEW information_schema.ROLE_COLUMN_GRANTS AS WITH RECURSIVE role_graph (c_parent_user, c_parent_host,                                 c_from_user, c_from_host,                                 c_to_user, c_to_host, role_path,                                 c_with_admin, c_enabled) AS   ((SELECT       INTERNAL_GET_USERNAME(), INTERNAL_GET_HOSTNAME(),       INTERNAL_GET_USERNAME(), INTERNAL_GET_HOSTNAME(),       CAST('' as CHAR(64) CHARSET utf8mb4),       CAST('' as CHAR(255) CHARSET utf8mb4),       CAST(SHA2(CONCAT(QUOTE(INTERNAL_GET_USERNAME()),'@',                        QUOTE(INTERNAL_GET_HOSTNAME())), 256)            AS CHAR(17000) CHARSET utf8mb4),       CAST('N' as CHAR(1) CHARSET utf8mb4),       FALSE     UNION       SELECT         INTERNAL_GET_USERNAME(), INTERNAL_GET_HOSTNAME(),         ROLE_NAME, ROLE_HOST,         INTERNAL_GET_USERNAME(), INTERNAL_GET_HOSTNAME(),         CAST(SHA2(CONCAT(QUOTE(ROLE_NAME),'@',                   CONVERT(QUOTE(ROLE_HOST) using utf8mb4)), 256)              AS CHAR(17000) CHARSET utf8mb4),         CAST('N' as CHAR(1) CHARSET utf8mb4),         FALSE       FROM JSON_TABLE(INTERNAL_GET_MANDATORY_ROLES_JSON(),            '$[*]' COLUMNS (            ROLE_NAME VARCHAR(255) CHARSET utf8mb4 PATH '$.ROLE_NAME',             ROLE_HOST VARCHAR(255) CHARSET utf8mb4 PATH '$.ROLE_HOST')             ) mandatory_roles       WHERE CONCAT(QUOTE(ROLE_NAME),'@',                    CONVERT(QUOTE(ROLE_HOST) using utf8mb4)) NOT IN             (SELECT CONCAT(QUOTE(FROM_USER),'@',                        CONVERT(QUOTE(FROM_HOST) using utf8mb4))              FROM mysql.role_edges              WHERE TO_USER = INTERNAL_GET_USERNAME() AND              CONVERT(TO_HOST using utf8mb4) = INTERNAL_GET_HOSTNAME())   )   UNION     SELECT c_parent_user, c_parent_host,       FROM_USER, FROM_HOST, TO_USER, TO_HOST,       IF(LOCATE(SHA2(CONCAT(QUOTE(FROM_USER),'@',                      CONVERT(QUOTE(FROM_HOST) using utf8mb4)), 256),                 role_path) = 0,          CONCAT(role_path,'->', SHA2(CONCAT(QUOTE(FROM_USER),'@',           CONVERT(QUOTE(FROM_HOST) using utf8mb4)), 256)), NULL),       WITH_ADMIN_OPTION,       IF(c_enabled OR        INTERNAL_IS_ENABLED_ROLE(FROM_USER, FROM_HOST), TRUE, FALSE)     FROM mysql.role_edges, role_graph     WHERE TO_USER = c_from_user AND           CONVERT(TO_HOST using utf8mb4)= c_from_host AND           role_path IS NOT NULL)
 SELECT DISTINCT 
  INTERNAL_GET_USERNAME(Grantor) AS GRANTOR,
  INTERNAL_GET_HOSTNAME(Grantor) AS GRANTOR_HOST,
  cp.User AS GRANTEE,
  cp.Host AS GRANTEE_HOST,
  'def' AS TABLE_CATALOG,
  cp.Db AS TABLE_SCHEMA,
  cp.Table_name AS TABLE_NAME,
  cp.Column_name AS COLUMN_NAME,
  cp.Column_priv AS PRIVILEGE_TYPE,
  IF(FIND_IN_SET('Grant',tp.Table_priv)>0, 'YES', 'NO') AS IS_GRANTABLE FROM 
  mysql.tables_priv tp 
   JOIN role_graph rg ON  tp.User = rg.c_from_user AND   CONVERT(tp.Host using utf8mb4) = rg.c_from_host
   JOIN mysql.columns_priv cp ON CONVERT(tp.Host using utf8mb4) = cp.Host AND cp.Db=tp.Db AND cp.User=tp.User AND cp.Table_name=tp.Table_name WHERE 
  cp.Column_priv > 0
   AND c_to_user != ''
   AND c_enabled = TRUE

INSERT INTO I_S_check_table(t) VALUES ("CREATE OR REPLACE DEFINER=`mysql.infoschema`@`localhost` VIEW information_schema.ROLE_COLUMN_GRANTS AS WITH RECURSIVE role_graph (c_parent_user, c_parent_host,                                 c_from_user, c_from_host,                                 c_to_user, c_to_host, role_path,                                 c_with_admin, c_enabled) AS   ((SELECT       INTERNAL_GET_USERNAME(), INTERNAL_GET_HOSTNAME(),       INTERNAL_GET_USERNAME(), INTERNAL_GET_HOSTNAME(),       CAST('' as CHAR(64) CHARSET utf8mb4),       CAST('' as CHAR(255) CHARSET utf8mb4),       CAST(SHA2(CONCAT(QUOTE(INTERNAL_GET_USERNAME()),'@',                        QUOTE(INTERNAL_GET_HOSTNAME())), 256)            AS CHAR(17000) CHARSET utf8mb4),       CAST('N' as CHAR(1) CHARSET utf8mb4),       FALSE     UNION       SELECT         INTERNAL_GET_USERNAME(), INTERNAL_GET_HOSTNAME(),         ROLE_NAME, ROLE_HOST,         INTERNAL_GET_USERNAME(), INTERNAL_GET_HOSTNAME(),         CAST(SHA2(CONCAT(QUOTE(ROLE_NAME),'@',                   CONVERT(QUOTE(ROLE_HOST) using utf8mb4)), 256)              AS CHAR(17000) CHARSET utf8mb4),         CAST('N' as CHAR(1) CHARSET utf8mb4),         FALSE       FROM JSON_TABLE(INTERNAL_GET_MANDATORY_ROLES_JSON(),            '$[*]' COLUMNS (            ROLE_NAME VARCHAR(255) CHARSET utf8mb4 PATH '$.ROLE_NAME',             ROLE_HOST VARCHAR(255) CHARSET utf8mb4 PATH '$.ROLE_HOST')             ) mandatory_roles       WHERE CONCAT(QUOTE(ROLE_NAME),'@',                    CONVERT(QUOTE(ROLE_HOST) using utf8mb4)) NOT IN             (SELECT CONCAT(QUOTE(FROM_USER),'@',                        CONVERT(QUOTE(FROM_HOST) using utf8mb4))              FROM mysql.role_edges              WHERE TO_USER = INTERNAL_GET_USERNAME() AND              CONVERT(TO_HOST using utf8mb4) = INTERNAL_GET_HOSTNAME())   )   UNION     SELECT c_parent_user, c_parent_host,       FROM_USER, FROM_HOST, TO_USER, TO_HOST,       IF(LOCATE(SHA2(CONCAT(QUOTE(FROM_USER),'@',                      CONVERT(QUOTE(FROM_HOST) using utf8mb4)), 256),                 role_path) = 0,          CONCAT(role_path,'->', SHA2(CONCAT(QUOTE(FROM_USER),'@',           CONVERT(QUOTE(FROM_HOST) using utf8mb4)), 256)), NULL),       WITH_ADMIN_OPTION,       IF(c_enabled OR        INTERNAL_IS_ENABLED_ROLE(FROM_USER, FROM_HOST), TRUE, FALSE)     FROM mysql.role_edges, role_graph     WHERE TO_USER = c_from_user AND           CONVERT(TO_HOST using utf8mb4)= c_from_host AND           role_path IS NOT NULL)
 SELECT DISTINCT 
  INTERNAL_GET_USERNAME(Grantor) AS GRANTOR,
  INTERNAL_GET_HOSTNAME(Grantor) AS GRANTOR_HOST,
  cp.User AS GRANTEE,
  cp.Host AS GRANTEE_HOST,
  'def' AS TABLE_CATALOG,
  cp.Db AS TABLE_SCHEMA,
  cp.Table_name AS TABLE_NAME,
  cp.Column_name AS COLUMN_NAME,
  cp.Column_priv AS PRIVILEGE_TYPE,
  IF(FIND_IN_SET('Grant',tp.Table_priv)>0, 'YES', 'NO') AS IS_GRANTABLE FROM 
  mysql.tables_priv tp 
   JOIN role_graph rg ON  tp.User = rg.c_from_user AND   CONVERT(tp.Host using utf8mb4) = rg.c_from_host
   JOIN mysql.columns_priv cp ON CONVERT(tp.Host using utf8mb4) = cp.Host AND cp.Db=tp.Db AND cp.User=tp.User AND cp.Table_name=tp.Table_name WHERE 
  cp.Column_priv > 0
   AND c_to_user != ''
   AND c_enabled = TRUE
");
CREATE OR REPLACE DEFINER=`mysql.infoschema`@`localhost` VIEW information_schema.ROLE_ROUTINE_GRANTS AS WITH RECURSIVE role_graph (c_parent_user, c_parent_host,                                 c_from_user, c_from_host,                                 c_to_user, c_to_host, role_path,                                 c_with_admin, c_enabled) AS   ((SELECT       INTERNAL_GET_USERNAME(), INTERNAL_GET_HOSTNAME(),       INTERNAL_GET_USERNAME(), INTERNAL_GET_HOSTNAME(),       CAST('' as CHAR(64) CHARSET utf8mb4),       CAST('' as CHAR(255) CHARSET utf8mb4),       CAST(SHA2(CONCAT(QUOTE(INTERNAL_GET_USERNAME()),'@',                        QUOTE(INTERNAL_GET_HOSTNAME())), 256)            AS CHAR(17000) CHARSET utf8mb4),       CAST('N' as CHAR(1) CHARSET utf8mb4),       FALSE     UNION       SELECT         INTERNAL_GET_USERNAME(), INTERNAL_GET_HOSTNAME(),         ROLE_NAME, ROLE_HOST,         INTERNAL_GET_USERNAME(), INTERNAL_GET_HOSTNAME(),         CAST(SHA2(CONCAT(QUOTE(ROLE_NAME),'@',                   CONVERT(QUOTE(ROLE_HOST) using utf8mb4)), 256)              AS CHAR(17000) CHARSET utf8mb4),         CAST('N' as CHAR(1) CHARSET utf8mb4),         FALSE       FROM JSON_TABLE(INTERNAL_GET_MANDATORY_ROLES_JSON(),            '$[*]' COLUMNS (            ROLE_NAME VARCHAR(255) CHARSET utf8mb4 PATH '$.ROLE_NAME',             ROLE_HOST VARCHAR(255) CHARSET utf8mb4 PATH '$.ROLE_HOST')             ) mandatory_roles       WHERE CONCAT(QUOTE(ROLE_NAME),'@',                    CONVERT(QUOTE(ROLE_HOST) using utf8mb4)) NOT IN             (SELECT CONCAT(QUOTE(FROM_USER),'@',                        CONVERT(QUOTE(FROM_HOST) using utf8mb4))              FROM mysql.role_edges              WHERE TO_USER = INTERNAL_GET_USERNAME() AND              CONVERT(TO_HOST using utf8mb4) = INTERNAL_GET_HOSTNAME())   )   UNION     SELECT c_parent_user, c_parent_host,       FROM_USER, FROM_HOST, TO_USER, TO_HOST,       IF(LOCATE(SHA2(CONCAT(QUOTE(FROM_USER),'@',                      CONVERT(QUOTE(FROM_HOST) using utf8mb4)), 256),                 role_path) = 0,          CONCAT(role_path,'->', SHA2(CONCAT(QUOTE(FROM_USER),'@',           CONVERT(QUOTE(FROM_HOST) using utf8mb4)), 256)), NULL),       WITH_ADMIN_OPTION,       IF(c_enabled OR        INTERNAL_IS_ENABLED_ROLE(FROM_USER, FROM_HOST), TRUE, FALSE)     FROM mysql.role_edges, role_graph     WHERE TO_USER = c_from_user AND           CONVERT(TO_HOST using utf8mb4)= c_from_host AND           role_path IS NOT NULL)
 SELECT DISTINCT 
  INTERNAL_GET_USERNAME(Grantor) AS GRANTOR,
  INTERNAL_GET_HOSTNAME(Grantor) AS GRANTOR_HOST,
  pp.User AS GRANTEE,
  pp.Host AS GRANTEE_HOST,
  'def' AS SPECIFIC_CATALOG,
  Db AS SPECIFIC_SCHEMA,
  Routine_name AS SPECIFIC_NAME,
  'def' AS ROUTINE_CATALOG,
  Db AS ROUTINE_SCHEMA,
  Routine_name AS ROUTINE_NAME,
  Proc_priv AS PRIVILEGE_TYPE,
  IF(FIND_IN_SET('Grant',Proc_priv)>0, 'YES', 'NO') AS IS_GRANTABLE FROM 
  mysql.procs_priv pp
   JOIN role_graph rg ON  pp.User = rg.c_from_user AND   CONVERT(pp.Host using utf8mb4) = rg.c_from_host WHERE 
  pp.Proc_priv > 0
   AND c_to_user != ''
   AND c_enabled = TRUE

INSERT INTO I_S_check_table(t) VALUES ("CREATE OR REPLACE DEFINER=`mysql.infoschema`@`localhost` VIEW information_schema.ROLE_ROUTINE_GRANTS AS WITH RECURSIVE role_graph (c_parent_user, c_parent_host,                                 c_from_user, c_from_host,                                 c_to_user, c_to_host, role_path,                                 c_with_admin, c_enabled) AS   ((SELECT       INTERNAL_GET_USERNAME(), INTERNAL_GET_HOSTNAME(),       INTERNAL_GET_USERNAME(), INTERNAL_GET_HOSTNAME(),       CAST('' as CHAR(64) CHARSET utf8mb4),       CAST('' as CHAR(255) CHARSET utf8mb4),       CAST(SHA2(CONCAT(QUOTE(INTERNAL_GET_USERNAME()),'@',                        QUOTE(INTERNAL_GET_HOSTNAME())), 256)            AS CHAR(17000) CHARSET utf8mb4),       CAST('N' as CHAR(1) CHARSET utf8mb4),       FALSE     UNION       SELECT         INTERNAL_GET_USERNAME(), INTERNAL_GET_HOSTNAME(),         ROLE_NAME, ROLE_HOST,         INTERNAL_GET_USERNAME(), INTERNAL_GET_HOSTNAME(),         CAST(SHA2(CONCAT(QUOTE(ROLE_NAME),'@',                   CONVERT(QUOTE(ROLE_HOST) using utf8mb4)), 256)              AS CHAR(17000) CHARSET utf8mb4),         CAST('N' as CHAR(1) CHARSET utf8mb4),         FALSE       FROM JSON_TABLE(INTERNAL_GET_MANDATORY_ROLES_JSON(),            '$[*]' COLUMNS (            ROLE_NAME VARCHAR(255) CHARSET utf8mb4 PATH '$.ROLE_NAME',             ROLE_HOST VARCHAR(255) CHARSET utf8mb4 PATH '$.ROLE_HOST')             ) mandatory_roles       WHERE CONCAT(QUOTE(ROLE_NAME),'@',                    CONVERT(QUOTE(ROLE_HOST) using utf8mb4)) NOT IN             (SELECT CONCAT(QUOTE(FROM_USER),'@',                        CONVERT(QUOTE(FROM_HOST) using utf8mb4))              FROM mysql.role_edges              WHERE TO_USER = INTERNAL_GET_USERNAME() AND              CONVERT(TO_HOST using utf8mb4) = INTERNAL_GET_HOSTNAME())   )   UNION     SELECT c_parent_user, c_parent_host,       FROM_USER, FROM_HOST, TO_USER, TO_HOST,       IF(LOCATE(SHA2(CONCAT(QUOTE(FROM_USER),'@',                      CONVERT(QUOTE(FROM_HOST) using utf8mb4)), 256),                 role_path) = 0,          CONCAT(role_path,'->', SHA2(CONCAT(QUOTE(FROM_USER),'@',           CONVERT(QUOTE(FROM_HOST) using utf8mb4)), 256)), NULL),       WITH_ADMIN_OPTION,       IF(c_enabled OR        INTERNAL_IS_ENABLED_ROLE(FROM_USER, FROM_HOST), TRUE, FALSE)     FROM mysql.role_edges, role_graph     WHERE TO_USER = c_from_user AND           CONVERT(TO_HOST using utf8mb4)= c_from_host AND           role_path IS NOT NULL)
 SELECT DISTINCT 
  INTERNAL_GET_USERNAME(Grantor) AS GRANTOR,
  INTERNAL_GET_HOSTNAME(Grantor) AS GRANTOR_HOST,
  pp.User AS GRANTEE,
  pp.Host AS GRANTEE_HOST,
  'def' AS SPECIFIC_CATALOG,
  Db AS SPECIFIC_SCHEMA,
  Routine_name AS SPECIFIC_NAME,
  'def' AS ROUTINE_CATALOG,
  Db AS ROUTINE_SCHEMA,
  Routine_name AS ROUTINE_NAME,
  Proc_priv AS PRIVILEGE_TYPE,
  IF(FIND_IN_SET('Grant',Proc_priv)>0, 'YES', 'NO') AS IS_GRANTABLE FROM 
  mysql.procs_priv pp
   JOIN role_graph rg ON  pp.User = rg.c_from_user AND   CONVERT(pp.Host using utf8mb4) = rg.c_from_host WHERE 
  pp.Proc_priv > 0
   AND c_to_user != ''
   AND c_enabled = TRUE
");
CREATE OR REPLACE DEFINER=`mysql.infoschema`@`localhost` VIEW information_schema.USER_ATTRIBUTES AS SELECT 
  user AS USER,
  host AS HOST,
  user_attributes->>"$.metadata" AS `ATTRIBUTE` FROM 
  mysql.user WHERE 
  CAN_ACCESS_USER(mysql.user.user,mysql.user.host)

INSERT INTO I_S_check_table(t) VALUES ('CREATE OR REPLACE DEFINER=`mysql.infoschema`@`localhost` VIEW information_schema.USER_ATTRIBUTES AS SELECT 
  user AS USER,
  host AS HOST,
  user_attributes->>"$.metadata" AS `ATTRIBUTE` FROM 
  mysql.user WHERE 
  CAN_ACCESS_USER(mysql.user.user,mysql.user.host)
');
SET debug = '-d,fetch_system_view_definition';
########################################################################
# PART 3. Verify the checksum of the generated meta data associated with
#         the current I_S version number.
########################################################################
include/assert.inc [Found expected number of system views in DD.]
include/assert.inc [Found expected number of system views in I_S_check_table.]
include/assert.inc [The group concat max length is sufficient.]
The schema checksum corresponds to I_S version 80030.
include/assert.inc [The schema checksum corresponds to a known I_S version.]
include/assert.inc [The schema checksum corresponds to
IS_VERSION 80030 stored on on disk.]
include/assert.inc [The stored I_S version is the latest published I_S version.]

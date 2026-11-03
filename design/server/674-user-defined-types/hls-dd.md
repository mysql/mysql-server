
# Data dictionary

## DD tables

### Table mysql.types;

This is a new table.

TODO: revise data_type enum, restrain to builtin types only.

```sql
CREATE TABLE `mysql`.`types` (
  `id` bigint unsigned NOT NULL AUTO_INCREMENT,
  `schema_id` bigint unsigned NOT NULL,
  `name` varchar(64) CHARACTER SET utf8mb3 COLLATE utf8mb3_general_ci NOT NULL,
  `data_type` enum(
    'MYSQL_TYPE_DECIMAL',
    'MYSQL_TYPE_TINY',
    'MYSQL_TYPE_SHORT',
    'MYSQL_TYPE_LONG',
    'MYSQL_TYPE_FLOAT',
    'MYSQL_TYPE_DOUBLE',
    'MYSQL_TYPE_NULL',
    'MYSQL_TYPE_TIMESTAMP',
    'MYSQL_TYPE_LONGLONG',
    'MYSQL_TYPE_INT24',
    'MYSQL_TYPE_DATE',
    'MYSQL_TYPE_TIME',
    'MYSQL_TYPE_DATETIME',
    'MYSQL_TYPE_YEAR',
    'MYSQL_TYPE_NEWDATE',
    'MYSQL_TYPE_VARCHAR',
    'MYSQL_TYPE_BIT',
    'MYSQL_TYPE_TIMESTAMP2',
    'MYSQL_TYPE_DATETIME2',
    'MYSQL_TYPE_TIME2',
    'MYSQL_TYPE_NEWDECIMAL',
    'MYSQL_TYPE_ENUM',
    'MYSQL_TYPE_SET',
    'MYSQL_TYPE_TINY_BLOB',
    'MYSQL_TYPE_MEDIUM_BLOB',
    'MYSQL_TYPE_LONG_BLOB',
    'MYSQL_TYPE_BLOB',
    'MYSQL_TYPE_VAR_STRING',
    'MYSQL_TYPE_STRING',
    'MYSQL_TYPE_GEOMETRY',
    'MYSQL_TYPE_JSON',
    'MYSQL_TYPE_VECTOR') COLLATE utf8mb3_bin NOT NULL,
  `is_unsigned` tinyint(1) DEFAULT NULL,
  `char_length` int unsigned DEFAULT NULL,
  `numeric_precision` int unsigned DEFAULT NULL,
  `numeric_scale` int unsigned DEFAULT NULL,
  `datetime_precision` int unsigned DEFAULT NULL,
  `collation_id` bigint unsigned DEFAULT NULL,
  `column_type_utf8` mediumtext COLLATE utf8mb3_bin NOT NULL,
  `is_explicit_collation` tinyint(1) DEFAULT NULL,
  `last_altered` timestamp NOT NULL,
  `created` timestamp NOT NULL,
  PRIMARY KEY (`id`),
  UNIQUE KEY `schema_id` (`schema_id`,`name`),
  KEY `collation_id` (`collation_id`),
  CONSTRAINT `types_ibfk_1` FOREIGN KEY (`schema_id`) REFERENCES `schemata` (`id`),
  CONSTRAINT `types_ibfk_2` FOREIGN KEY (`collation_id`) REFERENCES `collations` (`id`)
) /*!50100 TABLESPACE `mysql` */ ENGINE=InnoDB DEFAULT CHARSET=utf8mb3 COLLATE=utf8mb3_bin STATS_PERSISTENT=0 ROW_FORMAT=DYNAMIC
```

This table stores the type definition provided by the CREATE TYPE statement.

### Table mysql.columns;

This is an existing table.

TODO: revise constraints, use SCHEMA and TYPE instead of TYPE_ID

Add a column:

- `type_id bigint unsigned DEFAULT NULL`

When a column uses a user defined type,
column `type_id` represents the type used.

Add in KEY definition for `type_id`,
and a FOREIGN KEY constraint referencing mysql.types.

### Table mysql.routines;

This is an existing table.

TODO: revise constraints, use SCHEMA and TYPE instead of TYPE_ID

Add a column:

- `result_type_id bigint unsigned DEFAULT NULL`

When a stored function returns a user defined type,
column `result_type_id` represents the type used.

Add in KEY definition for `result_type_id`,
and a FOREIGN KEY constraint referencing mysql.types.

### Table mysql.parameters;

This is an existing table.

TODO: revise constraints, use SCHEMA and TYPE instead of TYPE_ID

Add a column:

- `type_id bigint unsigned DEFAULT NULL`

When a stored function parameter uses a user defined type,
column `type_id` represents the type used.

Add in KEY definition for `type_id`,
and a FOREIGN KEY constraint referencing mysql.types.

### Table mysql.methods;

New table.

Represents all the methods declared for a type.

TODO.

### Table mysql.method_parameters;

New table.

Represents all the parameters declared for a method.
Represents all the return type declared for a method.

TODO.

## INFORMATION_SCHEMA

### View INFORMATION_SCHEMA.TYPES

This is a new view.

```sql
mysql> describe INFORMATION_SCHEMA.TYPES;
+--------------------------+-----------------+------+-----+---------+-------+
| Field                    | Type            | Null | Key | Default | Extra |
+--------------------------+-----------------+------+-----+---------+-------+
| TYPE_CATALOG             | varchar(64)     | NO   |     | NULL    |       |
| TYPE_SCHEMA              | varchar(64)     | NO   |     | NULL    |       |
| TYPE_NAME                | varchar(64)     | NO   |     | NULL    |       |
| DATA_TYPE                | longtext        | YES  |     | NULL    |       |
| CHARACTER_MAXIMUM_LENGTH | bigint          | YES  |     | NULL    |       |
| CHARACTER_OCTET_LENGTH   | bigint          | YES  |     | NULL    |       |
| NUMERIC_PRECISION        | bigint unsigned | YES  |     | NULL    |       |
| NUMERIC_SCALE            | bigint unsigned | YES  |     | NULL    |       |
| DATETIME_PRECISION       | int unsigned    | YES  |     | NULL    |       |
| CHARACTER_SET_NAME       | varchar(64)     | YES  |     | NULL    |       |
| COLLATION_NAME           | varchar(64)     | YES  |     | NULL    |       |
| COLUMN_TYPE              | mediumtext      | NO   |     | NULL    |       |
+--------------------------+-----------------+------+-----+---------+-------+
```

View INFORMATION_SCHEMA.TYPES joins the following tables:

- TABLE mysql.types, for the type definitions
- TABLE mysql.catalogs, for the catalogs definitions
- TABLE mysql.schemata, for the schema definitions
- TABLE mysql.character_sets, for the character sets definitions
- TABLE mysql.collations, for the collations definitions

### View INFORMATION_SCHEMA.COLUMNS

This is an existing view.

Add columns:

- `TYPE_SCHEMA varchar(64)`
- `TYPE_NAME varchar(64)`

to represent the user defined type used by a column, if any.

This view now also joins TABLE `mysql`.`types`.

### View INFORMATION_SCHEMA.ROUTINES

This is an existing view.

Add columns:

- `RESULT_TYPE_SCHEMA varchar(64)`
- `RESULT_TYPE_NAME varchar(64)`

to represent the user defined type used in the result, if any.

This view now also joins TABLE `mysql`.`types`.

### View INFORMATION_SCHEMA.PARAMETERS

This is an existing view.

Add columns:

- `PARAMETER_TYPE_SCHEMA varchar(64)`
- `PARAMETER_TYPE_NAME varchar(64)`

to represent the user defined type used by a parameter, if any.

This view now also joins TABLE `mysql`.`types`.

### View INFORMATION_SCHEMA.METHODS

This is a new view.

TODO.

### View INFORMATION_SCHEMA.METHOD_PARAMETERS

This is a new view.

TODO.



# Data dictionary

## DD tables

### Table mysql.user_defined_types;

This is a new table.

TODO: revise data_type enum, restrain to builtin types only.

```sql
CREATE TABLE `mysql`.`user_defined_types` (
  `id` bigint unsigned NOT NULL AUTO_INCREMENT,
  `schema_id` bigint unsigned NOT NULL,
  `name` varchar(64) CHARACTER SET utf8mb3 COLLATE utf8mb3_general_ci NOT NULL,
  `data_type` enum(
    'MYSQL_BUILTIN_TYPE_DECIMAL',
    'MYSQL_BUILTIN_TYPE_TINY',
    'MYSQL_BUILTIN_TYPE_SHORT',
    'MYSQL_BUILTIN_TYPE_LONG',
    'MYSQL_BUILTIN_TYPE_FLOAT',
    'MYSQL_BUILTIN_TYPE_DOUBLE',
    'MYSQL_BUILTIN_TYPE_TIMESTAMP',
    'MYSQL_BUILTIN_TYPE_LONGLONG',
    'MYSQL_BUILTIN_TYPE_INT24',
    'MYSQL_BUILTIN_TYPE_DATE',
    'MYSQL_BUILTIN_TYPE_TIME',
    'MYSQL_BUILTIN_TYPE_DATETIME',
    'MYSQL_BUILTIN_TYPE_YEAR',
    'MYSQL_BUILTIN_TYPE_NEWDATE',
    'MYSQL_BUILTIN_TYPE_VARCHAR',
    'MYSQL_BUILTIN_TYPE_BIT',
    'MYSQL_BUILTIN_TYPE_TIMESTAMP2',
    'MYSQL_BUILTIN_TYPE_DATETIME2',
    'MYSQL_BUILTIN_TYPE_TIME2',
    'MYSQL_BUILTIN_TYPE_NEWDECIMAL',
    'MYSQL_BUILTIN_TYPE_TINY_BLOB',
    'MYSQL_BUILTIN_TYPE_MEDIUM_BLOB',
    'MYSQL_BUILTIN_TYPE_LONG_BLOB',
    'MYSQL_BUILTIN_TYPE_BLOB',
    'MYSQL_BUILTIN_TYPE_VAR_STRING',
    'MYSQL_BUILTIN_TYPE_STRING') COLLATE utf8mb3_bin NOT NULL,
  `type_category` enum('DISTINCT','STRUCTURED') COLLATE utf8mb3_bin NOT NULL,
  `is_instantiable` tinyint(1) DEFAULT NULL,
  `is_final` tinyint(1) DEFAULT NULL,
  `ordering_form` enum('NO_ORDER','EQUAL_ONLY','ORDER_FULL') COLLATE utf8mb3_bin NOT NULL,
  `ordering_category` enum('NO_CATEGORY','RELATIVE','MAP','STATE') COLLATE utf8mb3_bin NOT NULL,
  `ordering_method_id` bigint unsigned DEFAULT NULL,
  `is_unsigned` tinyint(1) DEFAULT NULL,
  `char_max_length` int unsigned DEFAULT NULL,
  `char_octet_length` int unsigned DEFAULT NULL,
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
  CONSTRAINT `user_defined_types_ibfk_1` FOREIGN KEY (`schema_id`) REFERENCES `schemata` (`id`),
  CONSTRAINT `user_defined_types_ibfk_2` FOREIGN KEY (`collation_id`) REFERENCES `collations` (`id`)
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

### Table mysql.method_specifications;

New table.

Represents all the methods declared for a type.

```sql
CREATE TABLE `mysql`.`method_specifications` (
  `id` bigint unsigned NOT NULL AUTO_INCREMENT,
  `type_id` bigint unsigned NOT NULL,
  `name` varchar(64) CHARACTER SET utf8mb3 COLLATE utf8mb3_general_ci NOT NULL,
  `specific_name` varchar(64) CHARACTER SET utf8mb3 COLLATE utf8mb3_general_ci NOT NULL,
  `is_static` tinyint(1) DEFAULT NULL,
  `is_overriding` tinyint(1) DEFAULT NULL,
  `is_constructor` tinyint(1) DEFAULT NULL,
  `is_unsigned` tinyint(1) DEFAULT NULL,
  `char_max_length` int unsigned DEFAULT NULL,
  `char_octet_length` int unsigned DEFAULT NULL,
  `numeric_precision` int unsigned DEFAULT NULL,
  `numeric_scale` int unsigned DEFAULT NULL,
  `datetime_precision` int unsigned DEFAULT NULL,
  `collation_id` bigint unsigned DEFAULT NULL,
  `is_explicit_collation` tinyint(1) DEFAULT NULL,
  `return_type_id` bigint unsigned DEFAULT NULL,
  `last_altered` timestamp NOT NULL,
  `created` timestamp NOT NULL,
  PRIMARY KEY (`id`),
  UNIQUE KEY `type_id` (`type_id`,`name`),
  CONSTRAINT `method_specifications_ibfk_1` FOREIGN KEY (`type_id`) REFERENCES `user_defined_types` (`id`)
) /*!50100 TABLESPACE `mysql` */ ENGINE=InnoDB DEFAULT CHARSET=utf8mb3 COLLATE=utf8mb3_bin STATS_PERSISTENT=0 ROW_FORMAT=DYNAMIC
```

TODO: column data_type, data_type_utf8 are missing

### Table mysql.type_method_parameters;

New table.

Represents all the parameters declared for a method.
Represents all the return type declared for a method.

```sql
CREATE TABLE `mysql`.`method_specification_parameters` (
  `id` bigint unsigned NOT NULL AUTO_INCREMENT,
  `method_id` bigint unsigned NOT NULL,
  `name` varchar(64) CHARACTER SET utf8mb3 COLLATE utf8mb3_general_ci NOT NULL,
  `is_result` tinyint(1) DEFAULT NULL,
  `is_unsigned` tinyint(1) DEFAULT NULL,
  `char_max_length` int unsigned DEFAULT NULL,
  `numeric_precision` int unsigned DEFAULT NULL,
  `numeric_scale` int unsigned DEFAULT NULL,
  `datetime_precision` int unsigned DEFAULT NULL,
  `collation_id` bigint unsigned DEFAULT NULL,
  `is_explicit_collation` tinyint(1) DEFAULT NULL,
  `last_altered` timestamp NOT NULL,
  `created` timestamp NOT NULL,
  PRIMARY KEY (`id`),
  UNIQUE KEY `method_id` (`method_id`,`name`),
  CONSTRAINT `method_specification_parameters_ibfk_1` FOREIGN KEY (`method_id`) REFERENCES `method_specifications` (`id`)
) /*!50100 TABLESPACE `mysql` */ ENGINE=InnoDB DEFAULT CHARSET=utf8mb3 COLLATE=utf8mb3_bin STATS_PERSISTENT=0 ROW_FORMAT=DYNAMIC
```

TODO: column data_type, data_type_utf8, ordinal_position are missing

## INFORMATION_SCHEMA

### View INFORMATION_SCHEMA.USER_DEFINED_TYPES

This is a new view.

TODO (in progress)

```sql
mysql> DESCRIBE INFORMATION_SCHEMA.USER_DEFINED_TYPES;
+----------------------------+----------------------------------------------+------+-----+---------+-------+
| Field                      | Type                                         | Null | Key | Default | Extra |
+----------------------------+----------------------------------------------+------+-----+---------+-------+
| USER_DEFINED_TYPE_CATALOG  | varchar(64)                                  | NO   |     | NULL    |       |
| USER_DEFINED_TYPE_SCHEMA   | varchar(64)                                  | NO   |     | NULL    |       |
| USER_DEFINED_TYPE_NAME     | varchar(64)                                  | NO   |     | NULL    |       |
| USER_DEFINED_TYPE_CATEGORY | enum('DISTINCT','STRUCTURED')                | NO   |     | NULL    |       |
| IS_INSTANTIABLE            | tinyint(1)                                   | YES  |     | NULL    |       |
| IS_FINAL                   | tinyint(1)                                   | YES  |     | NULL    |       |
| ORDERING_FORM              | enum('NO_ORDER','EQUAL_ONLY','ORDER_FULL')   | NO   |     | NULL    |       |
| ORDERING_CATEGORY          | enum('NO_CATEGORY','RELATIVE','MAP','STATE') | NO   |     | NULL    |       |
| DATA_TYPE                  | longtext                                     | YES  |     | NULL    |       |
| CHARACTER_MAXIMUM_LENGTH   | int unsigned                                 | YES  |     | NULL    |       |
| CHARACTER_OCTET_LENGTH     | int unsigned                                 | YES  |     | NULL    |       |
| CHARACTER_SET_NAME         | varchar(64)                                  | YES  |     | NULL    |       |
| COLLATION_NAME             | varchar(64)                                  | YES  |     | NULL    |       |
| NUMERIC_PRECISION          | bigint unsigned                              | YES  |     | NULL    |       |
| NUMERIC_SCALE              | bigint unsigned                              | YES  |     | NULL    |       |
| DATETIME_PRECISION         | int unsigned                                 | YES  |     | NULL    |       |
+----------------------------+----------------------------------------------+------+-----+---------+-------+
16 rows in set (0.005 sec)
```

View INFORMATION_SCHEMA.USER_DEFINED_TYPES joins the following tables:

- TABLE mysql.user_defined_types, for the type definitions
- TABLE mysql.catalogs, for the catalogs definitions
- TABLE mysql.schemata, for the schema definitions
- TABLE mysql.character_sets, for the character sets definitions
- TABLE mysql.collations, for the collations definitions

### View INFORMATION_SCHEMA.COLUMNS

This is an existing view.

Add columns:

- `UDT_CATALOG`
- `UDT_SCHEMA`
- `UDT_NAME`

to represent the user defined type used by a column, if any.

This view now also joins TABLE `mysql`.`user_defined_types`.

### View INFORMATION_SCHEMA.ROUTINES

This is an existing view.

Add columns:

- `TYPE_UDT_CATALOG`
- `TYPE_UDT_SCHEMA`
- `TYPE_UDT_NAME`

to represent the user defined type used in the result, if any.

TODO INVESTIGATE: Add columns:

- `UDT_CATALOG`
- `UDT_SCHEMA`
- `UDT_NAME`

to represent that the routime implements a method in a user defined type.

This assumes that routines:

* can be expressed in SQL,
* or can be declared as implemented externally (by a component as a service)

TODO INVESTIGATE: Add columns:

- `RESULT_CAST_TYPE_UDT_CATALOG`
- `RESULT_CAST_TYPE_UDT_SCHEMA`
- `RESULT_CAST_TYPE_UDT_NAME`
- `RESULT_CAST_DTD_IDENTIFIER`

to represent that the routine implements a CAST involving a user defined type.

This assumes that cast routines:

* can be expressed in SQL,
* or can be declared as implemented externally (by a component as a service)


This view now also joins TABLE `mysql`.`user_defined_types`.

### View INFORMATION_SCHEMA.PARAMETERS

This is an existing view.

Add columns:

- `UDT_CATALOG`
- `UDT_SCHEMA`
- `UDT_NAME`

to represent the user defined type used by a parameter, if any.

This view now also joins TABLE `mysql`.`user_defined_types`.

### View INFORMATION_SCHEMA.METHOD_SPECIFICATIONS

This is a new view.

TODO (in progress)

```sql
mysql> DESCRIBE INFORMATION_SCHEMA.METHOD_SPECIFICATIONS;
+--------------------------+-----------------+------+-----+---------+-------+
| Field                    | Type            | Null | Key | Default | Extra |
+--------------------------+-----------------+------+-----+---------+-------+
| SPECIFIC_NAME            | varchar(64)     | NO   |     | NULL    |       |
| UDT_CATALOG              | varchar(64)     | NO   |     | NULL    |       |
| UDT_SCHEMA               | varchar(64)     | NO   |     | NULL    |       |
| UDT_NAME                 | varchar(64)     | NO   |     | NULL    |       |
| METHOD_NAME              | varchar(64)     | NO   |     | NULL    |       |
| IS_STATIC                | tinyint(1)      | YES  |     | NULL    |       |
| IS_OVERRIDING            | tinyint(1)      | YES  |     | NULL    |       |
| IS_CONSTRUCTOR           | tinyint(1)      | YES  |     | NULL    |       |
| CHARACTER_MAXIMUM_LENGTH | int unsigned    | YES  |     | NULL    |       |
| CHARACTER_OCTET_LENGTH   | int unsigned    | YES  |     | NULL    |       |
| CHARACTER_SET_NAME       | varchar(64)     | YES  |     | NULL    |       |
| COLLATION_NAME           | varchar(64)     | YES  |     | NULL    |       |
| NUMERIC_PRECISION        | bigint unsigned | YES  |     | NULL    |       |
| NUMERIC_SCALE            | bigint unsigned | YES  |     | NULL    |       |
| DATETIME_PRECISION       | int unsigned    | YES  |     | NULL    |       |
+--------------------------+-----------------+------+-----+---------+-------+
15 rows in set (0.002 sec)
```

### View INFORMATION_SCHEMA.METHOD_SPECIFICATION_PARAMETERS

This is a new view.

TODO (in progress)

```sql
mysql> DESCRIBE INFORMATION_SCHEMA.METHOD_SPECIFICATION_PARAMETERS;
+--------------------------+-----------------+------+-----+---------+-------+
| Field                    | Type            | Null | Key | Default | Extra |
+--------------------------+-----------------+------+-----+---------+-------+
| UDT_CATALOG              | varchar(64)     | NO   |     | NULL    |       |
| UDT_SCHEMA               | varchar(64)     | NO   |     | NULL    |       |
| UDT_NAME                 | varchar(64)     | NO   |     | NULL    |       |
| METHOD_NAME              | varchar(64)     | NO   |     | NULL    |       |
| PARAMETER_NAME           | varchar(64)     | NO   |     | NULL    |       |
| IS_RESULT                | tinyint(1)      | YES  |     | NULL    |       |
| CHARACTER_MAXIMUM_LENGTH | int unsigned    | YES  |     | NULL    |       |
| CHARACTER_SET_NAME       | varchar(64)     | YES  |     | NULL    |       |
| COLLATION_NAME           | varchar(64)     | YES  |     | NULL    |       |
| NUMERIC_PRECISION        | bigint unsigned | YES  |     | NULL    |       |
| NUMERIC_SCALE            | bigint unsigned | YES  |     | NULL    |       |
| DATETIME_PRECISION       | int unsigned    | YES  |     | NULL    |       |
+--------------------------+-----------------+------+-----+---------+-------+
12 rows in set (0.002 sec)
```

create schema xtest default charset 'utf8mb4';


## I. Check validation schema setting when creating a collection. Check
#     result of insert operation which violates the check constraints.
#
# 1. Check collection creation with valid schema encoded as protobuf
#    string.
# 2. Check collection creation with valid schema encoded as
#    Mysqlx.Datatypes.Object.
# 3. Check that when validation is not enforced then it is possible to
#    insert noncopmlying data.
# 4. Check collection creation using schema encoded as
#    Mysqlx>datatypes.Object containing special characters in key/values.

## II. Check validation level setting.
#
# 1. Check validation level set to 'off'.
#   a. Check validation level for validation encoded as protobuf string.
#   b. Check validation level for validation encoded as
#      Mysqlx.Datatypes.Object.
# 2. Check validation level set to 'strict'.
#   a. Check validation level for validation encoded as protobuf string.
#   b. Check validation level for validation encoded as
#      Mysqlx.Datatypes.Object.
# 3. Assert that validation field is optional and check the default
#    setting for validation level.
#   a. Check validation level for validation encoded as protobuf string.
#   b. Check validation level for validation encoded as
#      Mysqlx.Datatypes.Object.
# 4. Check invalid value for validation level field.
#   a. Check validation level for validation encoded as protobuf string.
#   b. Check validation level for validation encoded as
#      Mysqlx.Datatypes.Object.
# 5. Check invalid collection option.

## III. Create collection without the optional validation field.

## IV. Create collection with empty validation schema field.
#
# 1. Check empty validation schema for validation encoded as protobuf
#    string.
# 2. Check empty validation schema for validation encoded as
#    Mysqlx.Datatypes.Object.

## V. Create collection with invalid validation options.
#
# 1. Validation contains only unknown fields.
# 2. Validation contains valid fields along with unknown fields.
# 3. Options is different type than "object".

## IV. Create collection with empty validation level.
#
# 1. Check empty validation level for validation encoded as protobuf
#    string.
# 2. Check empty validation level for validation encoded as
#    Mysqlx.Datatypes.Object.

#
# I.1

command ok
Table	Create Table
test_coll	CREATE TABLE `test_coll` (
  `doc` json DEFAULT NULL,
  `_id` varbinary(32) GENERATED ALWAYS AS (json_unquote(json_extract(`doc`,_utf8mb4'$._id'))) STORED NOT NULL,
  `_json_schema` json GENERATED ALWAYS AS (_utf8mb4'{"id":"http://json-schema.org/geo","$schema":"http://json-schema.org/draft-06/schema#","description":"A geographical coordinate","type":"object","properties":{"latitude":{"type":"number","minimum":-90,"maximum":90},"longitude":{"type":"number","minimum":-180,"maximum":180}},"required":["latitude","longitude"]}') VIRTUAL,
  PRIMARY KEY (`_id`),
  CONSTRAINT `$val_strict_889D1D09A01D7029B101675FBE738D1897A37F32` CHECK (json_schema_valid(`_json_schema`,`doc`))
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
0 rows affected
Got expected error:
Mysqlx.Error {
  severity: ERROR
  code: 5180
  msg: "Document is not valid according to the schema assigned to collection. The JSON document location \'#/latitude\' failed requirement \'maximum\' at JSON Schema location \'#/properties/latitude\'."
  sql_state: "HY000"
}


1 rows affected
Got expected error:
Mysqlx.Error {
  severity: ERROR
  code: 5180
  msg: "Document is not valid according to the schema assigned to collection. The JSON document location \'#/latitude\' failed requirement \'maximum\' at JSON Schema location \'#/properties/latitude\'."
  sql_state: "HY000"
}


1 rows affected
Rows matched: 1  Changed: 1  Warnings: 0
doc	_id	_json_schema
{"_id": "one", "latitude": 72, "longitude": 54}	one	{"id": "http://json-schema.org/geo", "type": "object", "$schema": "http://json-schema.org/draft-06/schema#", "required": ["latitude", "longitude"], "properties": {"latitude": {"type": "number", "maximum": 90, "minimum": -90}, "longitude": {"type": "number", "maximum": 180, "minimum": -180}}, "description": "A geographical coordinate"}
0 rows affected

0 rows affected

#
# I.2

command ok
Table	Create Table
test_coll	CREATE TABLE `test_coll` (
  `doc` json DEFAULT NULL,
  `_id` varbinary(32) GENERATED ALWAYS AS (json_unquote(json_extract(`doc`,_utf8mb4'$._id'))) STORED NOT NULL,
  `_json_schema` json GENERATED ALWAYS AS (_utf8mb4'{"id":"http://json-schema.org/geo","$schema":"http://json-schema.org/draft-06/schema#","description":"A geographical coordinate","type":"object","properties":{"latitude":{"type":"number","minimum":-90,"maximum":90},"longitude":{"type":"number","minimum":-180,"maximum":180}},"required":["latitude","longitude"]}') VIRTUAL,
  PRIMARY KEY (`_id`),
  CONSTRAINT `$val_strict_889D1D09A01D7029B101675FBE738D1897A37F32` CHECK (json_schema_valid(`_json_schema`,`doc`))
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
0 rows affected
Got expected error:
Mysqlx.Error {
  severity: ERROR
  code: 5180
  msg: "Document is not valid according to the schema assigned to collection. The JSON document location \'#/latitude\' failed requirement \'maximum\' at JSON Schema location \'#/properties/latitude\'."
  sql_state: "HY000"
}


1 rows affected
Got expected error:
Mysqlx.Error {
  severity: ERROR
  code: 5180
  msg: "Document is not valid according to the schema assigned to collection. The JSON document location \'#/latitude\' failed requirement \'maximum\' at JSON Schema location \'#/properties/latitude\'."
  sql_state: "HY000"
}


1 rows affected
Rows matched: 1  Changed: 1  Warnings: 0
doc	_id	_json_schema
{"_id": "one", "latitude": 72, "longitude": 54}	one	{"id": "http://json-schema.org/geo", "type": "object", "$schema": "http://json-schema.org/draft-06/schema#", "required": ["latitude", "longitude"], "properties": {"latitude": {"type": "number", "maximum": 90, "minimum": -90}, "longitude": {"type": "number", "maximum": 180, "minimum": -180}}, "description": "A geographical coordinate"}
0 rows affected

0 rows affected

#
# I.3

command ok

1 rows affected

1 rows affected
doc	_id	_json_schema
{"_id": "one", "latitude": 3312}	one	{"id": "http://json-schema.org/geo", "type": "object", "$schema": "http://json-schema.org/draft-06/schema#", "required": ["latitude", "longitude"], "properties": {"latitude": {"type": "number", "maximum": 90, "minimum": -90}, "longitude": {"type": "number", "maximum": 180, "minimum": -180}}, "description": "A geographical coordinate"}
{"_id": "two", "latitude": 45, "longitude": 54}	two	{"id": "http://json-schema.org/geo", "type": "object", "$schema": "http://json-schema.org/draft-06/schema#", "required": ["latitude", "longitude"], "properties": {"latitude": {"type": "number", "maximum": 90, "minimum": -90}, "longitude": {"type": "number", "maximum": 180, "minimum": -180}}, "description": "A geographical coordinate"}
0 rows affected

0 rows affected

#
# I.4

command ok
Table	Create Table
test_coll	CREATE TABLE `test_coll` (
  `doc` json DEFAULT NULL,
  `_id` varbinary(32) GENERATED ALWAYS AS (json_unquote(json_extract(`doc`,_utf8mb4'$._id'))) STORED NOT NULL,
  `_json_schema` json GENERATED ALWAYS AS (_utf8mb4'{"description":"t`y\\p"e o`b`\\\\c""t"}') VIRTUAL,
  PRIMARY KEY (`_id`),
  CONSTRAINT `$val_strict_889D1D09A01D7029B101675FBE738D1897A37F32` CHECK (json_schema_valid(`_json_schema`,`doc`))
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
0 rows affected

0 rows affected

#
# II.1.a

command ok

0 rows affected

command ok

0 rows affected

#
# II.1.b

command ok

0 rows affected

command ok

0 rows affected

#
# II.2.a

command ok

0 rows affected

command ok

0 rows affected

#
# II.2.b

command ok

0 rows affected

command ok

0 rows affected

#
# II.3.a

command ok

0 rows affected

#
# II.3.b

command ok

0 rows affected

#
# II.4.a
Got expected error:
Mysqlx.Error {
  severity: ERROR
  code: 5017
  msg: "Invalid \"validation.level\" argument. Allowed values are \'OFF\' and \'STRICT\'"
  sql_state: "HY000"
}


#
# II.4.b
Got expected error:
Mysqlx.Error {
  severity: ERROR
  code: 5017
  msg: "Invalid \"validation.level\" argument. Allowed values are \'OFF\' and \'STRICT\'"
  sql_state: "HY000"
}


#
# II.5
Got expected error:
Mysqlx.Error {
  severity: ERROR
  code: 5021
  msg: "\'foo\' is not a valid field for create_collection command"
  sql_state: "HY000"
}


#
# III

command ok
Table	Create Table
test_coll	CREATE TABLE `test_coll` (
  `doc` json DEFAULT NULL,
  `_id` varbinary(32) GENERATED ALWAYS AS (json_unquote(json_extract(`doc`,_utf8mb4'$._id'))) STORED NOT NULL,
  `_json_schema` json GENERATED ALWAYS AS (_utf8mb4'{"type":"object"}') VIRTUAL,
  PRIMARY KEY (`_id`),
  CONSTRAINT `$val_strict_889D1D09A01D7029B101675FBE738D1897A37F32` CHECK (json_schema_valid(`_json_schema`,`doc`)) /*!80016 NOT ENFORCED */
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
0 rows affected

0 rows affected

#
# IV.1

command ok
Table	Create Table
test_coll	CREATE TABLE `test_coll` (
  `doc` json DEFAULT NULL,
  `_id` varbinary(32) GENERATED ALWAYS AS (json_unquote(json_extract(`doc`,_utf8mb4'$._id'))) STORED NOT NULL,
  `_json_schema` json GENERATED ALWAYS AS (_utf8mb4'{}') VIRTUAL,
  PRIMARY KEY (`_id`),
  CONSTRAINT `$val_strict_889D1D09A01D7029B101675FBE738D1897A37F32` CHECK (json_schema_valid(`_json_schema`,`doc`))
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
0 rows affected

0 rows affected

#
# IV.2

command ok
Table	Create Table
test_coll	CREATE TABLE `test_coll` (
  `doc` json DEFAULT NULL,
  `_id` varbinary(32) GENERATED ALWAYS AS (json_unquote(json_extract(`doc`,_utf8mb4'$._id'))) STORED NOT NULL,
  `_json_schema` json GENERATED ALWAYS AS (_utf8mb4'{}') VIRTUAL,
  PRIMARY KEY (`_id`),
  CONSTRAINT `$val_strict_889D1D09A01D7029B101675FBE738D1897A37F32` CHECK (json_schema_valid(`_json_schema`,`doc`))
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
0 rows affected

0 rows affected

#
# V.1
Got expected error:
Mysqlx.Error {
  severity: ERROR
  code: 5021
  msg: "\'foo\' is not a valid field for create_collection command"
  sql_state: "HY000"
}


#
# V.2
Got expected error:
Mysqlx.Error {
  severity: ERROR
  code: 5021
  msg: "\'foo\' is not a valid field for create_collection command"
  sql_state: "HY000"
}


#
# V.3
Got expected error:
Mysqlx.Error {
  severity: ERROR
  code: 5016
  msg: "Invalid data type for argument \'options\', expected \'object\' type"
  sql_state: "HY000"
}


#
# VI.1

command ok

1 rows affected
Got expected error: Document is not valid according to the schema assigned to collection. The JSON document location '#' failed requirement 'required' at JSON Schema location '#'. (code 5180)

0 rows affected

#
# VI.2

command ok

1 rows affected
Got expected error: Document is not valid according to the schema assigned to collection. The JSON document location '#' failed requirement 'required' at JSON Schema location '#'. (code 5180)

0 rows affected
Mysqlx.Ok {
  msg: "bye!"
}
ok


## I. Check validation schema setting when creating a collection. Check
#     result of insert operation which violates the check constraints.
#
# 1. Check collection creation with valid schema encoded as protobuf
#    string.
# 2. Check collection creation with valid schema encoded as
#    Mysqlx.Datatypes.Object.
# 3. Check that when validation is not enforced then it is possible to
#    insert noncopmlying data.
# 4. Check collection creation using schema encoded as
#    Mysqlx>datatypes.Object containing special characters in key/values.

## II. Check validation level setting.
#
# 1. Check validation level set to 'off'.
#   a. Check validation level for validation encoded as protobuf string.
#   b. Check validation level for validation encoded as
#      Mysqlx.Datatypes.Object.
# 2. Check validation level set to 'strict'.
#   a. Check validation level for validation encoded as protobuf string.
#   b. Check validation level for validation encoded as
#      Mysqlx.Datatypes.Object.
# 3. Assert that validation field is optional and check the default
#    setting for validation level.
#   a. Check validation level for validation encoded as protobuf string.
#   b. Check validation level for validation encoded as
#      Mysqlx.Datatypes.Object.
# 4. Check invalid value for validation level field.
#   a. Check validation level for validation encoded as protobuf string.
#   b. Check validation level for validation encoded as
#      Mysqlx.Datatypes.Object.
# 5. Check invalid collection option.

## III. Create collection without the optional validation field.

## IV. Create collection with empty validation schema field.
#
# 1. Check empty validation schema for validation encoded as protobuf
#    string.
# 2. Check empty validation schema for validation encoded as
#    Mysqlx.Datatypes.Object.

## V. Create collection with invalid validation options.
#
# 1. Validation contains only unknown fields.
# 2. Validation contains valid fields along with unknown fields.
# 3. Options is different type than "object".

## IV. Create collection with empty validation level.
#
# 1. Check empty validation level for validation encoded as protobuf
#    string.
# 2. Check empty validation level for validation encoded as
#    Mysqlx.Datatypes.Object.

#
# I.1

command ok
Table	Create Table
test_coll	CREATE TABLE `test_coll` (
  `doc` json DEFAULT NULL,
  `_id` varbinary(32) GENERATED ALWAYS AS (json_unquote(json_extract(`doc`,_utf8mb4'$._id'))) STORED NOT NULL,
  `_json_schema` json GENERATED ALWAYS AS (_utf8mb4'{"id":"http://json-schema.org/geo","$schema":"http://json-schema.org/draft-06/schema#","description":"A geographical coordinate","type":"object","properties":{"latitude":{"type":"number","minimum":-90,"maximum":90},"longitude":{"type":"number","minimum":-180,"maximum":180}},"required":["latitude","longitude"]}') VIRTUAL,
  PRIMARY KEY (`_id`),
  CONSTRAINT `$val_strict_889D1D09A01D7029B101675FBE738D1897A37F32` CHECK (json_schema_valid(`_json_schema`,`doc`))
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
0 rows affected
Got expected error:
Mysqlx.Error {
  severity: ERROR
  code: 5180
  msg: "Document is not valid according to the schema assigned to collection. The JSON document location \'#/latitude\' failed requirement \'maximum\' at JSON Schema location \'#/properties/latitude\'."
  sql_state: "HY000"
}


1 rows affected
Got expected error:
Mysqlx.Error {
  severity: ERROR
  code: 5180
  msg: "Document is not valid according to the schema assigned to collection. The JSON document location \'#/latitude\' failed requirement \'maximum\' at JSON Schema location \'#/properties/latitude\'."
  sql_state: "HY000"
}


1 rows affected
Rows matched: 1  Changed: 1  Warnings: 0
doc	_id	_json_schema
{"_id": "one", "latitude": 72, "longitude": 54}	one	{"id": "http://json-schema.org/geo", "type": "object", "$schema": "http://json-schema.org/draft-06/schema#", "required": ["latitude", "longitude"], "properties": {"latitude": {"type": "number", "maximum": 90, "minimum": -90}, "longitude": {"type": "number", "maximum": 180, "minimum": -180}}, "description": "A geographical coordinate"}
0 rows affected

0 rows affected

#
# I.2

command ok
Table	Create Table
test_coll	CREATE TABLE `test_coll` (
  `doc` json DEFAULT NULL,
  `_id` varbinary(32) GENERATED ALWAYS AS (json_unquote(json_extract(`doc`,_utf8mb4'$._id'))) STORED NOT NULL,
  `_json_schema` json GENERATED ALWAYS AS (_utf8mb4'{"id":"http://json-schema.org/geo","$schema":"http://json-schema.org/draft-06/schema#","description":"A geographical coordinate","type":"object","properties":{"latitude":{"type":"number","minimum":-90,"maximum":90},"longitude":{"type":"number","minimum":-180,"maximum":180}},"required":["latitude","longitude"]}') VIRTUAL,
  PRIMARY KEY (`_id`),
  CONSTRAINT `$val_strict_889D1D09A01D7029B101675FBE738D1897A37F32` CHECK (json_schema_valid(`_json_schema`,`doc`))
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
0 rows affected
Got expected error:
Mysqlx.Error {
  severity: ERROR
  code: 5180
  msg: "Document is not valid according to the schema assigned to collection. The JSON document location \'#/latitude\' failed requirement \'maximum\' at JSON Schema location \'#/properties/latitude\'."
  sql_state: "HY000"
}


1 rows affected
Got expected error:
Mysqlx.Error {
  severity: ERROR
  code: 5180
  msg: "Document is not valid according to the schema assigned to collection. The JSON document location \'#/latitude\' failed requirement \'maximum\' at JSON Schema location \'#/properties/latitude\'."
  sql_state: "HY000"
}


1 rows affected
Rows matched: 1  Changed: 1  Warnings: 0
doc	_id	_json_schema
{"_id": "one", "latitude": 72, "longitude": 54}	one	{"id": "http://json-schema.org/geo", "type": "object", "$schema": "http://json-schema.org/draft-06/schema#", "required": ["latitude", "longitude"], "properties": {"latitude": {"type": "number", "maximum": 90, "minimum": -90}, "longitude": {"type": "number", "maximum": 180, "minimum": -180}}, "description": "A geographical coordinate"}
0 rows affected

0 rows affected

#
# I.3

command ok

1 rows affected

1 rows affected
doc	_id	_json_schema
{"_id": "one", "latitude": 3312}	one	{"id": "http://json-schema.org/geo", "type": "object", "$schema": "http://json-schema.org/draft-06/schema#", "required": ["latitude", "longitude"], "properties": {"latitude": {"type": "number", "maximum": 90, "minimum": -90}, "longitude": {"type": "number", "maximum": 180, "minimum": -180}}, "description": "A geographical coordinate"}
{"_id": "two", "latitude": 45, "longitude": 54}	two	{"id": "http://json-schema.org/geo", "type": "object", "$schema": "http://json-schema.org/draft-06/schema#", "required": ["latitude", "longitude"], "properties": {"latitude": {"type": "number", "maximum": 90, "minimum": -90}, "longitude": {"type": "number", "maximum": 180, "minimum": -180}}, "description": "A geographical coordinate"}
0 rows affected

0 rows affected

#
# I.4

command ok
Table	Create Table
test_coll	CREATE TABLE `test_coll` (
  `doc` json DEFAULT NULL,
  `_id` varbinary(32) GENERATED ALWAYS AS (json_unquote(json_extract(`doc`,_utf8mb4'$._id'))) STORED NOT NULL,
  `_json_schema` json GENERATED ALWAYS AS (_utf8mb4'{"description":"t`y\\p"e o`b`\\\\c""t"}') VIRTUAL,
  PRIMARY KEY (`_id`),
  CONSTRAINT `$val_strict_889D1D09A01D7029B101675FBE738D1897A37F32` CHECK (json_schema_valid(`_json_schema`,`doc`))
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
0 rows affected

0 rows affected

#
# II.1.a

command ok

0 rows affected

command ok

0 rows affected

#
# II.1.b

command ok

0 rows affected

command ok

0 rows affected

#
# II.2.a

command ok

0 rows affected

command ok

0 rows affected

#
# II.2.b

command ok

0 rows affected

command ok

0 rows affected

#
# II.3.a

command ok

0 rows affected

#
# II.3.b

command ok

0 rows affected

#
# II.4.a
Got expected error:
Mysqlx.Error {
  severity: ERROR
  code: 5017
  msg: "Invalid \"validation.level\" argument. Allowed values are \'OFF\' and \'STRICT\'"
  sql_state: "HY000"
}


#
# II.4.b
Got expected error:
Mysqlx.Error {
  severity: ERROR
  code: 5017
  msg: "Invalid \"validation.level\" argument. Allowed values are \'OFF\' and \'STRICT\'"
  sql_state: "HY000"
}


#
# II.5
Got expected error:
Mysqlx.Error {
  severity: ERROR
  code: 5021
  msg: "\'foo\' is not a valid field for create_collection command"
  sql_state: "HY000"
}


#
# III

command ok
Table	Create Table
test_coll	CREATE TABLE `test_coll` (
  `doc` json DEFAULT NULL,
  `_id` varbinary(32) GENERATED ALWAYS AS (json_unquote(json_extract(`doc`,_utf8mb4'$._id'))) STORED NOT NULL,
  `_json_schema` json GENERATED ALWAYS AS (_utf8mb4'{"type":"object"}') VIRTUAL,
  PRIMARY KEY (`_id`),
  CONSTRAINT `$val_strict_889D1D09A01D7029B101675FBE738D1897A37F32` CHECK (json_schema_valid(`_json_schema`,`doc`)) /*!80016 NOT ENFORCED */
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
0 rows affected

0 rows affected

#
# IV.1

command ok
Table	Create Table
test_coll	CREATE TABLE `test_coll` (
  `doc` json DEFAULT NULL,
  `_id` varbinary(32) GENERATED ALWAYS AS (json_unquote(json_extract(`doc`,_utf8mb4'$._id'))) STORED NOT NULL,
  `_json_schema` json GENERATED ALWAYS AS (_utf8mb4'{}') VIRTUAL,
  PRIMARY KEY (`_id`),
  CONSTRAINT `$val_strict_889D1D09A01D7029B101675FBE738D1897A37F32` CHECK (json_schema_valid(`_json_schema`,`doc`))
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
0 rows affected

0 rows affected

#
# IV.2

command ok
Table	Create Table
test_coll	CREATE TABLE `test_coll` (
  `doc` json DEFAULT NULL,
  `_id` varbinary(32) GENERATED ALWAYS AS (json_unquote(json_extract(`doc`,_utf8mb4'$._id'))) STORED NOT NULL,
  `_json_schema` json GENERATED ALWAYS AS (_utf8mb4'{}') VIRTUAL,
  PRIMARY KEY (`_id`),
  CONSTRAINT `$val_strict_889D1D09A01D7029B101675FBE738D1897A37F32` CHECK (json_schema_valid(`_json_schema`,`doc`))
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
0 rows affected

0 rows affected

#
# V.1
Got expected error:
Mysqlx.Error {
  severity: ERROR
  code: 5021
  msg: "\'foo\' is not a valid field for ensure_collection command"
  sql_state: "HY000"
}


#
# V.2
Got expected error:
Mysqlx.Error {
  severity: ERROR
  code: 5021
  msg: "\'foo\' is not a valid field for ensure_collection command"
  sql_state: "HY000"
}


#
# V.3
Got expected error:
Mysqlx.Error {
  severity: ERROR
  code: 5016
  msg: "Invalid data type for argument \'options\', expected \'object\' type"
  sql_state: "HY000"
}


#
# VI.1

command ok

1 rows affected
Got expected error: Document is not valid according to the schema assigned to collection. The JSON document location '#' failed requirement 'required' at JSON Schema location '#'. (code 5180)

0 rows affected

#
# VI.2

command ok

1 rows affected
Got expected error: Document is not valid according to the schema assigned to collection. The JSON document location '#' failed requirement 'required' at JSON Schema location '#'. (code 5180)

0 rows affected
Mysqlx.Ok {
  msg: "bye!"
}
ok


## I. Check that validation schema do not affect how ensure_collection
##    establishes whether to create a collection.
#
# 1. Try to create two collections with same name and identical
#    validation schema.
# 2. Try to create two collections with same name and different
#    validation schema.
# 3. Try to create two collections with same name, one containing
#    custom validation schema and the next without a validation schema.
# 4. Try to create two collections with different name and identical
#    validation schema.
# 5. Try to create two collections with different name and different
#    validation schema.

#
# I.1

command ok

command ok
RUN show create table xtest.test_coll
Table	Create Table
test_coll	CREATE TABLE `test_coll` (
  `doc` json DEFAULT NULL,
  `_id` varbinary(32) GENERATED ALWAYS AS (json_unquote(json_extract(`doc`,_utf8mb4'$._id'))) STORED NOT NULL,
  `_json_schema` json GENERATED ALWAYS AS (_utf8mb4'{"description":"bar"}') VIRTUAL,
  PRIMARY KEY (`_id`),
  CONSTRAINT `$val_strict_889D1D09A01D7029B101675FBE738D1897A37F32` CHECK (json_schema_valid(`_json_schema`,`doc`))
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
0 rows affected

0 rows affected

#
# I.2

command ok

command ok
RUN show create table xtest.test_coll
Table	Create Table
test_coll	CREATE TABLE `test_coll` (
  `doc` json DEFAULT NULL,
  `_id` varbinary(32) GENERATED ALWAYS AS (json_unquote(json_extract(`doc`,_utf8mb4'$._id'))) STORED NOT NULL,
  `_json_schema` json GENERATED ALWAYS AS (_utf8mb4'{"description":"foo"}') VIRTUAL,
  PRIMARY KEY (`_id`),
  CONSTRAINT `$val_strict_889D1D09A01D7029B101675FBE738D1897A37F32` CHECK (json_schema_valid(`_json_schema`,`doc`))
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
0 rows affected

0 rows affected

#
# I.3

command ok
Table	Create Table
test_coll	CREATE TABLE `test_coll` (
  `doc` json DEFAULT NULL,
  `_id` varbinary(32) GENERATED ALWAYS AS (json_unquote(json_extract(`doc`,_utf8mb4'$._id'))) STORED NOT NULL,
  `_json_schema` json GENERATED ALWAYS AS (_utf8mb4'{"description":"custom schema"}') VIRTUAL,
  PRIMARY KEY (`_id`),
  CONSTRAINT `$val_strict_889D1D09A01D7029B101675FBE738D1897A37F32` CHECK (json_schema_valid(`_json_schema`,`doc`))
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
0 rows affected

command ok
Table	Create Table
test_coll	CREATE TABLE `test_coll` (
  `doc` json DEFAULT NULL,
  `_id` varbinary(32) GENERATED ALWAYS AS (json_unquote(json_extract(`doc`,_utf8mb4'$._id'))) STORED NOT NULL,
  `_json_schema` json GENERATED ALWAYS AS (_utf8mb4'{"type":"object"}') VIRTUAL,
  PRIMARY KEY (`_id`),
  CONSTRAINT `$val_strict_889D1D09A01D7029B101675FBE738D1897A37F32` CHECK (json_schema_valid(`_json_schema`,`doc`)) /*!80016 NOT ENFORCED */
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
0 rows affected

0 rows affected

#
# I.4

command ok

command ok

0 rows affected

0 rows affected

#
# I.5

command ok

command ok

0 rows affected

0 rows affected
Mysqlx.Ok {
  msg: "bye!"
}
ok

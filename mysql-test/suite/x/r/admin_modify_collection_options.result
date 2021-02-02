create schema xtest;


## I. Try to modify non existing collection.

## II. Check modify_collection_options with invalid validation field.
#
# 1. Empty validation field given.
# 2. No validation field given.
# 3. Validation contains only unknown fields.
# 4. Validation contains valid fields along with unknown fields.
# 5. Options contain invalid field.

## III. Check modify_collection_options command with the validation
#       level field set.
#
# 1. Test with invalid validation level value.
# 2. Test turning on the validation.
# 3. Test turning off the validation.
# 4. Check case insensitive values for validation level setting.

## IV. Check modify_collection_options command with the validation
#      schema field set.
#
# 1. Test containing invalid schema field.
# 2. Test with empty schema field.
# 3. Replace default validation schema with one provided by a user.
# 4. Set validation schema encoded as protobuf string.
# 5. Set validation schema encoded as Mysqlx.Datatypes.Object.
# 6. Check special characters handling inside a validation schema.

## V. Check modify_collection_options command with both validation
#     schema and validation level fields set.
#
# 1. Set validation schema encoded as protobuf string and validation
#    level set to off.
# 2. Set validation schema encoded as Mysqlx.Datatypes.Object and
#    validation level set to strict.
# 3. Check modify_collection_options with custom schema and invalid
#    level field value.

## VI. Check modify_collection_options command when modification fails.
#
# 1. Insert some invalid data when validation is disabled, after that
#    modification must fail. Fix the offending row and modification
#    should pass this time.
# 2. Modification fails on existing data, update offending row so that
#    it will be in line with the new schema and try again, this time
#    modify_collumn_options must pass.
# 3. Insert invalid data when validation is disabled, modify collection
#    using new schema which would treat existing data as valid and
#    enabling the validation.

## VII. Check Mysqlx_stmt_modify_collection_options variable.

#
# I

0 rows affected
Warnings generated:
NOTE | 1051 | Unknown table 'xtest.tab'
Got expected error:
Mysqlx.Error {
  severity: ERROR
  code: 1146
  msg: "Table \'xtest.tab\' doesn\'t exist"
  sql_state: "42S02"
}


#
# II.1
Got expected error:
Mysqlx.Error {
  severity: ERROR
  code: 5020
  msg: "Arguments value used under \"validation\" must be an object with at least one field"
  sql_state: "HY000"
}


#
# II.2
Got expected error:
Mysqlx.Error {
  severity: ERROR
  code: 5016
  msg: "Invalid data type for argument \'options\', expected \'object\' type"
  sql_state: "HY000"
}


#
# II.3
Got expected error:
Mysqlx.Error {
  severity: ERROR
  code: 5021
  msg: "\'foo\' is not a valid field for modify_collection_options command"
  sql_state: "HY000"
}


#
# II.4
Got expected error:
Mysqlx.Error {
  severity: ERROR
  code: 5021
  msg: "\'foo\' is not a valid field for modify_collection_options command"
  sql_state: "HY000"
}


#
# II.5
Got expected error:
Mysqlx.Error {
  severity: ERROR
  code: 5021
  msg: "\'foo\' is not a valid field for modify_collection_options command"
  sql_state: "HY000"
}


#
# III.1
Got expected error:
Mysqlx.Error {
  severity: ERROR
  code: 5017
  msg: "Invalid \"validation.level\" argument. Allowed values are \'OFF\' and \'STRICT\'"
  sql_state: "HY000"
}


#
# III.2

command ok

command ok

0 rows affected

#
# III.3

command ok

command ok

0 rows affected

#
# III.4

command ok

command ok

0 rows affected

#
# IV.1

command ok
Got expected error:
Mysqlx.Error {
  severity: ERROR
  code: 5182
  msg: "Validation schema is not a valid JSON document"
  sql_state: "HY000"
}


1 rows affected

command ok

1 rows affected

0 rows affected

#
# IV.2

command ok

command ok
Got expected error:
Mysqlx.Error {
  severity: ERROR
  code: 3140
  msg: "Invalid JSON text: \"The document is empty.\" at position 0 in value for column \'test_coll._json_schema\'."
  sql_state: "22032"
}


0 rows affected

#
# IV.3

command ok
GENERATION_EXPRESSION
_utf8mb4\'{"type":"object"}\'
0 rows affected

command ok
GENERATION_EXPRESSION
_utf8mb4\'{"id":"some id","description":"modified"}\'
0 rows affected

0 rows affected

#
# IV.4

command ok
GENERATION_EXPRESSION
_utf8mb4\'{"description":"base"}\'
0 rows affected

command ok
GENERATION_EXPRESSION
_utf8mb4\'{"description":"modified"}\'
0 rows affected

0 rows affected

#
# IV.5

command ok

command ok
GENERATION_EXPRESSION
_utf8mb4\'{"description":"modified"}\'
0 rows affected

0 rows affected

#
# IV.6

command ok

command ok
GENERATION_EXPRESSION
_utf8mb4\'{"description":"fo"o`b@\\\\"}\'
0 rows affected

0 rows affected

#
# V.1

command ok

command ok
GENERATION_EXPRESSION
_utf8mb4\'{"description":"test"}\'
0 rows affected

0 rows affected

#
# V.2

command ok

command ok
GENERATION_EXPRESSION
_utf8mb4\'{"description":"test"}\'
0 rows affected

0 rows affected

#
# V.3
Got expected error:
Mysqlx.Error {
  severity: ERROR
  code: 5017
  msg: "Invalid \"validation.level\" argument. Allowed values are \'OFF\' and \'STRICT\'"
  sql_state: "HY000"
}


#
# VI.1

command ok

1 rows affected

1 rows affected
Got expected error:
Mysqlx.Error {
  severity: ERROR
  code: 5180
  msg: "Document is not valid according to the schema assigned to collection. The JSON document location \'#/latitude\' failed requirement \'maximum\' at JSON Schema location \'#/properties/latitude\'."
  sql_state: "HY000"
}


1 rows affected

command ok

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
Got expected error:
Mysqlx.Error {
  severity: ERROR
  code: 5180
  msg: "Document is not valid according to the schema assigned to collection. The JSON document location \'#/latitude\' failed requirement \'maximum\' at JSON Schema location \'#/properties/latitude\'."
  sql_state: "HY000"
}


0 rows affected

#
# VI.2

command ok

1 rows affected

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

command ok
Table	Create Table
test_coll	CREATE TABLE `test_coll` (
  `doc` json DEFAULT NULL,
  `_id` varbinary(32) GENERATED ALWAYS AS (json_unquote(json_extract(`doc`,_utf8mb4'$._id'))) STORED NOT NULL,
  `_json_schema` json GENERATED ALWAYS AS (_utf8mb4'{"type":"object","properties":{"latitude":{"type":"number","maximum":50}},"required":["latitude"]}') VIRTUAL,
  PRIMARY KEY (`_id`),
  CONSTRAINT `$val_strict_889D1D09A01D7029B101675FBE738D1897A37F32` CHECK (json_schema_valid(`_json_schema`,`doc`))
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
0 rows affected

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
Got expected error:
Mysqlx.Error {
  severity: ERROR
  code: 5180
  msg: "Document is not valid according to the schema assigned to collection. The JSON document location \'#/latitude\' failed requirement \'maximum\' at JSON Schema location \'#/properties/latitude\'."
  sql_state: "HY000"
}


0 rows affected

#
# VI.3

command ok

1 rows affected

1 rows affected

command ok

1 rows affected
Got expected error:
Mysqlx.Error {
  severity: ERROR
  code: 5180
  msg: "Document is not valid according to the schema assigned to collection. The JSON document location \'#/latitude\' failed requirement \'maximum\' at JSON Schema location \'#/properties/latitude\'."
  sql_state: "HY000"
}


0 rows affected

#
# VII
connecting...
active session is now 'temp'

command ok
Verify [Status variable "Mysqlx_stmt_modify_collection_options" needs to have a value of 0]

command ok
Verify [Status variable "Mysqlx_stmt_modify_collection_options" needs to have a value of 1]
Got expected error:
Mysqlx.Error {
  severity: ERROR
  code: 5017
  msg: "Invalid \"validation.level\" argument. Allowed values are \'OFF\' and \'STRICT\'"
  sql_state: "HY000"
}

Verify [Status variable "Mysqlx_stmt_modify_collection_options" needs to have a value of 2]

command ok
Verify [Status variable "Mysqlx_stmt_modify_collection_options" needs to have a value of 3]

0 rows affected
closing session temp
Mysqlx.Ok {
  msg: "bye!"
}
switched to session default
Mysqlx.Ok {
  msg: "bye!"
}
ok

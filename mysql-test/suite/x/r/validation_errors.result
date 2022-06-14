create schema xtest;


## I. Check validation errors on collection insert.
#
# 1. Insert one invalid value to an empty collection.
# 2. Insert one invalid value to a non-empty collection.
# 3. Insert multiple values where one is invalid.
# 4. Insert multiple invalid values.

## II. Check validation errors on collection update.
# 1. Update one element in a collection.
# 2. Update multiple elements in a collection.

## III. Check validation errors on schema enable.
# 1. Enable schema for collection with one element not matching the
#    schema.
# 2. Enable schema for collection with multiple elements not matching
#    the schema.

## IV. Check validation errors on schema update.
# 1. Update schema for collection with one element not matching the
#    new schema.
# 2. Update schema for collection with multiple elements not matching
#    the new schema.

#
# I.1

command ok
Mysqlx.Error {
  severity: ERROR
  code: 5180
  msg: "Document is not valid according to the schema assigned to collection. The JSON document location \'#/size\' failed requirement \'maximum\' at JSON Schema location \'#/properties/size\'."
  sql_state: "HY000"
}


0 rows affected

#
# I.2

command ok

1 rows affected

1 rows affected

1 rows affected
Mysqlx.Error {
  severity: ERROR
  code: 5180
  msg: "Document is not valid according to the schema assigned to collection. The JSON document location \'#/size\' failed requirement \'minimum\' at JSON Schema location \'#/properties/size\'."
  sql_state: "HY000"
}


0 rows affected

#
# I.3

command ok
Mysqlx.Error {
  severity: ERROR
  code: 5180
  msg: "Document is not valid according to the schema assigned to collection. The JSON document location \'#/size\' failed requirement \'minimum\' at JSON Schema location \'#/properties/size\'."
  sql_state: "HY000"
}


0 rows affected

#
# I.4

command ok
Mysqlx.Error {
  severity: ERROR
  code: 5180
  msg: "Document is not valid according to the schema assigned to collection. The JSON document location \'#/size\' failed requirement \'maximum\' at JSON Schema location \'#/properties/size\'."
  sql_state: "HY000"
}


0 rows affected

#
# II.1

command ok

1 rows affected

1 rows affected

1 rows affected
Mysqlx.Error {
  severity: ERROR
  code: 5180
  msg: "Document is not valid according to the schema assigned to collection. The JSON document location \'#/size\' failed requirement \'minimum\' at JSON Schema location \'#/properties/size\'."
  sql_state: "HY000"
}


0 rows affected

#
# II.2

command ok

1 rows affected

1 rows affected

1 rows affected
Mysqlx.Error {
  severity: ERROR
  code: 5180
  msg: "Document is not valid according to the schema assigned to collection. The JSON document location \'#/size\' failed requirement \'minimum\' at JSON Schema location \'#/properties/size\'."
  sql_state: "HY000"
}


0 rows affected

#
# III.1

command ok

1 rows affected

1 rows affected

1 rows affected
Mysqlx.Error {
  severity: ERROR
  code: 5180
  msg: "Document is not valid according to the schema assigned to collection. The JSON document location \'#/size\' failed requirement \'minimum\' at JSON Schema location \'#/properties/size\'."
  sql_state: "HY000"
}


0 rows affected

#
# III.2

command ok

1 rows affected

1 rows affected

1 rows affected
Mysqlx.Error {
  severity: ERROR
  code: 5180
  msg: "Document is not valid according to the schema assigned to collection. The JSON document location \'#/size\' failed requirement \'maximum\' at JSON Schema location \'#/properties/size\'."
  sql_state: "HY000"
}


0 rows affected

#
# IV.1

command ok

1 rows affected

1 rows affected

1 rows affected
Mysqlx.Error {
  severity: ERROR
  code: 5180
  msg: "Document is not valid according to the schema assigned to collection. The JSON document location \'#/size\' failed requirement \'minimum\' at JSON Schema location \'#/properties/size\'."
  sql_state: "HY000"
}


0 rows affected

#
# IV.2

command ok

1 rows affected

1 rows affected

1 rows affected
Mysqlx.Error {
  severity: ERROR
  code: 5180
  msg: "Document is not valid according to the schema assigned to collection. The JSON document location \'#/size\' failed requirement \'minimum\' at JSON Schema location \'#/properties/size\'."
  sql_state: "HY000"
}


0 rows affected
Mysqlx.Ok {
  msg: "bye!"
}
ok

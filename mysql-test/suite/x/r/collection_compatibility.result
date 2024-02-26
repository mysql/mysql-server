create schema xtest default charset 'utf8mb4';


## I. Create collection with respect to old style collections.
#
# 1. Create collection when there is an old collection with same name.
# 2. Create collection when there is an old collection with different
#    name.

## II. Create collection using ensure_collection with respect to old
##     style collections.
#
# 1. Create collection when there is an old collection with same name.

## III. List collection objects created with and without validation
##      schema
#
# 1. List just old style collection.
# 2. List old and new collection types.
# 3. List old and new collection types with regular table.
# 4. Drop old collection and list new collection type and a regular
#    table.

## IV. Call modify_collection_options on a schema created without
##     a schema validation
#
# 1. Modify old collection by trying to change only the validation
#    level.
# 2. Modify old collection by trying to change only the validation
#    schema.
# 3. Modify old collection by trying to change both validation schema
#    and level.
# 4. Modify old collection containing data that is not valid according
#    to the new schema,
#    modification must fail and old table layout should not be changed.
#    Update the
#    faulty value, modification must pass and layout should be updated.

## V. Call get_collection_options on a schema created withoud a schema
##    validation.
#
# 1. Fetch valid option.
# 1. Fetch invalid option.
RUN USE xtest;

0 rows affected

#
# I.1
RUN CREATE TABLE xtest.foobar (doc JSON, _id VARCHAR(32) GENERATED ALWAYS AS
 (JSON_UNQUOTE(JSON_EXTRACT(doc, '$._id'))) STORED PRIMARY KEY)
 CHARSET utf8mb4 ENGINE=InnoDB

0 rows affected
Got expected error:
Mysqlx.Error {
  severity: ERROR
  code: 1050
  msg: "Table \'foobar\' already exists"
  sql_state: "42S01"
}

Got expected error:
Mysqlx.Error {
  severity: ERROR
  code: 1050
  msg: "Table \'foobar\' already exists"
  sql_state: "42S01"
}

RUN SHOW CREATE TABLE xtest.foobar
Table	Create Table
foobar	CREATE TABLE `foobar` (
  `doc` json DEFAULT NULL,
  `_id` varchar(32) GENERATED ALWAYS AS (json_unquote(json_extract(`doc`,_utf8mb4'$._id'))) STORED NOT NULL,
  PRIMARY KEY (`_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
0 rows affected
RUN DROP TABLE IF EXISTS xtest.foobar

0 rows affected

#
# I.2
RUN CREATE TABLE xtest.foo (doc JSON, _id VARCHAR(32) GENERATED ALWAYS AS
 (JSON_UNQUOTE(JSON_EXTRACT(doc, '$._id'))) STORED PRIMARY KEY)
 CHARSET utf8mb4 ENGINE=InnoDB

0 rows affected

command ok
RUN SHOW CREATE TABLE xtest.foo
Table	Create Table
foo	CREATE TABLE `foo` (
  `doc` json DEFAULT NULL,
  `_id` varchar(32) GENERATED ALWAYS AS (json_unquote(json_extract(`doc`,_utf8mb4'$._id'))) STORED NOT NULL,
  PRIMARY KEY (`_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
0 rows affected
RUN SHOW CREATE TABLE xtest.bar
Table	Create Table
bar	CREATE TABLE `bar` (
  `doc` json DEFAULT NULL,
  `_id` varbinary(32) GENERATED ALWAYS AS (json_unquote(json_extract(`doc`,_utf8mb4'$._id'))) STORED NOT NULL,
  `_json_schema` json GENERATED ALWAYS AS (_utf8mb4'{"type":"object"}') VIRTUAL,
  PRIMARY KEY (`_id`),
  CONSTRAINT `$val_strict_E8D46CE25265E545D225A8A6F1BAF642FEBEE5CB` CHECK (json_schema_valid(`_json_schema`,`doc`))
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
0 rows affected
RUN DROP TABLE IF EXISTS xtest.foo

0 rows affected
RUN DROP TABLE IF EXISTS xtest.bar

0 rows affected

#
# II.1
RUN CREATE TABLE xtest.foobar (doc JSON, _id VARCHAR(32) GENERATED ALWAYS AS
 (JSON_UNQUOTE(JSON_EXTRACT(doc, '$._id'))) STORED PRIMARY KEY)
 CHARSET utf8mb4 ENGINE=InnoDB

0 rows affected

command ok

command ok

command ok
RUN SHOW CREATE TABLE xtest.foobar
Table	Create Table
foobar	CREATE TABLE `foobar` (
  `doc` json DEFAULT NULL,
  `_id` varchar(32) GENERATED ALWAYS AS (json_unquote(json_extract(`doc`,_utf8mb4'$._id'))) STORED NOT NULL,
  `_json_schema` json GENERATED ALWAYS AS (_utf8mb4'{"type":"object"}') VIRTUAL,
  PRIMARY KEY (`_id`),
  CONSTRAINT `$val_strict_9B500343BC52E2911172EB52AE5CF4847604C6E5` CHECK (json_schema_valid(`_json_schema`,`doc`)) /*!80016 NOT ENFORCED */
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
0 rows affected
RUN DROP TABLE IF EXISTS xtest.foobar

0 rows affected

#
# III.1
RUN CREATE TABLE xtest.old_coll (doc JSON, _id VARCHAR(32) GENERATED ALWAYS AS
 (JSON_UNQUOTE(JSON_EXTRACT(doc, '$._id'))) STORED PRIMARY KEY)
 CHARSET utf8mb4 ENGINE=InnoDB

0 rows affected
name	type
old_coll	COLLECTION
command ok

#
# III.2

command ok
name	type
new_coll2	COLLECTION
old_coll	COLLECTION
command ok

command ok
name	type
new_coll2	COLLECTION
new_coll3	COLLECTION
old_coll	COLLECTION
command ok

#
# III.3
RUN CREATE TABLE xtest.regular_tab (doc JSON);

0 rows affected
name	type
new_coll2	COLLECTION
new_coll3	COLLECTION
old_coll	COLLECTION
regular_tab	TABLE
command ok

#
# III.4
RUN DROP TABLE IF EXISTS xtest.old_coll

0 rows affected
name	type
new_coll2	COLLECTION
new_coll3	COLLECTION
regular_tab	TABLE
command ok

0 rows affected

#
# IV.1
RUN CREATE TABLE xtest.old_coll (doc JSON, _id VARCHAR(32) GENERATED ALWAYS AS
 (JSON_UNQUOTE(JSON_EXTRACT(doc, '$._id'))) STORED PRIMARY KEY)
 CHARSET utf8mb4 ENGINE=InnoDB

0 rows affected
RUN SELECT column_name FROM information_schema.columns WHERE table_name = 'old_coll' ORDER BY column_name ASC
COLUMN_NAME
_id
doc
0 rows affected
send Mysqlx.Crud.Insert {
  collection {
    name: "old_coll"
    schema: "xtest"
  }
  data_model: DOCUMENT
  row {
    field {
      type: LITERAL
      literal {
        type: V_STRING
        v_string {
          value: "{\"_id\": \"1\", \"name\": \"Robb\", \"amount\": 125.21}"
        }
      }
    }
  }
}


1 rows affected

command ok
RUN SELECT column_name FROM information_schema.columns WHERE table_name = 'old_coll' ORDER BY column_name ASC
COLUMN_NAME
_id
_json_schema
doc
0 rows affected
RUN SELECT enforced FROM information_schema.table_constraints WHERE  constraint_schema = "xtest" AND table_name ='old_coll' AND  constraint_name LIKE '$val_strict_%'
RUN DROP TABLE IF EXISTS xtest.old_coll

0 rows affected

#
# IV.2
RUN CREATE TABLE xtest.old_coll (doc JSON, _id VARCHAR(32) GENERATED ALWAYS AS
 (JSON_UNQUOTE(JSON_EXTRACT(doc, '$._id'))) STORED PRIMARY KEY)
 CHARSET utf8mb4 ENGINE=InnoDB

0 rows affected
RUN SELECT column_name FROM information_schema.columns WHERE table_name = 'old_coll' ORDER BY column_name ASC
COLUMN_NAME
_id
doc
0 rows affected
send Mysqlx.Crud.Insert {
  collection {
    name: "old_coll"
    schema: "xtest"
  }
  data_model: DOCUMENT
  row {
    field {
      type: LITERAL
      literal {
        type: V_STRING
        v_string {
          value: "{\"_id\": \"1\", \"name\": \"Robb\", \"amount\": 125.21}"
        }
      }
    }
  }
}


1 rows affected

command ok
RUN SELECT column_name FROM information_schema.columns WHERE table_name = 'old_coll' ORDER BY column_name ASC
COLUMN_NAME
_id
_json_schema
doc
0 rows affected
RUN SELECT enforced FROM information_schema.table_constraints WHERE  constraint_schema = "xtest" AND table_name ='old_coll' AND  constraint_name LIKE '$val_strict_%'
RUN DROP TABLE IF EXISTS xtest.old_coll

0 rows affected

#
# IV.3
RUN CREATE TABLE xtest.old_coll (doc JSON, _id VARCHAR(32) GENERATED ALWAYS AS
 (JSON_UNQUOTE(JSON_EXTRACT(doc, '$._id'))) STORED PRIMARY KEY)
 CHARSET utf8mb4 ENGINE=InnoDB

0 rows affected
RUN SELECT column_name FROM information_schema.columns WHERE table_name = 'old_coll' ORDER BY column_name ASC
COLUMN_NAME
_id
doc
0 rows affected
send Mysqlx.Crud.Insert {
  collection {
    name: "old_coll"
    schema: "xtest"
  }
  data_model: DOCUMENT
  row {
    field {
      type: LITERAL
      literal {
        type: V_STRING
        v_string {
          value: "{\"_id\": \"1\", \"name\": \"Robb\", \"amount\": 125.21}"
        }
      }
    }
  }
}


1 rows affected

command ok
RUN SELECT column_name FROM information_schema.columns WHERE table_name = 'old_coll' ORDER BY column_name ASC
COLUMN_NAME
_id
_json_schema
doc
0 rows affected
RUN SELECT enforced FROM information_schema.table_constraints WHERE  constraint_schema = "xtest" AND table_name ='old_coll' AND  constraint_name LIKE '$val_strict_%'
RUN DROP TABLE IF EXISTS xtest.old_coll

0 rows affected

#
# IV.4
RUN CREATE TABLE xtest.old_coll (doc JSON, _id VARCHAR(32) GENERATED ALWAYS AS
 (JSON_UNQUOTE(JSON_EXTRACT(doc, '$._id'))) STORED PRIMARY KEY)
 CHARSET utf8mb4 ENGINE=InnoDB

0 rows affected
RUN SELECT column_name FROM information_schema.columns WHERE table_name = 'old_coll' ORDER BY column_name ASC
COLUMN_NAME
_id
doc
0 rows affected
send Mysqlx.Crud.Insert {
  collection {
    name: "old_coll"
    schema: "xtest"
  }
  data_model: DOCUMENT
  row {
    field {
      type: LITERAL
      literal {
        type: V_STRING
        v_string {
          value: "{\"_id\": \"1\", \"name\": \"Robb\", \"amount\": 125.21}"
        }
      }
    }
  }
}


1 rows affected
send Mysqlx.Crud.Insert {
  collection {
    name: "old_coll"
    schema: "xtest"
  }
  data_model: DOCUMENT
  row {
    field {
      type: LITERAL
      literal {
        type: V_STRING
        v_string {
          value: "{\"_id\": \"2\", \"name\": \"Bob\", \"amount\": 11.19}"
        }
      }
    }
  }
}


1 rows affected
send Mysqlx.Crud.Insert {
  collection {
    name: "old_coll"
    schema: "xtest"
  }
  data_model: DOCUMENT
  row {
    field {
      type: LITERAL
      literal {
        type: V_STRING
        v_string {
          value: "{\"_id\": \"3\", \"name\": \"John\", \"amount\": 223.17}"
        }
      }
    }
  }
}


1 rows affected

command ok
Got expected error:
Mysqlx.Error {
  severity: ERROR
  code: 5180
  msg: "Document is not valid according to the schema assigned to collection. The JSON document location \'#/amount\' failed requirement \'minimum\' at JSON Schema location \'#/properties/amount\'."
  sql_state: "HY000"
}

send Mysqlx.Crud.Update {
  collection {
    name: "old_coll"
    schema: "xtest"
  }
  data_model: DOCUMENT
  criteria {
    type: OPERATOR
    operator {
      name: "=="
      param {
        type: IDENT
        identifier {
          document_path {
            type: MEMBER
            value: "amount"
          }
        }
      }
      param {
        type: LITERAL
        literal {
          type: V_DOUBLE
          v_double: 11.19
        }
      }
    }
  }
  operation {
    source {
      document_path {
        type: MEMBER
        value: "amount"
      }
    }
    operation: ITEM_SET
    value {
      type: LITERAL
      literal {
        type: V_DOUBLE
        v_double: 200
      }
    }
  }
}


1 rows affected
Rows matched: 1  Changed: 1  Warnings: 0

command ok
RUN SELECT column_name FROM information_schema.columns WHERE table_name = 'old_coll' ORDER BY column_name ASC
COLUMN_NAME
_id
_json_schema
doc
0 rows affected
RUN DROP TABLE IF EXISTS xtest.old_coll

0 rows affected

#
# V.1
RUN CREATE TABLE xtest.old_coll (doc JSON, _id VARCHAR(32) GENERATED ALWAYS AS
 (JSON_UNQUOTE(JSON_EXTRACT(doc, '$._id'))) STORED PRIMARY KEY)
 CHARSET utf8mb4 ENGINE=InnoDB

0 rows affected
Result
{ "validation": { "level": "off", "schema": {"type": "object"} } }
command ok
RUN DROP TABLE IF EXISTS xtest.old_coll

0 rows affected

#
# V.2
RUN CREATE TABLE xtest.old_coll (doc JSON, _id VARCHAR(32) GENERATED ALWAYS AS
 (JSON_UNQUOTE(JSON_EXTRACT(doc, '$._id'))) STORED PRIMARY KEY)
 CHARSET utf8mb4 ENGINE=InnoDB

0 rows affected
Got expected error:
Mysqlx.Error {
  severity: ERROR
  code: 5181
  msg: "Requested collection option \'foo\' doesn\'t exist."
  sql_state: "HY000"
}

RUN DROP TABLE IF EXISTS xtest.old_coll

0 rows affected
Mysqlx.Ok {
  msg: "bye!"
}
ok


## I. Create collection with respect to old style collections.
#
# 1. Create collection when there is an old collection with same name.
# 2. Create collection when there is an old collection with different
#    name.

## II. Create collection using ensure_collection with respect to old
##     style collections.
#
# 1. Create collection when there is an old collection with same name.

## III. List collection objects created with and without validation
##      schema
#
# 1. List just old style collection.
# 2. List old and new collection types.
# 3. List old and new collection types with regular table.
# 4. Drop old collection and list new collection type and a regular
#    table.

## IV. Call modify_collection_options on a schema created without
##     a schema validation
#
# 1. Modify old collection by trying to change only the validation
#    level.
# 2. Modify old collection by trying to change only the validation
#    schema.
# 3. Modify old collection by trying to change both validation schema
#    and level.
# 4. Modify old collection containing data that is not valid according
#    to the new schema,
#    modification must fail and old table layout should not be changed.
#    Update the
#    faulty value, modification must pass and layout should be updated.

## V. Call get_collection_options on a schema created withoud a schema
##    validation.
#
# 1. Fetch valid option.
# 1. Fetch invalid option.
RUN USE xtest;

0 rows affected

#
# I.1
RUN CREATE TABLE xtest.foobar (doc JSON, _id VARBINARY(32) GENERATED ALWAYS AS
 (JSON_UNQUOTE(JSON_EXTRACT(doc, '$._id'))) STORED PRIMARY KEY) CHARSET utf8mb4
 ENGINE=InnoDB

0 rows affected
Got expected error:
Mysqlx.Error {
  severity: ERROR
  code: 1050
  msg: "Table \'foobar\' already exists"
  sql_state: "42S01"
}

Got expected error:
Mysqlx.Error {
  severity: ERROR
  code: 1050
  msg: "Table \'foobar\' already exists"
  sql_state: "42S01"
}

RUN SHOW CREATE TABLE xtest.foobar
Table	Create Table
foobar	CREATE TABLE `foobar` (
  `doc` json DEFAULT NULL,
  `_id` varbinary(32) GENERATED ALWAYS AS (json_unquote(json_extract(`doc`,_utf8mb4'$._id'))) STORED NOT NULL,
  PRIMARY KEY (`_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
0 rows affected
RUN DROP TABLE IF EXISTS xtest.foobar

0 rows affected

#
# I.2
RUN CREATE TABLE xtest.foo (doc JSON, _id VARBINARY(32) GENERATED ALWAYS AS
 (JSON_UNQUOTE(JSON_EXTRACT(doc, '$._id'))) STORED PRIMARY KEY) CHARSET utf8mb4
 ENGINE=InnoDB

0 rows affected

command ok
RUN SHOW CREATE TABLE xtest.foo
Table	Create Table
foo	CREATE TABLE `foo` (
  `doc` json DEFAULT NULL,
  `_id` varbinary(32) GENERATED ALWAYS AS (json_unquote(json_extract(`doc`,_utf8mb4'$._id'))) STORED NOT NULL,
  PRIMARY KEY (`_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
0 rows affected
RUN SHOW CREATE TABLE xtest.bar
Table	Create Table
bar	CREATE TABLE `bar` (
  `doc` json DEFAULT NULL,
  `_id` varbinary(32) GENERATED ALWAYS AS (json_unquote(json_extract(`doc`,_utf8mb4'$._id'))) STORED NOT NULL,
  `_json_schema` json GENERATED ALWAYS AS (_utf8mb4'{"type":"object"}') VIRTUAL,
  PRIMARY KEY (`_id`),
  CONSTRAINT `$val_strict_E8D46CE25265E545D225A8A6F1BAF642FEBEE5CB` CHECK (json_schema_valid(`_json_schema`,`doc`))
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
0 rows affected
RUN DROP TABLE IF EXISTS xtest.foo

0 rows affected
RUN DROP TABLE IF EXISTS xtest.bar

0 rows affected

#
# II.1
RUN CREATE TABLE xtest.foobar (doc JSON, _id VARBINARY(32) GENERATED ALWAYS AS
 (JSON_UNQUOTE(JSON_EXTRACT(doc, '$._id'))) STORED PRIMARY KEY) CHARSET utf8mb4
 ENGINE=InnoDB

0 rows affected

command ok

command ok

command ok
RUN SHOW CREATE TABLE xtest.foobar
Table	Create Table
foobar	CREATE TABLE `foobar` (
  `doc` json DEFAULT NULL,
  `_id` varbinary(32) GENERATED ALWAYS AS (json_unquote(json_extract(`doc`,_utf8mb4'$._id'))) STORED NOT NULL,
  `_json_schema` json GENERATED ALWAYS AS (_utf8mb4'{"type":"object"}') VIRTUAL,
  PRIMARY KEY (`_id`),
  CONSTRAINT `$val_strict_9B500343BC52E2911172EB52AE5CF4847604C6E5` CHECK (json_schema_valid(`_json_schema`,`doc`)) /*!80016 NOT ENFORCED */
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
0 rows affected
RUN DROP TABLE IF EXISTS xtest.foobar

0 rows affected

#
# III.1
RUN CREATE TABLE xtest.old_coll (doc JSON, _id VARBINARY(32) GENERATED ALWAYS AS
 (JSON_UNQUOTE(JSON_EXTRACT(doc, '$._id'))) STORED PRIMARY KEY) CHARSET utf8mb4
 ENGINE=InnoDB

0 rows affected
name	type
old_coll	COLLECTION
command ok

#
# III.2

command ok
name	type
new_coll2	COLLECTION
old_coll	COLLECTION
command ok

command ok
name	type
new_coll2	COLLECTION
new_coll3	COLLECTION
old_coll	COLLECTION
command ok

#
# III.3
RUN CREATE TABLE xtest.regular_tab (doc JSON);

0 rows affected
name	type
new_coll2	COLLECTION
new_coll3	COLLECTION
old_coll	COLLECTION
regular_tab	TABLE
command ok

#
# III.4
RUN DROP TABLE IF EXISTS xtest.old_coll

0 rows affected
name	type
new_coll2	COLLECTION
new_coll3	COLLECTION
regular_tab	TABLE
command ok

0 rows affected

#
# IV.1
RUN CREATE TABLE xtest.old_coll (doc JSON, _id VARBINARY(32) GENERATED ALWAYS AS
 (JSON_UNQUOTE(JSON_EXTRACT(doc, '$._id'))) STORED PRIMARY KEY) CHARSET utf8mb4
 ENGINE=InnoDB

0 rows affected
RUN SELECT column_name FROM information_schema.columns WHERE table_name = 'old_coll' ORDER BY column_name ASC
COLUMN_NAME
_id
doc
0 rows affected
send Mysqlx.Crud.Insert {
  collection {
    name: "old_coll"
    schema: "xtest"
  }
  data_model: DOCUMENT
  row {
    field {
      type: LITERAL
      literal {
        type: V_STRING
        v_string {
          value: "{\"_id\": \"1\", \"name\": \"Robb\", \"amount\": 125.21}"
        }
      }
    }
  }
}


1 rows affected

command ok
RUN SELECT column_name FROM information_schema.columns WHERE table_name = 'old_coll' ORDER BY column_name ASC
COLUMN_NAME
_id
_json_schema
doc
0 rows affected
RUN SELECT enforced FROM information_schema.table_constraints WHERE  constraint_schema = "xtest" AND table_name ='old_coll' AND  constraint_name LIKE '$val_strict_%'
RUN DROP TABLE IF EXISTS xtest.old_coll

0 rows affected

#
# IV.2
RUN CREATE TABLE xtest.old_coll (doc JSON, _id VARBINARY(32) GENERATED ALWAYS AS
 (JSON_UNQUOTE(JSON_EXTRACT(doc, '$._id'))) STORED PRIMARY KEY) CHARSET utf8mb4
 ENGINE=InnoDB

0 rows affected
RUN SELECT column_name FROM information_schema.columns WHERE table_name = 'old_coll' ORDER BY column_name ASC
COLUMN_NAME
_id
doc
0 rows affected
send Mysqlx.Crud.Insert {
  collection {
    name: "old_coll"
    schema: "xtest"
  }
  data_model: DOCUMENT
  row {
    field {
      type: LITERAL
      literal {
        type: V_STRING
        v_string {
          value: "{\"_id\": \"1\", \"name\": \"Robb\", \"amount\": 125.21}"
        }
      }
    }
  }
}


1 rows affected

command ok
RUN SELECT column_name FROM information_schema.columns WHERE table_name = 'old_coll' ORDER BY column_name ASC
COLUMN_NAME
_id
_json_schema
doc
0 rows affected
RUN SELECT enforced FROM information_schema.table_constraints WHERE  constraint_schema = "xtest" AND table_name ='old_coll' AND  constraint_name LIKE '$val_strict_%'
RUN DROP TABLE IF EXISTS xtest.old_coll

0 rows affected

#
# IV.3
RUN CREATE TABLE xtest.old_coll (doc JSON, _id VARBINARY(32) GENERATED ALWAYS AS
 (JSON_UNQUOTE(JSON_EXTRACT(doc, '$._id'))) STORED PRIMARY KEY) CHARSET utf8mb4
 ENGINE=InnoDB

0 rows affected
RUN SELECT column_name FROM information_schema.columns WHERE table_name = 'old_coll' ORDER BY column_name ASC
COLUMN_NAME
_id
doc
0 rows affected
send Mysqlx.Crud.Insert {
  collection {
    name: "old_coll"
    schema: "xtest"
  }
  data_model: DOCUMENT
  row {
    field {
      type: LITERAL
      literal {
        type: V_STRING
        v_string {
          value: "{\"_id\": \"1\", \"name\": \"Robb\", \"amount\": 125.21}"
        }
      }
    }
  }
}


1 rows affected

command ok
RUN SELECT column_name FROM information_schema.columns WHERE table_name = 'old_coll' ORDER BY column_name ASC
COLUMN_NAME
_id
_json_schema
doc
0 rows affected
RUN SELECT enforced FROM information_schema.table_constraints WHERE  constraint_schema = "xtest" AND table_name ='old_coll' AND  constraint_name LIKE '$val_strict_%'
RUN DROP TABLE IF EXISTS xtest.old_coll

0 rows affected

#
# IV.4
RUN CREATE TABLE xtest.old_coll (doc JSON, _id VARBINARY(32) GENERATED ALWAYS AS
 (JSON_UNQUOTE(JSON_EXTRACT(doc, '$._id'))) STORED PRIMARY KEY) CHARSET utf8mb4
 ENGINE=InnoDB

0 rows affected
RUN SELECT column_name FROM information_schema.columns WHERE table_name = 'old_coll' ORDER BY column_name ASC
COLUMN_NAME
_id
doc
0 rows affected
send Mysqlx.Crud.Insert {
  collection {
    name: "old_coll"
    schema: "xtest"
  }
  data_model: DOCUMENT
  row {
    field {
      type: LITERAL
      literal {
        type: V_STRING
        v_string {
          value: "{\"_id\": \"1\", \"name\": \"Robb\", \"amount\": 125.21}"
        }
      }
    }
  }
}


1 rows affected
send Mysqlx.Crud.Insert {
  collection {
    name: "old_coll"
    schema: "xtest"
  }
  data_model: DOCUMENT
  row {
    field {
      type: LITERAL
      literal {
        type: V_STRING
        v_string {
          value: "{\"_id\": \"2\", \"name\": \"Bob\", \"amount\": 11.19}"
        }
      }
    }
  }
}


1 rows affected
send Mysqlx.Crud.Insert {
  collection {
    name: "old_coll"
    schema: "xtest"
  }
  data_model: DOCUMENT
  row {
    field {
      type: LITERAL
      literal {
        type: V_STRING
        v_string {
          value: "{\"_id\": \"3\", \"name\": \"John\", \"amount\": 223.17}"
        }
      }
    }
  }
}


1 rows affected

command ok
Got expected error:
Mysqlx.Error {
  severity: ERROR
  code: 5180
  msg: "Document is not valid according to the schema assigned to collection. The JSON document location \'#/amount\' failed requirement \'minimum\' at JSON Schema location \'#/properties/amount\'."
  sql_state: "HY000"
}

send Mysqlx.Crud.Update {
  collection {
    name: "old_coll"
    schema: "xtest"
  }
  data_model: DOCUMENT
  criteria {
    type: OPERATOR
    operator {
      name: "=="
      param {
        type: IDENT
        identifier {
          document_path {
            type: MEMBER
            value: "amount"
          }
        }
      }
      param {
        type: LITERAL
        literal {
          type: V_DOUBLE
          v_double: 11.19
        }
      }
    }
  }
  operation {
    source {
      document_path {
        type: MEMBER
        value: "amount"
      }
    }
    operation: ITEM_SET
    value {
      type: LITERAL
      literal {
        type: V_DOUBLE
        v_double: 200
      }
    }
  }
}


1 rows affected
Rows matched: 1  Changed: 1  Warnings: 0

command ok
RUN SELECT column_name FROM information_schema.columns WHERE table_name = 'old_coll' ORDER BY column_name ASC
COLUMN_NAME
_id
_json_schema
doc
0 rows affected
RUN DROP TABLE IF EXISTS xtest.old_coll

0 rows affected

#
# V.1
RUN CREATE TABLE xtest.old_coll (doc JSON, _id VARBINARY(32) GENERATED ALWAYS AS
 (JSON_UNQUOTE(JSON_EXTRACT(doc, '$._id'))) STORED PRIMARY KEY) CHARSET utf8mb4
 ENGINE=InnoDB

0 rows affected
Result
{ "validation": { "level": "off", "schema": {"type": "object"} } }
command ok
RUN DROP TABLE IF EXISTS xtest.old_coll

0 rows affected

#
# V.2
RUN CREATE TABLE xtest.old_coll (doc JSON, _id VARBINARY(32) GENERATED ALWAYS AS
 (JSON_UNQUOTE(JSON_EXTRACT(doc, '$._id'))) STORED PRIMARY KEY) CHARSET utf8mb4
 ENGINE=InnoDB

0 rows affected
Got expected error:
Mysqlx.Error {
  severity: ERROR
  code: 5181
  msg: "Requested collection option \'foo\' doesn\'t exist."
  sql_state: "HY000"
}

RUN DROP TABLE IF EXISTS xtest.old_coll

0 rows affected
Mysqlx.Ok {
  msg: "bye!"
}
ok


## I. Create collection with respect to old style collections.
#
# 1. Create collection when there is an old collection with same name.
# 2. Create collection when there is an old collection with different
#    name.

## II. Create collection using ensure_collection with respect to old
##     style collections.
#
# 1. Create collection when there is an old collection with same name.

## III. List collection objects created with and without validation
##      schema
#
# 1. List just old style collection.
# 2. List old and new collection types.
# 3. List old and new collection types with regular table.
# 4. Drop old collection and list new collection type and a regular
#    table.

## IV. Call modify_collection_options on a schema created without
##     a schema validation
#
# 1. Modify old collection by trying to change only the validation
#    level.
# 2. Modify old collection by trying to change only the validation
#    schema.
# 3. Modify old collection by trying to change both validation schema
#    and level.
# 4. Modify old collection containing data that is not valid according
#    to the new schema,
#    modification must fail and old table layout should not be changed.
#    Update the
#    faulty value, modification must pass and layout should be updated.

## V. Call get_collection_options on a schema created withoud a schema
##    validation.
#
# 1. Fetch valid option.
# 1. Fetch invalid option.
RUN USE xtest;

0 rows affected

#
# I.1
RUN CREATE TABLE xtest.foobar (`doc` json DEFAULT NULL, `_id` varbinary(32)
 GENERATED ALWAYS AS (json_unquote(json_extract(`doc`,_utf8mb4'$._id'))) STORED
 NOT NULL, `_json_schema` json GENERATED ALWAYS AS (_utf8mb4'{"type":"object"}')
 VIRTUAL, PRIMARY KEY (`_id`), CONSTRAINT
 `$val_strict_F2DE1896B6B5A7BAD6369FB94E3B214EBC2F27EA` CHECK
 (JSON_SCHEMA_VALID(`_json_schema`,`doc`)) ENFORCED) CHARSET utf8mb4
 ENGINE=InnoDB

0 rows affected
Got expected error:
Mysqlx.Error {
  severity: ERROR
  code: 1050
  msg: "Table \'foobar\' already exists"
  sql_state: "42S01"
}

Got expected error:
Mysqlx.Error {
  severity: ERROR
  code: 1050
  msg: "Table \'foobar\' already exists"
  sql_state: "42S01"
}

RUN SHOW CREATE TABLE xtest.foobar
Table	Create Table
foobar	CREATE TABLE `foobar` (
  `doc` json DEFAULT NULL,
  `_id` varbinary(32) GENERATED ALWAYS AS (json_unquote(json_extract(`doc`,_utf8mb4'$._id'))) STORED NOT NULL,
  `_json_schema` json GENERATED ALWAYS AS (_utf8mb4'{"type":"object"}') VIRTUAL,
  PRIMARY KEY (`_id`),
  CONSTRAINT `$val_strict_F2DE1896B6B5A7BAD6369FB94E3B214EBC2F27EA` CHECK (json_schema_valid(`_json_schema`,`doc`))
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
0 rows affected
RUN DROP TABLE IF EXISTS xtest.foobar

0 rows affected

#
# I.2
RUN CREATE TABLE xtest.foo (`doc` json DEFAULT NULL, `_id` varbinary(32)
 GENERATED ALWAYS AS (json_unquote(json_extract(`doc`,_utf8mb4'$._id'))) STORED
 NOT NULL, `_json_schema` json GENERATED ALWAYS AS (_utf8mb4'{"type":"object"}')
 VIRTUAL, PRIMARY KEY (`_id`), CONSTRAINT
 `$val_strict_F2DE1896B6B5A7BAD6369FB94E3B214EBC2F27EA` CHECK
 (JSON_SCHEMA_VALID(`_json_schema`,`doc`)) ENFORCED) CHARSET utf8mb4
 ENGINE=InnoDB

0 rows affected

command ok
RUN SHOW CREATE TABLE xtest.foo
Table	Create Table
foo	CREATE TABLE `foo` (
  `doc` json DEFAULT NULL,
  `_id` varbinary(32) GENERATED ALWAYS AS (json_unquote(json_extract(`doc`,_utf8mb4'$._id'))) STORED NOT NULL,
  `_json_schema` json GENERATED ALWAYS AS (_utf8mb4'{"type":"object"}') VIRTUAL,
  PRIMARY KEY (`_id`),
  CONSTRAINT `$val_strict_F2DE1896B6B5A7BAD6369FB94E3B214EBC2F27EA` CHECK (json_schema_valid(`_json_schema`,`doc`))
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
0 rows affected
RUN SHOW CREATE TABLE xtest.bar
Table	Create Table
bar	CREATE TABLE `bar` (
  `doc` json DEFAULT NULL,
  `_id` varbinary(32) GENERATED ALWAYS AS (json_unquote(json_extract(`doc`,_utf8mb4'$._id'))) STORED NOT NULL,
  `_json_schema` json GENERATED ALWAYS AS (_utf8mb4'{"type":"object"}') VIRTUAL,
  PRIMARY KEY (`_id`),
  CONSTRAINT `$val_strict_E8D46CE25265E545D225A8A6F1BAF642FEBEE5CB` CHECK (json_schema_valid(`_json_schema`,`doc`))
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
0 rows affected
RUN DROP TABLE IF EXISTS xtest.foo

0 rows affected
RUN DROP TABLE IF EXISTS xtest.bar

0 rows affected

#
# II.1
RUN CREATE TABLE xtest.foobar (`doc` json DEFAULT NULL, `_id` varbinary(32)
 GENERATED ALWAYS AS (json_unquote(json_extract(`doc`,_utf8mb4'$._id'))) STORED
 NOT NULL, `_json_schema` json GENERATED ALWAYS AS (_utf8mb4'{"type":"object"}')
 VIRTUAL, PRIMARY KEY (`_id`), CONSTRAINT
 `$val_strict_F2DE1896B6B5A7BAD6369FB94E3B214EBC2F27EA` CHECK
 (JSON_SCHEMA_VALID(`_json_schema`,`doc`)) ENFORCED) CHARSET utf8mb4
 ENGINE=InnoDB

0 rows affected

command ok

command ok

command ok
RUN SHOW CREATE TABLE xtest.foobar
Table	Create Table
foobar	CREATE TABLE `foobar` (
  `doc` json DEFAULT NULL,
  `_id` varbinary(32) GENERATED ALWAYS AS (json_unquote(json_extract(`doc`,_utf8mb4'$._id'))) STORED NOT NULL,
  `_json_schema` json GENERATED ALWAYS AS (_utf8mb4'{"type":"object"}') VIRTUAL,
  PRIMARY KEY (`_id`),
  CONSTRAINT `$val_strict_F2DE1896B6B5A7BAD6369FB94E3B214EBC2F27EA` CHECK (json_schema_valid(`_json_schema`,`doc`))
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
0 rows affected
RUN DROP TABLE IF EXISTS xtest.foobar

0 rows affected

#
# III.1
RUN CREATE TABLE xtest.old_coll (`doc` json DEFAULT NULL, `_id` varbinary(32)
 GENERATED ALWAYS AS (json_unquote(json_extract(`doc`,_utf8mb4'$._id'))) STORED
 NOT NULL, `_json_schema` json GENERATED ALWAYS AS (_utf8mb4'{"type":"object"}')
 VIRTUAL, PRIMARY KEY (`_id`), CONSTRAINT
 `$val_strict_F2DE1896B6B5A7BAD6369FB94E3B214EBC2F27EA` CHECK
 (JSON_SCHEMA_VALID(`_json_schema`,`doc`)) ENFORCED) CHARSET utf8mb4
 ENGINE=InnoDB

0 rows affected
name	type
old_coll	COLLECTION
command ok

#
# III.2

command ok
name	type
new_coll2	COLLECTION
old_coll	COLLECTION
command ok

command ok
name	type
new_coll2	COLLECTION
new_coll3	COLLECTION
old_coll	COLLECTION
command ok

#
# III.3
RUN CREATE TABLE xtest.regular_tab (doc JSON);

0 rows affected
name	type
new_coll2	COLLECTION
new_coll3	COLLECTION
old_coll	COLLECTION
regular_tab	TABLE
command ok

#
# III.4
RUN DROP TABLE IF EXISTS xtest.old_coll

0 rows affected
name	type
new_coll2	COLLECTION
new_coll3	COLLECTION
regular_tab	TABLE
command ok

0 rows affected

#
# IV.1
RUN CREATE TABLE xtest.old_coll (`doc` json DEFAULT NULL, `_id` varbinary(32)
 GENERATED ALWAYS AS (json_unquote(json_extract(`doc`,_utf8mb4'$._id'))) STORED
 NOT NULL, `_json_schema` json GENERATED ALWAYS AS (_utf8mb4'{"type":"object"}')
 VIRTUAL, PRIMARY KEY (`_id`), CONSTRAINT
 `$val_strict_F2DE1896B6B5A7BAD6369FB94E3B214EBC2F27EA` CHECK
 (JSON_SCHEMA_VALID(`_json_schema`,`doc`)) ENFORCED) CHARSET utf8mb4
 ENGINE=InnoDB

0 rows affected
RUN SELECT column_name FROM information_schema.columns WHERE table_name = 'old_coll' ORDER BY column_name ASC
COLUMN_NAME
_id
_json_schema
doc
0 rows affected
send Mysqlx.Crud.Insert {
  collection {
    name: "old_coll"
    schema: "xtest"
  }
  data_model: DOCUMENT
  row {
    field {
      type: LITERAL
      literal {
        type: V_STRING
        v_string {
          value: "{\"_id\": \"1\", \"name\": \"Robb\", \"amount\": 125.21}"
        }
      }
    }
  }
}


1 rows affected

command ok
RUN SELECT column_name FROM information_schema.columns WHERE table_name = 'old_coll' ORDER BY column_name ASC
COLUMN_NAME
_id
_json_schema
doc
0 rows affected
RUN SELECT enforced FROM information_schema.table_constraints WHERE  constraint_schema = "xtest" AND table_name ='old_coll' AND  constraint_name LIKE '$val_strict_%'
RUN DROP TABLE IF EXISTS xtest.old_coll

0 rows affected

#
# IV.2
RUN CREATE TABLE xtest.old_coll (`doc` json DEFAULT NULL, `_id` varbinary(32)
 GENERATED ALWAYS AS (json_unquote(json_extract(`doc`,_utf8mb4'$._id'))) STORED
 NOT NULL, `_json_schema` json GENERATED ALWAYS AS (_utf8mb4'{"type":"object"}')
 VIRTUAL, PRIMARY KEY (`_id`), CONSTRAINT
 `$val_strict_F2DE1896B6B5A7BAD6369FB94E3B214EBC2F27EA` CHECK
 (JSON_SCHEMA_VALID(`_json_schema`,`doc`)) ENFORCED) CHARSET utf8mb4
 ENGINE=InnoDB

0 rows affected
RUN SELECT column_name FROM information_schema.columns WHERE table_name = 'old_coll' ORDER BY column_name ASC
COLUMN_NAME
_id
_json_schema
doc
0 rows affected
send Mysqlx.Crud.Insert {
  collection {
    name: "old_coll"
    schema: "xtest"
  }
  data_model: DOCUMENT
  row {
    field {
      type: LITERAL
      literal {
        type: V_STRING
        v_string {
          value: "{\"_id\": \"1\", \"name\": \"Robb\", \"amount\": 125.21}"
        }
      }
    }
  }
}


1 rows affected

command ok
RUN SELECT column_name FROM information_schema.columns WHERE table_name = 'old_coll' ORDER BY column_name ASC
COLUMN_NAME
_id
_json_schema
doc
0 rows affected
RUN SELECT enforced FROM information_schema.table_constraints WHERE  constraint_schema = "xtest" AND table_name ='old_coll' AND  constraint_name LIKE '$val_strict_%'
RUN DROP TABLE IF EXISTS xtest.old_coll

0 rows affected

#
# IV.3
RUN CREATE TABLE xtest.old_coll (`doc` json DEFAULT NULL, `_id` varbinary(32)
 GENERATED ALWAYS AS (json_unquote(json_extract(`doc`,_utf8mb4'$._id'))) STORED
 NOT NULL, `_json_schema` json GENERATED ALWAYS AS (_utf8mb4'{"type":"object"}')
 VIRTUAL, PRIMARY KEY (`_id`), CONSTRAINT
 `$val_strict_F2DE1896B6B5A7BAD6369FB94E3B214EBC2F27EA` CHECK
 (JSON_SCHEMA_VALID(`_json_schema`,`doc`)) ENFORCED) CHARSET utf8mb4
 ENGINE=InnoDB

0 rows affected
RUN SELECT column_name FROM information_schema.columns WHERE table_name = 'old_coll' ORDER BY column_name ASC
COLUMN_NAME
_id
_json_schema
doc
0 rows affected
send Mysqlx.Crud.Insert {
  collection {
    name: "old_coll"
    schema: "xtest"
  }
  data_model: DOCUMENT
  row {
    field {
      type: LITERAL
      literal {
        type: V_STRING
        v_string {
          value: "{\"_id\": \"1\", \"name\": \"Robb\", \"amount\": 125.21}"
        }
      }
    }
  }
}


1 rows affected

command ok
RUN SELECT column_name FROM information_schema.columns WHERE table_name = 'old_coll' ORDER BY column_name ASC
COLUMN_NAME
_id
_json_schema
doc
0 rows affected
RUN SELECT enforced FROM information_schema.table_constraints WHERE  constraint_schema = "xtest" AND table_name ='old_coll' AND  constraint_name LIKE '$val_strict_%'
RUN DROP TABLE IF EXISTS xtest.old_coll

0 rows affected

#
# IV.4
RUN CREATE TABLE xtest.old_coll (`doc` json DEFAULT NULL, `_id` varbinary(32)
 GENERATED ALWAYS AS (json_unquote(json_extract(`doc`,_utf8mb4'$._id'))) STORED
 NOT NULL, `_json_schema` json GENERATED ALWAYS AS (_utf8mb4'{"type":"object"}')
 VIRTUAL, PRIMARY KEY (`_id`), CONSTRAINT
 `$val_strict_F2DE1896B6B5A7BAD6369FB94E3B214EBC2F27EA` CHECK
 (JSON_SCHEMA_VALID(`_json_schema`,`doc`)) ENFORCED) CHARSET utf8mb4
 ENGINE=InnoDB

0 rows affected
RUN SELECT column_name FROM information_schema.columns WHERE table_name = 'old_coll' ORDER BY column_name ASC
COLUMN_NAME
_id
_json_schema
doc
0 rows affected
send Mysqlx.Crud.Insert {
  collection {
    name: "old_coll"
    schema: "xtest"
  }
  data_model: DOCUMENT
  row {
    field {
      type: LITERAL
      literal {
        type: V_STRING
        v_string {
          value: "{\"_id\": \"1\", \"name\": \"Robb\", \"amount\": 125.21}"
        }
      }
    }
  }
}


1 rows affected
send Mysqlx.Crud.Insert {
  collection {
    name: "old_coll"
    schema: "xtest"
  }
  data_model: DOCUMENT
  row {
    field {
      type: LITERAL
      literal {
        type: V_STRING
        v_string {
          value: "{\"_id\": \"2\", \"name\": \"Bob\", \"amount\": 11.19}"
        }
      }
    }
  }
}


1 rows affected
send Mysqlx.Crud.Insert {
  collection {
    name: "old_coll"
    schema: "xtest"
  }
  data_model: DOCUMENT
  row {
    field {
      type: LITERAL
      literal {
        type: V_STRING
        v_string {
          value: "{\"_id\": \"3\", \"name\": \"John\", \"amount\": 223.17}"
        }
      }
    }
  }
}


1 rows affected

command ok
Got expected error:
Mysqlx.Error {
  severity: ERROR
  code: 5180
  msg: "Document is not valid according to the schema assigned to collection. The JSON document location \'#/amount\' failed requirement \'minimum\' at JSON Schema location \'#/properties/amount\'."
  sql_state: "HY000"
}

send Mysqlx.Crud.Update {
  collection {
    name: "old_coll"
    schema: "xtest"
  }
  data_model: DOCUMENT
  criteria {
    type: OPERATOR
    operator {
      name: "=="
      param {
        type: IDENT
        identifier {
          document_path {
            type: MEMBER
            value: "amount"
          }
        }
      }
      param {
        type: LITERAL
        literal {
          type: V_DOUBLE
          v_double: 11.19
        }
      }
    }
  }
  operation {
    source {
      document_path {
        type: MEMBER
        value: "amount"
      }
    }
    operation: ITEM_SET
    value {
      type: LITERAL
      literal {
        type: V_DOUBLE
        v_double: 200
      }
    }
  }
}


1 rows affected
Rows matched: 1  Changed: 1  Warnings: 0

command ok
RUN SELECT column_name FROM information_schema.columns WHERE table_name = 'old_coll' ORDER BY column_name ASC
COLUMN_NAME
_id
_json_schema
doc
0 rows affected
RUN DROP TABLE IF EXISTS xtest.old_coll

0 rows affected

#
# V.1
RUN CREATE TABLE xtest.old_coll (`doc` json DEFAULT NULL, `_id` varbinary(32)
 GENERATED ALWAYS AS (json_unquote(json_extract(`doc`,_utf8mb4'$._id'))) STORED
 NOT NULL, `_json_schema` json GENERATED ALWAYS AS (_utf8mb4'{"type":"object"}')
 VIRTUAL, PRIMARY KEY (`_id`), CONSTRAINT
 `$val_strict_F2DE1896B6B5A7BAD6369FB94E3B214EBC2F27EA` CHECK
 (JSON_SCHEMA_VALID(`_json_schema`,`doc`)) ENFORCED) CHARSET utf8mb4
 ENGINE=InnoDB

0 rows affected
Result
{ "validation": { "level": "strict", "schema": {"type":"object"} } }
command ok
RUN DROP TABLE IF EXISTS xtest.old_coll

0 rows affected

#
# V.2
RUN CREATE TABLE xtest.old_coll (`doc` json DEFAULT NULL, `_id` varbinary(32)
 GENERATED ALWAYS AS (json_unquote(json_extract(`doc`,_utf8mb4'$._id'))) STORED
 NOT NULL, `_json_schema` json GENERATED ALWAYS AS (_utf8mb4'{"type":"object"}')
 VIRTUAL, PRIMARY KEY (`_id`), CONSTRAINT
 `$val_strict_F2DE1896B6B5A7BAD6369FB94E3B214EBC2F27EA` CHECK
 (JSON_SCHEMA_VALID(`_json_schema`,`doc`)) ENFORCED) CHARSET utf8mb4
 ENGINE=InnoDB

0 rows affected
Got expected error:
Mysqlx.Error {
  severity: ERROR
  code: 5181
  msg: "Requested collection option \'foo\' doesn\'t exist."
  sql_state: "HY000"
}

RUN DROP TABLE IF EXISTS xtest.old_coll

0 rows affected
Mysqlx.Ok {
  msg: "bye!"
}
ok

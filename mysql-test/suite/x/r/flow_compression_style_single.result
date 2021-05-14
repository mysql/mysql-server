#
## Setup tests: tables and stored procedures

## A. Execute the test using zlib compression
#


## Test setup


## I. Validate that X Plugin sends compressed messages in StmtExecute resultset (GRAPH 2)
#
# 1. Assert SELECTs empty resultset
# 2. Assert SELECTs resultset with one column and one row
# 3. Assert SELECTs resultset with one column and multiple rows
# 4. Assert SELECTs resultset with multiple columns and multiple rows
# 5. Assert SQL that returns multiple resultsets
#
## II. Validate that X Plugin sends compressed messages in Crud.Insert resultset (GRAPH 2)
#
# 1. Assert Crud.Insert resultset for single row insert
# 2. Assert Crud.Insert resultset for multiple rows insert
#
## III. Validate that X Plugin sends compressed messages in Crud.Find resultset (GRAPH 2)
#
# 1. Assert Crud.Find empty resultset
# 2. Assert Crud.Find resultset with multiple rows
#
## IV. Validate that X Plugin sends compressed messages in Crud.Update resultset (GRAPH 2)
#
# 1. Assert Crud.Update resultset with no hits
# 2. Assert Crud.Update resultset with multiple updated rows
#
## V. Validate that X Plugin sends compressed messages in Crud.Delete resultset (GRAPH 2)
#
# 1. Assert Crud.Delete resultset with no hits
# 2. Assert Crud.Delete resultset with multiple deleted rows
#
## VI. Validate that X Plugin sends compressed messages in PrepStmt resultset (GRAPH 2)
#
# 1. Assert PrepStmt resultset with out-params
# 2. Assert Cursor resultset that was suspended

# Create table "xtbl" and fill it with data
# Create collection "coll" and fill it with data

#
# I.1
RUN SELECT phrase FROM xtbl WHERE prio < 0
[ASSERT COMPRESSION]
[ASSERT COMPRESSION]
[ASSERT COMPRESSION]
[ASSERT Mysqlx.Sql.StmtExecuteOk]

#
# I.2
RUN SELECT phrase FROM xtbl LIMIT 1
[ASSERT COMPRESSION]
[ASSERT COMPRESSION]
[ASSERT COMPRESSION]
[ASSERT COMPRESSION]
[ASSERT Mysqlx.Sql.StmtExecuteOk]

#
# I.3
RUN SELECT phrase FROM xtbl LIMIT 4
[ASSERT COMPRESSION]
[ASSERT COMPRESSION]
[ASSERT COMPRESSION]
[ASSERT COMPRESSION]
[ASSERT COMPRESSION]
[ASSERT COMPRESSION]
[ASSERT COMPRESSION]
[ASSERT Mysqlx.Sql.StmtExecuteOk]

#
# I.4
RUN SELECT phrase, prio FROM xtbl LIMIT 4
[ASSERT COMPRESSION]
[ASSERT COMPRESSION]
[ASSERT COMPRESSION]
[ASSERT COMPRESSION]
[ASSERT COMPRESSION]
[ASSERT COMPRESSION]
[ASSERT COMPRESSION]
[ASSERT COMPRESSION]
[ASSERT Mysqlx.Sql.StmtExecuteOk]

#
# I.5
RUN CALL two_resultsets()
[ASSERT COMPRESSION]
[ASSERT COMPRESSION]
[ASSERT COMPRESSION]
[ASSERT COMPRESSION]
[ASSERT COMPRESSION]
[ASSERT COMPRESSION]
[ASSERT COMPRESSION]
[ASSERT COMPRESSION]
[ASSERT Mysqlx.Sql.StmtExecuteOk]

#
# II.1
send Mysqlx.Crud.Insert {
  collection {
    name: "coll"
    schema: "xtest"
  }
  data_model: DOCUMENT
  row {
    field {
      type: LITERAL
      literal {
        type: V_STRING
        v_string {
          value: "{\"some_field\":\"ok\"}"
        }
      }
    }
  }
}

[ASSERT COMPRESSION]
[ASSERT COMPRESSION]
[ASSERT Mysqlx.Sql.StmtExecuteOk]

#
# II.2
send Mysqlx.Crud.Insert {
  collection {
    name: "coll"
    schema: "xtest"
  }
  data_model: DOCUMENT
  row {
    field {
      type: LITERAL
      literal {
        type: V_STRING
        v_string {
          value: "{\"ints\":1}"
        }
      }
    }
  }
  row {
    field {
      type: LITERAL
      literal {
        type: V_STRING
        v_string {
          value: "{\"ints\":2}"
        }
      }
    }
  }
  row {
    field {
      type: LITERAL
      literal {
        type: V_STRING
        v_string {
          value: "{\"ints\":3}"
        }
      }
    }
  }
  row {
    field {
      type: LITERAL
      literal {
        type: V_STRING
        v_string {
          value: "{\"ints\":4}"
        }
      }
    }
  }
}

[ASSERT COMPRESSION]
[ASSERT COMPRESSION]
[ASSERT COMPRESSION]
[ASSERT Mysqlx.Sql.StmtExecuteOk]

#
# III.1
send Mysqlx.Crud.Find {
  collection {
    name: "coll"
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
            value: "not_existing_member"
          }
        }
      }
      param {
        type: LITERAL
        literal {
          type: V_OCTETS
          v_octets {
            value: "non-existing-value"
          }
        }
      }
    }
  }
}

[ASSERT COMPRESSION]
[ASSERT COMPRESSION]
[ASSERT Mysqlx.Sql.StmtExecuteOk]

#
# III.2
send Mysqlx.Crud.Find {
  collection {
    name: "coll"
    schema: "xtest"
  }
  data_model: DOCUMENT
  limit {
    row_count: 4
  }
}

[ASSERT COMPRESSION]
[ASSERT COMPRESSION]
[ASSERT COMPRESSION]
[ASSERT COMPRESSION]
[ASSERT COMPRESSION]
[ASSERT COMPRESSION]
[ASSERT Mysqlx.Sql.StmtExecuteOk]

#
# IV.1
send Mysqlx.Crud.Update {
  collection {
    name: "coll"
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
            value: "not_existing_member"
          }
        }
      }
      param {
        type: LITERAL
        literal {
          type: V_OCTETS
          v_octets {
            value: "non-existing-value"
          }
        }
      }
    }
  }
  operation {
    source {
      document_path {
        type: MEMBER
        value: "foo"
      }
    }
    operation: ITEM_SET
    value {
      type: LITERAL
      literal {
        type: V_NULL
      }
    }
  }
}

[ASSERT COMPRESSION]
[ASSERT COMPRESSION]
[ASSERT Mysqlx.Sql.StmtExecuteOk]

#
# IV.2
send Mysqlx.Crud.Update {
  collection {
    name: "coll"
    schema: "xtest"
  }
  data_model: DOCUMENT
  operation {
    source {
      document_path {
        type: MEMBER
        value: "foo2"
      }
    }
    operation: ITEM_SET
    value {
      type: LITERAL
      literal {
        type: V_NULL
      }
    }
  }
}

[ASSERT COMPRESSION]
[ASSERT COMPRESSION]
[ASSERT Mysqlx.Sql.StmtExecuteOk]

#
# V.1
send Mysqlx.Crud.Delete {
  collection {
    name: "coll"
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
            value: "not_existing_member"
          }
        }
      }
      param {
        type: LITERAL
        literal {
          type: V_OCTETS
          v_octets {
            value: "non-existing-value"
          }
        }
      }
    }
  }
}

[ASSERT COMPRESSION]
[ASSERT Mysqlx.Sql.StmtExecuteOk]

#
# V.2
send Mysqlx.Crud.Delete {
  collection {
    name: "coll"
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
            value: "prio"
          }
        }
      }
      param {
        type: LITERAL
        literal {
          type: V_SINT
          v_signed_int: 1
        }
      }
    }
  }
}

[ASSERT COMPRESSION]
[ASSERT Mysqlx.Sql.StmtExecuteOk]

#
# VI.1
send Mysqlx.Prepare.Prepare {
  stmt_id: 1
  stmt {
    type: STMT
    stmt_execute {
      stmt: "call out_param_no_select(10, ?)"
    }
  }
}

RUN recvok
send Mysqlx.Prepare.Execute {
  stmt_id: 1
  args {
    type: SCALAR
    scalar {
      type: V_STRING
      v_string {
        value: "@my_var"
      }
    }
  }
}

[ASSERT COMPRESSION]
[ASSERT COMPRESSION]
[ASSERT COMPRESSION]
[ASSERT COMPRESSION]
[ASSERT COMPRESSION]
[ASSERT Mysqlx.Sql.StmtExecuteOk]

#
# VI.2
send Mysqlx.Prepare.Prepare {
  stmt_id: 2
  stmt {
    type: STMT
    stmt_execute {
      stmt: "SELECT phrase FROM xtbl"
    }
  }
}

RUN recvok
send Mysqlx.Cursor.Open {
  cursor_id: 2
  stmt {
    type: PREPARE_EXECUTE
    prepare_execute {
      stmt_id: 2
    }
  }
  fetch_rows: 1
}

[ASSERT COMPRESSION]
[ASSERT COMPRESSION]
[ASSERT COMPRESSION]
[ASSERT Mysqlx.Sql.StmtExecuteOk]
Mysqlx.Ok {
  msg: "bye!"
}
ok

## B. Execute the test using lz4 compression
#


## Test setup


## I. Validate that X Plugin sends compressed messages in StmtExecute resultset (GRAPH 2)
#
# 1. Assert SELECTs empty resultset
# 2. Assert SELECTs resultset with one column and one row
# 3. Assert SELECTs resultset with one column and multiple rows
# 4. Assert SELECTs resultset with multiple columns and multiple rows
# 5. Assert SQL that returns multiple resultsets
#
## II. Validate that X Plugin sends compressed messages in Crud.Insert resultset (GRAPH 2)
#
# 1. Assert Crud.Insert resultset for single row insert
# 2. Assert Crud.Insert resultset for multiple rows insert
#
## III. Validate that X Plugin sends compressed messages in Crud.Find resultset (GRAPH 2)
#
# 1. Assert Crud.Find empty resultset
# 2. Assert Crud.Find resultset with multiple rows
#
## IV. Validate that X Plugin sends compressed messages in Crud.Update resultset (GRAPH 2)
#
# 1. Assert Crud.Update resultset with no hits
# 2. Assert Crud.Update resultset with multiple updated rows
#
## V. Validate that X Plugin sends compressed messages in Crud.Delete resultset (GRAPH 2)
#
# 1. Assert Crud.Delete resultset with no hits
# 2. Assert Crud.Delete resultset with multiple deleted rows
#
## VI. Validate that X Plugin sends compressed messages in PrepStmt resultset (GRAPH 2)
#
# 1. Assert PrepStmt resultset with out-params
# 2. Assert Cursor resultset that was suspended

# Create table "xtbl" and fill it with data
# Create collection "coll" and fill it with data

#
# I.1
RUN SELECT phrase FROM xtbl WHERE prio < 0
[ASSERT COMPRESSION]
[ASSERT COMPRESSION]
[ASSERT COMPRESSION]
[ASSERT Mysqlx.Sql.StmtExecuteOk]

#
# I.2
RUN SELECT phrase FROM xtbl LIMIT 1
[ASSERT COMPRESSION]
[ASSERT COMPRESSION]
[ASSERT COMPRESSION]
[ASSERT COMPRESSION]
[ASSERT Mysqlx.Sql.StmtExecuteOk]

#
# I.3
RUN SELECT phrase FROM xtbl LIMIT 4
[ASSERT COMPRESSION]
[ASSERT COMPRESSION]
[ASSERT COMPRESSION]
[ASSERT COMPRESSION]
[ASSERT COMPRESSION]
[ASSERT COMPRESSION]
[ASSERT COMPRESSION]
[ASSERT Mysqlx.Sql.StmtExecuteOk]

#
# I.4
RUN SELECT phrase, prio FROM xtbl LIMIT 4
[ASSERT COMPRESSION]
[ASSERT COMPRESSION]
[ASSERT COMPRESSION]
[ASSERT COMPRESSION]
[ASSERT COMPRESSION]
[ASSERT COMPRESSION]
[ASSERT COMPRESSION]
[ASSERT COMPRESSION]
[ASSERT Mysqlx.Sql.StmtExecuteOk]

#
# I.5
RUN CALL two_resultsets()
[ASSERT COMPRESSION]
[ASSERT COMPRESSION]
[ASSERT COMPRESSION]
[ASSERT COMPRESSION]
[ASSERT COMPRESSION]
[ASSERT COMPRESSION]
[ASSERT COMPRESSION]
[ASSERT COMPRESSION]
[ASSERT Mysqlx.Sql.StmtExecuteOk]

#
# II.1
send Mysqlx.Crud.Insert {
  collection {
    name: "coll"
    schema: "xtest"
  }
  data_model: DOCUMENT
  row {
    field {
      type: LITERAL
      literal {
        type: V_STRING
        v_string {
          value: "{\"some_field\":\"ok\"}"
        }
      }
    }
  }
}

[ASSERT COMPRESSION]
[ASSERT COMPRESSION]
[ASSERT Mysqlx.Sql.StmtExecuteOk]

#
# II.2
send Mysqlx.Crud.Insert {
  collection {
    name: "coll"
    schema: "xtest"
  }
  data_model: DOCUMENT
  row {
    field {
      type: LITERAL
      literal {
        type: V_STRING
        v_string {
          value: "{\"ints\":1}"
        }
      }
    }
  }
  row {
    field {
      type: LITERAL
      literal {
        type: V_STRING
        v_string {
          value: "{\"ints\":2}"
        }
      }
    }
  }
  row {
    field {
      type: LITERAL
      literal {
        type: V_STRING
        v_string {
          value: "{\"ints\":3}"
        }
      }
    }
  }
  row {
    field {
      type: LITERAL
      literal {
        type: V_STRING
        v_string {
          value: "{\"ints\":4}"
        }
      }
    }
  }
}

[ASSERT COMPRESSION]
[ASSERT COMPRESSION]
[ASSERT COMPRESSION]
[ASSERT Mysqlx.Sql.StmtExecuteOk]

#
# III.1
send Mysqlx.Crud.Find {
  collection {
    name: "coll"
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
            value: "not_existing_member"
          }
        }
      }
      param {
        type: LITERAL
        literal {
          type: V_OCTETS
          v_octets {
            value: "non-existing-value"
          }
        }
      }
    }
  }
}

[ASSERT COMPRESSION]
[ASSERT COMPRESSION]
[ASSERT Mysqlx.Sql.StmtExecuteOk]

#
# III.2
send Mysqlx.Crud.Find {
  collection {
    name: "coll"
    schema: "xtest"
  }
  data_model: DOCUMENT
  limit {
    row_count: 4
  }
}

[ASSERT COMPRESSION]
[ASSERT COMPRESSION]
[ASSERT COMPRESSION]
[ASSERT COMPRESSION]
[ASSERT COMPRESSION]
[ASSERT COMPRESSION]
[ASSERT Mysqlx.Sql.StmtExecuteOk]

#
# IV.1
send Mysqlx.Crud.Update {
  collection {
    name: "coll"
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
            value: "not_existing_member"
          }
        }
      }
      param {
        type: LITERAL
        literal {
          type: V_OCTETS
          v_octets {
            value: "non-existing-value"
          }
        }
      }
    }
  }
  operation {
    source {
      document_path {
        type: MEMBER
        value: "foo"
      }
    }
    operation: ITEM_SET
    value {
      type: LITERAL
      literal {
        type: V_NULL
      }
    }
  }
}

[ASSERT COMPRESSION]
[ASSERT COMPRESSION]
[ASSERT Mysqlx.Sql.StmtExecuteOk]

#
# IV.2
send Mysqlx.Crud.Update {
  collection {
    name: "coll"
    schema: "xtest"
  }
  data_model: DOCUMENT
  operation {
    source {
      document_path {
        type: MEMBER
        value: "foo2"
      }
    }
    operation: ITEM_SET
    value {
      type: LITERAL
      literal {
        type: V_NULL
      }
    }
  }
}

[ASSERT COMPRESSION]
[ASSERT COMPRESSION]
[ASSERT Mysqlx.Sql.StmtExecuteOk]

#
# V.1
send Mysqlx.Crud.Delete {
  collection {
    name: "coll"
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
            value: "not_existing_member"
          }
        }
      }
      param {
        type: LITERAL
        literal {
          type: V_OCTETS
          v_octets {
            value: "non-existing-value"
          }
        }
      }
    }
  }
}

[ASSERT COMPRESSION]
[ASSERT Mysqlx.Sql.StmtExecuteOk]

#
# V.2
send Mysqlx.Crud.Delete {
  collection {
    name: "coll"
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
            value: "prio"
          }
        }
      }
      param {
        type: LITERAL
        literal {
          type: V_SINT
          v_signed_int: 1
        }
      }
    }
  }
}

[ASSERT COMPRESSION]
[ASSERT Mysqlx.Sql.StmtExecuteOk]

#
# VI.1
send Mysqlx.Prepare.Prepare {
  stmt_id: 1
  stmt {
    type: STMT
    stmt_execute {
      stmt: "call out_param_no_select(10, ?)"
    }
  }
}

RUN recvok
send Mysqlx.Prepare.Execute {
  stmt_id: 1
  args {
    type: SCALAR
    scalar {
      type: V_STRING
      v_string {
        value: "@my_var"
      }
    }
  }
}

[ASSERT COMPRESSION]
[ASSERT COMPRESSION]
[ASSERT COMPRESSION]
[ASSERT COMPRESSION]
[ASSERT COMPRESSION]
[ASSERT Mysqlx.Sql.StmtExecuteOk]

#
# VI.2
send Mysqlx.Prepare.Prepare {
  stmt_id: 2
  stmt {
    type: STMT
    stmt_execute {
      stmt: "SELECT phrase FROM xtbl"
    }
  }
}

RUN recvok
send Mysqlx.Cursor.Open {
  cursor_id: 2
  stmt {
    type: PREPARE_EXECUTE
    prepare_execute {
      stmt_id: 2
    }
  }
  fetch_rows: 1
}

[ASSERT COMPRESSION]
[ASSERT COMPRESSION]
[ASSERT COMPRESSION]
[ASSERT Mysqlx.Sql.StmtExecuteOk]
Mysqlx.Ok {
  msg: "bye!"
}
ok

## C. Execute the test using zstd compression
#


## Test setup


## I. Validate that X Plugin sends compressed messages in StmtExecute resultset (GRAPH 2)
#
# 1. Assert SELECTs empty resultset
# 2. Assert SELECTs resultset with one column and one row
# 3. Assert SELECTs resultset with one column and multiple rows
# 4. Assert SELECTs resultset with multiple columns and multiple rows
# 5. Assert SQL that returns multiple resultsets
#
## II. Validate that X Plugin sends compressed messages in Crud.Insert resultset (GRAPH 2)
#
# 1. Assert Crud.Insert resultset for single row insert
# 2. Assert Crud.Insert resultset for multiple rows insert
#
## III. Validate that X Plugin sends compressed messages in Crud.Find resultset (GRAPH 2)
#
# 1. Assert Crud.Find empty resultset
# 2. Assert Crud.Find resultset with multiple rows
#
## IV. Validate that X Plugin sends compressed messages in Crud.Update resultset (GRAPH 2)
#
# 1. Assert Crud.Update resultset with no hits
# 2. Assert Crud.Update resultset with multiple updated rows
#
## V. Validate that X Plugin sends compressed messages in Crud.Delete resultset (GRAPH 2)
#
# 1. Assert Crud.Delete resultset with no hits
# 2. Assert Crud.Delete resultset with multiple deleted rows
#
## VI. Validate that X Plugin sends compressed messages in PrepStmt resultset (GRAPH 2)
#
# 1. Assert PrepStmt resultset with out-params
# 2. Assert Cursor resultset that was suspended

# Create table "xtbl" and fill it with data
# Create collection "coll" and fill it with data

#
# I.1
RUN SELECT phrase FROM xtbl WHERE prio < 0
[ASSERT COMPRESSION]
[ASSERT COMPRESSION]
[ASSERT COMPRESSION]
[ASSERT Mysqlx.Sql.StmtExecuteOk]

#
# I.2
RUN SELECT phrase FROM xtbl LIMIT 1
[ASSERT COMPRESSION]
[ASSERT COMPRESSION]
[ASSERT COMPRESSION]
[ASSERT COMPRESSION]
[ASSERT Mysqlx.Sql.StmtExecuteOk]

#
# I.3
RUN SELECT phrase FROM xtbl LIMIT 4
[ASSERT COMPRESSION]
[ASSERT COMPRESSION]
[ASSERT COMPRESSION]
[ASSERT COMPRESSION]
[ASSERT COMPRESSION]
[ASSERT COMPRESSION]
[ASSERT COMPRESSION]
[ASSERT Mysqlx.Sql.StmtExecuteOk]

#
# I.4
RUN SELECT phrase, prio FROM xtbl LIMIT 4
[ASSERT COMPRESSION]
[ASSERT COMPRESSION]
[ASSERT COMPRESSION]
[ASSERT COMPRESSION]
[ASSERT COMPRESSION]
[ASSERT COMPRESSION]
[ASSERT COMPRESSION]
[ASSERT COMPRESSION]
[ASSERT Mysqlx.Sql.StmtExecuteOk]

#
# I.5
RUN CALL two_resultsets()
[ASSERT COMPRESSION]
[ASSERT COMPRESSION]
[ASSERT COMPRESSION]
[ASSERT COMPRESSION]
[ASSERT COMPRESSION]
[ASSERT COMPRESSION]
[ASSERT COMPRESSION]
[ASSERT COMPRESSION]
[ASSERT Mysqlx.Sql.StmtExecuteOk]

#
# II.1
send Mysqlx.Crud.Insert {
  collection {
    name: "coll"
    schema: "xtest"
  }
  data_model: DOCUMENT
  row {
    field {
      type: LITERAL
      literal {
        type: V_STRING
        v_string {
          value: "{\"some_field\":\"ok\"}"
        }
      }
    }
  }
}

[ASSERT COMPRESSION]
[ASSERT COMPRESSION]
[ASSERT Mysqlx.Sql.StmtExecuteOk]

#
# II.2
send Mysqlx.Crud.Insert {
  collection {
    name: "coll"
    schema: "xtest"
  }
  data_model: DOCUMENT
  row {
    field {
      type: LITERAL
      literal {
        type: V_STRING
        v_string {
          value: "{\"ints\":1}"
        }
      }
    }
  }
  row {
    field {
      type: LITERAL
      literal {
        type: V_STRING
        v_string {
          value: "{\"ints\":2}"
        }
      }
    }
  }
  row {
    field {
      type: LITERAL
      literal {
        type: V_STRING
        v_string {
          value: "{\"ints\":3}"
        }
      }
    }
  }
  row {
    field {
      type: LITERAL
      literal {
        type: V_STRING
        v_string {
          value: "{\"ints\":4}"
        }
      }
    }
  }
}

[ASSERT COMPRESSION]
[ASSERT COMPRESSION]
[ASSERT COMPRESSION]
[ASSERT Mysqlx.Sql.StmtExecuteOk]

#
# III.1
send Mysqlx.Crud.Find {
  collection {
    name: "coll"
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
            value: "not_existing_member"
          }
        }
      }
      param {
        type: LITERAL
        literal {
          type: V_OCTETS
          v_octets {
            value: "non-existing-value"
          }
        }
      }
    }
  }
}

[ASSERT COMPRESSION]
[ASSERT COMPRESSION]
[ASSERT Mysqlx.Sql.StmtExecuteOk]

#
# III.2
send Mysqlx.Crud.Find {
  collection {
    name: "coll"
    schema: "xtest"
  }
  data_model: DOCUMENT
  limit {
    row_count: 4
  }
}

[ASSERT COMPRESSION]
[ASSERT COMPRESSION]
[ASSERT COMPRESSION]
[ASSERT COMPRESSION]
[ASSERT COMPRESSION]
[ASSERT COMPRESSION]
[ASSERT Mysqlx.Sql.StmtExecuteOk]

#
# IV.1
send Mysqlx.Crud.Update {
  collection {
    name: "coll"
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
            value: "not_existing_member"
          }
        }
      }
      param {
        type: LITERAL
        literal {
          type: V_OCTETS
          v_octets {
            value: "non-existing-value"
          }
        }
      }
    }
  }
  operation {
    source {
      document_path {
        type: MEMBER
        value: "foo"
      }
    }
    operation: ITEM_SET
    value {
      type: LITERAL
      literal {
        type: V_NULL
      }
    }
  }
}

[ASSERT COMPRESSION]
[ASSERT COMPRESSION]
[ASSERT Mysqlx.Sql.StmtExecuteOk]

#
# IV.2
send Mysqlx.Crud.Update {
  collection {
    name: "coll"
    schema: "xtest"
  }
  data_model: DOCUMENT
  operation {
    source {
      document_path {
        type: MEMBER
        value: "foo2"
      }
    }
    operation: ITEM_SET
    value {
      type: LITERAL
      literal {
        type: V_NULL
      }
    }
  }
}

[ASSERT COMPRESSION]
[ASSERT COMPRESSION]
[ASSERT Mysqlx.Sql.StmtExecuteOk]

#
# V.1
send Mysqlx.Crud.Delete {
  collection {
    name: "coll"
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
            value: "not_existing_member"
          }
        }
      }
      param {
        type: LITERAL
        literal {
          type: V_OCTETS
          v_octets {
            value: "non-existing-value"
          }
        }
      }
    }
  }
}

[ASSERT COMPRESSION]
[ASSERT Mysqlx.Sql.StmtExecuteOk]

#
# V.2
send Mysqlx.Crud.Delete {
  collection {
    name: "coll"
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
            value: "prio"
          }
        }
      }
      param {
        type: LITERAL
        literal {
          type: V_SINT
          v_signed_int: 1
        }
      }
    }
  }
}

[ASSERT COMPRESSION]
[ASSERT Mysqlx.Sql.StmtExecuteOk]

#
# VI.1
send Mysqlx.Prepare.Prepare {
  stmt_id: 1
  stmt {
    type: STMT
    stmt_execute {
      stmt: "call out_param_no_select(10, ?)"
    }
  }
}

RUN recvok
send Mysqlx.Prepare.Execute {
  stmt_id: 1
  args {
    type: SCALAR
    scalar {
      type: V_STRING
      v_string {
        value: "@my_var"
      }
    }
  }
}

[ASSERT COMPRESSION]
[ASSERT COMPRESSION]
[ASSERT COMPRESSION]
[ASSERT COMPRESSION]
[ASSERT COMPRESSION]
[ASSERT Mysqlx.Sql.StmtExecuteOk]

#
# VI.2
send Mysqlx.Prepare.Prepare {
  stmt_id: 2
  stmt {
    type: STMT
    stmt_execute {
      stmt: "SELECT phrase FROM xtbl"
    }
  }
}

RUN recvok
send Mysqlx.Cursor.Open {
  cursor_id: 2
  stmt {
    type: PREPARE_EXECUTE
    prepare_execute {
      stmt_id: 2
    }
  }
  fetch_rows: 1
}

[ASSERT COMPRESSION]
[ASSERT COMPRESSION]
[ASSERT COMPRESSION]
[ASSERT Mysqlx.Sql.StmtExecuteOk]
Mysqlx.Ok {
  msg: "bye!"
}
ok
